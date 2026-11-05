// Copyright 2026 CodeLikeMe. All Rights Reserved.

#include "Online/ALKSessionsOnlineProvider.h"
#include "Online/ALKOnlineConfig.h"
#include "Lobby/ALKLobbyStatics.h"
#include "AssetproLobbyKit.h"
#include "Containers/Ticker.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/PlatformProcess.h"
#include "Interfaces/OnlineExternalUIInterface.h"
#include "Interfaces/OnlineFriendsInterface.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "Interfaces/OnlinePresenceInterface.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "Interfaces/VoiceInterface.h"
#include "Misc/CoreMisc.h"
#include "Misc/EngineVersionComparison.h"
#include "Online/OnlineSessionNames.h"
#include "OnlineSessionSettings.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemNames.h"
#include "OnlineSubsystemTypes.h"
#include "OnlineSubsystemUtils.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ALKSessionsOnlineProvider)

/** Everything of the engine online service this provider holds on to. */
struct FALKSessionsProviderImpl
{
	/** The search that is running. */
	TSharedPtr<FOnlineSessionSearch> ActiveSearch;
	/** The last finished search: its results are what Join Session joins. */
	TSharedPtr<FOnlineSessionSearch> LastSearch;
	/** Sessions that came from an invite or a friend, not from a search. */
	TArray<FOnlineSessionSearchResult> DirectResults;
	/** The session last joined, for Rejoin Session. */
	TSharedPtr<FOnlineSessionSearchResult> RejoinResult;

	/** The interfaces as last resolved, for when the world is already gone (shutdown). */
	TWeakPtr<IOnlineSession, ESPMode::ThreadSafe> Sessions;
	TWeakPtr<IOnlineFriends, ESPMode::ThreadSafe> Friends;
	TWeakPtr<IOnlineVoice, ESPMode::ThreadSafe> Voice;

	FDelegateHandle CreateHandle;
	FDelegateHandle FindHandle;
	FDelegateHandle JoinHandle;
	FDelegateHandle InviteAcceptedHandle;
	FDelegateHandle InviteReceivedHandle;
	FDelegateHandle FindFriendHandle;
	FDelegateHandle FriendsChangedHandle;
	FDelegateHandle TalkingHandle;
	FTSTicker::FDelegateHandle FindTimeoutHandle;
};

namespace ALKSessionsPrivate
{
	/** The session settings the kit advertises. Short, upper case: every service takes them. */
	static const FName SessionKeyName(TEXT("ALK_NAME"));
	static const FName SessionKeyMode(TEXT("ALK_MODE"));
	static const FName SessionKeyMap(TEXT("ALK_MAP"));
	static const FName SessionKeyPassword(TEXT("ALK_PW"));
	static const FName SessionKeyVersion(TEXT("ALK_VER"));
	static const FName SessionKeyHost(TEXT("ALK_HOST"));
	static const FName SessionKeyRegion(TEXT("ALK_REGION"));
	static const FName SessionKeyRules(TEXT("ALK_RULES"));

	/**
	 * A local network answer packet is small (about a thousand bytes for the whole session), so what is advertised is
	 * kept short: rules longer than this are left out (the lobby still shows them), names are cut to fit.
	 */
	static constexpr int32 MaxAdvertisedRulesLength = 220;
	static constexpr int32 MaxAdvertisedNameLength = 150;
	static constexpr int32 MaxAdvertisedHostLength = 80;
	/** Sessions that did not come from a search are numbered from here down in Search Result Index. */
	static constexpr int32 DirectResultBase = -2;
	static constexpr int32 LocalUserNum = 0;

	static IOnlineSubsystem* GetService(const UObject* Provider)
	{
		// The world picks the right service instance when several games run in one process (play in editor).
		return Online::GetSubsystem(Provider ? Provider->GetWorld() : nullptr);
	}

	/**
	 * The session interface this provider works with. Resolved once, with the world (it picks the right service instance in
	 * play in editor), then kept: at shutdown the world may already be gone.
	 */
	static IOnlineSessionPtr GetSessions(const UObject* Provider, const TSharedPtr<FALKSessionsProviderImpl>& Impl)
	{
		if (!Impl.IsValid())
		{
			return nullptr;
		}
		IOnlineSessionPtr Sessions = Impl->Sessions.Pin();
		if (!Sessions.IsValid() && Provider && Provider->GetWorld())
		{
			if (IOnlineSubsystem* Service = GetService(Provider))
			{
				Sessions = Service->GetSessionInterface();
				Impl->Sessions = Sessions;
			}
		}
		return Sessions;
	}

	static FUniqueNetIdPtr GetLocalNetId(const UObject* Provider)
	{
		IOnlineSubsystem* Service = GetService(Provider);
		const IOnlineIdentityPtr Identity = Service ? Service->GetIdentityInterface() : nullptr;
		return Identity.IsValid() ? Identity->GetUniquePlayerId(LocalUserNum) : nullptr;
	}

	static FUniqueNetIdPtr MakeNetId(const UObject* Provider, const FString& UserId)
	{
		IOnlineSubsystem* Service = GetService(Provider);
		const IOnlineIdentityPtr Identity = Service ? Service->GetIdentityInterface() : nullptr;
		return Identity.IsValid() && !UserId.IsEmpty() ? Identity->CreateUniquePlayerId(UserId) : nullptr;
	}

	/** Text escaped for a session setting, shortened from the end until the escaped form fits. */
	static FString EscapeToFit(const FString& Text, int32 MaxEscapedLength)
	{
		FString Raw = Text;
		FString Escaped = FALKOptionString::Escape(Raw);
		while (Escaped.Len() > MaxEscapedLength && Raw.Len() > 0)
		{
			Raw.LeftChopInline(1);
			Escaped = FALKOptionString::Escape(Raw);
		}
		return Escaped;
	}

	static void FillAdvertisedSettings(FOnlineSessionSettings& Settings, const FALKHostSettings& Host, const FString& HostName, const FString& Version, const FString& Region)
	{
		const EOnlineDataAdvertisementType::Type Advertise = EOnlineDataAdvertisementType::ViaOnlineServiceAndPing;
		const FString Mode = Host.ModeId.IsNone() ? FString() : Host.ModeId.ToString();
		const FString Map = Host.MapId.IsNone() ? FString() : Host.MapId.ToString();
		const FString Rules = Host.Rules.ToOptionsString();

		// Escaped text is plain ASCII, which is all a local network packet carries.
		Settings.Set(SessionKeyName, EscapeToFit(Host.ServerName, MaxAdvertisedNameLength), Advertise);
		Settings.Set(SessionKeyMode, FALKOptionString::Escape(Mode), Advertise);
		Settings.Set(SessionKeyMap, FALKOptionString::Escape(Map), Advertise);
		Settings.Set(SessionKeyPassword, static_cast<int32>(Host.HasPassword() ? 1 : 0), Advertise);
		Settings.Set(SessionKeyVersion, FALKOptionString::Escape(Version), Advertise);
		Settings.Set(SessionKeyHost, EscapeToFit(HostName.Left(24), MaxAdvertisedHostLength), Advertise);
		Settings.Set(SessionKeyRegion, FALKOptionString::Escape(Region), Advertise);
		Settings.Set(SessionKeyRules, Rules.Len() <= MaxAdvertisedRulesLength ? Rules : FString(), Advertise);

		// The engine's own keys, for tools and services that read them.
		Settings.Set(SETTING_GAMEMODE, FALKOptionString::Escape(Mode), Advertise);
		Settings.Set(SETTING_MAPNAME, FALKOptionString::Escape(Map), Advertise);
		Settings.Set(SETTING_NUMBOTS, Host.BotCount, Advertise);
	}

	static FString GetStringSetting(const FOnlineSessionSettings& Settings, FName Key)
	{
		FString Value;
		Settings.Get(Key, Value);
		return FALKOptionString::Unescape(Value);
	}

	static EALKConnectionError JoinResultToError(int32 Result)
	{
		switch (Result)
		{
		case EOnJoinSessionCompleteResult::Success: return EALKConnectionError::None;
		case EOnJoinSessionCompleteResult::SessionIsFull: return EALKConnectionError::ServerFull;
		case EOnJoinSessionCompleteResult::SessionDoesNotExist: return EALKConnectionError::SessionNotFound;
		case EOnJoinSessionCompleteResult::CouldNotRetrieveAddress: return EALKConnectionError::NetworkError;
		default: return EALKConnectionError::Unknown;
		}
	}
}

// ---------------------------------------------------------------- Service

FString UALKSessionsOnlineProvider::GetServiceName() const
{
	const IOnlineSubsystem* Service = ALKSessionsPrivate::GetService(this);
	return Service ? Service->GetSubsystemName().ToString() : FString();
}

bool UALKSessionsOnlineProvider::IsLANOnly() const
{
	const IOnlineSubsystem* Service = ALKSessionsPrivate::GetService(this);
	return !Service || Service->GetSubsystemName() == NULL_SUBSYSTEM;
}

void UALKSessionsOnlineProvider::InitializeProvider_Implementation(UGameInstance* InGameInstance)
{
	Super::InitializeProvider_Implementation(InGameInstance);
	Impl = MakeShared<FALKSessionsProviderImpl>();
	BindServiceEvents();
}

void UALKSessionsOnlineProvider::ShutdownProvider_Implementation()
{
	UnbindServiceEvents();
	if (Impl.IsValid())
	{
		if (Impl->FindTimeoutHandle.IsValid())
		{
			FTSTicker::GetCoreTicker().RemoveTicker(Impl->FindTimeoutHandle);
		}
		Impl.Reset();
	}
	bCreatePending = bDestroyPending = bJoinPending = bFindPending = false;
	Super::ShutdownProvider_Implementation();
}

void UALKSessionsOnlineProvider::BindServiceEvents()
{
	if (bEventsBound || !Impl.IsValid() || !GetWorld())
	{
		return;
	}
	IOnlineSubsystem* Service = ALKSessionsPrivate::GetService(this);
	if (!Service)
	{
		return;
	}
	bEventsBound = true;

	const IOnlineSessionPtr Sessions = Service->GetSessionInterface();
	if (Sessions.IsValid())
	{
		Impl->Sessions = Sessions;

		// The player accepted an invite in the platform's own window: join that game.
		Impl->InviteAcceptedHandle = Sessions->AddOnSessionUserInviteAcceptedDelegate_Handle(FOnSessionUserInviteAcceptedDelegate::CreateWeakLambda(this,
			[this](const bool bWasSuccessful, const int32 /*ControllerId*/, FUniqueNetIdPtr /*UserId*/, const FOnlineSessionSearchResult& InviteResult)
			{
				if (bWasSuccessful && InviteResult.IsValid())
				{
					NotifyJoinRequested(RememberDirectResult(InviteResult));
				}
			}));

		// An invite arrived: the menus show it, and Respond To Game Invite joins it.
		Impl->InviteReceivedHandle = Sessions->AddOnSessionInviteReceivedDelegate_Handle(FOnSessionInviteReceivedDelegate::CreateWeakLambda(this,
			[this](const FUniqueNetId& /*UserId*/, const FUniqueNetId& FromId, const FString& /*AppId*/, const FOnlineSessionSearchResult& InviteResult)
			{
				if (!InviteResult.IsValid())
				{
					return;
				}
				const FString FromUserId = FromId.ToString();
				const FALKSessionInfo Session = RememberDirectResult(InviteResult);
				FString FromName = Session.HostName;
				if (const FALKFriend* Friend = CachedFriends.FindByPredicate([&FromUserId](const FALKFriend& Each) { return Each.UserId == FromUserId; }))
				{
					FromName = Friend->DisplayName;
				}
				PendingInvites.Add(FromUserId, Session);
				NotifyGameInviteReceived(FromUserId, FromName, Session);
			}));
	}

	const IOnlineFriendsPtr Friends = Service->GetFriendsInterface();
	if (Friends.IsValid())
	{
		Impl->Friends = Friends;
		Impl->FriendsChangedHandle = Friends->AddOnFriendsChangeDelegate_Handle(ALKSessionsPrivate::LocalUserNum, FOnFriendsChangeDelegate::CreateWeakLambda(this, [this]()
		{
			RebuildFriends();
			NotifyFriendsChanged();
		}));
	}

	const IOnlineVoicePtr Voice = Service->GetVoiceInterface();
	if (Voice.IsValid())
	{
		Impl->Voice = Voice;
		Impl->TalkingHandle = Voice->AddOnPlayerTalkingStateChangedDelegate_Handle(FOnPlayerTalkingStateChangedDelegate::CreateWeakLambda(this,
			[this](FUniqueNetIdRef TalkerId, bool bIsTalking)
			{
				NotifyPlayerTalking(TalkerId->ToString(), bIsTalking);
			}));
	}
}

void UALKSessionsOnlineProvider::UnbindServiceEvents()
{
	if (!Impl.IsValid())
	{
		return;
	}
	if (const IOnlineSessionPtr Sessions = Impl->Sessions.Pin())
	{
		Sessions->ClearOnCreateSessionCompleteDelegate_Handle(Impl->CreateHandle);
		Sessions->ClearOnFindSessionsCompleteDelegate_Handle(Impl->FindHandle);
		Sessions->ClearOnJoinSessionCompleteDelegate_Handle(Impl->JoinHandle);
		Sessions->ClearOnSessionUserInviteAcceptedDelegate_Handle(Impl->InviteAcceptedHandle);
		Sessions->ClearOnSessionInviteReceivedDelegate_Handle(Impl->InviteReceivedHandle);
		Sessions->ClearOnFindFriendSessionCompleteDelegate_Handle(ALKSessionsPrivate::LocalUserNum, Impl->FindFriendHandle);
	}
	if (const IOnlineFriendsPtr Friends = Impl->Friends.Pin())
	{
		Friends->ClearOnFriendsChangeDelegate_Handle(ALKSessionsPrivate::LocalUserNum, Impl->FriendsChangedHandle);
	}
	if (const IOnlineVoicePtr Voice = Impl->Voice.Pin())
	{
		Voice->ClearOnPlayerTalkingStateChangedDelegate_Handle(Impl->TalkingHandle);
	}
	bEventsBound = false;
}

// ---------------------------------------------------------------- Identity

FString UALKSessionsOnlineProvider::GetLocalUserId_Implementation() const
{
	const FUniqueNetIdPtr NetId = ALKSessionsPrivate::GetLocalNetId(this);
	return NetId.IsValid() ? NetId->ToString() : Super::GetLocalUserId_Implementation();
}

FString UALKSessionsOnlineProvider::GetLocalDisplayName_Implementation() const
{
	IOnlineSubsystem* Service = ALKSessionsPrivate::GetService(this);
	const IOnlineIdentityPtr Identity = Service ? Service->GetIdentityInterface() : nullptr;
	if (Identity.IsValid() && Service->GetSubsystemName() != NULL_SUBSYSTEM)
	{
		const FString Nickname = Identity->GetPlayerNickname(ALKSessionsPrivate::LocalUserNum);
		if (!Nickname.IsEmpty())
		{
			return Nickname;
		}
	}
	// The built-in local network service names players after the machine and a long id: the account name reads better.
	return FString(FPlatformProcess::UserName());
}

bool UALKSessionsOnlineProvider::IsOnline_Implementation() const
{
	IOnlineSubsystem* Service = ALKSessionsPrivate::GetService(this);
	return Service && Service->GetSessionInterface().IsValid();
}

// ---------------------------------------------------------------- Host

void UALKSessionsOnlineProvider::HostSession_Implementation(const FALKHostSettings& Settings)
{
	BindServiceEvents();
	const IOnlineSessionPtr Sessions = ALKSessionsPrivate::GetSessions(this, Impl);
	if (!Impl.IsValid() || !Sessions.IsValid())
	{
		UE_LOG(LogALK, Warning, TEXT("Host Session: no online service with sessions is loaded. Enable an Online Subsystem plugin and set DefaultPlatformService."));
		NotifyHostComplete(false, EALKConnectionError::NetworkError);
		return;
	}
	if (bCreatePending)
	{
		return;
	}

	PendingHostSettings = Settings;
	if (Sessions->GetNamedSession(NAME_GameSession))
	{
		// A session from an earlier game is still around: remove it first, then create the new one.
		bHostAfterDestroy = true;
		bJoinAfterDestroy = false;
		DestroyCurrentSession();
		return;
	}
	CreatePendingSession();
}

void UALKSessionsOnlineProvider::CreatePendingSession()
{
	const IOnlineSessionPtr Sessions = ALKSessionsPrivate::GetSessions(this, Impl);
	if (!Impl.IsValid() || !Sessions.IsValid())
	{
		NotifyHostComplete(false, EALKConnectionError::NetworkError);
		return;
	}

	const bool bLAN = PendingHostSettings.bLAN || IsLANOnly();
	const bool bDedicated = IsRunningDedicatedServer();

	FOnlineSessionSettings Settings;
	Settings.NumPublicConnections = FMath::Max(1, PendingHostSettings.MaxPlayers);
	Settings.NumPrivateConnections = 0;
	Settings.bIsLANMatch = bLAN;
	Settings.bIsDedicated = bDedicated;
	Settings.bShouldAdvertise = !PendingHostSettings.bPrivate;
	Settings.bAllowJoinInProgress = true;
	Settings.bAllowInvites = true;
	Settings.bUsesPresence = !bLAN && !bDedicated;
	Settings.bUseLobbiesIfAvailable = !bLAN && !bDedicated;
	Settings.bAllowJoinViaPresence = !bLAN && !bDedicated;
	Settings.bAllowJoinViaPresenceFriendsOnly = false;
	Settings.bUsesStats = false;
	Settings.bAntiCheatProtected = false;
	ALKSessionsPrivate::FillAdvertisedSettings(Settings, PendingHostSettings, GetLocalDisplayName(), GetOnlineConfig()->GetResolvedBuildVersion(), CurrentRegion);

	bCreatePending = true;
	Impl->CreateHandle = Sessions->AddOnCreateSessionCompleteDelegate_Handle(FOnCreateSessionCompleteDelegate::CreateUObject(this, &ThisClass::HandleCreateComplete));

	const FUniqueNetIdPtr NetId = ALKSessionsPrivate::GetLocalNetId(this);
	const bool bStarted = NetId.IsValid()
		? Sessions->CreateSession(*NetId, NAME_GameSession, Settings)
		: Sessions->CreateSession(ALKSessionsPrivate::LocalUserNum, NAME_GameSession, Settings);
	if (!bStarted && bCreatePending)
	{
		// Refused without the delegate being called.
		HandleCreateComplete(NAME_GameSession, false);
	}
}

void UALKSessionsOnlineProvider::HandleCreateComplete(FName SessionName, bool bWasSuccessful)
{
	if (!bCreatePending || SessionName != NAME_GameSession)
	{
		return;
	}
	bCreatePending = false;
	if (Impl.IsValid())
	{
		if (const IOnlineSessionPtr Sessions = ALKSessionsPrivate::GetSessions(this, Impl))
		{
			Sessions->ClearOnCreateSessionCompleteDelegate_Handle(Impl->CreateHandle);
		}
		Impl->CreateHandle.Reset();
	}
	FinishHost(bWasSuccessful, EALKConnectionError::NetworkError);
}

void UALKSessionsOnlineProvider::FinishHost(bool bSuccess, EALKConnectionError Error)
{
	if (!bSuccess)
	{
		UE_LOG(LogALK, Warning, TEXT("Host Session: the online service could not create the session."));
	}
	NotifyHostComplete(bSuccess, Error);
}

// ---------------------------------------------------------------- Destroy

void UALKSessionsOnlineProvider::DestroyCurrentSession()
{
	const IOnlineSessionPtr Sessions = ALKSessionsPrivate::GetSessions(this, Impl);
	if (bDestroyPending)
	{
		return;
	}
	if (!Sessions.IsValid() || !Sessions->GetNamedSession(NAME_GameSession))
	{
		bDestroyPending = true;
		HandleDestroyComplete(NAME_GameSession, true);
		return;
	}
	bDestroyPending = true;
	const bool bStarted = Sessions->DestroySession(NAME_GameSession, FOnDestroySessionCompleteDelegate::CreateUObject(this, &ThisClass::HandleDestroyComplete));
	if (!bStarted && bDestroyPending)
	{
		HandleDestroyComplete(NAME_GameSession, false);
	}
}

void UALKSessionsOnlineProvider::HandleDestroyComplete(FName SessionName, bool bWasSuccessful)
{
	if (!bDestroyPending || SessionName != NAME_GameSession)
	{
		return;
	}
	bDestroyPending = false;

	const bool bHost = bHostAfterDestroy;
	const bool bJoin = bJoinAfterDestroy;
	const bool bNotify = bNotifyAfterDestroy;
	bHostAfterDestroy = bJoinAfterDestroy = bNotifyAfterDestroy = false;

	if (bNotify)
	{
		NotifySessionDestroyed();
	}
	if (bHost)
	{
		CreatePendingSession();
	}
	else if (bJoin)
	{
		JoinPendingSession();
	}
}

void UALKSessionsOnlineProvider::LeaveSession_Implementation()
{
	// A search or join in flight has nothing to come back to.
	if (bFindPending)
	{
		EndFind();
	}
	bHostAfterDestroy = false;
	bJoinAfterDestroy = false;
	if (bDestroyPending)
	{
		bNotifyAfterDestroy = true;
		return;
	}
	bNotifyAfterDestroy = true;
	DestroyCurrentSession();
}

void UALKSessionsOnlineProvider::StartSession_Implementation()
{
	const IOnlineSessionPtr Sessions = ALKSessionsPrivate::GetSessions(this, Impl);
	if (!Sessions.IsValid() || !Sessions->GetNamedSession(NAME_GameSession))
	{
		return;
	}
	const EOnlineSessionState::Type State = Sessions->GetSessionState(NAME_GameSession);
	if (State == EOnlineSessionState::Pending || State == EOnlineSessionState::Ended)
	{
		Sessions->StartSession(NAME_GameSession);
	}
}

void UALKSessionsOnlineProvider::EndSession_Implementation()
{
	const IOnlineSessionPtr Sessions = ALKSessionsPrivate::GetSessions(this, Impl);
	if (!Sessions.IsValid() || !Sessions->GetNamedSession(NAME_GameSession))
	{
		return;
	}
	if (Sessions->GetSessionState(NAME_GameSession) == EOnlineSessionState::InProgress)
	{
		Sessions->EndSession(NAME_GameSession);
	}
}

void UALKSessionsOnlineProvider::UpdateSession_Implementation(const FALKHostSettings& Settings)
{
	const IOnlineSessionPtr Sessions = ALKSessionsPrivate::GetSessions(this, Impl);
	const FOnlineSessionSettings* Current = Sessions.IsValid() ? Sessions->GetSessionSettings(NAME_GameSession) : nullptr;
	if (!Current)
	{
		return;
	}
	FOnlineSessionSettings Updated = *Current;
	Updated.NumPublicConnections = FMath::Max(1, Settings.MaxPlayers);
	Updated.bShouldAdvertise = !Settings.bPrivate;
	ALKSessionsPrivate::FillAdvertisedSettings(Updated, Settings, GetLocalDisplayName(), GetOnlineConfig()->GetResolvedBuildVersion(), CurrentRegion);
	Sessions->UpdateSession(NAME_GameSession, Updated, true);
}

// ---------------------------------------------------------------- Find

void UALKSessionsOnlineProvider::FindSessions_Implementation(const FALKSessionFilter& Filter)
{
	BeginFind(Filter, false);
}

void UALKSessionsOnlineProvider::CancelFindSessions_Implementation()
{
	if (bFindPending && !bFindForMatchmaking)
	{
		EndFind();
	}
}

void UALKSessionsOnlineProvider::BeginFind(const FALKSessionFilter& Filter, bool bForMatchmaking)
{
	BindServiceEvents();
	const IOnlineSessionPtr Sessions = ALKSessionsPrivate::GetSessions(this, Impl);
	if (!Impl.IsValid() || !Sessions.IsValid())
	{
		UE_LOG(LogALK, Warning, TEXT("Find Sessions: no online service with sessions is loaded."));
		if (bForMatchmaking)
		{
			NotifyMatchmakingFailed(EALKConnectionError::NetworkError);
		}
		else
		{
			TArray<FALKSessionInfo> Nothing;
			AugmentFindResults(Filter, Nothing);
			NotifyFindComplete(Nothing.Num() > 0, Nothing);
		}
		return;
	}
	if (bFindPending)
	{
		// One search at a time: the new one replaces the old.
		EndFind();
	}

	const UALKOnlineConfig* Config = GetOnlineConfig();
	const TSharedRef<FOnlineSessionSearch> Search = MakeShared<FOnlineSessionSearch>();
	Search->MaxSearchResults = Config->MaxSearchResults;
	Search->bIsLanQuery = Filter.bLANOnly || IsLANOnly();
	Search->PingBucketSize = 50;
	Search->TimeoutInSeconds = Config->FindTimeoutSeconds;
	if (!Search->bIsLanQuery)
	{
		// Hosted games are lobbies on services that have them.
		Search->QuerySettings.Set(SEARCH_LOBBIES, true, EOnlineComparisonOp::Equals);
#if UE_VERSION_OLDER_THAN(5, 5, 0)
		// Before 5.5 some services pick lobby searches by the presence key (removed from the engine in 5.7).
		Search->QuerySettings.Set(FName(TEXT("PRESENCESEARCH")), true, EOnlineComparisonOp::Equals);
#endif
	}

	Impl->ActiveSearch = Search;
	ActiveFilter = Filter;
	bFindPending = true;
	bFindForMatchmaking = bForMatchmaking;
	Impl->FindHandle = Sessions->AddOnFindSessionsCompleteDelegate_Handle(FOnFindSessionsCompleteDelegate::CreateUObject(this, &ThisClass::HandleFindComplete));

	// A search that never answers still ends.
	Impl->FindTimeoutHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this, [this](float)
	{
		if (Impl.IsValid())
		{
			Impl->FindTimeoutHandle.Reset();
		}
		if (bFindPending)
		{
			UE_LOG(LogALK, Log, TEXT("Find Sessions: the search took too long and was ended with what it had."));
			HandleFindComplete(true);
		}
		return false;
	}), Config->FindTimeoutSeconds + 2.f);

	const FUniqueNetIdPtr NetId = ALKSessionsPrivate::GetLocalNetId(this);
	const bool bStarted = NetId.IsValid() ? Sessions->FindSessions(*NetId, Search) : Sessions->FindSessions(ALKSessionsPrivate::LocalUserNum, Search);
	if (!bStarted && bFindPending && Impl.IsValid() && Impl->ActiveSearch == Search)
	{
		HandleFindComplete(false);
	}
}

TArray<FALKSessionInfo> UALKSessionsOnlineProvider::EndFind()
{
	TArray<FALKSessionInfo> Results;
	if (!bFindPending || !Impl.IsValid())
	{
		bFindPending = false;
		return Results;
	}
	bFindPending = false;

	const TSharedPtr<FOnlineSessionSearch> Search = Impl->ActiveSearch;
	Impl->ActiveSearch.Reset();
	if (Impl->FindTimeoutHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(Impl->FindTimeoutHandle);
		Impl->FindTimeoutHandle.Reset();
	}
	if (const IOnlineSessionPtr Sessions = Impl->Sessions.Pin())
	{
		Sessions->ClearOnFindSessionsCompleteDelegate_Handle(Impl->FindHandle);
		if (Search.IsValid() && Search->SearchState == EOnlineAsyncTaskState::InProgress)
		{
			Sessions->CancelFindSessions();
		}
	}
	Impl->FindHandle.Reset();

	if (Search.IsValid())
	{
		Impl->LastSearch = Search;
		const FString LocalUserId = GetLocalUserId();
		for (int32 Index = 0; Index < Search->SearchResults.Num(); ++Index)
		{
			const FOnlineSessionSearchResult& Result = Search->SearchResults[Index];
			if (!Result.IsValid())
			{
				continue;
			}
			// The player's own hosted game is not something to join.
			if (Result.Session.OwningUserId.IsValid() && Result.Session.OwningUserId->ToString() == LocalUserId)
			{
				continue;
			}
			Results.Add(MakeSessionInfo(Result.Session, Result.PingInMs, Index));
		}
	}
	return Results;
}

void UALKSessionsOnlineProvider::HandleFindComplete(bool bWasSuccessful)
{
	if (!bFindPending)
	{
		return;
	}
	const bool bForMatchmaking = bFindForMatchmaking;
	TArray<FALKSessionInfo> Results = EndFind();
	if (bForMatchmaking)
	{
		FinishQuickMatch(Results);
	}
	else
	{
		AugmentFindResults(ActiveFilter, Results);
		NotifyFindComplete(bWasSuccessful || Results.Num() > 0, Results);
	}
}

FALKSessionInfo UALKSessionsOnlineProvider::MakeSessionInfo(const FOnlineSession& Session, int32 PingMs, int32 SearchResultIndex) const
{
	using namespace ALKSessionsPrivate;
	const FOnlineSessionSettings& Settings = Session.SessionSettings;

	FALKSessionInfo Info;
	Info.SessionId = Session.GetSessionIdStr();
	Info.HostUserId = Session.OwningUserId.IsValid() ? Session.OwningUserId->ToString() : FString();
	Info.HostName = GetStringSetting(Settings, SessionKeyHost);
	if (Info.HostName.IsEmpty())
	{
		Info.HostName = Session.OwningUserName;
	}
	Info.ServerName = GetStringSetting(Settings, SessionKeyName);
	if (Info.ServerName.IsEmpty())
	{
		Info.ServerName = Info.HostName;
	}

	const FString Mode = GetStringSetting(Settings, SessionKeyMode);
	const FString Map = GetStringSetting(Settings, SessionKeyMap);
	Info.ModeId = Mode.IsEmpty() ? NAME_None : FName(*Mode);
	Info.MapId = Map.IsEmpty() ? NAME_None : FName(*Map);

	Info.MaxPlayers = Settings.NumPublicConnections + Settings.NumPrivateConnections;
	Info.CurrentPlayers = FMath::Clamp(Info.MaxPlayers - Session.NumOpenPublicConnections - Session.NumOpenPrivateConnections, 0, FMath::Max(0, Info.MaxPlayers));
	// Services that cannot measure a ping report a very large number.
	Info.PingMs = PingMs >= 0 && PingMs < 9999 ? PingMs : INDEX_NONE;
	Info.bLAN = Settings.bIsLANMatch;

	int32 PasswordFlag = 0;
	Settings.Get(SessionKeyPassword, PasswordFlag);
	Info.bPasswordProtected = PasswordFlag != 0;

	Info.Region = GetStringSetting(Settings, SessionKeyRegion);
	Info.BuildVersion = GetStringSetting(Settings, SessionKeyVersion);
	Info.bVersionMatches = Info.BuildVersion.IsEmpty() || Info.BuildVersion == GetOnlineConfig()->GetResolvedBuildVersion();

	FString Rules;
	Settings.Get(SessionKeyRules, Rules);
	Info.Rules = FALKGameRules::FromOptionsString(Rules).ToMap();
	Info.SearchResultIndex = SearchResultIndex;
	return Info;
}

FALKSessionInfo UALKSessionsOnlineProvider::RememberDirectResult(const FOnlineSessionSearchResult& Result)
{
	if (!Impl.IsValid())
	{
		return FALKSessionInfo();
	}
	const FString SessionId = Result.Session.GetSessionIdStr();
	int32 Index = Impl->DirectResults.IndexOfByPredicate([&SessionId](const FOnlineSessionSearchResult& Each) { return Each.Session.GetSessionIdStr() == SessionId; });
	if (Index == INDEX_NONE)
	{
		Index = Impl->DirectResults.Add(Result);
	}
	else
	{
		Impl->DirectResults[Index] = Result;
	}
	return MakeSessionInfo(Result.Session, Result.PingInMs, ALKSessionsPrivate::DirectResultBase - Index);
}

const FOnlineSessionSearchResult* UALKSessionsOnlineProvider::FindSearchResult(const FALKSessionInfo& Session) const
{
	if (!Impl.IsValid())
	{
		return nullptr;
	}
	auto Matches = [&Session](const FOnlineSessionSearchResult& Result)
	{
		return Result.IsValid() && (Session.SessionId.IsEmpty() || Result.Session.GetSessionIdStr() == Session.SessionId);
	};

	const int32 DirectIndex = ALKSessionsPrivate::DirectResultBase - Session.SearchResultIndex;
	if (Session.SearchResultIndex <= ALKSessionsPrivate::DirectResultBase && Impl->DirectResults.IsValidIndex(DirectIndex) && Matches(Impl->DirectResults[DirectIndex]))
	{
		return &Impl->DirectResults[DirectIndex];
	}
	if (Impl->LastSearch.IsValid())
	{
		const TArray<FOnlineSessionSearchResult>& Results = Impl->LastSearch->SearchResults;
		if (Results.IsValidIndex(Session.SearchResultIndex) && Matches(Results[Session.SearchResultIndex]))
		{
			return &Results[Session.SearchResultIndex];
		}
		// The index is from an older search: look the session up by its id.
		if (!Session.SessionId.IsEmpty())
		{
			if (const FOnlineSessionSearchResult* Found = Results.FindByPredicate(Matches))
			{
				return Found;
			}
		}
	}
	if (!Session.SessionId.IsEmpty())
	{
		if (const FOnlineSessionSearchResult* Found = Impl->DirectResults.FindByPredicate(Matches))
		{
			return Found;
		}
		if (Impl->RejoinResult.IsValid() && Matches(*Impl->RejoinResult))
		{
			return Impl->RejoinResult.Get();
		}
	}
	return nullptr;
}

bool UALKSessionsOnlineProvider::GetCurrentSession_Implementation(FALKSessionInfo& OutSession) const
{
	const IOnlineSessionPtr Sessions = ALKSessionsPrivate::GetSessions(this, Impl);
	const FNamedOnlineSession* Named = Sessions.IsValid() ? Sessions->GetNamedSession(NAME_GameSession) : nullptr;
	if (!Named)
	{
		return false;
	}
	OutSession = MakeSessionInfo(*Named, INDEX_NONE, INDEX_NONE);
	OutSession.bInProgress = Named->SessionState == EOnlineSessionState::InProgress;
	return true;
}

bool UALKSessionsOnlineProvider::GetSessionConnectString_Implementation(const FALKSessionInfo& Session, FString& OutConnectString) const
{
	const IOnlineSessionPtr Sessions = ALKSessionsPrivate::GetSessions(this, Impl);
	const FOnlineSessionSearchResult* Result = FindSearchResult(Session);
	// Only local network games have an address worth keeping; a platform lobby is reached through the service.
	if (!Sessions.IsValid() || !Result || !Result->Session.SessionSettings.bIsLANMatch)
	{
		return false;
	}
	return Sessions->GetResolvedConnectString(*Result, NAME_GamePort, OutConnectString) && !OutConnectString.IsEmpty();
}

// ---------------------------------------------------------------- Join

void UALKSessionsOnlineProvider::JoinSession_Implementation(const FALKSessionInfo& Session, const FString& Password)
{
	BindServiceEvents();
	const IOnlineSessionPtr Sessions = ALKSessionsPrivate::GetSessions(this, Impl);
	if (!Impl.IsValid() || !Sessions.IsValid())
	{
		NotifyJoinComplete(false, EALKConnectionError::NetworkError, FString());
		return;
	}
	if (bJoinPending)
	{
		return;
	}
	if (!FindSearchResult(Session))
	{
		NotifyJoinComplete(false, EALKConnectionError::SessionNotFound, FString());
		return;
	}

	PendingJoinSession = Session;
	if (Sessions->GetNamedSession(NAME_GameSession))
	{
		bJoinAfterDestroy = true;
		bHostAfterDestroy = false;
		DestroyCurrentSession();
		return;
	}
	JoinPendingSession();
}

void UALKSessionsOnlineProvider::JoinPendingSession()
{
	const IOnlineSessionPtr Sessions = ALKSessionsPrivate::GetSessions(this, Impl);
	const FOnlineSessionSearchResult* Found = FindSearchResult(PendingJoinSession);
	if (!Sessions.IsValid() || !Found)
	{
		NotifyJoinComplete(false, EALKConnectionError::SessionNotFound, FString());
		return;
	}

	// A copy: the search results may be replaced while the join runs.
	FOnlineSessionSearchResult Result = *Found;
	if (!Result.Session.SessionSettings.bIsLANMatch && !Result.Session.SessionSettings.bIsDedicated)
	{
		// Some services do not send these back with a search result, and the join then looks for the wrong kind of session.
		Result.Session.SessionSettings.bUsesPresence = true;
		Result.Session.SessionSettings.bUseLobbiesIfAvailable = true;
	}

	// Kept for Rejoin Session.
	Impl->RejoinResult = MakeShared<FOnlineSessionSearchResult>(Result);
	RejoinSessionInfo = PendingJoinSession;

	bJoinPending = true;
	Impl->JoinHandle = Sessions->AddOnJoinSessionCompleteDelegate_Handle(FOnJoinSessionCompleteDelegate::CreateWeakLambda(this,
		[this](FName SessionName, EOnJoinSessionCompleteResult::Type JoinResult)
		{
			HandleJoinComplete(SessionName, static_cast<int32>(JoinResult));
		}));

	const FUniqueNetIdPtr NetId = ALKSessionsPrivate::GetLocalNetId(this);
	const bool bStarted = NetId.IsValid()
		? Sessions->JoinSession(*NetId, NAME_GameSession, Result)
		: Sessions->JoinSession(ALKSessionsPrivate::LocalUserNum, NAME_GameSession, Result);
	if (!bStarted && bJoinPending)
	{
		HandleJoinComplete(NAME_GameSession, static_cast<int32>(EOnJoinSessionCompleteResult::UnknownError));
	}
}

void UALKSessionsOnlineProvider::HandleJoinComplete(FName SessionName, int32 Result)
{
	if (!bJoinPending || SessionName != NAME_GameSession)
	{
		return;
	}
	bJoinPending = false;

	const IOnlineSessionPtr Sessions = ALKSessionsPrivate::GetSessions(this, Impl);
	if (Sessions.IsValid())
	{
		Sessions->ClearOnJoinSessionCompleteDelegate_Handle(Impl->JoinHandle);
		Impl->JoinHandle.Reset();
	}

	if (Result != EOnJoinSessionCompleteResult::Success || !Sessions.IsValid())
	{
		bHasRejoinSession = false;
		FinishJoin(false, ALKSessionsPrivate::JoinResultToError(Result), FString());
		return;
	}

	FString ConnectString;
	if (!Sessions->GetResolvedConnectString(NAME_GameSession, ConnectString) || ConnectString.IsEmpty())
	{
		// Joined, but nowhere to travel to: leave the session again so the next attempt starts clean.
		bHasRejoinSession = false;
		Sessions->DestroySession(NAME_GameSession);
		FinishJoin(false, EALKConnectionError::NetworkError, FString());
		return;
	}
	bHasRejoinSession = true;
	FinishJoin(true, EALKConnectionError::None, ConnectString);
}

void UALKSessionsOnlineProvider::FinishJoin(bool bSuccess, EALKConnectionError Error, const FString& ConnectString)
{
	NotifyJoinComplete(bSuccess, Error, ConnectString);
}

bool UALKSessionsOnlineProvider::GetRejoinInfo_Implementation(FALKSessionInfo& OutSession) const
{
	if (!bHasRejoinSession || !Impl.IsValid() || !Impl->RejoinResult.IsValid())
	{
		return false;
	}
	OutSession = RejoinSessionInfo;
	return true;
}

void UALKSessionsOnlineProvider::RejoinSession_Implementation()
{
	if (!bHasRejoinSession || !Impl.IsValid() || !Impl->RejoinResult.IsValid())
	{
		NotifyJoinComplete(false, EALKConnectionError::SessionNotFound, FString());
		return;
	}
	JoinSession(RememberDirectResult(*Impl->RejoinResult), FString());
}

// ---------------------------------------------------------------- Quick match

void UALKSessionsOnlineProvider::StartMatchmaking_Implementation(FName PlaylistId)
{
	BeginQuickMatchSearch(PlaylistId);
}

void UALKSessionsOnlineProvider::CancelMatchmaking_Implementation()
{
	StopQuickMatchSearch();
	NotifyMatchmakingFailed(EALKConnectionError::Cancelled);
}

void UALKSessionsOnlineProvider::BeginQuickMatchSearch(FName PlaylistId)
{
	QuickMatchPlaylist = PlaylistId;
	const FALKPlaylist* Playlist = GetOnlineConfig()->FindPlaylist(PlaylistId);

	FALKSessionFilter Filter;
	Filter.ModeId = Playlist ? Playlist->ModeId : NAME_None;
	Filter.bHideFull = true;
	Filter.bHidePasswordProtected = true;
	Filter.Region = CurrentRegion;
	BeginFind(Filter, true);
}

bool UALKSessionsOnlineProvider::StopQuickMatchSearch()
{
	if (bFindPending && bFindForMatchmaking)
	{
		EndFind();
		return true;
	}
	return false;
}

void UALKSessionsOnlineProvider::FinishQuickMatch(const TArray<FALKSessionInfo>& Results)
{
	const UALKOnlineConfig* Config = GetOnlineConfig();
	FALKSessionInfo Best;
	if (UALKLobbyStatics::PickBestSession(Results, ActiveFilter, Best))
	{
		NotifyMatchReady(FString(), Best);
		return;
	}
	if (!Config->bQuickMatchHostsWhenNothingFound)
	{
		NotifyMatchmakingFailed(EALKConnectionError::SessionNotFound);
		return;
	}

	// Nothing to join: this player hosts the playlist's mode, and the next searcher finds it.
	const FALKPlaylist* Playlist = Config->FindPlaylist(QuickMatchPlaylist);
	FALKHostSettings Settings;
	Settings.ModeId = Playlist ? Playlist->ModeId : ActiveFilter.ModeId;
	const FALKModeDefinition* Mode = Config->FindMode(Settings.ModeId);
	Settings.MaxPlayers = Playlist && Playlist->MaxPlayers > 0 ? Playlist->MaxPlayers : (Mode ? Mode->DefaultMaxPlayers : Config->DefaultMaxPlayers);
	Settings.bLAN = IsLANOnly();
	NotifyMatchHostRequested(Config->SanitizeHostSettings(Settings, GetLocalDisplayName()));
}

// ---------------------------------------------------------------- Friends and invites

void UALKSessionsOnlineProvider::QueryFriends_Implementation()
{
	BindServiceEvents();
	const IOnlineFriendsPtr Friends = Impl.IsValid() ? Impl->Friends.Pin() : nullptr;
	if (!Friends.IsValid())
	{
		CachedFriends.Reset();
		NotifyFriendsChanged();
		return;
	}
	const bool bStarted = Friends->ReadFriendsList(ALKSessionsPrivate::LocalUserNum, EFriendsLists::ToString(EFriendsLists::Default),
		FOnReadFriendsListComplete::CreateWeakLambda(this, [this](int32 /*LocalUserNum*/, bool /*bWasSuccessful*/, const FString& /*ListName*/, const FString& /*ErrorStr*/)
		{
			RebuildFriends();
			NotifyFriendsChanged();
		}));
	if (!bStarted)
	{
		RebuildFriends();
		NotifyFriendsChanged();
	}
}

void UALKSessionsOnlineProvider::RebuildFriends()
{
	CachedFriends.Reset();
	const IOnlineFriendsPtr Friends = Impl.IsValid() ? Impl->Friends.Pin() : nullptr;
	TArray<TSharedRef<FOnlineFriend>> List;
	if (!Friends.IsValid() || !Friends->GetFriendsList(ALKSessionsPrivate::LocalUserNum, EFriendsLists::ToString(EFriendsLists::Default), List))
	{
		return;
	}
	for (const TSharedRef<FOnlineFriend>& Each : List)
	{
		const FOnlineUserPresence& Presence = Each->GetPresence();
		FALKFriend Friend;
		Friend.UserId = Each->GetUserId()->ToString();
		Friend.DisplayName = Each->GetDisplayName();
		if (!Presence.bIsOnline)
		{
			Friend.Presence = EALKPresence::Offline;
		}
		else if (Presence.bIsPlayingThisGame)
		{
			Friend.Presence = Presence.bIsJoinable ? EALKPresence::InLobby : EALKPresence::InGame;
		}
		else
		{
			Friend.Presence = EALKPresence::Online;
		}
		Friend.StatusText = FText::FromString(Presence.Status.StatusStr);
		Friend.bJoinable = Presence.bIsOnline && Presence.bIsPlayingThisGame && Presence.bIsJoinable;
		Friend.bInvitable = Presence.bIsOnline;
		CachedFriends.Add(MoveTemp(Friend));
	}
	// Who can be reached first, then by name.
	CachedFriends.StableSort([](const FALKFriend& A, const FALKFriend& B)
	{
		const bool bOnlineA = A.Presence != EALKPresence::Offline;
		const bool bOnlineB = B.Presence != EALKPresence::Offline;
		return bOnlineA != bOnlineB ? bOnlineA : A.DisplayName.Compare(B.DisplayName, ESearchCase::IgnoreCase) < 0;
	});
}

TArray<FALKFriend> UALKSessionsOnlineProvider::GetFriends_Implementation() const
{
	return CachedFriends;
}

void UALKSessionsOnlineProvider::InviteFriendToGame_Implementation(const FString& UserId)
{
	const IOnlineSessionPtr Sessions = ALKSessionsPrivate::GetSessions(this, Impl);
	const FUniqueNetIdPtr FriendId = ALKSessionsPrivate::MakeNetId(this, UserId);
	if (Sessions.IsValid() && FriendId.IsValid() && Sessions->GetNamedSession(NAME_GameSession))
	{
		Sessions->SendSessionInviteToFriend(ALKSessionsPrivate::LocalUserNum, NAME_GameSession, *FriendId);
	}
}

void UALKSessionsOnlineProvider::JoinFriend_Implementation(const FString& UserId)
{
	BindServiceEvents();
	const IOnlineSessionPtr Sessions = ALKSessionsPrivate::GetSessions(this, Impl);
	const FUniqueNetIdPtr FriendId = ALKSessionsPrivate::MakeNetId(this, UserId);
	if (!Sessions.IsValid() || !FriendId.IsValid())
	{
		return;
	}
	Sessions->ClearOnFindFriendSessionCompleteDelegate_Handle(ALKSessionsPrivate::LocalUserNum, Impl->FindFriendHandle);
	Impl->FindFriendHandle = Sessions->AddOnFindFriendSessionCompleteDelegate_Handle(ALKSessionsPrivate::LocalUserNum, FOnFindFriendSessionCompleteDelegate::CreateWeakLambda(this,
		[this](int32 /*LocalUserNum*/, bool bWasSuccessful, const TArray<FOnlineSessionSearchResult>& Results)
		{
			if (Impl.IsValid())
			{
				if (const IOnlineSessionPtr Pinned = Impl->Sessions.Pin())
				{
					Pinned->ClearOnFindFriendSessionCompleteDelegate_Handle(ALKSessionsPrivate::LocalUserNum, Impl->FindFriendHandle);
				}
				Impl->FindFriendHandle.Reset();
			}
			if (bWasSuccessful && Results.Num() > 0 && Results[0].IsValid())
			{
				NotifyJoinRequested(RememberDirectResult(Results[0]));
			}
		}));
	Sessions->FindFriendSession(ALKSessionsPrivate::LocalUserNum, *FriendId);
}

void UALKSessionsOnlineProvider::RespondToGameInvite_Implementation(const FString& FromUserId, bool bAccept)
{
	FALKSessionInfo Session;
	if (PendingInvites.RemoveAndCopyValue(FromUserId, Session) && bAccept)
	{
		NotifyJoinRequested(Session);
	}
}

void UALKSessionsOnlineProvider::ShowInviteOverlay_Implementation()
{
	IOnlineSubsystem* Service = ALKSessionsPrivate::GetService(this);
	const IOnlineExternalUIPtr ExternalUI = Service ? Service->GetExternalUIInterface() : nullptr;
	if (ExternalUI.IsValid())
	{
		ExternalUI->ShowInviteUI(ALKSessionsPrivate::LocalUserNum, NAME_GameSession);
	}
}

// ---------------------------------------------------------------- Voice

void UALKSessionsOnlineProvider::SetPlayerMuted_Implementation(const FString& UserId, bool bMuted)
{
	const IOnlineVoicePtr Voice = Impl.IsValid() ? Impl->Voice.Pin() : nullptr;
	const FUniqueNetIdPtr PlayerId = ALKSessionsPrivate::MakeNetId(this, UserId);
	if (Voice.IsValid() && PlayerId.IsValid())
	{
		if (bMuted)
		{
			Voice->MuteRemoteTalker(ALKSessionsPrivate::LocalUserNum, *PlayerId, false);
		}
		else
		{
			Voice->UnmuteRemoteTalker(ALKSessionsPrivate::LocalUserNum, *PlayerId, false);
		}
	}
}

void UALKSessionsOnlineProvider::SetVoiceMode_Implementation(EALKVoiceMode Mode)
{
	VoiceMode = Mode;
	BindServiceEvents();
	const IOnlineVoicePtr Voice = Impl.IsValid() ? Impl->Voice.Pin() : nullptr;
	if (!Voice.IsValid())
	{
		return;
	}
	if (Mode == EALKVoiceMode::OpenMic)
	{
		Voice->StartNetworkedVoice(ALKSessionsPrivate::LocalUserNum);
	}
	else
	{
		// Off, and push to talk until the key goes down.
		Voice->StopNetworkedVoice(ALKSessionsPrivate::LocalUserNum);
	}
}

void UALKSessionsOnlineProvider::SetPushToTalkActive_Implementation(bool bActive)
{
	const IOnlineVoicePtr Voice = Impl.IsValid() ? Impl->Voice.Pin() : nullptr;
	if (!Voice.IsValid() || VoiceMode != EALKVoiceMode::PushToTalk)
	{
		return;
	}
	if (bActive)
	{
		Voice->StartNetworkedVoice(ALKSessionsPrivate::LocalUserNum);
	}
	else
	{
		Voice->StopNetworkedVoice(ALKSessionsPrivate::LocalUserNum);
	}
}

void UALKSessionsOnlineProvider::SetRegion_Implementation(const FString& RegionId)
{
	CurrentRegion = RegionId;
}
