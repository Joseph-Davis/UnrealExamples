// Copyright 2026 CodeLikeMe. All Rights Reserved.

#include "Lobby/ALKNetGameModeBase.h"
#include "Lobby/ALKGameSession.h"
#include "Lobby/ALKLobbyGameState.h"
#include "Lobby/ALKLobbyPlayerController.h"
#include "Lobby/ALKLobbyPlayerState.h"
#include "Lobby/ALKLobbyStatics.h"
#include "Lobby/ALKPlayerNetComponent.h"
#include "Online/ALKOnlineConfig.h"
#include "Online/ALKOnlineSubsystem.h"
#include "AssetproLobbyKit.h"
#include "ALKDeveloperSettings.h"
#include "Engine/NetConnection.h"
#include "Engine/World.h"
#include "GameFramework/GameSession.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Misc/Crc.h"
#include "TimerManager.h"
#include "UObject/Package.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ALKNetGameModeBase)

#define LOCTEXT_NAMESPACE "ALKLobby"

namespace ALKNetGameModePrivate
{
	static const FName JoinEvent(TEXT("Join"));
	static const FName LeaveEvent(TEXT("Leave"));

	/** Made-up, neutral names for placeholder players. */
	static const TCHAR* const PlaceholderNames[] = {
		TEXT("Kestrel"), TEXT("Juno_R"), TEXT("OddBadger"), TEXT("Mirela"), TEXT("tin_can"), TEXT("Vasquez77"),
		TEXT("Nomad"), TEXT("PixelMoth"), TEXT("Harrow"), TEXT("Lumen"), TEXT("Quill"), TEXT("Sable")
	};
	static constexpr int32 PlaceholderNameCount = UE_ARRAY_COUNT(PlaceholderNames);
	static const TCHAR* const PlaceholderIdPrefix = TEXT("placeholder-");

	static int32 StableHash(const FString& Text)
	{
		return static_cast<int32>(FCrc::StrCrc32(*Text.ToLower()) & 0x7FFFFFFF);
	}

	/** The teams of every player who takes part, except one (the player a team is being picked for). */
	static TArray<int32> GetOtherTeams(const TArray<AALKLobbyPlayerState*>& Players, const AALKLobbyPlayerState* Except)
	{
		TArray<int32> Teams;
		for (const AALKLobbyPlayerState* Each : Players)
		{
			if (Each && Each != Except && !Each->bSpectating)
			{
				Teams.Add(Each->TeamId);
			}
		}
		return Teams;
	}
}

AALKNetGameModeBase::AALKNetGameModeBase()
{
	PlayerControllerClass = AALKLobbyPlayerController::StaticClass();
	PlayerStateClass = AALKLobbyPlayerState::StaticClass();
	GameStateClass = AALKLobbyGameState::StaticClass();
	GameSessionClass = AALKGameSession::StaticClass();
	bUseSeamlessTravel = true;
	PlaceholderStream.Initialize(4242);
}

UALKOnlineSubsystem* AALKNetGameModeBase::GetOnline() const
{
	return UALKOnlineSubsystem::Get(this);
}

AALKLobbyGameState* AALKNetGameModeBase::GetLobbyGameState() const
{
	return GetGameState<AALKLobbyGameState>();
}

TArray<AALKLobbyPlayerState*> AALKNetGameModeBase::GetLobbyPlayers() const
{
	const AALKLobbyGameState* LobbyState = GetLobbyGameState();
	return LobbyState ? LobbyState->GetLobbyPlayers() : TArray<AALKLobbyPlayerState*>();
}

TArray<AALKLobbyPlayerState*> AALKNetGameModeBase::GetPlaceholders() const
{
	TArray<AALKLobbyPlayerState*> Placeholders = GetLobbyPlayers();
	Placeholders.RemoveAll([](const AALKLobbyPlayerState* Each) { return !Each->bPlaceholder; });
	return Placeholders;
}

bool AALKNetGameModeBase::IsHost(const APlayerController* Player) const
{
	const AALKLobbyPlayerState* State = Player ? Player->GetPlayerState<AALKLobbyPlayerState>() : nullptr;
	return State && State->bHost;
}

AALKLobbyPlayerState* AALKNetGameModeBase::FindPlayerByPlayerId(int32 PlayerId) const
{
	for (AALKLobbyPlayerState* Each : GetLobbyPlayers())
	{
		if (Each->GetPlayerId() == PlayerId)
		{
			return Each;
		}
	}
	return nullptr;
}

APlayerController* AALKNetGameModeBase::FindControllerByPlayerId(int32 PlayerId) const
{
	const AALKLobbyPlayerState* State = FindPlayerByPlayerId(PlayerId);
	return State ? State->GetPlayerController() : nullptr;
}

void AALKNetGameModeBase::SendNotice(APlayerController* Player, const FText& Text) const
{
	if (UALKPlayerNetComponent* NetComponent = UALKPlayerNetComponent::Get(Player))
	{
		NetComponent->ClientNotify(Text);
	}
}

// ---------------------------------------------------------------- Start up

void AALKNetGameModeBase::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	// The settings live in the online subsystem (it outlives the level); the level URL carries the ones that changed, and
	// is all there is on a server that was started straight into this level.
	UALKOnlineSubsystem* Online = GetOnline();
	const UALKOnlineConfig* Config = UALKDeveloperSettings::GetOnlineConfig();
	ServerSettings = Online ? Online->GetServerSettings() : FALKHostSettings();
	ServerSettings.ApplyTravelOptions(Options);
	ServerSettings = Config->SanitizeHostSettings(ServerSettings, FString());

	if (Online)
	{
		Online->SetServerSettings(ServerSettings, false);
		if (GetNetMode() == NM_DedicatedServer)
		{
			Online->RegisterDedicatedServer(ServerSettings);
		}
	}
	if (GameSession)
	{
		GameSession->MaxPlayers = ServerSettings.MaxPlayers;
	}
}

void AALKNetGameModeBase::InitGameState()
{
	Super::InitGameState();
	ApplySettingsToGameState();
}

void AALKNetGameModeBase::ApplySettingsToGameState()
{
	AALKLobbyGameState* LobbyState = GetLobbyGameState();
	if (!LobbyState)
	{
		return;
	}
	const UALKOnlineConfig* Config = UALKDeveloperSettings::GetOnlineConfig();
	LobbyState->ServerName = ServerSettings.ServerName;
	LobbyState->ModeId = ServerSettings.ModeId;
	LobbyState->MapId = ServerSettings.MapId;
	LobbyState->Rules = ServerSettings.Rules;
	LobbyState->MaxPlayers = ServerSettings.MaxPlayers;
	LobbyState->TeamCount = Config->GetTeamCount(ServerSettings.ModeId);
	LobbyState->BotCount = ServerSettings.BotCount;
	LobbyState->bPasswordProtected = ServerSettings.HasPassword();
	if (const UALKOnlineSubsystem* Online = GetOnline())
	{
		// Empty until the server listens; Start Play sets it again.
		LobbyState->LobbyCode = Online->GetLobbyCode();
	}
	LobbyState->MarkLobbyStateChanged();
}

void AALKNetGameModeBase::SyncPlaceholderBots()
{
	const UALKOnlineConfig* Config = UALKDeveloperSettings::GetOnlineConfig();
	if (!Config || !Config->bBotsArePlaceholders)
	{
		return;
	}
	const int32 Wanted = FMath::Max(0, ServerSettings.BotCount);
	const int32 Seated = GetPlaceholderCount();
	if (Seated > Wanted)
	{
		RemovePlaceholderPlayers();
		AddPlaceholderPlayers(Wanted);
	}
	else if (Seated < Wanted)
	{
		AddPlaceholderPlayers(Wanted - Seated);
	}
}

void AALKNetGameModeBase::StartPlay()
{
	Super::StartPlay();

	// The server listens by now, so the code for its address can be made.
	AALKLobbyGameState* LobbyState = GetLobbyGameState();
	const UALKOnlineSubsystem* Online = GetOnline();
	if (LobbyState && Online)
	{
		const FString Code = Online->GetLobbyCode();
		if (LobbyState->LobbyCode != Code)
		{
			LobbyState->LobbyCode = Code;
			LobbyState->MarkLobbyStateChanged();
		}
	}

	RestorePlaceholders();
	SyncPlaceholderBots();
	UpdateHost();

	LastFrameCounter = GFrameCounter;
	LastTickRateTime = FPlatformTime::Seconds();
	GetWorldTimerManager().SetTimer(TickRateTimer, this, &ThisClass::UpdateServerTickRate, 1.f, true);
}

void AALKNetGameModeBase::UpdateServerTickRate()
{
	const double Now = FPlatformTime::Seconds();
	const double Elapsed = Now - LastTickRateTime;
	if (AALKLobbyGameState* LobbyState = GetLobbyGameState())
	{
		if (Elapsed > 0.1)
		{
			LobbyState->ServerTickRate = FMath::RoundToInt32(static_cast<double>(GFrameCounter - LastFrameCounter) / Elapsed);
		}
	}
	LastFrameCounter = GFrameCounter;
	LastTickRateTime = Now;
}

// ---------------------------------------------------------------- Who may join

void AALKNetGameModeBase::PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage)
{
	const UALKOnlineSubsystem* Online = GetOnline();
	const UALKOnlineConfig* Config = UALKDeveloperSettings::GetOnlineConfig();

	// The reasons are the ALKNetReason strings: the joining machine's online subsystem shows the matching dialog.
	const FString UserId = UniqueId.IsValid() ? UniqueId.ToString() : FString();
	if (Online && Online->IsBanned(UserId, Address))
	{
		ErrorMessage = ALKNetReason::Banned;
		return;
	}

	FString ClientVersion;
	if (FALKOptionString::GetOption(Options, ALKNetOption::Version, ClientVersion) && ClientVersion != Config->GetResolvedBuildVersion())
	{
		ErrorMessage = ALKNetReason::WrongVersion;
		return;
	}

	if (ServerSettings.HasPassword())
	{
		FString OfferedPassword;
		FALKOptionString::GetOption(Options, ALKNetOption::Password, OfferedPassword);
		if (OfferedPassword != ServerSettings.Password)
		{
			ErrorMessage = ALKNetReason::WrongPassword;
			return;
		}
	}

	// Real players only: a placeholder gives up its seat.
	if (GetNumPlayers() + NumTravellingPlayers >= ServerSettings.MaxPlayers)
	{
		ErrorMessage = ALKNetReason::ServerFull;
		return;
	}

	Super::PreLogin(Options, Address, UniqueId, ErrorMessage);
}

void AALKNetGameModeBase::GenericPlayerInitialization(AController* C)
{
	Super::GenericPlayerInitialization(C);
	if (APlayerController* Player = Cast<APlayerController>(C))
	{
		OnPlayerJoined(Player);
	}
}

void AALKNetGameModeBase::OnPlayerJoined(APlayerController* Player)
{
	AALKLobbyPlayerState* State = Player ? Player->GetPlayerState<AALKLobbyPlayerState>() : nullptr;
	if (!State)
	{
		return;
	}

	// The id everything else goes by: the backend's when there is one.
	FString UserId = State->GetUniqueId().IsValid() ? State->GetUniqueId().ToString() : FString();
	if (UserId.IsEmpty())
	{
		UserId = FString::Printf(TEXT("Player-%d"), State->GetPlayerId());
	}
	State->SetUserId(UserId);
	State->SetPlaceholder(false);

	// After a travel that was not seamless the player state is new: the roster written before it has their seat.
	int32 PreferredSlot = State->Slot;
	int32 PreferredTeam = State->TeamId;
	FALKRosterEntry Entry;
	const UALKOnlineSubsystem* Online = GetOnline();
	if (State->Slot < 0 && Online && Online->FindRosterEntry(UserId, Entry) && !Entry.bPlaceholder)
	{
		PreferredSlot = Entry.Slot;
		PreferredTeam = Entry.TeamId;
		State->SetLoadoutId(Entry.LoadoutId);
		State->SetColorIndex(Entry.ColorIndex);
		State->SetPlayerLevel(Entry.PlayerLevel);
		State->SetSpectating(Entry.bSpectating);
	}
	const bool bNewToGame = PreferredSlot < 0;
	AssignSlotAndTeam(State, PreferredSlot, PreferredTeam);
	UpdateHost();

	if (bNewToGame && !Player->IsLocalController() && Player->GetNetMode() != NM_Standalone)
	{
		// A moment later, when the name the player chose has arrived.
		FTimerHandle Handle;
		GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(this, [this, WeakState = TWeakObjectPtr<AALKLobbyPlayerState>(State)]()
		{
			if (AALKLobbyPlayerState* Joined = WeakState.Get())
			{
				AnnouncePlayer(Joined, true);
			}
		}), 1.f, false);
	}
	K2_OnLobbyPlayerJoined(Player);
	OnRosterChanged();
}

void AALKNetGameModeBase::Logout(AController* Exiting)
{
	OnPlayerLeft(Exiting);
	Super::Logout(Exiting);
	// Once their player state is out of the list: a new host if the old one left, and the lobby's start check.
	GetWorldTimerManager().SetTimerForNextTick(this, &ThisClass::HandleDelayedHostUpdate);
}

void AALKNetGameModeBase::OnPlayerLeft(AController* Player)
{
	if (AALKLobbyPlayerState* State = Player ? Player->GetPlayerState<AALKLobbyPlayerState>() : nullptr)
	{
		AnnouncePlayer(State, false);
	}
	K2_OnLobbyPlayerLeft(Player);
}

void AALKNetGameModeBase::HandleDelayedHostUpdate()
{
	UpdateHost();
	OnRosterChanged();
}

void AALKNetGameModeBase::AnnouncePlayer(AALKLobbyPlayerState* Player, bool bJoined)
{
	AALKLobbyGameState* LobbyState = GetLobbyGameState();
	if (!LobbyState || !Player)
	{
		return;
	}
	FALKFeedEvent Event;
	Event.EventId = bJoined ? ALKNetGameModePrivate::JoinEvent : ALKNetGameModePrivate::LeaveEvent;
	Event.ActorName = Player->GetPlayerName();
	Event.ActorTeam = Player->TeamId;
	Event.ActorStableIndex = Player->GetStableIndex();
	Event.Text = FText::Format(bJoined ? LOCTEXT("PlayerJoined", "{0} joined") : LOCTEXT("PlayerLeft", "{0} left"), FText::FromString(Event.ActorName));
	LobbyState->BroadcastFeedEvent(Event);
}

// ---------------------------------------------------------------- Slots, teams, host

void AALKNetGameModeBase::AssignSlotAndTeam(AALKLobbyPlayerState* Player, int32 PreferredSlot, int32 PreferredTeam)
{
	AALKLobbyGameState* LobbyState = GetLobbyGameState();
	if (!Player || !LobbyState)
	{
		return;
	}
	const int32 MaxPlayers = FMath::Max(1, LobbyState->MaxPlayers);

	auto GetTakenSlots = [this, Player]()
	{
		TArray<int32> Taken;
		for (const AALKLobbyPlayerState* Each : GetLobbyPlayers())
		{
			if (Each != Player && Each->Slot >= 0)
			{
				Taken.Add(Each->Slot);
			}
		}
		return Taken;
	};

	// ---- Slot
	TArray<int32> TakenSlots = GetTakenSlots();
	int32 Slot = PreferredSlot >= 0 && PreferredSlot < MaxPlayers && !TakenSlots.Contains(PreferredSlot) ? PreferredSlot : UALKLobbyStatics::FindFreeSlot(TakenSlots, MaxPlayers);
	if (Slot == INDEX_NONE && !Player->bPlaceholder)
	{
		// Every seat is taken: a placeholder gives its seat to the real player.
		const TArray<AALKLobbyPlayerState*> Placeholders = GetPlaceholders();
		if (Placeholders.Num() > 0)
		{
			AALKLobbyPlayerState* Removed = Placeholders.Last();
			Slot = Removed->Slot;
			Removed->Destroy();
			TakenSlots = GetTakenSlots();
			TakenSlots.Remove(Slot);
		}
	}
	if (Slot == INDEX_NONE)
	{
		// Should not happen (Pre Login refuses a full game): a seat past the limit rather than none.
		Slot = UALKLobbyStatics::FindFreeSlot(TakenSlots, MaxPlayers + 64);
	}
	Player->SetSlot(Slot);

	// ---- Team
	const int32 TeamCount = LobbyState->TeamCount;
	if (TeamCount <= 0)
	{
		Player->SetTeamId(INDEX_NONE);
		return;
	}
	const TArray<int32> OtherTeams = ALKNetGameModePrivate::GetOtherTeams(GetLobbyPlayers(), Player);
	const bool bKeepPreferred = PreferredTeam >= 0 && UALKLobbyStatics::CanJoinTeam(OtherTeams, TeamCount, MaxPlayers, PreferredTeam);
	Player->SetTeamId(bKeepPreferred ? PreferredTeam : UALKLobbyStatics::PickSmallestTeam(OtherTeams, TeamCount));
}

void AALKNetGameModeBase::ReassignTeams()
{
	const AALKLobbyGameState* LobbyState = GetLobbyGameState();
	if (!LobbyState)
	{
		return;
	}
	// One by one, in slot order, as if they had just joined: the teams come out even.
	const TArray<AALKLobbyPlayerState*> Players = GetLobbyPlayers();
	for (AALKLobbyPlayerState* Each : Players)
	{
		Each->SetTeamId(INDEX_NONE);
	}
	if (LobbyState->TeamCount <= 0)
	{
		return;
	}
	TArray<int32> Teams;
	for (AALKLobbyPlayerState* Each : Players)
	{
		const int32 Team = UALKLobbyStatics::PickSmallestTeam(Teams, LobbyState->TeamCount);
		Each->SetTeamId(Team);
		if (!Each->bSpectating)
		{
			Teams.Add(Team);
		}
	}
}

void AALKNetGameModeBase::UpdateHost()
{
	const TArray<AALKLobbyPlayerState*> Players = GetLobbyPlayers();
	AALKLobbyPlayerState* Host = nullptr;

	// A listen server: its own player. A dedicated server: the host stays the host; the first real player otherwise.
	for (AALKLobbyPlayerState* Each : Players)
	{
		const APlayerController* Controller = Each->GetPlayerController();
		if (Controller && Controller->IsLocalController())
		{
			Host = Each;
			break;
		}
	}
	if (!Host)
	{
		for (AALKLobbyPlayerState* Each : Players)
		{
			if (Each->bHost && !Each->bPlaceholder && Each->GetPlayerController())
			{
				Host = Each;
				break;
			}
		}
	}
	if (!Host)
	{
		for (AALKLobbyPlayerState* Each : Players)
		{
			if (!Each->bPlaceholder && Each->GetPlayerController())
			{
				Host = Each;
				break;
			}
		}
	}
	for (AALKLobbyPlayerState* Each : Players)
	{
		Each->SetHost(Each == Host);
	}
}

// ---------------------------------------------------------------- Requests from players

void AALKNetGameModeBase::SetPlayerReady(AALKLobbyPlayerState* Player, bool bReady)
{
	if (Player)
	{
		Player->SetReady(bReady);
	}
}

void AALKNetGameModeBase::HandleSetReady(APlayerController* Player, bool bReady)
{
	const AALKLobbyGameState* LobbyState = GetLobbyGameState();
	AALKLobbyPlayerState* State = Player ? Player->GetPlayerState<AALKLobbyPlayerState>() : nullptr;
	// Ready only means something while the lobby waits or counts down.
	if (State && LobbyState && (LobbyState->Phase == EALKLobbyPhase::Waiting || LobbyState->Phase == EALKLobbyPhase::Countdown))
	{
		SetPlayerReady(State, bReady);
	}
}

void AALKNetGameModeBase::HandleRequestTeam(APlayerController* Player, int32 TeamId)
{
	const AALKLobbyGameState* LobbyState = GetLobbyGameState();
	AALKLobbyPlayerState* State = Player ? Player->GetPlayerState<AALKLobbyPlayerState>() : nullptr;
	if (!State || !LobbyState || !LobbyState->IsInLobby() || LobbyState->Phase == EALKLobbyPhase::Starting || State->TeamId == TeamId)
	{
		return;
	}
	if (!UALKDeveloperSettings::GetOnlineConfig()->bAllowTeamChoice && !IsHost(Player))
	{
		SendNotice(Player, LOCTEXT("TeamChoiceOff", "Only the host can change teams."));
		return;
	}
	const TArray<int32> OtherTeams = ALKNetGameModePrivate::GetOtherTeams(GetLobbyPlayers(), State);
	if (!UALKLobbyStatics::CanJoinTeam(OtherTeams, LobbyState->TeamCount, LobbyState->MaxPlayers, TeamId))
	{
		SendNotice(Player, LOCTEXT("TeamFull", "That team is full."));
		return;
	}
	State->SetTeamId(TeamId);
	OnRosterChanged();
}

void AALKNetGameModeBase::HandleSetLoadout(APlayerController* Player, FName LoadoutId)
{
	if (AALKLobbyPlayerState* State = Player ? Player->GetPlayerState<AALKLobbyPlayerState>() : nullptr)
	{
		State->SetLoadoutId(LoadoutId);
	}
}

void AALKNetGameModeBase::HandleSetColor(APlayerController* Player, int32 ColorIndex)
{
	if (AALKLobbyPlayerState* State = Player ? Player->GetPlayerState<AALKLobbyPlayerState>() : nullptr)
	{
		State->SetColorIndex(FMath::Clamp(ColorIndex, 0, FMath::Max(0, UALKDeveloperSettings::GetOnlineConfig()->PlayerColorCount - 1)));
	}
}

void AALKNetGameModeBase::HandleSetSpectating(APlayerController* Player, bool bSpectating)
{
	const AALKLobbyGameState* LobbyState = GetLobbyGameState();
	AALKLobbyPlayerState* State = Player ? Player->GetPlayerState<AALKLobbyPlayerState>() : nullptr;
	if (!State || !LobbyState || State->bSpectating == bSpectating || LobbyState->Phase == EALKLobbyPhase::Starting)
	{
		return;
	}
	State->SetSpectating(bSpectating);
	State->SetReady(false);
	if (!bSpectating)
	{
		// Back among the players: the old team if it still has room, the smallest one otherwise.
		AssignSlotAndTeam(State, State->Slot, State->TeamId);
	}
	OnRosterChanged();
}

void AALKNetGameModeBase::HandleKickPlayer(APlayerController* Host, int32 PlayerId, bool bBan)
{
	if (!IsHost(Host))
	{
		return;
	}
	AALKLobbyPlayerState* Target = FindPlayerByPlayerId(PlayerId);
	if (!Target)
	{
		return;
	}
	if (Target->bPlaceholder)
	{
		Target->Destroy();
		OnRosterChanged();
		return;
	}
	KickPlayer(Target->GetPlayerController(), bBan);
}

void AALKNetGameModeBase::HandleUnbanPlayer(APlayerController* Host, const FString& UserId)
{
	UALKOnlineSubsystem* Online = GetOnline();
	if (IsHost(Host) && Online)
	{
		Online->RemoveBan(UserId);
	}
}

void AALKNetGameModeBase::HandleMovePlayerToTeam(APlayerController* Host, int32 PlayerId, int32 TeamId)
{
	const AALKLobbyGameState* LobbyState = GetLobbyGameState();
	AALKLobbyPlayerState* Target = FindPlayerByPlayerId(PlayerId);
	if (!IsHost(Host) || !Target || !LobbyState || Target->TeamId == TeamId)
	{
		return;
	}
	const TArray<int32> OtherTeams = ALKNetGameModePrivate::GetOtherTeams(GetLobbyPlayers(), Target);
	if (!UALKLobbyStatics::CanJoinTeam(OtherTeams, LobbyState->TeamCount, LobbyState->MaxPlayers, TeamId))
	{
		SendNotice(Host, LOCTEXT("TeamFull", "That team is full."));
		return;
	}
	Target->SetTeamId(TeamId);
	OnRosterChanged();
}

bool AALKNetGameModeBase::KickPlayer(APlayerController* Player, bool bBan)
{
	// Never the listen server's own player, and never the host.
	if (!Player || Player->IsLocalController() || IsHost(Player) || !GameSession)
	{
		return false;
	}
	const AALKLobbyPlayerState* State = Player->GetPlayerState<AALKLobbyPlayerState>();
	UALKOnlineSubsystem* Online = GetOnline();
	if (bBan && Online)
	{
		UNetConnection* Connection = Player->GetNetConnection();
		const FString Address = Connection ? Connection->LowLevelGetRemoteAddress(false) : FString();
		Online->AddBan(State ? State->GetUserId() : FString(), State ? State->GetPlayerName() : FString(), Address);
	}

	// The reason first, on the same channel that then closes: it arrives before the connection is gone.
	if (UALKPlayerNetComponent* NetComponent = UALKPlayerNetComponent::Get(Player))
	{
		NetComponent->ClientKicked(bBan ? ALKNetReason::Banned : ALKNetReason::Kicked);
	}
	const FText Reason = bBan ? LOCTEXT("BannedByHost", "The host has banned you from this game.") : LOCTEXT("KickedByHost", "The host removed you from the game.");
	return GameSession->KickPlayer(Player, Reason);
}

// ---------------------------------------------------------------- Chat and feed

void AALKNetGameModeBase::RouteChatMessage(const FALKChatMessage& Message)
{
	DeliverChatMessage(GetWorld(), Message);
}

void AALKNetGameModeBase::DeliverChatMessage(UWorld* World, const FALKChatMessage& Message)
{
	if (!World)
	{
		return;
	}
	// The sender's party, for the Party channel.
	FString SenderParty;
	if (Message.Channel == EALKChatChannel::Party)
	{
		if (const AALKLobbyGameState* LobbyState = World->GetGameState<AALKLobbyGameState>())
		{
			const AALKLobbyPlayerState* Sender = LobbyState->FindPlayerByUserId(Message.SenderId);
			SenderParty = Sender ? Sender->PartyId : FString();
		}
	}

	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* Controller = It->Get();
		UALKPlayerNetComponent* NetComponent = UALKPlayerNetComponent::Get(Controller);
		if (!NetComponent)
		{
			continue;
		}
		const AALKLobbyPlayerState* State = Controller->GetPlayerState<AALKLobbyPlayerState>();
		const bool bIsSender = State && !Message.SenderId.IsEmpty() && State->GetUserId() == Message.SenderId;

		bool bReaches = false;
		switch (Message.Channel)
		{
		case EALKChatChannel::Team:
			bReaches = bIsSender || (State && State->TeamId == Message.TeamId);
			break;
		case EALKChatChannel::Party:
			bReaches = bIsSender || (State && !SenderParty.IsEmpty() && State->PartyId == SenderParty);
			break;
		case EALKChatChannel::Whisper:
			bReaches = bIsSender || (State && State->GetUserId() == Message.WhisperTargetId);
			break;
		case EALKChatChannel::All:
		case EALKChatChannel::System:
		default:
			bReaches = true;
			break;
		}
		if (bReaches)
		{
			NetComponent->ClientReceiveChat(Message);
		}
	}
}

void AALKNetGameModeBase::BroadcastSystemMessage(const FString& Text)
{
	const FString Clean = UALKLobbyStatics::SanitizeChatText(Text, UALKDeveloperSettings::GetOnlineConfig()->ChatMaxLength);
	if (Clean.IsEmpty())
	{
		return;
	}
	FALKChatMessage Message;
	Message.Time = FDateTime::UtcNow();
	Message.Channel = EALKChatChannel::System;
	Message.Text = Clean;
	RouteChatMessage(Message);
}

void AALKNetGameModeBase::BroadcastFeedEvent(const FALKFeedEvent& Event)
{
	if (AALKLobbyGameState* LobbyState = GetLobbyGameState())
	{
		LobbyState->BroadcastFeedEvent(Event);
	}
}

// ---------------------------------------------------------------- Placeholder players

AALKLobbyPlayerState* AALKNetGameModeBase::SpawnPlaceholder(const FString& Name, int32 PreferredSlot, int32 PreferredTeam)
{
	UWorld* World = GetWorld();
	if (!World || !GetLobbyGameState())
	{
		return nullptr;
	}
	UClass* StateClass = PlayerStateClass && PlayerStateClass->IsChildOf(AALKLobbyPlayerState::StaticClass()) ? PlayerStateClass.Get() : AALKLobbyPlayerState::StaticClass();
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.ObjectFlags |= RF_Transient;
	AALKLobbyPlayerState* State = World->SpawnActor<AALKLobbyPlayerState>(StateClass, Params);
	if (!State)
	{
		return nullptr;
	}

	// Everything about a placeholder comes from its name, so it looks the same every time.
	const int32 Hash = ALKNetGameModePrivate::StableHash(Name);
	const UALKOnlineConfig* Config = UALKDeveloperSettings::GetOnlineConfig();
	if (GameSession)
	{
		State->SetPlayerId(GameSession->GetNextPlayerID());
	}
	State->SetPlayerName(Name);
	State->SetIsABot(true);
	State->SetPlaceholder(true);
	State->SetUserId(ALKNetGameModePrivate::PlaceholderIdPrefix + Name.ToLower());
	State->SetPlayerLevel(3 + Hash % 40);
	State->SetColorIndex(Hash / 7 % FMath::Max(1, Config->PlayerColorCount));
	State->SetDisplayedPing(12 + Hash / 13 % 90);
	AssignSlotAndTeam(State, PreferredSlot, PreferredTeam);
	return State;
}

int32 AALKNetGameModeBase::AddPlaceholderPlayers(int32 Count)
{
	using namespace ALKNetGameModePrivate;
	const AALKLobbyGameState* LobbyState = GetLobbyGameState();
	if (!HasAuthority() || !LobbyState || Count <= 0)
	{
		return 0;
	}

	int32 Added = 0;
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const TArray<AALKLobbyPlayerState*> Players = GetLobbyPlayers();
		if (Players.Num() >= LobbyState->MaxPlayers)
		{
			break;
		}
		// The next name of the list nobody has; a number is added once the list is used up.
		TArray<FString> TakenNames;
		for (const AALKLobbyPlayerState* Each : Players)
		{
			TakenNames.Add(Each->GetPlayerName());
		}
		FString Name;
		for (int32 NameIndex = 0; NameIndex < PlaceholderNameCount; ++NameIndex)
		{
			const FString Candidate = PlaceholderNames[NameIndex];
			if (!TakenNames.ContainsByPredicate([&Candidate](const FString& Each) { return Each.Equals(Candidate, ESearchCase::IgnoreCase); }))
			{
				Name = Candidate;
				break;
			}
		}
		if (Name.IsEmpty())
		{
			Name = UALKLobbyStatics::MakeUniquePlayerName(PlaceholderNames[GetPlaceholderCount() % PlaceholderNameCount], TakenNames);
		}
		if (!SpawnPlaceholder(Name, INDEX_NONE, INDEX_NONE))
		{
			break;
		}
		++Added;
	}

	if (Added > 0)
	{
		// They ready up one after the other; the last one holds out until Set Placeholders Ready.
		const TArray<AALKLobbyPlayerState*> Placeholders = GetPlaceholders();
		for (int32 Index = 0; Index < Placeholders.Num(); ++Index)
		{
			const bool bHoldsOut = !bAllPlaceholdersReady && Index == Placeholders.Num() - 1;
			if (!Placeholders[Index]->bReady && !bHoldsOut)
			{
				SchedulePlaceholderReady(Placeholders[Index]);
			}
		}
		OnRosterChanged();
	}
	return Added;
}

void AALKNetGameModeBase::RemovePlaceholderPlayers()
{
	if (!HasAuthority())
	{
		return;
	}
	const TArray<AALKLobbyPlayerState*> Placeholders = GetPlaceholders();
	for (AALKLobbyPlayerState* Each : Placeholders)
	{
		Each->Destroy();
	}
	if (Placeholders.Num() > 0)
	{
		OnRosterChanged();
	}
}

void AALKNetGameModeBase::SetPlaceholdersReady(bool bReady)
{
	if (!HasAuthority())
	{
		return;
	}
	bAllPlaceholdersReady = bReady;
	for (AALKLobbyPlayerState* Each : GetPlaceholders())
	{
		SetPlayerReady(Each, bReady);
	}
}

int32 AALKNetGameModeBase::GetPlaceholderCount() const
{
	return GetPlaceholders().Num();
}

void AALKNetGameModeBase::SchedulePlaceholderReady(AALKLobbyPlayerState* Placeholder)
{
	const AALKLobbyGameState* LobbyState = GetLobbyGameState();
	if (!Placeholder || !LobbyState || !LobbyState->IsInLobby())
	{
		return;
	}
	FTimerHandle Handle;
	GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateUObject(this, &ThisClass::ReadyPlaceholder, TWeakObjectPtr<AALKLobbyPlayerState>(Placeholder)),
		PlaceholderStream.FRandRange(1.f, 4.f), false);
}

void AALKNetGameModeBase::ReadyPlaceholder(TWeakObjectPtr<AALKLobbyPlayerState> Placeholder)
{
	const AALKLobbyGameState* LobbyState = GetLobbyGameState();
	AALKLobbyPlayerState* State = Placeholder.Get();
	if (State && LobbyState && (LobbyState->Phase == EALKLobbyPhase::Waiting || LobbyState->Phase == EALKLobbyPhase::Countdown))
	{
		SetPlayerReady(State, true);
	}
}

bool AALKNetGameModeBase::SendPlaceholderChat(const FString& Text)
{
	const TArray<AALKLobbyPlayerState*> Placeholders = GetPlaceholders();
	const FString Clean = UALKLobbyStatics::SanitizeChatText(Text, UALKDeveloperSettings::GetOnlineConfig()->ChatMaxLength);
	if (Placeholders.Num() == 0 || Clean.IsEmpty())
	{
		return false;
	}
	const AALKLobbyPlayerState* Speaker = Placeholders[PlaceholderStream.RandRange(0, Placeholders.Num() - 1)];
	FALKChatMessage Message;
	Message.Time = FDateTime::UtcNow();
	Message.Channel = EALKChatChannel::All;
	Message.SenderId = Speaker->GetUserId();
	Message.SenderName = Speaker->GetPlayerName();
	Message.TeamId = Speaker->TeamId;
	Message.SenderStableIndex = Speaker->GetStableIndex();
	Message.Text = Clean;
	RouteChatMessage(Message);
	return true;
}

void AALKNetGameModeBase::RestorePlaceholders()
{
	const UALKOnlineSubsystem* Online = GetOnline();
	if (!Online || GetPlaceholderCount() > 0)
	{
		return;
	}
	// The placeholders of the level before this one, back in their seats.
	int32 Restored = 0;
	for (const FALKRosterEntry& Entry : Online->GetRoster())
	{
		if (!Entry.bPlaceholder || Entry.DisplayName.IsEmpty())
		{
			continue;
		}
		if (AALKLobbyPlayerState* State = SpawnPlaceholder(Entry.DisplayName, Entry.Slot, Entry.TeamId))
		{
			State->SetLoadoutId(Entry.LoadoutId);
			State->SetColorIndex(Entry.ColorIndex);
			State->SetPlayerLevel(Entry.PlayerLevel);
			State->SetDisplayedPing(Entry.PingMs);
			++Restored;
		}
	}
	if (Restored > 0)
	{
		const TArray<AALKLobbyPlayerState*> Placeholders = GetPlaceholders();
		for (int32 Index = 0; Index < Placeholders.Num(); ++Index)
		{
			if (bAllPlaceholdersReady || Index < Placeholders.Num() - 1)
			{
				SchedulePlaceholderReady(Placeholders[Index]);
			}
		}
		OnRosterChanged();
	}
}

// ---------------------------------------------------------------- Travel

void AALKNetGameModeBase::GetSeamlessTravelActorList(bool bToTransition, TArray<AActor*>& ActorList)
{
	Super::GetSeamlessTravelActorList(bToTransition, ActorList);
	// Placeholders have no controller to carry them over: the next game mode makes them again from the roster, the same
	// way whether the travel is seamless or not.
	ActorList.RemoveAll([](const AActor* Each)
	{
		const AALKLobbyPlayerState* State = Cast<AALKLobbyPlayerState>(Each);
		return !IsValid(Each) || (State && State->bPlaceholder);
	});
}

void AALKNetGameModeBase::StoreRosterForTravel()
{
	UALKOnlineSubsystem* Online = GetOnline();
	if (!Online)
	{
		return;
	}
	TArray<FALKRosterEntry> Entries;
	for (const AALKLobbyPlayerState* Each : GetLobbyPlayers())
	{
		FALKRosterEntry& Entry = Entries.AddDefaulted_GetRef();
		Entry.UserId = Each->GetUserId();
		Entry.DisplayName = Each->GetPlayerName();
		Entry.Slot = Each->Slot;
		Entry.TeamId = Each->TeamId;
		Entry.LoadoutId = Each->LoadoutId;
		Entry.ColorIndex = Each->ColorIndex;
		Entry.PlayerLevel = Each->PlayerLevel;
		Entry.bSpectating = Each->bSpectating;
		Entry.bPlaceholder = Each->bPlaceholder;
		Entry.PingMs = FMath::RoundToInt(Each->GetPingInMilliseconds());
	}
	Online->StoreRoster(Entries);
}

bool AALKNetGameModeBase::TravelToLevel(const FString& LevelURL)
{
	UWorld* World = GetWorld();
	if (!World || LevelURL.IsEmpty())
	{
		return false;
	}
	StoreRosterForTravel();

	// Absolute, so nothing of the old URL comes along; a listen server has to say "listen" again to stay one.
	FString URL = LevelURL;
	if (GetNetMode() == NM_ListenServer)
	{
		URL += TEXT("?listen");
	}
	URL += ServerSettings.ToTravelOptions();

	// Seamless travel does not work everywhere: in the editor it is off unless the project turned it on, and going to the
	// level that is already loaded (the empty level fallback) needs a full reload. Saying so in the URL keeps the log clean.
	FString LevelPackage = LevelURL;
	int32 OptionsStart = INDEX_NONE;
	if (LevelPackage.FindChar(TEXT('?'), OptionsStart))
	{
		LevelPackage.LeftInline(OptionsStart);
	}
	const bool bSameLevel = UWorld::RemovePIEPrefix(World->GetOutermost()->GetName()) == LevelPackage;
	bool bEditorBlocksSeamless = false;
	if (World->WorldType == EWorldType::PIE)
	{
		const IConsoleVariable* AllowSeamless = IConsoleManager::Get().FindConsoleVariable(TEXT("net.AllowPIESeamlessTravel"));
		bEditorBlocksSeamless = !AllowSeamless || !AllowSeamless->GetBool();
	}
	if (bUseSeamlessTravel && (bSameLevel || bEditorBlocksSeamless))
	{
		URL += TEXT("?NoSeamlessTravel");
	}

	UE_LOG(LogALK, Log, TEXT("Server travel to %s"), *URL);
	if (!World->ServerTravel(URL, true))
	{
		return false;
	}

	// Placeholders do not travel: the next game mode seats them again from the roster stored above. They leave now, so the
	// game state, which does travel, holds no player state that goes away with this level (a seamless travel would trip
	// over the leftover entries when it collects the actors to keep).
	for (AALKLobbyPlayerState* Placeholder : GetPlaceholders())
	{
		Placeholder->Destroy();
	}

	// The engine waits a few seconds before a level change that is not seamless, so clients get the message. With
	// nobody else connected there is nobody to wait for.
	bool bHasRemotePlayers = false;
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* Controller = It->Get();
		bHasRemotePlayers |= Controller && !Controller->IsLocalController();
	}
	if (!bHasRemotePlayers && !World->IsInSeamlessTravel())
	{
		World->NextSwitchCountdown = 0.f;
	}
	return true;
}

bool AALKNetGameModeBase::AllowPausing(APlayerController* PC)
{
	// A menu that pauses the game (the pause menu) must not freeze everyone: only the host's pause request does.
	if (GetNetMode() != NM_Standalone && !bHostPauseRequest)
	{
		return false;
	}
	return Super::AllowPausing(PC);
}

#undef LOCTEXT_NAMESPACE
