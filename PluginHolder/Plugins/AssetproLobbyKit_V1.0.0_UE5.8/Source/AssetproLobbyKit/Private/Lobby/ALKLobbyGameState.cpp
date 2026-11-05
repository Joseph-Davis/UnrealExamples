// Copyright 2026 CodeLikeMe. All Rights Reserved.

#include "Lobby/ALKLobbyGameState.h"
#include "Lobby/ALKLobbyPlayerState.h"
#include "Lobby/ALKLobbyStatics.h"
#include "Lobby/ALKPlayerNetComponent.h"
#include "Online/ALKOnlineConfig.h"
#include "Online/ALKOnlineSubsystem.h"
#include "ALKDeveloperSettings.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ALKLobbyGameState)

namespace ALKLobbyGameStatePrivate
{
	static const FName JoinEvent(TEXT("Join"));
	static const FName LeaveEvent(TEXT("Leave"));

	static void SortBySlot(TArray<AALKLobbyPlayerState*>& Players)
	{
		Players.StableSort([](const AALKLobbyPlayerState& A, const AALKLobbyPlayerState& B)
		{
			// Players without a slot yet go last.
			const int32 SlotA = A.Slot < 0 ? MAX_int32 : A.Slot;
			const int32 SlotB = B.Slot < 0 ? MAX_int32 : B.Slot;
			return SlotA < SlotB;
		});
	}
}

void AALKLobbyGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AALKLobbyGameState, ServerName);
	DOREPLIFETIME(AALKLobbyGameState, ModeId);
	DOREPLIFETIME(AALKLobbyGameState, MapId);
	DOREPLIFETIME(AALKLobbyGameState, Rules);
	DOREPLIFETIME(AALKLobbyGameState, MaxPlayers);
	DOREPLIFETIME(AALKLobbyGameState, TeamCount);
	DOREPLIFETIME(AALKLobbyGameState, BotCount);
	DOREPLIFETIME(AALKLobbyGameState, bPasswordProtected);
	DOREPLIFETIME(AALKLobbyGameState, LobbyCode);
	DOREPLIFETIME(AALKLobbyGameState, Phase);
	DOREPLIFETIME(AALKLobbyGameState, CountdownEndServerTime);
	DOREPLIFETIME(AALKLobbyGameState, bForcedStart);
	DOREPLIFETIME(AALKLobbyGameState, MapVotes);
	DOREPLIFETIME(AALKLobbyGameState, TeamScores);
	DOREPLIFETIME(AALKLobbyGameState, MatchEndServerTime);
	DOREPLIFETIME(AALKLobbyGameState, ScoreLimit);
	DOREPLIFETIME(AALKLobbyGameState, WinnerTeam);
	DOREPLIFETIME(AALKLobbyGameState, MVPPlayer);
	DOREPLIFETIME(AALKLobbyGameState, PostMatchEndServerTime);
	DOREPLIFETIME(AALKLobbyGameState, bServerPaused);
	DOREPLIFETIME(AALKLobbyGameState, ServerTickRate);
}

void AALKLobbyGameState::BeginPlay()
{
	Super::BeginPlay();
	NotifyLobbyStateChanged();
}

void AALKLobbyGameState::OnRep_LobbyState()
{
	NotifyLobbyStateChanged();
}

void AALKLobbyGameState::NotifyLobbyStateChanged()
{
	// The online subsystem follows the phase: In Lobby or In Match.
	if (!bPhaseReported || ReportedPhase != Phase)
	{
		if (UALKOnlineSubsystem* Online = UALKOnlineSubsystem::Get(this))
		{
			bPhaseReported = true;
			ReportedPhase = Phase;
			Online->NotifyLobbyPhase(Phase);
		}
	}
	OnLobbyStateChanged.Broadcast();
	OnLobbyStateChangedNative.Broadcast();
}

void AALKLobbyGameState::MarkLobbyStateChanged()
{
	if (HasAuthority())
	{
		ForceNetUpdate();
		NotifyLobbyStateChanged();
	}
}

void AALKLobbyGameState::SetPhase(EALKLobbyPhase NewPhase)
{
	if (HasAuthority() && Phase != NewPhase)
	{
		Phase = NewPhase;
		MarkLobbyStateChanged();
	}
}

void AALKLobbyGameState::NotifyPlayersChanged()
{
	OnPlayersChanged.Broadcast();
	OnPlayersChangedNative.Broadcast();
}

void AALKLobbyGameState::AddPlayerState(APlayerState* PlayerState)
{
	const int32 Before = PlayerArray.Num();
	Super::AddPlayerState(PlayerState);
	if (PlayerArray.Num() != Before)
	{
		NotifyPlayersChanged();
	}
}

void AALKLobbyGameState::RemovePlayerState(APlayerState* PlayerState)
{
	const int32 Before = PlayerArray.Num();
	Super::RemovePlayerState(PlayerState);
	if (PlayerArray.Num() != Before)
	{
		NotifyPlayersChanged();
	}
}

// ---------------------------------------------------------------- Reading

TArray<AALKLobbyPlayerState*> AALKLobbyGameState::GetLobbyPlayers() const
{
	TArray<AALKLobbyPlayerState*> Players;
	for (APlayerState* Each : PlayerArray)
	{
		if (AALKLobbyPlayerState* Player = Cast<AALKLobbyPlayerState>(Each))
		{
			Players.Add(Player);
		}
	}
	ALKLobbyGameStatePrivate::SortBySlot(Players);
	return Players;
}

TArray<AALKLobbyPlayerState*> AALKLobbyGameState::GetPlayersOnTeam(int32 TeamId) const
{
	TArray<AALKLobbyPlayerState*> Players;
	for (APlayerState* Each : PlayerArray)
	{
		AALKLobbyPlayerState* Player = Cast<AALKLobbyPlayerState>(Each);
		if (Player && !Player->bSpectating && Player->TeamId == TeamId)
		{
			Players.Add(Player);
		}
	}
	ALKLobbyGameStatePrivate::SortBySlot(Players);
	return Players;
}

TArray<AALKLobbyPlayerState*> AALKLobbyGameState::GetSpectators() const
{
	TArray<AALKLobbyPlayerState*> Players;
	for (APlayerState* Each : PlayerArray)
	{
		AALKLobbyPlayerState* Player = Cast<AALKLobbyPlayerState>(Each);
		if (Player && Player->bSpectating)
		{
			Players.Add(Player);
		}
	}
	ALKLobbyGameStatePrivate::SortBySlot(Players);
	return Players;
}

AALKLobbyPlayerState* AALKLobbyGameState::GetPlayerInSlot(int32 Slot) const
{
	for (APlayerState* Each : PlayerArray)
	{
		AALKLobbyPlayerState* Player = Cast<AALKLobbyPlayerState>(Each);
		if (Player && Player->Slot == Slot)
		{
			return Player;
		}
	}
	return nullptr;
}

AALKLobbyPlayerState* AALKLobbyGameState::FindPlayerByUserId(const FString& UserId) const
{
	if (UserId.IsEmpty())
	{
		return nullptr;
	}
	for (APlayerState* Each : PlayerArray)
	{
		AALKLobbyPlayerState* Player = Cast<AALKLobbyPlayerState>(Each);
		if (Player && Player->UserId == UserId)
		{
			return Player;
		}
	}
	return nullptr;
}

AALKLobbyPlayerState* AALKLobbyGameState::GetHostPlayer() const
{
	for (APlayerState* Each : PlayerArray)
	{
		AALKLobbyPlayerState* Player = Cast<AALKLobbyPlayerState>(Each);
		if (Player && Player->bHost)
		{
			return Player;
		}
	}
	return nullptr;
}

int32 AALKLobbyGameState::GetActivePlayerCount() const
{
	int32 Count = 0;
	for (const APlayerState* Each : PlayerArray)
	{
		const AALKLobbyPlayerState* Player = Cast<AALKLobbyPlayerState>(Each);
		Count += Player && !Player->bSpectating ? 1 : 0;
	}
	return Count;
}

int32 AALKLobbyGameState::GetReadyCount() const
{
	int32 Count = 0;
	for (const APlayerState* Each : PlayerArray)
	{
		const AALKLobbyPlayerState* Player = Cast<AALKLobbyPlayerState>(Each);
		Count += Player && !Player->bSpectating && Player->bReady ? 1 : 0;
	}
	return Count;
}

bool AALKLobbyGameState::AreAllPlayersReady() const
{
	const int32 Active = GetActivePlayerCount();
	return Active > 0 && GetReadyCount() >= Active;
}

EALKStartBlocker AALKLobbyGameState::GetStartBlocker(bool bForce) const
{
	const UALKOnlineConfig* Config = UALKDeveloperSettings::GetOnlineConfig();
	const bool bHasMap = !Config->GetMatchTravelURL(ModeId, MapId).IsEmpty();
	return UALKLobbyStatics::GetStartBlocker(GetActivePlayerCount(), GetReadyCount(), Config->MinPlayersToStart, bHasMap, bForce);
}

float AALKLobbyGameState::GetCountdownRemaining() const
{
	if (Phase != EALKLobbyPhase::Countdown || CountdownEndServerTime <= 0.0)
	{
		return 0.f;
	}
	return FMath::Max(0.f, static_cast<float>(CountdownEndServerTime - GetServerWorldTimeSeconds()));
}

int32 AALKLobbyGameState::GetMapVoteCount(FName InMapId) const
{
	const FALKMapVote* Found = MapVotes.FindByPredicate([InMapId](const FALKMapVote& Each) { return Each.MapId == InMapId; });
	return Found ? Found->Votes : 0;
}

float AALKLobbyGameState::GetMatchTimeRemaining() const
{
	if (MatchEndServerTime <= 0.0)
	{
		return -1.f;
	}
	return FMath::Max(0.f, static_cast<float>(MatchEndServerTime - GetServerWorldTimeSeconds()));
}

float AALKLobbyGameState::GetPostMatchTimeRemaining() const
{
	if (Phase != EALKLobbyPhase::PostMatch || PostMatchEndServerTime <= 0.0)
	{
		return -1.f;
	}
	return FMath::Max(0.f, static_cast<float>(PostMatchEndServerTime - GetServerWorldTimeSeconds()));
}

int32 AALKLobbyGameState::GetTeamScore(int32 TeamId) const
{
	return TeamScores.IsValidIndex(TeamId) ? TeamScores[TeamId] : 0;
}

// ---------------------------------------------------------------- Feed

void AALKLobbyGameState::BroadcastFeedEvent(const FALKFeedEvent& Event)
{
	if (HasAuthority())
	{
		MulticastFeedEvent(Event);
	}
}

void AALKLobbyGameState::MulticastFeedEvent_Implementation(const FALKFeedEvent& Event)
{
	OnFeedEvent.Broadcast(Event);
	OnFeedEventNative.Broadcast(Event);

	// Joins and leaves also read as system lines in the chat, in this machine's language.
	const bool bJoinOrLeave = Event.EventId == ALKLobbyGameStatePrivate::JoinEvent || Event.EventId == ALKLobbyGameStatePrivate::LeaveEvent;
	if (bJoinOrLeave && !Event.Text.IsEmpty() && GetNetMode() != NM_DedicatedServer && UALKDeveloperSettings::GetOnlineConfig()->bAnnounceJoinLeaveInChat)
	{
		if (UALKOnlineSubsystem* Online = UALKOnlineSubsystem::Get(this))
		{
			// In streamer mode the line would give the name away: it is left out.
			if (!Online->IsStreamerMode())
			{
				Online->AddSystemMessage(Event.Text);
			}
		}
	}
}
