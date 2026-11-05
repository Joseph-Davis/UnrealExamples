// Copyright 2026 CodeLikeMe. All Rights Reserved.

#include "Online/ALKOnlineSubsystem.h"
#include "Online/ALKOnlineConfig.h"
#include "Online/ALKOnlineProvider.h"
#include "Online/ALKSessionsOnlineProvider.h"
#include "Lobby/ALKLobbyGameState.h"
#include "Lobby/ALKLobbyPlayerState.h"
#include "Lobby/ALKNetGameModeBase.h"
#include "Lobby/ALKLobbyStatics.h"
#include "Lobby/ALKPlayerNetComponent.h"
#include "AssetproLobbyKit.h"
#include "ALKDeveloperSettings.h"
#include "ALKMenuConfig.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/NetDriver.h"
#include "Engine/PendingNetGame.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameMapsSettings.h"
#include "HAL/PlatformProcess.h"
#include "IPAddress.h"
#include "IpNetDriver.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/PackageName.h"
#include "Misc/Parse.h"
#include "SocketSubsystem.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ALKOnlineSubsystem)

#define LOCTEXT_NAMESPACE "ALKOnline"

namespace ALKOnlinePrivate
{
	/** Seconds a host or join may sit without the provider answering before it is given up. */
	static constexpr double ActionTimeout = 40.0;
	/** Seconds a kicked client waits for the server to close the connection before it leaves by itself. */
	static constexpr double KickGrace = 1.5;
	/** Seconds after a level load before a connection error is announced, so the menu is there to hear it. */
	static constexpr double AnnounceDelay = 0.3;
	/** Seconds between two tries to get back to the main menu level. */
	static constexpr double MenuTravelRetry = 3.0;
	/** Seconds between refreshes of the "last connected" time kept for Rejoin Match. */
	static constexpr double RejoinTouchInterval = 30.0;

	static FString StripPort(const FString& Address)
	{
		FString Host;
		FString Port;
		return Address.Split(TEXT(":"), &Host, &Port, ESearchCase::CaseSensitive, ESearchDir::FromEnd) ? Host : Address;
	}

	static AALKLobbyPlayerState* FindPlayerByUserId(const UWorld* World, const FString& UserId)
	{
		const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
		if (!GameState || UserId.IsEmpty())
		{
			return nullptr;
		}
		for (APlayerState* Each : GameState->PlayerArray)
		{
			AALKLobbyPlayerState* Player = Cast<AALKLobbyPlayerState>(Each);
			if (Player && Player->GetUserId() == UserId)
			{
				return Player;
			}
		}
		return nullptr;
	}
}

// ---------------------------------------------------------------- Lifecycle

void UALKOnlineSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	LoadSave();

	// A dedicated server takes its password from the command line: a password never goes into a level URL.
	FString CommandLinePassword;
	if (FParse::Value(FCommandLine::Get(), TEXT("ALKPassword="), CommandLinePassword))
	{
		ServerSettings.Password = CommandLinePassword;
	}

	const UALKOnlineConfig* Config = GetConfig();
	if (Config->bEnableOnline)
	{
		UClass* ProviderClass = Config->ProviderClass.Get();
		if (!ProviderClass || ProviderClass->HasAnyClassFlags(CLASS_Abstract))
		{
			ProviderClass = UALKSessionsOnlineProvider::StaticClass();
		}
		Provider = NewObject<UALKOnlineProvider>(this, ProviderClass);
		Provider->OnHostComplete.AddUObject(this, &ThisClass::HandleHostComplete);
		Provider->OnFindComplete.AddUObject(this, &ThisClass::HandleFindComplete);
		Provider->OnJoinComplete.AddUObject(this, &ThisClass::HandleJoinComplete);
		Provider->OnSessionDestroyed.AddUObject(this, &ThisClass::HandleSessionDestroyed);
		Provider->OnJoinRequested.AddUObject(this, &ThisClass::HandleJoinRequested);
		Provider->OnMatchFound.AddUObject(this, &ThisClass::HandleMatchFound);
		Provider->OnMatchAcceptProgress.AddUObject(this, &ThisClass::HandleMatchAcceptProgress);
		Provider->OnMatchmakingFailed.AddUObject(this, &ThisClass::HandleMatchmakingFailed);
		Provider->OnMatchReady.AddUObject(this, &ThisClass::HandleMatchReady);
		Provider->OnMatchHostRequested.AddUObject(this, &ThisClass::HandleMatchHostRequested);
		Provider->OnPartyChanged.AddUObject(this, &ThisClass::HandlePartyChanged);
		Provider->OnPartyInviteReceived.AddUObject(this, &ThisClass::HandlePartyInvite);
		Provider->OnPartyChatReceived.AddUObject(this, &ThisClass::HandlePartyChat);
		Provider->OnFriendsChanged.AddUObject(this, &ThisClass::HandleFriendsChanged);
		Provider->OnGameInviteReceived.AddUObject(this, &ThisClass::HandleGameInvite);
		Provider->OnWhisperReceived.AddUObject(this, &ThisClass::HandleWhisper);
		Provider->OnPlayerTalking.AddUObject(this, &ThisClass::HandlePlayerTalking);
		Provider->OnLeaderboardReceived.AddUObject(this, &ThisClass::HandleLeaderboard);
		Provider->InitializeProvider(GetGameInstance());
	}

	if (GEngine)
	{
		NetworkFailureHandle = GEngine->OnNetworkFailure().AddUObject(this, &ThisClass::HandleNetworkFailure);
		TravelFailureHandle = GEngine->OnTravelFailure().AddUObject(this, &ThisClass::HandleTravelFailure);
	}
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		PreClientTravelHandle = GameInstance->OnNotifyPreClientTravel().AddUObject(this, &ThisClass::HandlePreClientTravel);
	}
	PostLoadMapHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &ThisClass::HandlePostLoadMap);
	TickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &ThisClass::Tick), 0.1f);
}

void UALKOnlineSubsystem::Deinitialize()
{
	FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
	TickerHandle.Reset();
	FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadMapHandle);
	if (GEngine)
	{
		GEngine->OnNetworkFailure().Remove(NetworkFailureHandle);
		GEngine->OnTravelFailure().Remove(TravelFailureHandle);
	}
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		GameInstance->OnNotifyPreClientTravel().Remove(PreClientTravelHandle);
	}

	if (Provider)
	{
		// Do not leave a session behind on the service.
		if (State != EALKOnlineState::Idle)
		{
			Provider->LeaveSession();
		}
		Provider->OnHostComplete.RemoveAll(this);
		Provider->OnFindComplete.RemoveAll(this);
		Provider->OnJoinComplete.RemoveAll(this);
		Provider->OnSessionDestroyed.RemoveAll(this);
		Provider->OnJoinRequested.RemoveAll(this);
		Provider->OnMatchFound.RemoveAll(this);
		Provider->OnMatchAcceptProgress.RemoveAll(this);
		Provider->OnMatchmakingFailed.RemoveAll(this);
		Provider->OnMatchReady.RemoveAll(this);
		Provider->OnMatchHostRequested.RemoveAll(this);
		Provider->OnPartyChanged.RemoveAll(this);
		Provider->OnPartyInviteReceived.RemoveAll(this);
		Provider->OnPartyChatReceived.RemoveAll(this);
		Provider->OnFriendsChanged.RemoveAll(this);
		Provider->OnGameInviteReceived.RemoveAll(this);
		Provider->OnWhisperReceived.RemoveAll(this);
		Provider->OnPlayerTalking.RemoveAll(this);
		Provider->OnLeaderboardReceived.RemoveAll(this);
		Provider->ShutdownProvider();
		Provider = nullptr;
	}

	if (Save && bSaveDirty)
	{
		// The last changes, written before the game goes away.
		UGameplayStatics::SaveGameToSlot(Save, GetSaveSlotName(), 0);
		bSaveDirty = false;
	}
	Super::Deinitialize();
}

UALKOnlineSubsystem* UALKOnlineSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = GEngine && WorldContextObject ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<UALKOnlineSubsystem>() : nullptr;
}

UWorld* UALKOnlineSubsystem::GetGameWorld() const
{
	return GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
}

APlayerController* UALKOnlineSubsystem::GetLocalController() const
{
	return GetGameInstance() ? GetGameInstance()->GetFirstLocalPlayerController() : nullptr;
}

const UALKOnlineConfig* UALKOnlineSubsystem::GetConfig() const
{
	return UALKDeveloperSettings::GetOnlineConfig();
}

UALKPlayerNetComponent* UALKOnlineSubsystem::GetLocalNetComponent() const
{
	return UALKPlayerNetComponent::Get(GetLocalController());
}

// ---------------------------------------------------------------- State

void UALKOnlineSubsystem::SetState(EALKOnlineState NewState)
{
	if (State == NewState)
	{
		return;
	}
	const EALKOnlineState OldState = State;
	State = NewState;
	StateEnterTime = FPlatformTime::Seconds();
	UE_LOG(LogALK, Log, TEXT("Online state: %s -> %s"), *UEnum::GetValueAsString(OldState), *UEnum::GetValueAsString(NewState));

	// A new game starts with an empty chat: the last game's lines (All, Team and System) are forgotten. Party and
	// Whisper lines belong to people, not to a game, and stay.
	if (NewState == EALKOnlineState::Hosting || NewState == EALKOnlineState::Joining)
	{
		ChatHistory.RemoveAll([](const FALKChatMessage& Message)
		{
			return Message.Channel != EALKChatChannel::Party && Message.Channel != EALKChatChannel::Whisper;
		});
	}

	// A quick match that reached a game (or fell back to the menu) is over.
	if (MatchmakingState == EALKMatchmakingState::Joining && (NewState == EALKOnlineState::InLobby || NewState == EALKOnlineState::InMatch || NewState == EALKOnlineState::Idle))
	{
		SetMatchmakingState(EALKMatchmakingState::Idle);
	}
	OnStateChanged.Broadcast(NewState, OldState);
	OnStateChangedNative.Broadcast(NewState, OldState);
}

void UALKOnlineSubsystem::SetMatchmakingState(EALKMatchmakingState NewState)
{
	if (MatchmakingState != NewState)
	{
		MatchmakingState = NewState;
		BroadcastMatchmakingChanged();
	}
}

void UALKOnlineSubsystem::BroadcastMatchmakingChanged()
{
	OnMatchmakingChanged.Broadcast(MatchmakingState);
	OnMatchmakingChangedNative.Broadcast(MatchmakingState);
}

bool UALKOnlineSubsystem::IsHost() const
{
	if (bIsHost)
	{
		return true;
	}
	const APlayerController* Controller = GetLocalController();
	const AALKLobbyPlayerState* Player = Controller ? Controller->GetPlayerState<AALKLobbyPlayerState>() : nullptr;
	return Player && Player->bHost;
}

FString UALKOnlineSubsystem::GetLocalUserId() const
{
	return Provider ? Provider->GetLocalUserId() : FString(TEXT("LocalPlayer"));
}

FString UALKOnlineSubsystem::GetLocalDisplayName() const
{
	if (Save && !Save->PlayerName.IsEmpty())
	{
		return Save->PlayerName;
	}
	return Provider ? Provider->GetLocalDisplayName() : FString(FPlatformProcess::UserName());
}

void UALKOnlineSubsystem::SetPlayerName(const FString& Name)
{
	if (!Save)
	{
		return;
	}
	const FString Clean = UALKLobbyStatics::SanitizePlayerName(Name, GetConfig()->PlayerNameMaxLength);
	if (Save->PlayerName == Clean)
	{
		return;
	}
	Save->PlayerName = Clean;
	SaveNow();
	if (UALKPlayerNetComponent* NetComponent = GetLocalNetComponent())
	{
		NetComponent->ServerSetPlayerName(GetLocalDisplayName());
	}
}

bool UALKOnlineSubsystem::CanStartOnlineAction() const
{
	return Provider && !bReturnToMenuPending && (State == EALKOnlineState::Idle || State == EALKOnlineState::Searching);
}

void UALKOnlineSubsystem::ResetGameState()
{
	// A hosted game's password ends with the game (a dedicated server keeps the one it was started with).
	if (bIsHost && !bDedicatedHost)
	{
		ServerSettings.Password.Reset();
	}
	bIsHost = false;
	bDedicatedHost = false;
	bRejoining = false;
	bJoinRemembered = false;
	JoinedSession = FALKSessionInfo();
	JoinedConnectString.Reset();
	JoinPassword.Reset();
	Roster.Reset();
	KickError = EALKConnectionError::None;
	KickDeadline = 0.0;
}

// ---------------------------------------------------------------- Host

bool UALKOnlineSubsystem::HostGame(const FALKHostSettings& Settings)
{
	if (!CanStartOnlineAction())
	{
		return false;
	}
	const UALKOnlineConfig* Config = GetConfig();
	if (Config->GetLobbyTravelURL().IsEmpty())
	{
		UE_LOG(LogALK, Warning, TEXT("Host Game: set Lobby Level in your Assetpro Online Config (a level whose game mode is ALK Lobby Game Mode), or turn Use Empty Level Fallback back on."));
		return false;
	}
	if (State == EALKOnlineState::Searching)
	{
		Provider->CancelFindSessions();
	}

	ServerSettings = Config->SanitizeHostSettings(Settings, GetLocalDisplayName());
	if (Save)
	{
		Save->LastHostSettings = ServerSettings;
		Save->LastHostSettings.Password.Reset();
		Save->bHasLastHostSettings = true;
		bSaveDirty = true;
	}
	bDedicatedHost = false;
	SetState(EALKOnlineState::Hosting);
	Provider->HostSession(ServerSettings);
	return true;
}

void UALKOnlineSubsystem::RegisterDedicatedServer(const FALKHostSettings& Settings)
{
	if (!Provider || bIsHost || State != EALKOnlineState::Idle)
	{
		return;
	}
	const FString Password = ServerSettings.Password;
	ServerSettings = GetConfig()->SanitizeHostSettings(Settings, FString());
	if (ServerSettings.Password.IsEmpty())
	{
		ServerSettings.Password = Password;
	}
	bDedicatedHost = true;
	SetState(EALKOnlineState::Hosting);
	Provider->HostSession(ServerSettings);
}

void UALKOnlineSubsystem::HandleHostComplete(bool bSuccess, EALKConnectionError Error)
{
	if (State != EALKOnlineState::Hosting)
	{
		return;
	}
	if (!bSuccess)
	{
		if (MatchmakingState == EALKMatchmakingState::Joining)
		{
			LastMatchmakingError = Error;
			SetMatchmakingState(EALKMatchmakingState::Failed);
		}
		FailWithError(Error, false);
		return;
	}

	bIsHost = true;
	if (bDedicatedHost)
	{
		// Already in the level: nothing to open.
		SetState(EALKOnlineState::InLobby);
		return;
	}
	if (!OpenLobbyAsHost())
	{
		FailWithError(EALKConnectionError::Unknown, false);
	}
}

bool UALKOnlineSubsystem::OpenLobbyAsHost()
{
	// The lobby level, or the engine's empty level with the lobby game mode forced on it when none is set.
	const FString Level = GetConfig()->GetLobbyTravelURL();
	UWorld* World = GetGameWorld();
	if (Level.IsEmpty() || !World || !GEngine)
	{
		return false;
	}
	// The password stays in this subsystem: level URLs are passed on to clients on every server travel.
	const FString URL = Level + TEXT("?listen") + ServerSettings.ToTravelOptions();
	if (APlayerController* Controller = GetLocalController())
	{
		Controller->ClientTravel(URL, TRAVEL_Absolute);
	}
	else
	{
		GEngine->SetClientTravel(World, *URL, TRAVEL_Absolute);
	}
	return true;
}

void UALKOnlineSubsystem::SetServerSettings(const FALKHostSettings& Settings, bool bUpdateSession)
{
	ServerSettings = Settings;
	if (bUpdateSession && Provider && bIsHost)
	{
		Provider->UpdateSession(ServerSettings);
	}
}

void UALKOnlineSubsystem::NotifyServerMatchStarted()
{
	if (Provider && bIsHost)
	{
		Provider->StartSession();
	}
}

void UALKOnlineSubsystem::NotifyServerMatchEnded()
{
	if (Provider && bIsHost)
	{
		Provider->EndSession();
	}
}

bool UALKOnlineSubsystem::FindRosterEntry(const FString& UserId, FALKRosterEntry& OutEntry) const
{
	const FALKRosterEntry* Found = UserId.IsEmpty() ? nullptr : Roster.FindByPredicate([&UserId](const FALKRosterEntry& Each) { return Each.UserId == UserId; });
	if (Found)
	{
		OutEntry = *Found;
	}
	return Found != nullptr;
}

// ---------------------------------------------------------------- Find

void UALKOnlineSubsystem::FindGames(const FALKSessionFilter& Filter)
{
	SearchFilter = Filter;
	if (!CanStartOnlineAction())
	{
		if (!Provider)
		{
			const TArray<FALKSessionInfo> Nothing;
			OnSessionsFound.Broadcast(Nothing);
			OnSessionsFoundNative.Broadcast(Nothing);
		}
		return;
	}
	SetState(EALKOnlineState::Searching);
	Provider->FindSessions(Filter);
}

void UALKOnlineSubsystem::CancelFindGames()
{
	if (Provider && State == EALKOnlineState::Searching)
	{
		Provider->CancelFindSessions();
		SetState(EALKOnlineState::Idle);
	}
}

void UALKOnlineSubsystem::HandleFindComplete(bool bSuccess, const TArray<FALKSessionInfo>& Results)
{
	SearchResults = Results;

	// Favourites that were found again: their picture and address are refreshed.
	if (Save && Provider)
	{
		for (FALKServerBookmark& Bookmark : Save->FavouriteServers)
		{
			const FString Key = Bookmark.Session.GetFavouriteKey();
			if (const FALKSessionInfo* Live = Results.FindByPredicate([&Key](const FALKSessionInfo& Each) { return Each.GetFavouriteKey() == Key; }))
			{
				Bookmark.Session = *Live;
				FString ConnectString;
				if (Provider->GetSessionConnectString(*Live, ConnectString))
				{
					Bookmark.ConnectString = ConnectString;
				}
				bSaveDirty = true;
			}
		}
	}

	if (State == EALKOnlineState::Searching)
	{
		SetState(EALKOnlineState::Idle);
	}
	const TArray<FALKSessionInfo> Shown = GetLastSearchResults();
	OnSessionsFound.Broadcast(Shown);
	OnSessionsFoundNative.Broadcast(Shown);
}

TArray<FALKSessionInfo> UALKOnlineSubsystem::GetLastSearchResults() const
{
	return FALKSessionFilter::Apply(SearchFilter, SearchResults);
}

TArray<FALKSessionInfo> UALKOnlineSubsystem::GetSearchResults(const FALKSessionFilter& Filter, EALKSessionSort SortBy, bool bAscending) const
{
	TArray<FALKSessionInfo> Result = FALKSessionFilter::Apply(Filter, SearchResults);
	FALKSessionFilter::Sort(Result, SortBy, bAscending);
	return Result;
}

// ---------------------------------------------------------------- Join

bool UALKOnlineSubsystem::JoinGame(const FALKSessionInfo& Session, const FString& Password)
{
	if (!CanStartOnlineAction())
	{
		return false;
	}
	// What can be known before asking the host.
	if (!Session.bVersionMatches)
	{
		FailWithError(EALKConnectionError::WrongVersion, false);
		return false;
	}
	if (Session.IsFull())
	{
		FailWithError(EALKConnectionError::ServerFull, false);
		return false;
	}
	if (Session.bPasswordProtected && Password.IsEmpty())
	{
		FailWithError(EALKConnectionError::WrongPassword, false);
		return false;
	}
	if (State == EALKOnlineState::Searching)
	{
		Provider->CancelFindSessions();
	}

	ResetGameState();
	JoinedSession = Session;
	JoinPassword = Password;
	SetState(EALKOnlineState::Joining);
	Provider->JoinSession(Session, Password);
	return true;
}

bool UALKOnlineSubsystem::JoinByCode(const FString& CodeOrAddress, const FString& Password)
{
	if (!CanStartOnlineAction())
	{
		return false;
	}
	const FString Text = CodeOrAddress.TrimStartAndEnd();
	if (Text.IsEmpty())
	{
		return false;
	}
	if (State == EALKOnlineState::Searching)
	{
		Provider->CancelFindSessions();
	}

	ResetGameState();
	JoinPassword = Password;
	SetState(EALKOnlineState::Joining);

	// A backend with codes of its own takes it first.
	if (Provider->JoinByCode(Text, Password))
	{
		return true;
	}
	FString Address;
	if (State == EALKOnlineState::Joining && FALKLobbyCode::ResolveJoinTarget(Text, Address) && TravelToServer(Address))
	{
		return true;
	}
	if (State == EALKOnlineState::Joining)
	{
		FailWithError(EALKConnectionError::SessionNotFound, false);
	}
	return false;
}

bool UALKOnlineSubsystem::JoinBookmark(const FALKServerBookmark& Bookmark, const FString& Password)
{
	// The live game, when the last search saw it: joined through the session like any other.
	const FString Key = Bookmark.Session.GetFavouriteKey();
	if (const FALKSessionInfo* Live = SearchResults.FindByPredicate([&Key](const FALKSessionInfo& Each) { return Each.GetFavouriteKey() == Key; }))
	{
		return JoinGame(*Live, Password);
	}
	if (!Bookmark.ConnectString.IsEmpty())
	{
		const FALKSessionInfo Remembered = Bookmark.Session;
		if (JoinByCode(Bookmark.ConnectString, Password))
		{
			JoinedSession = Remembered;
			return true;
		}
		return false;
	}
	if (CanStartOnlineAction())
	{
		FailWithError(EALKConnectionError::SessionNotFound, false);
	}
	return false;
}

void UALKOnlineSubsystem::HandleJoinComplete(bool bSuccess, EALKConnectionError Error, const FString& ConnectString)
{
	if (State != EALKOnlineState::Joining)
	{
		return;
	}
	if (bSuccess && TravelToServer(ConnectString))
	{
		return;
	}
	// A rejoin through the session failed: the remembered address may still work.
	if (bRejoining && Save && !Save->RejoinConnectString.IsEmpty())
	{
		bRejoining = false;
		if (TravelToServer(Save->RejoinConnectString))
		{
			return;
		}
	}
	FailWithError(bSuccess ? EALKConnectionError::NetworkError : Error, false);
}

bool UALKOnlineSubsystem::TravelToServer(const FString& Address)
{
	UWorld* World = GetGameWorld();
	if (Address.IsEmpty() || !World || !GEngine)
	{
		return false;
	}
	FString URL = Address;
	URL += FALKOptionString::MakeOption(ALKNetOption::Version, GetConfig()->GetResolvedBuildVersion());
	if (!JoinPassword.IsEmpty())
	{
		URL += FALKOptionString::MakeOption(ALKNetOption::Password, JoinPassword);
	}
	JoinedConnectString = Address;
	UE_LOG(LogALK, Log, TEXT("Joining %s"), *Address);
	if (APlayerController* Controller = GetLocalController())
	{
		Controller->ClientTravel(URL, TRAVEL_Absolute);
	}
	else
	{
		GEngine->SetClientTravel(World, *URL, TRAVEL_Absolute);
	}
	return true;
}

void UALKOnlineSubsystem::HandleJoinRequested(const FALKSessionInfo& Session)
{
	if (State == EALKOnlineState::Hosting || State == EALKOnlineState::Joining || State == EALKOnlineState::Leaving)
	{
		return;
	}
	if (IsInGame())
	{
		// Out of the current game first; the join follows once the menu is back.
		QueuedJoin = Session;
		bHasQueuedJoin = true;
		LeaveGame();
		return;
	}
	if (MatchmakingState != EALKMatchmakingState::Idle)
	{
		CancelQuickMatch();
	}
	JoinGame(Session, FString());
}

// ---------------------------------------------------------------- Leave

void UALKOnlineSubsystem::LeaveGame()
{
	if (State == EALKOnlineState::Idle || State == EALKOnlineState::Leaving)
	{
		return;
	}
	UWorld* World = GetGameWorld();
	// A join that is still connecting has not left the menus, although the world already reports a net mode (taken from the
	// pending connection): stop the attempt here. Travelling to the menu instead would load it as a client's level, with no
	// game mode and no menu, until the attempt times out.
	FWorldContext* Context = World && GEngine ? GEngine->GetWorldContextFromWorld(World) : nullptr;
	const bool bStillConnecting = State == EALKOnlineState::Joining && Context && !World->GetNetDriver() && (Context->PendingNetGame != nullptr || !Context->TravelURL.IsEmpty());
	if (bStillConnecting)
	{
		GEngine->CancelPending(World);
		Context->TravelURL.Reset();
	}
	const bool bInNetGame = World && !bStillConnecting && World->GetNetMode() != NM_Standalone;

	// Leaving on purpose: nothing to rejoin.
	ClearRejoin();
	if (MatchmakingState != EALKMatchmakingState::Idle)
	{
		CancelQuickMatch();
	}
	if (Provider)
	{
		if (State == EALKOnlineState::Searching)
		{
			Provider->CancelFindSessions();
		}
		Provider->LeaveSession();
	}

	if (!bInNetGame)
	{
		// Still in the menus (a host or join that had not travelled yet).
		ResetGameState();
		SetState(EALKOnlineState::Idle);
		return;
	}

	if (bIsHost)
	{
		// Tell the clients why the game ends before the connection closes.
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			APlayerController* Controller = It->Get();
			if (Controller && !Controller->IsLocalController())
			{
				if (UALKPlayerNetComponent* NetComponent = UALKPlayerNetComponent::Get(Controller))
				{
					NetComponent->ClientKicked(ALKNetReason::HostLeft);
				}
			}
		}
	}
	SetState(EALKOnlineState::Leaving);
	bReturnToMenuPending = true;
	RequestMenuTravel();
}

void UALKOnlineSubsystem::HandleSessionDestroyed()
{
	// The travel back to the menu is driven by the state; nothing else waits for this.
}

FString UALKOnlineSubsystem::GetMenuLevelPath() const
{
	const FString Menu = UALKDeveloperSettings::GetMenuConfig()->MainMenuLevel.ToSoftObjectPath().GetLongPackageName();
	if (!Menu.IsEmpty())
	{
		return Menu;
	}
	// No main menu level set: the project's default map is the menu.
	return FPackageName::ObjectPathToPackageName(UGameMapsSettings::GetGameDefaultMap());
}

bool UALKOnlineSubsystem::IsMenuWorld(const UWorld* World) const
{
	if (!World)
	{
		return false;
	}
	// A lobby or match can run in the very level the menu uses (the empty level fallback): the game mode tells them apart.
	if (World->GetAuthGameMode<AALKNetGameModeBase>())
	{
		return false;
	}
	const FString Menu = GetMenuLevelPath();
	return Menu.IsEmpty() || UWorld::RemovePIEPrefix(World->GetOutermost()->GetName()) == Menu;
}

void UALKOnlineSubsystem::RequestMenuTravel()
{
	UWorld* World = GetGameWorld();
	const FString Menu = GetMenuLevelPath();
	LastMenuTravelRequest = FPlatformTime::Seconds();
	if (!World || !GEngine || Menu.IsEmpty())
	{
		return;
	}
	// An absolute travel to a local level closes the connection (or the listen server) on the way.
	GEngine->SetClientTravel(World, *Menu, TRAVEL_Absolute);
}

// ---------------------------------------------------------------- Failures

void UALKOnlineSubsystem::FailWithError(EALKConnectionError Error, bool bReturnToMenu)
{
	// What the server said just before closing the connection is the better reason.
	const bool bServerSaidWhy = KickError != EALKConnectionError::None;
	if (bServerSaidWhy)
	{
		Error = KickError;
	}
	if (Error == EALKConnectionError::None)
	{
		Error = EALKConnectionError::Unknown;
	}
	UE_LOG(LogALK, Log, TEXT("Online error: %s"), *UEnum::GetValueAsString(Error));

	const bool bWasClientInGame = IsInGame() && !bIsHost;
	PendingError = Error;
	bAnnounceErrorPending = true;
	KickError = EALKConnectionError::None;
	KickDeadline = 0.0;

	// A dropped connection can be rejoined. A game the server ended or removed this player from cannot.
	const bool bDropped = Error == EALKConnectionError::TimedOut || Error == EALKConnectionError::NetworkError || Error == EALKConnectionError::HostLeft;
	if (bServerSaidWhy || Error == EALKConnectionError::Kicked || Error == EALKConnectionError::Banned)
	{
		ClearRejoin();
	}
	else if (bWasClientInGame && bDropped)
	{
		TouchRejoinInfo(true);
	}

	if (MatchmakingState != EALKMatchmakingState::Idle && MatchmakingState != EALKMatchmakingState::Failed)
	{
		SetMatchmakingState(EALKMatchmakingState::Idle);
	}
	bHasQueuedJoin = false;
	if (Provider)
	{
		Provider->LeaveSession();
	}

	if (bReturnToMenu)
	{
		bReturnToMenuPending = true;
		SetState(EALKOnlineState::Leaving);
	}
	else
	{
		ResetGameState();
		SetState(EALKOnlineState::Idle);
	}
}

void UALKOnlineSubsystem::SimulateConnectionError(EALKConnectionError Error)
{
	if (Error == EALKConnectionError::None || bReturnToMenuPending)
	{
		return;
	}
	const UWorld* World = GetGameWorld();
	FailWithError(Error, World && (World->GetNetMode() != NM_Standalone || !IsMenuWorld(World)));
}

bool UALKOnlineSubsystem::ConsumePendingConnectionError(EALKConnectionError& OutError, FText& OutTitle, FText& OutMessage)
{
	OutError = PendingError;
	FALKConnectionErrors::GetText(PendingError, OutTitle, OutMessage);
	if (PendingError == EALKConnectionError::None)
	{
		return false;
	}
	PendingError = EALKConnectionError::None;
	bAnnounceErrorPending = false;
	return true;
}

bool UALKOnlineSubsystem::IsOurWorld(const UWorld* World, const UNetDriver* NetDriver) const
{
	if (World)
	{
		return World->GetGameInstance() == GetGameInstance();
	}
	// A connection that never got a world (still connecting): find its game by the net driver.
	if (NetDriver && GEngine)
	{
		if (const FWorldContext* Context = GEngine->GetWorldContextFromPendingNetGameNetDriver(NetDriver))
		{
			return Context->OwningGameInstance == GetGameInstance();
		}
	}
	return State == EALKOnlineState::Joining;
}

void UALKOnlineSubsystem::HandleNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString)
{
	if (State == EALKOnlineState::Idle || State == EALKOnlineState::Searching || bReturnToMenuPending || !IsOurWorld(World, NetDriver))
	{
		return;
	}
	// Only the game's own connection: beacons and replays have their own handling.
	if (NetDriver && NetDriver->NetDriverName != NAME_GameNetDriver && NetDriver->NetDriverName != NAME_PendingNetDriver)
	{
		return;
	}
	const bool bServer = NetDriver && NetDriver->GetNetMode() != NM_Client;

	EALKConnectionError Error = EALKConnectionError::NetworkError;
	switch (FailureType)
	{
	case ENetworkFailure::ConnectionLost:
		if (bServer)
		{
			// A client dropped: the host carries on.
			return;
		}
		// The usual reason a client loses the server is that the host closed the game.
		Error = EALKConnectionError::HostLeft;
		break;

	case ENetworkFailure::ConnectionTimeout:
		if (bServer)
		{
			return;
		}
		Error = EALKConnectionError::TimedOut;
		break;

	case ENetworkFailure::FailureReceived:
	case ENetworkFailure::PendingConnectionFailure:
		// The text is what the server's Pre Login answered (the ALKNetReason strings), or the engine's own message.
		Error = FALKConnectionErrors::FromReason(ErrorString);
		if (Error == EALKConnectionError::None || Error == EALKConnectionError::Unknown)
		{
			Error = EALKConnectionError::NetworkError;
		}
		break;

	case ENetworkFailure::OutdatedClient:
	case ENetworkFailure::OutdatedServer:
		Error = EALKConnectionError::WrongVersion;
		break;

	case ENetworkFailure::NetGuidMismatch:
	case ENetworkFailure::NetChecksumMismatch:
		if (bServer)
		{
			return;
		}
		// The two games do not have the same content.
		Error = EALKConnectionError::WrongVersion;
		break;

	default:
		// Net driver could not be created or could not listen (the port is taken...).
		Error = EALKConnectionError::NetworkError;
		break;
	}

	UE_LOG(LogALK, Log, TEXT("Network failure %s: '%s'"), ENetworkFailure::ToString(FailureType), *ErrorString);
	FailWithError(Error, true);
}

void UALKOnlineSubsystem::HandleTravelFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& ErrorString)
{
	if (State == EALKOnlineState::Idle || State == EALKOnlineState::Searching || bReturnToMenuPending || !World || World->GetGameInstance() != GetGameInstance())
	{
		return;
	}
	EALKConnectionError Error = EALKConnectionError::NetworkError;
	switch (FailureType)
	{
	case ETravelFailure::PackageMissing:
	case ETravelFailure::PackageVersion:
		// The host's level is not in this game: another version.
		Error = EALKConnectionError::WrongVersion;
		break;
	case ETravelFailure::NoLevel:
	case ETravelFailure::LoadMapFailure:
	case ETravelFailure::InvalidURL:
		Error = bIsHost ? EALKConnectionError::Unknown : EALKConnectionError::SessionNotFound;
		break;
	default:
		break;
	}
	UE_LOG(LogALK, Log, TEXT("Travel failure %s: '%s'"), ETravelFailure::ToString(FailureType), *ErrorString);
	FailWithError(Error, true);
}

void UALKOnlineSubsystem::HandlePreClientTravel(const FString& PendingURL, ETravelType TravelType, bool bIsSeamlessTravel)
{
	// The level is about to change (a join, the lobby starting the match, the return to the lobby): whatever connection
	// error is waiting should be announced after it, not into a menu that is about to go away.
	LastMapLoadTime = FPlatformTime::Seconds();
}

void UALKOnlineSubsystem::HandlePostLoadMap(UWorld* World)
{
	if (!World || World->GetGameInstance() != GetGameInstance())
	{
		return;
	}
	LastMapLoadTime = FPlatformTime::Seconds();
	if (bReturnToMenuPending)
	{
		// Tick finishes the way back.
		return;
	}

	if (World->GetNetMode() == NM_Standalone)
	{
		// Out of the game without Leave Game (your own code opened a level): tidy up.
		if (IsInGame() || State == EALKOnlineState::Leaving)
		{
			if (Provider)
			{
				Provider->LeaveSession();
			}
			ResetGameState();
			SetState(EALKOnlineState::Idle);
		}
	}
	else if (State == EALKOnlineState::Hosting || State == EALKOnlineState::Joining)
	{
		// Arrived. The ALK game state says "match" a moment later when the level is not a lobby.
		SetState(EALKOnlineState::InLobby);
		if (World->GetNetMode() == NM_Client)
		{
			RememberJoinedServer();
		}
	}
}

void UALKOnlineSubsystem::NotifyLobbyPhase(EALKLobbyPhase Phase)
{
	const UWorld* World = GetGameWorld();
	if (bReturnToMenuPending || State == EALKOnlineState::Leaving || !World || World->GetNetMode() == NM_Standalone)
	{
		return;
	}
	const bool bMatch = Phase == EALKLobbyPhase::InMatch || Phase == EALKLobbyPhase::PostMatch;
	SetState(bMatch ? EALKOnlineState::InMatch : EALKOnlineState::InLobby);
	if (World->GetNetMode() == NM_Client)
	{
		RememberJoinedServer();
	}
	else
	{
		bIsHost = true;
	}
}

void UALKOnlineSubsystem::NotifyKicked(const FString& Reason)
{
	EALKConnectionError Error = FALKConnectionErrors::FromReason(Reason);
	if (Error == EALKConnectionError::None || Error == EALKConnectionError::Unknown)
	{
		Error = EALKConnectionError::Kicked;
	}
	KickError = Error;
	// The server closes the connection next; if it does not, this client leaves by itself.
	KickDeadline = FPlatformTime::Seconds() + ALKOnlinePrivate::KickGrace;
}

bool UALKOnlineSubsystem::Tick(float DeltaTime)
{
	const double Now = FPlatformTime::Seconds();
	UWorld* World = GetGameWorld();

	// A found match nobody answered: the provider normally ends it; this is the safety net.
	if (MatchmakingState == EALKMatchmakingState::MatchFound && Now > MatchAcceptDeadline + 1.0)
	{
		RespondToMatch(false);
	}

	if (KickError != EALKConnectionError::None && KickDeadline > 0.0 && Now >= KickDeadline && !bReturnToMenuPending)
	{
		FailWithError(KickError, true);
	}

	if (bSaveDirty && Now - LastSaveTime > 2.0)
	{
		SaveNow();
	}

	if (IsInGame() && !bIsHost && World && World->GetNetMode() == NM_Client && Now - LastRejoinTouch > ALKOnlinePrivate::RejoinTouchInterval)
	{
		TouchRejoinInfo(true);
	}

	const FWorldContext* Context = World && GEngine ? GEngine->GetWorldContextFromWorld(World) : nullptr;
	if (!Context || !Context->TravelURL.IsEmpty() || Context->PendingNetGame != nullptr || World->IsInSeamlessTravel())
	{
		// A level change or a connection attempt is under way: everything below waits for it.
		return true;
	}

	if ((State == EALKOnlineState::Hosting || State == EALKOnlineState::Joining) && World->GetNetMode() == NM_Standalone && Now - StateEnterTime > ALKOnlinePrivate::ActionTimeout)
	{
		UE_LOG(LogALK, Warning, TEXT("The online provider did not answer in time."));
		FailWithError(EALKConnectionError::TimedOut, false);
	}

	if (bReturnToMenuPending)
	{
		if (World->GetNetMode() != NM_Standalone || !IsMenuWorld(World))
		{
			// Still in the game level (or the engine fell back to another level): go to the main menu.
			if (Now - LastMenuTravelRequest > ALKOnlinePrivate::MenuTravelRetry)
			{
				RequestMenuTravel();
			}
			return true;
		}
		bReturnToMenuPending = false;
		ResetGameState();
		SetState(EALKOnlineState::Idle);

		if (bHasQueuedJoin)
		{
			bHasQueuedJoin = false;
			JoinGame(QueuedJoin, FString());
		}
	}

	if (bAnnounceErrorPending && PendingError != EALKConnectionError::None && Now - LastMapLoadTime > ALKOnlinePrivate::AnnounceDelay && World->HasBegunPlay() && GetLocalController())
	{
		bAnnounceErrorPending = false;
		FText Title;
		FText Message;
		FALKConnectionErrors::GetText(PendingError, Title, Message);
		const EALKConnectionError Error = PendingError;
		OnConnectionError.Broadcast(Error, Title, Message);
		OnConnectionErrorNative.Broadcast(Error, Title, Message);
	}
	return true;
}

// ---------------------------------------------------------------- The current game

FALKSessionInfo UALKOnlineSubsystem::GetCurrentSession() const
{
	FALKSessionInfo Info;
	if (State == EALKOnlineState::Idle || State == EALKOnlineState::Searching)
	{
		return Info;
	}
	const UALKOnlineConfig* Config = GetConfig();
	if (bIsHost)
	{
		if (Provider)
		{
			Provider->GetCurrentSession(Info);
		}
		Info.ServerName = ServerSettings.ServerName;
		Info.HostName = GetLocalDisplayName();
		Info.HostUserId = GetLocalUserId();
		Info.ModeId = ServerSettings.ModeId;
		Info.MapId = ServerSettings.MapId;
		Info.MaxPlayers = ServerSettings.MaxPlayers;
		Info.bPasswordProtected = ServerSettings.HasPassword();
		Info.bLAN = ServerSettings.bLAN;
		Info.Rules = ServerSettings.Rules.ToMap();
		Info.PingMs = 0;
	}
	else
	{
		Info = JoinedSession;
	}

	// What the game itself says is newer than what the browser showed.
	const UWorld* World = GetGameWorld();
	if (const AALKLobbyGameState* GameState = World ? World->GetGameState<AALKLobbyGameState>() : nullptr)
	{
		if (!GameState->ServerName.IsEmpty())
		{
			Info.ServerName = GameState->ServerName;
		}
		Info.ModeId = GameState->ModeId;
		Info.MapId = GameState->MapId;
		Info.MaxPlayers = GameState->MaxPlayers;
		Info.CurrentPlayers = GameState->PlayerArray.Num();
		Info.bPasswordProtected = GameState->bPasswordProtected;
		Info.Rules = GameState->Rules.ToMap();
		Info.bInProgress = GameState->Phase == EALKLobbyPhase::InMatch || GameState->Phase == EALKLobbyPhase::PostMatch;
	}
	Info.BuildVersion = Config->GetResolvedBuildVersion();
	Info.bVersionMatches = true;
	return Info;
}

FString UALKOnlineSubsystem::MakeHostLobbyCode() const
{
	if (Provider)
	{
		const FString ProviderCode = Provider->GetLobbyCode();
		if (!ProviderCode.IsEmpty())
		{
			return ProviderCode;
		}
	}
	UWorld* World = GetGameWorld();
	UNetDriver* NetDriver = World ? World->GetNetDriver() : nullptr;
	// An address only helps when players connect by address: the engine's plain IP net driver.
	if (!NetDriver || NetDriver->GetClass() != UIpNetDriver::StaticClass() || World->GetNetMode() == NM_Client)
	{
		return FString();
	}
	ISocketSubsystem* Sockets = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	if (!Sockets)
	{
		return FString();
	}
	bool bCanBindAll = false;
	const TSharedRef<FInternetAddr> LocalAddress = Sockets->GetLocalHostAddr(*GLog, bCanBindAll);
	uint32 IPv4 = 0;
	if (LocalAddress->IsValid())
	{
		LocalAddress->GetIp(IPv4);
	}
	if (IPv4 == 0)
	{
		return FString();
	}
	const TSharedPtr<const FInternetAddr> Bound = NetDriver->GetLocalAddr();
	const int32 Port = Bound.IsValid() && Bound->GetPort() > 0 ? Bound->GetPort() : World->URL.Port;
	return FALKLobbyCode::EncodeIPv4(IPv4, Port);
}

FString UALKOnlineSubsystem::GetLobbyCode() const
{
	const UWorld* World = GetGameWorld();
	if (const AALKLobbyGameState* GameState = World ? World->GetGameState<AALKLobbyGameState>() : nullptr)
	{
		if (!GameState->LobbyCode.IsEmpty())
		{
			return GameState->LobbyCode;
		}
	}
	return bIsHost ? MakeHostLobbyCode() : FString();
}

FString UALKOnlineSubsystem::GetDisplayLobbyCode() const
{
	const FString Code = GetLobbyCode();
	return bStreamerMode ? FALKLobbyCode::Mask(Code) : Code;
}

void UALKOnlineSubsystem::RememberJoinedServer()
{
	UWorld* World = GetGameWorld();
	if (!Save || !World || World->GetNetMode() != NM_Client)
	{
		return;
	}
	if (JoinedConnectString.IsEmpty())
	{
		// Joined some other way (the console): the address is in the world's URL.
		JoinedConnectString = FString::Printf(TEXT("%s:%d"), *World->URL.Host, World->URL.Port);
	}
	// Written when the level is entered, and once more when the game state brought the server's real name.
	const FALKSessionInfo Session = GetCurrentSession();
	if (bJoinRemembered && Save->RejoinServerName == Session.ServerName)
	{
		return;
	}
	bJoinRemembered = true;

	Save->RejoinConnectString = JoinedConnectString;
	Save->RejoinSessionId = Session.SessionId;
	Save->RejoinServerName = Session.ServerName;
	Save->RejoinTime = FDateTime::UtcNow();
	LastRejoinTouch = FPlatformTime::Seconds();
	LastJoinPassword = JoinPassword;

	const int32 MaxRecent = GetConfig()->RecentServersMax;
	const FString Address = JoinedConnectString;
	Save->RecentServers.RemoveAll([&Address](const FALKServerBookmark& Each) { return Each.ConnectString == Address; });
	if (MaxRecent > 0)
	{
		FALKServerBookmark Bookmark;
		Bookmark.Session = Session;
		Bookmark.ConnectString = Address;
		Bookmark.Time = FDateTime::UtcNow();
		Save->RecentServers.Insert(Bookmark, 0);
		if (Save->RecentServers.Num() > MaxRecent)
		{
			Save->RecentServers.SetNum(MaxRecent);
		}
	}
	SaveNow();
	OnListsChanged.Broadcast();
	OnListsChangedNative.Broadcast();
}

void UALKOnlineSubsystem::TouchRejoinInfo(bool bSaveNow)
{
	LastRejoinTouch = FPlatformTime::Seconds();
	if (Save && !Save->RejoinConnectString.IsEmpty())
	{
		Save->RejoinTime = FDateTime::UtcNow();
		if (bSaveNow)
		{
			SaveNow();
		}
		else
		{
			bSaveDirty = true;
		}
	}
}

bool UALKOnlineSubsystem::CanRejoinMatch() const
{
	if (!Provider || State != EALKOnlineState::Idle || bReturnToMenuPending)
	{
		return false;
	}
	if (Save && !Save->RejoinConnectString.IsEmpty())
	{
		const double Seconds = (FDateTime::UtcNow() - Save->RejoinTime).GetTotalSeconds();
		return Seconds >= 0.0 && Seconds <= GetConfig()->ReconnectWindowSeconds;
	}
	return false;
}

FString UALKOnlineSubsystem::GetRejoinServerName() const
{
	return Save ? Save->RejoinServerName : FString();
}

bool UALKOnlineSubsystem::RejoinMatch(const FString& Password)
{
	if (!CanRejoinMatch())
	{
		return false;
	}
	const FString Address = Save->RejoinConnectString;
	const FString KeptPassword = Password.IsEmpty() ? LastJoinPassword : Password;

	ResetGameState();
	JoinPassword = KeptPassword;
	JoinedSession.ServerName = Save->RejoinServerName;
	JoinedSession.SessionId = Save->RejoinSessionId;
	SetState(EALKOnlineState::Joining);

	// Through the session when the backend still knows it (a platform lobby); by the remembered address otherwise.
	FALKSessionInfo ProviderSession;
	if (Provider->GetRejoinInfo(ProviderSession))
	{
		bRejoining = true;
		JoinedSession = ProviderSession;
		Provider->RejoinSession();
		return true;
	}
	if (TravelToServer(Address))
	{
		return true;
	}
	FailWithError(EALKConnectionError::SessionNotFound, false);
	return false;
}

void UALKOnlineSubsystem::ClearRejoin()
{
	if (Save && !Save->RejoinConnectString.IsEmpty())
	{
		Save->RejoinConnectString.Reset();
		Save->RejoinSessionId.Reset();
		Save->RejoinServerName.Reset();
		bSaveDirty = true;
	}
}

// ---------------------------------------------------------------- Quick match

bool UALKOnlineSubsystem::StartQuickMatch(FName PlaylistId)
{
	if (!CanStartOnlineAction() || (MatchmakingState != EALKMatchmakingState::Idle && MatchmakingState != EALKMatchmakingState::Failed))
	{
		return false;
	}
	if (State == EALKOnlineState::Searching)
	{
		CancelFindGames();
	}
	QueuePlaylist = PlaylistId;
	QueueStartTime = FPlatformTime::Seconds();
	FoundMatch = FALKSessionInfo();
	MatchPlayersAccepted = 0;
	MatchPlayersNeeded = 0;
	LastMatchmakingError = EALKConnectionError::None;
	SetMatchmakingState(EALKMatchmakingState::Searching);
	Provider->StartMatchmaking(PlaylistId);
	return true;
}

void UALKOnlineSubsystem::CancelQuickMatch()
{
	if (MatchmakingState == EALKMatchmakingState::Idle)
	{
		return;
	}
	if (MatchmakingState == EALKMatchmakingState::Failed || MatchmakingState == EALKMatchmakingState::Joining)
	{
		// Nothing is searching any more.
		SetMatchmakingState(EALKMatchmakingState::Idle);
		return;
	}
	if (Provider)
	{
		Provider->CancelMatchmaking();
	}
	// Whether or not the provider answered.
	SetMatchmakingState(EALKMatchmakingState::Idle);
}

void UALKOnlineSubsystem::RespondToMatch(bool bAccept)
{
	if (MatchmakingState != EALKMatchmakingState::MatchFound || !Provider)
	{
		return;
	}
	if (bAccept)
	{
		// First, so an answer that completes the match right away finds the right state.
		SetMatchmakingState(EALKMatchmakingState::Accepted);
		Provider->RespondToMatch(true);
		return;
	}
	Provider->RespondToMatch(false);
	if (MatchmakingState == EALKMatchmakingState::MatchFound)
	{
		SetMatchmakingState(EALKMatchmakingState::Idle);
	}
}

float UALKOnlineSubsystem::GetQueueElapsedSeconds() const
{
	const bool bQueued = MatchmakingState == EALKMatchmakingState::Searching || MatchmakingState == EALKMatchmakingState::MatchFound || MatchmakingState == EALKMatchmakingState::Accepted;
	return bQueued ? static_cast<float>(FPlatformTime::Seconds() - QueueStartTime) : 0.f;
}

float UALKOnlineSubsystem::GetEstimatedWaitSeconds(FName PlaylistId) const
{
	return Provider ? Provider->GetEstimatedWaitSeconds(PlaylistId) : -1.f;
}

float UALKOnlineSubsystem::GetMatchAcceptRemaining() const
{
	const bool bWaiting = MatchmakingState == EALKMatchmakingState::MatchFound || MatchmakingState == EALKMatchmakingState::Accepted;
	return bWaiting ? FMath::Max(0.f, static_cast<float>(MatchAcceptDeadline - FPlatformTime::Seconds())) : 0.f;
}

void UALKOnlineSubsystem::HandleMatchFound(const FALKSessionInfo& Session, float AcceptSeconds, int32 Accepted, int32 Needed)
{
	if (MatchmakingState != EALKMatchmakingState::Searching)
	{
		return;
	}
	FoundMatch = Session;
	MatchAcceptDeadline = FPlatformTime::Seconds() + FMath::Max(0.f, AcceptSeconds);
	MatchPlayersAccepted = Accepted;
	MatchPlayersNeeded = Needed;
	SetMatchmakingState(EALKMatchmakingState::MatchFound);
	OnMatchFound.Broadcast(Session, AcceptSeconds, Accepted, Needed);
	OnMatchFoundNative.Broadcast(Session, AcceptSeconds, Accepted, Needed);
}

void UALKOnlineSubsystem::HandleMatchAcceptProgress(int32 Accepted, int32 Needed)
{
	if (MatchmakingState != EALKMatchmakingState::MatchFound && MatchmakingState != EALKMatchmakingState::Accepted)
	{
		return;
	}
	MatchPlayersAccepted = Accepted;
	MatchPlayersNeeded = Needed;
	BroadcastMatchmakingChanged();
}

void UALKOnlineSubsystem::HandleMatchmakingFailed(EALKConnectionError Error)
{
	if (MatchmakingState == EALKMatchmakingState::Idle || MatchmakingState == EALKMatchmakingState::Failed)
	{
		return;
	}
	LastMatchmakingError = Error;
	SetMatchmakingState(Error == EALKConnectionError::Cancelled ? EALKMatchmakingState::Idle : EALKMatchmakingState::Failed);
}

void UALKOnlineSubsystem::HandleMatchReady(const FString& ConnectString, const FALKSessionInfo& Session)
{
	if (MatchmakingState == EALKMatchmakingState::Idle || MatchmakingState == EALKMatchmakingState::Failed || !CanStartOnlineAction())
	{
		return;
	}
	SetMatchmakingState(EALKMatchmakingState::Joining);
	if (!ConnectString.IsEmpty())
	{
		ResetGameState();
		JoinedSession = Session;
		SetState(EALKOnlineState::Joining);
		if (!TravelToServer(ConnectString))
		{
			FailWithError(EALKConnectionError::NetworkError, false);
		}
		return;
	}
	if (!JoinGame(Session, FString()) && MatchmakingState == EALKMatchmakingState::Joining)
	{
		LastMatchmakingError = EALKConnectionError::SessionNotFound;
		SetMatchmakingState(EALKMatchmakingState::Failed);
	}
}

void UALKOnlineSubsystem::HandleMatchHostRequested(const FALKHostSettings& Settings)
{
	if (MatchmakingState == EALKMatchmakingState::Idle || MatchmakingState == EALKMatchmakingState::Failed)
	{
		return;
	}
	SetMatchmakingState(EALKMatchmakingState::Joining);
	if (!HostGame(Settings))
	{
		LastMatchmakingError = EALKConnectionError::SessionNotFound;
		SetMatchmakingState(EALKMatchmakingState::Failed);
	}
}

// ---------------------------------------------------------------- Party

TArray<FALKPartyMember> UALKOnlineSubsystem::GetPartyMembers() const
{
	return Provider ? Provider->GetPartyMembers() : TArray<FALKPartyMember>();
}

bool UALKOnlineSubsystem::IsInParty() const
{
	return GetPartyMembers().Num() > 1;
}

bool UALKOnlineSubsystem::IsPartyLeader() const
{
	const TArray<FALKPartyMember> Members = GetPartyMembers();
	if (Members.Num() <= 1)
	{
		return true;
	}
	const FALKPartyMember* Local = Members.FindByPredicate([](const FALKPartyMember& Each) { return Each.bLocal; });
	return Local && Local->bLeader;
}

int32 UALKOnlineSubsystem::GetPartyMaxSize() const
{
	return Provider ? Provider->GetPartyMaxSize() : 1;
}

void UALKOnlineSubsystem::InviteToParty(const FString& UserId)
{
	if (Provider && !UserId.IsEmpty())
	{
		Provider->InviteToParty(UserId);
	}
}

void UALKOnlineSubsystem::KickFromParty(const FString& UserId)
{
	if (Provider && !UserId.IsEmpty())
	{
		Provider->KickFromParty(UserId);
	}
}

void UALKOnlineSubsystem::PromotePartyMember(const FString& UserId)
{
	if (Provider && !UserId.IsEmpty())
	{
		Provider->PromotePartyMember(UserId);
	}
}

void UALKOnlineSubsystem::LeaveParty()
{
	if (Provider)
	{
		Provider->LeaveParty();
	}
}

void UALKOnlineSubsystem::SetPartyReady(bool bReady)
{
	if (Provider)
	{
		Provider->SetPartyReady(bReady);
	}
}

void UALKOnlineSubsystem::HandlePartyChanged()
{
	// Who is in the party decides who the in-game Party chat channel reaches.
	if (UALKPlayerNetComponent* NetComponent = GetLocalNetComponent())
	{
		NetComponent->ServerSetPartyId(GetPartyId());
	}
	OnPartyChanged.Broadcast();
	OnPartyChangedNative.Broadcast();
}

FString UALKOnlineSubsystem::GetPartyId() const
{
	const TArray<FALKPartyMember> Members = GetPartyMembers();
	if (Members.Num() <= 1)
	{
		return FString();
	}
	const FALKPartyMember* Leader = Members.FindByPredicate([](const FALKPartyMember& Each) { return Each.bLeader; });
	return Leader ? Leader->UserId : Members[0].UserId;
}

void UALKOnlineSubsystem::HandlePartyInvite(const FString& FromUserId, const FString& FromName)
{
	if (IsPlayerBlocked(FromUserId))
	{
		return;
	}
	FALKInvite Invite;
	Invite.FromUserId = FromUserId;
	Invite.FromName = FromName;
	Invite.bPartyInvite = true;
	OnInviteReceived.Broadcast(Invite);
	OnInviteReceivedNative.Broadcast(Invite);
}

void UALKOnlineSubsystem::HandlePartyChat(const FString& FromUserId, const FString& FromName, const FString& Text)
{
	FALKChatMessage Message;
	Message.Time = FDateTime::UtcNow();
	Message.Channel = EALKChatChannel::Party;
	Message.SenderId = FromUserId;
	Message.SenderName = FromName;
	Message.Text = Text;
	ReceiveChatMessage(Message);
}

// ---------------------------------------------------------------- Friends and invites

void UALKOnlineSubsystem::QueryFriends()
{
	if (Provider)
	{
		Provider->QueryFriends();
	}
}

TArray<FALKFriend> UALKOnlineSubsystem::GetFriends() const
{
	return Provider ? Provider->GetFriends() : TArray<FALKFriend>();
}

TArray<FALKFriend> UALKOnlineSubsystem::GetRecentPlayers() const
{
	TArray<FALKFriend> Result;
	const UWorld* World = GetGameWorld();
	if (Save)
	{
		for (const FALKRecentPlayer& Recent : Save->RecentPlayers)
		{
			FALKFriend Friend;
			Friend.UserId = Recent.UserId;
			Friend.DisplayName = Recent.DisplayName;
			// Still in the same game counts as online; anything else is not known.
			const bool bHere = ALKOnlinePrivate::FindPlayerByUserId(World, Recent.UserId) != nullptr;
			Friend.Presence = bHere ? (State == EALKOnlineState::InMatch ? EALKPresence::InGame : EALKPresence::InLobby) : EALKPresence::Offline;
			Result.Add(Friend);
		}
	}
	if (Provider)
	{
		for (const FALKFriend& Friend : Provider->GetRecentPlayers())
		{
			if (!Result.ContainsByPredicate([&Friend](const FALKFriend& Each) { return Each.UserId == Friend.UserId; }))
			{
				Result.Add(Friend);
			}
		}
	}
	return Result;
}

void UALKOnlineSubsystem::InviteFriendToGame(const FString& UserId)
{
	if (Provider && !UserId.IsEmpty())
	{
		Provider->InviteFriendToGame(UserId);
	}
}

void UALKOnlineSubsystem::JoinFriend(const FString& UserId)
{
	if (Provider && !UserId.IsEmpty())
	{
		Provider->JoinFriend(UserId);
	}
}

void UALKOnlineSubsystem::ShowInviteOverlay()
{
	if (Provider)
	{
		Provider->ShowInviteOverlay();
	}
}

void UALKOnlineSubsystem::RespondToInvite(const FALKInvite& Invite, bool bAccept)
{
	if (!Provider)
	{
		return;
	}
	if (Invite.bPartyInvite)
	{
		Provider->RespondToPartyInvite(Invite.FromUserId, bAccept);
	}
	else
	{
		Provider->RespondToGameInvite(Invite.FromUserId, bAccept);
	}
}

void UALKOnlineSubsystem::HandleFriendsChanged()
{
	OnFriendsChanged.Broadcast();
	OnFriendsChangedNative.Broadcast();
}

void UALKOnlineSubsystem::HandleGameInvite(const FString& FromUserId, const FString& FromName, const FALKSessionInfo& Session)
{
	if (IsPlayerBlocked(FromUserId))
	{
		return;
	}
	FALKInvite Invite;
	Invite.FromUserId = FromUserId;
	Invite.FromName = FromName;
	Invite.bPartyInvite = false;
	Invite.Session = Session;
	OnInviteReceived.Broadcast(Invite);
	OnInviteReceivedNative.Broadcast(Invite);
}

void UALKOnlineSubsystem::HandleWhisper(const FString& FromUserId, const FString& FromName, const FString& ToUserId, const FString& Text)
{
	FALKChatMessage Message;
	Message.Time = FDateTime::UtcNow();
	Message.Channel = EALKChatChannel::Whisper;
	Message.SenderId = FromUserId;
	Message.SenderName = FromName;
	Message.WhisperTargetId = ToUserId;
	Message.Text = Text;
	ReceiveChatMessage(Message);
}

void UALKOnlineSubsystem::HandlePlayerTalking(const FString& UserId, bool bTalking)
{
	if (bTalking && IsPlayerMuted(UserId))
	{
		return;
	}
	OnPlayerTalking.Broadcast(UserId, bTalking);
	OnPlayerTalkingNative.Broadcast(UserId, bTalking);
}

// ---------------------------------------------------------------- Profiles, reports, mutes

bool UALKOnlineSubsystem::GetPlayerProfile(const FString& UserId, FALKPlayerProfile& OutProfile) const
{
	return Provider && !UserId.IsEmpty() && Provider->GetPlayerProfile(UserId, OutProfile);
}

void UALKOnlineSubsystem::ReportPlayer(const FString& UserId, FName ReasonId, const FString& Comment)
{
	if (UserId.IsEmpty())
	{
		return;
	}
	const FString CleanComment = Comment.TrimStartAndEnd().Left(500);
	UE_LOG(LogALK, Log, TEXT("Player reported: %s (%s)"), *UserId, *ReasonId.ToString());
	OnPlayerReported.Broadcast(UserId, ReasonId, CleanComment);
	OnPlayerReportedNative.Broadcast(UserId, ReasonId, CleanComment);
	if (Provider)
	{
		Provider->ReportPlayer(UserId, ReasonId, CleanComment);
	}
}

void UALKOnlineSubsystem::SetPlayerMuted(const FString& UserId, bool bMuted)
{
	if (!Save || UserId.IsEmpty() || IsPlayerMuted(UserId) == bMuted)
	{
		return;
	}
	if (bMuted)
	{
		Save->MutedPlayers.Add(UserId);
	}
	else
	{
		Save->MutedPlayers.Remove(UserId);
	}
	if (Provider)
	{
		Provider->SetPlayerMuted(UserId, bMuted || IsPlayerBlocked(UserId));
	}
	bSaveDirty = true;
	OnListsChanged.Broadcast();
	OnListsChangedNative.Broadcast();
}

bool UALKOnlineSubsystem::IsPlayerMuted(const FString& UserId) const
{
	return Save && Save->MutedPlayers.Contains(UserId);
}

void UALKOnlineSubsystem::SetPlayerBlocked(const FString& UserId, bool bBlocked)
{
	if (!Save || UserId.IsEmpty() || IsPlayerBlocked(UserId) == bBlocked)
	{
		return;
	}
	if (bBlocked)
	{
		Save->BlockedPlayers.Add(UserId);
	}
	else
	{
		Save->BlockedPlayers.Remove(UserId);
	}
	if (Provider)
	{
		Provider->SetPlayerBlocked(UserId, bBlocked);
		Provider->SetPlayerMuted(UserId, bBlocked || IsPlayerMuted(UserId));
	}
	bSaveDirty = true;
	OnListsChanged.Broadcast();
	OnListsChangedNative.Broadcast();
}

bool UALKOnlineSubsystem::IsPlayerBlocked(const FString& UserId) const
{
	return Save && Save->BlockedPlayers.Contains(UserId);
}

TArray<FString> UALKOnlineSubsystem::GetMutedPlayers() const
{
	return Save ? Save->MutedPlayers : TArray<FString>();
}

TArray<FString> UALKOnlineSubsystem::GetBlockedPlayers() const
{
	return Save ? Save->BlockedPlayers : TArray<FString>();
}

// ---------------------------------------------------------------- Chat

bool UALKOnlineSubsystem::SendChat(EALKChatChannel Channel, const FString& Text, const FString& WhisperTargetId)
{
	if (!bTextChatEnabled || Channel == EALKChatChannel::System)
	{
		return false;
	}
	const FString Clean = UALKLobbyStatics::SanitizeChatText(Text, GetConfig()->ChatMaxLength);
	if (Clean.IsEmpty())
	{
		return false;
	}
	UALKPlayerNetComponent* NetComponent = GetLocalNetComponent();
	const UWorld* World = GetGameWorld();
	const bool bInNetGame = NetComponent && World && World->GetNetMode() != NM_Standalone;

	switch (Channel)
	{
	case EALKChatChannel::Party:
		// The backend's party chat reaches the party wherever its members are.
		if (Provider && IsInParty())
		{
			Provider->SendPartyChat(Clean);
			return true;
		}
		if (bInNetGame)
		{
			NetComponent->ServerSendChat(Channel, Clean, FString());
			return true;
		}
		return false;

	case EALKChatChannel::Whisper:
		if (WhisperTargetId.IsEmpty())
		{
			return false;
		}
		// Through the game when they are in it; through the backend otherwise.
		if (bInNetGame && ALKOnlinePrivate::FindPlayerByUserId(World, WhisperTargetId))
		{
			NetComponent->ServerSendChat(Channel, Clean, WhisperTargetId);
			return true;
		}
		if (Provider)
		{
			Provider->SendWhisper(WhisperTargetId, Clean);
			return true;
		}
		return false;

	case EALKChatChannel::All:
	case EALKChatChannel::Team:
	default:
		if (!bInNetGame)
		{
			return false;
		}
		NetComponent->ServerSendChat(Channel, Clean, FString());
		return true;
	}
}

bool UALKOnlineSubsystem::ReceiveChatMessage(const FALKChatMessage& Message, FALKChatMessage* OutShown)
{
	FALKChatMessage Shown = Message;
	if (Shown.Channel != EALKChatChannel::System)
	{
		if (!bTextChatEnabled)
		{
			return false;
		}
		const bool bOwn = Shown.SenderId == GetLocalUserId();
		if (!bOwn && (IsPlayerMuted(Shown.SenderId) || IsPlayerBlocked(Shown.SenderId)))
		{
			return false;
		}
		Shown.Text = FilterChatText(Shown.Text);
	}
	if (Shown.Text.IsEmpty())
	{
		return false;
	}
	if (Shown.Time.GetTicks() == 0)
	{
		Shown.Time = FDateTime::UtcNow();
	}

	ChatHistory.Add(Shown);
	const int32 MaxLines = FMath::Max(1, GetConfig()->ChatHistorySize);
	if (ChatHistory.Num() > MaxLines)
	{
		ChatHistory.RemoveAt(0, ChatHistory.Num() - MaxLines);
	}
	if (OutShown)
	{
		*OutShown = Shown;
	}
	OnChatMessage.Broadcast(Shown);
	OnChatMessageNative.Broadcast(Shown);
	return true;
}

void UALKOnlineSubsystem::ClearChatHistory()
{
	ChatHistory.Reset();
}

void UALKOnlineSubsystem::AddSystemMessage(const FText& Text)
{
	FALKChatMessage Message;
	Message.Time = FDateTime::UtcNow();
	Message.Channel = EALKChatChannel::System;
	Message.Text = Text.ToString();
	ReceiveChatMessage(Message);
}

FString UALKOnlineSubsystem::FilterChatText(const FString& Text) const
{
	if (!bChatFilterEnabled)
	{
		return Text;
	}
	return Provider ? Provider->FilterChatText(Text) : UALKLobbyStatics::MaskFilteredWords(Text, GetConfig()->FilteredWords);
}

// ---------------------------------------------------------------- Favourites, recent servers, presets

bool UALKOnlineSubsystem::IsFavourite(const FALKSessionInfo& Session) const
{
	const FString Key = Session.GetFavouriteKey();
	return Save && !Key.IsEmpty() && Save->FavouriteServers.ContainsByPredicate([&Key](const FALKServerBookmark& Each) { return Each.Session.GetFavouriteKey() == Key; });
}

void UALKOnlineSubsystem::SetFavourite(const FALKSessionInfo& Session, bool bFavourite)
{
	const FString Key = Session.GetFavouriteKey();
	if (!Save || Key.IsEmpty() || IsFavourite(Session) == bFavourite)
	{
		return;
	}
	if (bFavourite)
	{
		FALKServerBookmark Bookmark;
		Bookmark.Session = Session;
		Bookmark.Time = FDateTime::UtcNow();
		if (Provider)
		{
			Provider->GetSessionConnectString(Session, Bookmark.ConnectString);
		}
		Save->FavouriteServers.Add(Bookmark);
	}
	else
	{
		Save->FavouriteServers.RemoveAll([&Key](const FALKServerBookmark& Each) { return Each.Session.GetFavouriteKey() == Key; });
	}
	SaveNow();
	OnListsChanged.Broadcast();
	OnListsChangedNative.Broadcast();
}

TArray<FALKServerBookmark> UALKOnlineSubsystem::GetFavouriteServers() const
{
	return Save ? Save->FavouriteServers : TArray<FALKServerBookmark>();
}

TArray<FALKServerBookmark> UALKOnlineSubsystem::GetRecentServers() const
{
	return Save ? Save->RecentServers : TArray<FALKServerBookmark>();
}

void UALKOnlineSubsystem::ClearRecentServers()
{
	if (Save && Save->RecentServers.Num() > 0)
	{
		Save->RecentServers.Reset();
		SaveNow();
		OnListsChanged.Broadcast();
		OnListsChangedNative.Broadcast();
	}
}

TArray<FALKRulePreset> UALKOnlineSubsystem::GetRulePresets() const
{
	return Save ? Save->RulePresets : TArray<FALKRulePreset>();
}

void UALKOnlineSubsystem::SaveRulePreset(const FString& Name, FName ModeId, const FALKGameRules& Rules)
{
	const FString Clean = Name.TrimStartAndEnd().Left(32);
	if (!Save || Clean.IsEmpty())
	{
		return;
	}
	FALKRulePreset* Preset = Save->RulePresets.FindByPredicate([&Clean](const FALKRulePreset& Each) { return Each.Name.Equals(Clean, ESearchCase::IgnoreCase); });
	if (!Preset)
	{
		Preset = &Save->RulePresets.AddDefaulted_GetRef();
	}
	Preset->Name = Clean;
	Preset->ModeId = ModeId;
	Preset->Rules = Rules;
	SaveNow();
	OnListsChanged.Broadcast();
	OnListsChangedNative.Broadcast();
}

bool UALKOnlineSubsystem::DeleteRulePreset(const FString& Name)
{
	const FString Clean = Name.TrimStartAndEnd();
	if (!Save || Save->RulePresets.RemoveAll([&Clean](const FALKRulePreset& Each) { return Each.Name.Equals(Clean, ESearchCase::IgnoreCase); }) == 0)
	{
		return false;
	}
	SaveNow();
	OnListsChanged.Broadcast();
	OnListsChangedNative.Broadcast();
	return true;
}

FALKHostSettings UALKOnlineSubsystem::GetLastHostSettings() const
{
	const UALKOnlineConfig* Config = GetConfig();
	if (Save && Save->bHasLastHostSettings)
	{
		return Config->SanitizeHostSettings(Save->LastHostSettings, GetLocalDisplayName());
	}
	return Config->MakeDefaultHostSettings(GetLocalDisplayName());
}

void UALKOnlineSubsystem::NoteRecentPlayer(const FString& UserId, const FString& DisplayName)
{
	const int32 MaxRecent = GetConfig()->RecentPlayersMax;
	if (!Save || MaxRecent <= 0 || UserId.IsEmpty() || DisplayName.IsEmpty() || UserId == GetLocalUserId())
	{
		return;
	}
	// Already at the top with this name: nothing new to remember.
	if (Save->RecentPlayers.Num() > 0 && Save->RecentPlayers[0].UserId == UserId && Save->RecentPlayers[0].DisplayName == DisplayName)
	{
		return;
	}
	Save->RecentPlayers.RemoveAll([&UserId](const FALKRecentPlayer& Each) { return Each.UserId == UserId; });
	FALKRecentPlayer Recent;
	Recent.UserId = UserId;
	Recent.DisplayName = DisplayName;
	Recent.LastSeen = FDateTime::UtcNow();
	Save->RecentPlayers.Insert(Recent, 0);
	if (Save->RecentPlayers.Num() > MaxRecent)
	{
		Save->RecentPlayers.SetNum(MaxRecent);
	}
	bSaveDirty = true;
	OnListsChanged.Broadcast();
	OnListsChangedNative.Broadcast();
}

// ---------------------------------------------------------------- Ban list

TArray<FALKBanEntry> UALKOnlineSubsystem::GetBanList() const
{
	return Save ? Save->BannedPlayers : TArray<FALKBanEntry>();
}

void UALKOnlineSubsystem::AddBan(const FString& UserId, const FString& DisplayName, const FString& Address)
{
	if (!Save || (UserId.IsEmpty() && Address.IsEmpty()))
	{
		return;
	}
	const FString Host = ALKOnlinePrivate::StripPort(Address);
	Save->BannedPlayers.RemoveAll([&UserId](const FALKBanEntry& Each) { return !UserId.IsEmpty() && Each.UserId == UserId; });
	FALKBanEntry Entry;
	Entry.UserId = UserId;
	Entry.DisplayName = DisplayName;
	Entry.Address = Host;
	Entry.BannedAt = FDateTime::UtcNow();
	Save->BannedPlayers.Add(Entry);
	SaveNow();
	OnListsChanged.Broadcast();
	OnListsChangedNative.Broadcast();
}

bool UALKOnlineSubsystem::RemoveBan(const FString& UserId)
{
	if (!Save || Save->BannedPlayers.RemoveAll([&UserId](const FALKBanEntry& Each) { return Each.UserId == UserId; }) == 0)
	{
		return false;
	}
	SaveNow();
	OnListsChanged.Broadcast();
	OnListsChangedNative.Broadcast();
	return true;
}

bool UALKOnlineSubsystem::IsBanned(const FString& UserId, const FString& Address) const
{
	if (!Save)
	{
		return false;
	}
	const FString Host = ALKOnlinePrivate::StripPort(Address);
	return Save->BannedPlayers.ContainsByPredicate([&UserId, &Host](const FALKBanEntry& Each)
	{
		return (!UserId.IsEmpty() && Each.UserId == UserId) || (!Host.IsEmpty() && !Each.Address.IsEmpty() && Each.Address == Host);
	});
}

// ---------------------------------------------------------------- Leaderboards

void UALKOnlineSubsystem::QueryLeaderboard(FName BoardId, int32 MaxRows)
{
	if (Provider)
	{
		Provider->QueryLeaderboard(BoardId, FMath::Clamp(MaxRows, 1, 200));
	}
	else
	{
		HandleLeaderboard(BoardId, TArray<FALKLeaderboardRow>());
	}
}

void UALKOnlineSubsystem::SubmitScore(FName BoardId, double Score)
{
	if (Provider)
	{
		Provider->SubmitScore(BoardId, Score);
	}
}

void UALKOnlineSubsystem::HandleLeaderboard(FName BoardId, const TArray<FALKLeaderboardRow>& Rows)
{
	OnLeaderboardReceived.Broadcast(BoardId, Rows);
	OnLeaderboardReceivedNative.Broadcast(BoardId, Rows);
}

// ---------------------------------------------------------------- Preferences

void UALKOnlineSubsystem::SetStreamerMode(bool bEnabled)
{
	bStreamerMode = bEnabled;
}

void UALKOnlineSubsystem::SetChatFilterEnabled(bool bEnabled)
{
	bChatFilterEnabled = bEnabled;
}

void UALKOnlineSubsystem::SetTextChatEnabled(bool bEnabled)
{
	bTextChatEnabled = bEnabled;
}

void UALKOnlineSubsystem::SetShowPing(bool bEnabled)
{
	bShowPing = bEnabled;
}

void UALKOnlineSubsystem::SetRegion(const FString& RegionId)
{
	Region = RegionId;
	if (Provider)
	{
		Provider->SetRegion(RegionId);
	}
}

TArray<FALKOnlineOption> UALKOnlineSubsystem::GetRegions() const
{
	return Provider ? Provider->GetRegions() : TArray<FALKOnlineOption>();
}

void UALKOnlineSubsystem::SetCrossplayEnabled(bool bEnabled)
{
	bCrossplayEnabled = bEnabled;
	if (Provider)
	{
		Provider->SetCrossplayEnabled(bEnabled);
	}
}

void UALKOnlineSubsystem::SetVoiceMode(EALKVoiceMode Mode)
{
	VoiceMode = Mode;
	if (Provider)
	{
		Provider->SetVoiceMode(Mode);
	}
}

void UALKOnlineSubsystem::SetPushToTalkActive(bool bActive)
{
	if (Provider && VoiceMode == EALKVoiceMode::PushToTalk)
	{
		Provider->SetPushToTalkActive(bActive);
	}
}

TArray<FALKOnlineOption> UALKOnlineSubsystem::GetAudioInputDevices() const
{
	return Provider ? Provider->GetAudioInputDevices() : TArray<FALKOnlineOption>();
}

TArray<FALKOnlineOption> UALKOnlineSubsystem::GetAudioOutputDevices() const
{
	return Provider ? Provider->GetAudioOutputDevices() : TArray<FALKOnlineOption>();
}

void UALKOnlineSubsystem::SetAudioInputDevice(const FString& DeviceId)
{
	if (Provider)
	{
		Provider->SetAudioInputDevice(DeviceId);
	}
}

void UALKOnlineSubsystem::SetAudioOutputDevice(const FString& DeviceId)
{
	if (Provider)
	{
		Provider->SetAudioOutputDevice(DeviceId);
	}
}

void UALKOnlineSubsystem::SetMicVolume(float Volume)
{
	if (Provider)
	{
		Provider->SetMicVolume(FMath::Clamp(Volume, 0.f, 1.f));
	}
}

void UALKOnlineSubsystem::SetVoiceVolume(float Volume)
{
	if (Provider)
	{
		Provider->SetVoiceVolume(FMath::Clamp(Volume, 0.f, 1.f));
	}
}

FString UALKOnlineSubsystem::GetDisplayName(const FString& RealName, int32 StableIndex) const
{
	if (!bStreamerMode)
	{
		return RealName;
	}
	return StableIndex >= 0
		? FText::Format(LOCTEXT("StreamerPlayerNumbered", "Player {0}"), FText::AsNumber(StableIndex + 1)).ToString()
		: LOCTEXT("StreamerPlayer", "Player").ToString();
}

FALKNetStats UALKOnlineSubsystem::GetNetStats() const
{
	return FALKNetStats::FromWorld(GetGameWorld());
}

// ---------------------------------------------------------------- Save

FString UALKOnlineSubsystem::GetSaveSlotName()
{
	return UALKDeveloperSettings::GetMenuConfig()->SaveSlotPrefix + TEXT("_Online");
}

void UALKOnlineSubsystem::LoadSave()
{
	const FString Slot = GetSaveSlotName();
	Save = UGameplayStatics::DoesSaveGameExist(Slot, 0) ? Cast<UALKOnlineSaveGame>(UGameplayStatics::LoadGameFromSlot(Slot, 0)) : nullptr;
	if (!Save)
	{
		Save = NewObject<UALKOnlineSaveGame>(this);
	}
}

void UALKOnlineSubsystem::SaveNow()
{
	bSaveDirty = false;
	LastSaveTime = FPlatformTime::Seconds();
	if (Save)
	{
		UGameplayStatics::AsyncSaveGameToSlot(Save, GetSaveSlotName(), 0);
	}
}

#undef LOCTEXT_NAMESPACE
