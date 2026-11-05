// Copyright 2026 CodeLikeMe. All Rights Reserved.

#include "Lobby/ALKLobbyGameMode.h"
#include "Lobby/ALKLobbyGameState.h"
#include "Lobby/ALKLobbyPlayerState.h"
#include "Lobby/ALKLobbyStatics.h"
#include "Online/ALKOnlineConfig.h"
#include "Online/ALKOnlineSubsystem.h"
#include "AssetproLobbyKit.h"
#include "ALKDeveloperSettings.h"
#include "Engine/World.h"
#include "GameFramework/GameSession.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ALKLobbyGameMode)

#define LOCTEXT_NAMESPACE "ALKLobby"

AALKLobbyGameMode::AALKLobbyGameMode()
{
	// The lobby never "starts a match" in the engine's sense: players wait here, without a pawn.
	bDelayedStart = true;
	DefaultPawnClass = nullptr;
}

void AALKLobbyGameMode::InitGameState()
{
	Super::InitGameState();
	if (AALKLobbyGameState* LobbyState = GetLobbyGameState())
	{
		LobbyState->Phase = EALKLobbyPhase::Waiting;
		LobbyState->CountdownEndServerTime = 0.0;
		LobbyState->bForcedStart = false;
		LobbyState->MapVotes.Reset();
		LobbyState->MarkLobbyStateChanged();
	}
	// Back from a match: the session is open for players again.
	if (UALKOnlineSubsystem* Online = GetOnline())
	{
		Online->NotifyServerMatchEnded();
	}
}

void AALKLobbyGameMode::OnPlayerJoined(APlayerController* Player)
{
	// A fresh start in the lobby, also for players coming back from a match.
	if (AALKLobbyPlayerState* State = Player ? Player->GetPlayerState<AALKLobbyPlayerState>() : nullptr)
	{
		State->SetReady(false);
		State->SetVotedMapId(NAME_None);
		State->ResetMatchStats();
	}
	Super::OnPlayerJoined(Player);
}

void AALKLobbyGameMode::SetPlayerReady(AALKLobbyPlayerState* Player, bool bReady)
{
	Super::SetPlayerReady(Player, bReady);
	EvaluateStart();
}

void AALKLobbyGameMode::OnRosterChanged()
{
	RefreshMapVotes();
	EvaluateStart();
}

// ---------------------------------------------------------------- Start conditions and countdown

void AALKLobbyGameMode::EvaluateStart()
{
	const AALKLobbyGameState* LobbyState = GetLobbyGameState();
	if (!LobbyState)
	{
		return;
	}
	if (LobbyState->Phase == EALKLobbyPhase::Waiting)
	{
		if (LobbyState->GetStartBlocker(false) == EALKStartBlocker::None)
		{
			BeginCountdown(false);
		}
	}
	else if (LobbyState->Phase == EALKLobbyPhase::Countdown)
	{
		// A forced countdown only stops for what force cannot skip (too few players, no map).
		if (LobbyState->GetStartBlocker(LobbyState->bForcedStart) != EALKStartBlocker::None)
		{
			CancelCountdown();
		}
	}
}

void AALKLobbyGameMode::BeginCountdown(bool bForced)
{
	AALKLobbyGameState* LobbyState = GetLobbyGameState();
	if (!LobbyState)
	{
		return;
	}
	const UALKOnlineConfig* Config = UALKDeveloperSettings::GetOnlineConfig();
	const float Seconds = bForced ? Config->ForceStartCountdownSeconds : Config->LobbyCountdownSeconds;

	LobbyState->bForcedStart = bForced;
	LobbyState->CountdownEndServerTime = LobbyState->GetServerWorldTimeSeconds() + FMath::Max(0.f, Seconds);
	LobbyState->Phase = EALKLobbyPhase::Countdown;
	LobbyState->MarkLobbyStateChanged();

	GetWorldTimerManager().ClearTimer(CountdownTimer);
	if (Seconds > 0.f)
	{
		GetWorldTimerManager().SetTimer(CountdownTimer, this, &ThisClass::FinishCountdown, Seconds, false);
	}
	else
	{
		FinishCountdown();
	}
}

void AALKLobbyGameMode::CancelCountdown()
{
	AALKLobbyGameState* LobbyState = GetLobbyGameState();
	GetWorldTimerManager().ClearTimer(CountdownTimer);
	if (LobbyState && LobbyState->Phase == EALKLobbyPhase::Countdown)
	{
		LobbyState->bForcedStart = false;
		LobbyState->CountdownEndServerTime = 0.0;
		LobbyState->Phase = EALKLobbyPhase::Waiting;
		LobbyState->MarkLobbyStateChanged();
	}
}

void AALKLobbyGameMode::FinishCountdown()
{
	AALKLobbyGameState* LobbyState = GetLobbyGameState();
	GetWorldTimerManager().ClearTimer(CountdownTimer);
	if (!LobbyState || LobbyState->Phase != EALKLobbyPhase::Countdown)
	{
		return;
	}
	// Still good to go? (Someone may have left in the last moment.)
	if (LobbyState->GetStartBlocker(LobbyState->bForcedStart) != EALKStartBlocker::None || !TravelToMatch())
	{
		CancelCountdown();
	}
}

EALKStartBlocker AALKLobbyGameMode::StartCountdown(bool bForce)
{
	const AALKLobbyGameState* LobbyState = GetLobbyGameState();
	if (!LobbyState || LobbyState->Phase == EALKLobbyPhase::Starting)
	{
		return EALKStartBlocker::None;
	}
	const bool bMayForce = bForce && UALKDeveloperSettings::GetOnlineConfig()->bAllowForceStart;
	const EALKStartBlocker Blocker = LobbyState->GetStartBlocker(bMayForce);
	if (Blocker != EALKStartBlocker::None)
	{
		return Blocker;
	}
	// "Forced" only when force was needed: a countdown everyone is ready for still stops when someone un-readies.
	const bool bForced = bMayForce && LobbyState->GetStartBlocker(false) != EALKStartBlocker::None;
	if (LobbyState->Phase != EALKLobbyPhase::Countdown || (bForced && !LobbyState->bForcedStart))
	{
		BeginCountdown(bForced);
	}
	return EALKStartBlocker::None;
}

EALKStartBlocker AALKLobbyGameMode::StartMatchNow(bool bForce)
{
	AALKLobbyGameState* LobbyState = GetLobbyGameState();
	if (!LobbyState || LobbyState->Phase == EALKLobbyPhase::Starting)
	{
		return EALKStartBlocker::None;
	}
	const bool bMayForce = bForce && UALKDeveloperSettings::GetOnlineConfig()->bAllowForceStart;
	const EALKStartBlocker Blocker = LobbyState->GetStartBlocker(bMayForce);
	if (Blocker != EALKStartBlocker::None)
	{
		return Blocker;
	}
	GetWorldTimerManager().ClearTimer(CountdownTimer);
	if (!TravelToMatch())
	{
		CancelCountdown();
		return EALKStartBlocker::NoMap;
	}
	return EALKStartBlocker::None;
}

bool AALKLobbyGameMode::TravelToMatch()
{
	AALKLobbyGameState* LobbyState = GetLobbyGameState();
	const UALKOnlineConfig* Config = UALKDeveloperSettings::GetOnlineConfig();
	if (!LobbyState)
	{
		return false;
	}

	// The map: the vote's winner, the host's choice when nobody voted.
	if (Config->bAllowMapVote)
	{
		RefreshMapVotes();
		const int32 Seed = static_cast<int32>(FDateTime::UtcNow().GetTicks() & 0x7FFFFFFF);
		ServerSettings.MapId = UALKLobbyStatics::ResolveMapVote(LobbyState->MapVotes, ServerSettings.MapId, Seed);
	}
	const FString LevelURL = Config->GetMatchTravelURL(ServerSettings.ModeId, ServerSettings.MapId);
	if (LevelURL.IsEmpty())
	{
		UE_LOG(LogALK, Warning, TEXT("The match cannot start: map '%s' has no Level in the Assetpro Online Config."), *ServerSettings.MapId.ToString());
		return false;
	}

	UALKOnlineSubsystem* Online = GetOnline();
	if (Online)
	{
		Online->SetServerSettings(ServerSettings, true);
	}
	LobbyState->MapId = ServerSettings.MapId;
	LobbyState->Phase = EALKLobbyPhase::Starting;
	LobbyState->MarkLobbyStateChanged();
	K2_OnMatchStarting(ServerSettings.ModeId, ServerSettings.MapId);

	// From here the session counts as in progress (no more joins unless the service allows join in progress).
	if (Online)
	{
		Online->NotifyServerMatchStarted();
	}
	if (!TravelToLevel(LevelURL))
	{
		if (Online)
		{
			Online->NotifyServerMatchEnded();
		}
		LobbyState->Phase = EALKLobbyPhase::Countdown;
		return false;
	}
	return true;
}

// ---------------------------------------------------------------- Requests from players

void AALKLobbyGameMode::HandleStartMatch(APlayerController* Host, bool bForce)
{
	if (!IsHost(Host))
	{
		return;
	}
	// The host pressing Start is the host being ready.
	if (AALKLobbyPlayerState* State = Host->GetPlayerState<AALKLobbyPlayerState>())
	{
		State->SetReady(true);
	}
	const EALKStartBlocker Blocker = StartCountdown(bForce);
	if (Blocker != EALKStartBlocker::None)
	{
		SendNotice(Host, UALKLobbyStatics::GetStartBlockerText(Blocker));
	}
}

void AALKLobbyGameMode::HandleCancelCountdown(APlayerController* Host)
{
	const AALKLobbyGameState* LobbyState = GetLobbyGameState();
	if (!IsHost(Host) || !LobbyState || LobbyState->Phase != EALKLobbyPhase::Countdown)
	{
		return;
	}
	// The host is not ready any more, or the countdown would start again at once.
	if (AALKLobbyPlayerState* State = Host->GetPlayerState<AALKLobbyPlayerState>())
	{
		State->SetReady(false);
	}
	CancelCountdown();
}

void AALKLobbyGameMode::HandleVoteMap(APlayerController* Player, FName MapId)
{
	const AALKLobbyGameState* LobbyState = GetLobbyGameState();
	const UALKOnlineConfig* Config = UALKDeveloperSettings::GetOnlineConfig();
	AALKLobbyPlayerState* State = Player ? Player->GetPlayerState<AALKLobbyPlayerState>() : nullptr;
	if (!State || !LobbyState || !Config->bAllowMapVote || LobbyState->Phase == EALKLobbyPhase::Starting)
	{
		return;
	}
	// Only maps the current mode can be played on (None takes the vote back).
	if (!MapId.IsNone())
	{
		const FALKMapDefinition* Map = Config->FindMap(MapId);
		if (!Map || !Map->SupportsMode(LobbyState->ModeId))
		{
			return;
		}
	}
	State->SetVotedMapId(MapId);
	RefreshMapVotes();
}

void AALKLobbyGameMode::RefreshMapVotes()
{
	AALKLobbyGameState* LobbyState = GetLobbyGameState();
	if (!LobbyState)
	{
		return;
	}
	TArray<FName> Votes;
	for (const AALKLobbyPlayerState* Each : GetLobbyPlayers())
	{
		Votes.Add(Each->VotedMapId);
	}
	const TArray<FALKMapVote> Tallies = UALKLobbyStatics::TallyMapVotes(Votes);

	bool bChanged = Tallies.Num() != LobbyState->MapVotes.Num();
	for (int32 Index = 0; !bChanged && Index < Tallies.Num(); ++Index)
	{
		bChanged = Tallies[Index].MapId != LobbyState->MapVotes[Index].MapId || Tallies[Index].Votes != LobbyState->MapVotes[Index].Votes;
	}
	if (bChanged)
	{
		LobbyState->MapVotes = Tallies;
		LobbyState->MarkLobbyStateChanged();
	}
}

void AALKLobbyGameMode::HandleChangeMap(APlayerController* Host, FName MapId)
{
	FALKHostSettings Settings = ServerSettings;
	Settings.MapId = MapId;
	HandleChangeMatchSettings(Host, Settings);
}

void AALKLobbyGameMode::HandleChangeMatchSettings(APlayerController* Host, const FALKHostSettings& Settings)
{
	if (IsHost(Host))
	{
		ChangeMatchSettings(Settings);
	}
}

void AALKLobbyGameMode::ChangeMatchSettings(const FALKHostSettings& Settings)
{
	AALKLobbyGameState* LobbyState = GetLobbyGameState();
	if (!LobbyState || LobbyState->Phase == EALKLobbyPhase::Starting)
	{
		return;
	}
	const UALKOnlineConfig* Config = UALKDeveloperSettings::GetOnlineConfig();
	FALKHostSettings NewSettings = Config->SanitizeHostSettings(Settings, ServerSettings.ServerName);

	// What a running session cannot change.
	NewSettings.bLAN = ServerSettings.bLAN;
	// Nobody loses their seat: the limit never goes below the players who are here.
	if (NewSettings.MaxPlayers < GetNumPlayers())
	{
		NewSettings.MaxPlayers = ServerSettings.MaxPlayers;
	}

	const bool bModeChanged = NewSettings.ModeId != ServerSettings.ModeId;
	const bool bMapChanged = NewSettings.MapId != ServerSettings.MapId;
	const bool bBotsChanged = NewSettings.BotCount != ServerSettings.BotCount;
	const int32 OldTeamCount = LobbyState->TeamCount;

	ServerSettings = NewSettings;
	if (UALKOnlineSubsystem* Online = GetOnline())
	{
		Online->SetServerSettings(ServerSettings, true);
	}
	if (GameSession)
	{
		GameSession->MaxPlayers = ServerSettings.MaxPlayers;
	}
	ApplySettingsToGameState();

	if (bModeChanged || bMapChanged)
	{
		// A different game: everyone looks again and readies up again; votes for maps of the old mode are gone.
		GetWorldTimerManager().ClearTimer(CountdownTimer);
		CancelCountdown();
		for (AALKLobbyPlayerState* Each : GetLobbyPlayers())
		{
			Each->SetReady(false);
			if (bModeChanged)
			{
				Each->SetVotedMapId(NAME_None);
			}
			if (Each->bPlaceholder)
			{
				SchedulePlaceholderReady(Each);
			}
		}
	}
	if (LobbyState->TeamCount != OldTeamCount)
	{
		ReassignTeams();
	}

	// Placeholders past a lowered player limit leave; real players keep their seats.
	for (AALKLobbyPlayerState* Each : GetLobbyPlayers())
	{
		if (Each->Slot >= ServerSettings.MaxPlayers)
		{
			if (Each->bPlaceholder)
			{
				Each->Destroy();
			}
			else
			{
				AssignSlotAndTeam(Each, INDEX_NONE, Each->TeamId);
			}
		}
	}
	if (bBotsChanged)
	{
		SyncPlaceholderBots();
		OnBotCountChanged(ServerSettings.BotCount);
	}
	OnRosterChanged();
}

#undef LOCTEXT_NAMESPACE
