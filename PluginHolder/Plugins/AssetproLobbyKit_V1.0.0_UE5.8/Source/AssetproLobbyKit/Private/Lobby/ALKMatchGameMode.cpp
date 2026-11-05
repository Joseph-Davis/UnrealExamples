// Copyright 2026 CodeLikeMe. All Rights Reserved.

#include "Lobby/ALKMatchGameMode.h"
#include "Lobby/ALKLobbyGameState.h"
#include "Lobby/ALKLobbyPlayerState.h"
#include "Lobby/ALKLobbyStatics.h"
#include "Online/ALKOnlineConfig.h"
#include "Online/ALKOnlineSubsystem.h"
#include "AssetproLobbyKit.h"
#include "ALKDeveloperSettings.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ALKMatchGameMode)

#define LOCTEXT_NAMESPACE "ALKLobby"

namespace ALKMatchGameModePrivate
{
	static const FName TimeLimitRule(TEXT("TimeLimit"));
	static const FName ScoreLimitRule(TEXT("ScoreLimit"));
	static const FName KillEvent(TEXT("Kill"));
}

AALKMatchGameMode::AALKMatchGameMode()
{
	SimulationStream.Initialize(1337);
}

void AALKMatchGameMode::InitGameState()
{
	Super::InitGameState();
	if (AALKLobbyGameState* LobbyState = GetLobbyGameState())
	{
		LobbyState->Phase = EALKLobbyPhase::InMatch;
		LobbyState->TeamScores.Init(0, FMath::Max(0, LobbyState->TeamCount));
		LobbyState->ScoreLimit = FMath::Max(0, ServerSettings.Rules.GetInt(ALKMatchGameModePrivate::ScoreLimitRule));
		LobbyState->MatchEndServerTime = 0.0;
		LobbyState->PostMatchEndServerTime = 0.0;
		LobbyState->WinnerTeam = INDEX_NONE;
		LobbyState->MVPPlayer = nullptr;
		LobbyState->bServerPaused = false;
		LobbyState->MapVotes.Reset();
		LobbyState->MarkLobbyStateChanged();
	}
}

void AALKMatchGameMode::OnPlayerJoined(APlayerController* Player)
{
	// The base gives back the lobby's slot and team (or seats a late joiner).
	Super::OnPlayerJoined(Player);
	if (AALKLobbyPlayerState* State = Player ? Player->GetPlayerState<AALKLobbyPlayerState>() : nullptr)
	{
		State->SetReady(false);
		State->SetRematchVote(false);
	}
}

void AALKMatchGameMode::HandleMatchHasStarted()
{
	Super::HandleMatchHasStarted();

	AALKLobbyGameState* LobbyState = GetLobbyGameState();
	if (!LobbyState)
	{
		return;
	}
	for (AALKLobbyPlayerState* Each : GetLobbyPlayers())
	{
		Each->ResetMatchStats();
	}

	// "TimeLimit" is in minutes; 0 is no limit.
	const float Minutes = ServerSettings.Rules.GetFloat(ALKMatchGameModePrivate::TimeLimitRule);
	LobbyState->MatchEndServerTime = Minutes > 0.f ? LobbyState->GetServerWorldTimeSeconds() + Minutes * 60.0 : 0.0;
	LobbyState->Phase = EALKLobbyPhase::InMatch;
	LobbyState->MarkLobbyStateChanged();

	RequestedWinnerTeam = INDEX_NONE;
	RequestedWinner.Reset();
	GetWorldTimerManager().SetTimer(LimitsTimer, this, &ThisClass::CheckMatchLimits, 0.5f, true);
	OnBotCountChanged(ServerSettings.BotCount);
}

void AALKMatchGameMode::CheckMatchLimits()
{
	const AALKLobbyGameState* LobbyState = GetLobbyGameState();
	if (!LobbyState || !IsMatchInProgress())
	{
		return;
	}
	if (LobbyState->MatchEndServerTime > 0.0 && LobbyState->GetServerWorldTimeSeconds() >= LobbyState->MatchEndServerTime)
	{
		FinishMatch(INDEX_NONE);
	}
}

// ---------------------------------------------------------------- Scoring

void AALKMatchGameMode::AddTeamScore(int32 TeamId, int32 Amount)
{
	AALKLobbyGameState* LobbyState = GetLobbyGameState();
	if (!LobbyState || !IsMatchInProgress() || !LobbyState->TeamScores.IsValidIndex(TeamId) || Amount == 0)
	{
		return;
	}
	LobbyState->TeamScores[TeamId] = FMath::Max(0, LobbyState->TeamScores[TeamId] + Amount);
	LobbyState->MarkLobbyStateChanged();
	if (UALKLobbyStatics::IsLimitReached(static_cast<float>(LobbyState->TeamScores[TeamId]), static_cast<float>(LobbyState->ScoreLimit)))
	{
		FinishMatch(TeamId);
	}
}

void AALKMatchGameMode::ReportKill(AController* Killer, AController* Victim, AController* Assist)
{
	ReportKillByPlayerState(
		Killer ? Killer->GetPlayerState<AALKLobbyPlayerState>() : nullptr,
		Victim ? Victim->GetPlayerState<AALKLobbyPlayerState>() : nullptr,
		Assist ? Assist->GetPlayerState<AALKLobbyPlayerState>() : nullptr);
}

void AALKMatchGameMode::ReportKillByPlayerState(AALKLobbyPlayerState* Killer, AALKLobbyPlayerState* Victim, AALKLobbyPlayerState* Assist)
{
	AALKLobbyGameState* LobbyState = GetLobbyGameState();
	if (!LobbyState || !Victim || !IsMatchInProgress())
	{
		return;
	}
	const bool bTeams = LobbyState->TeamCount > 0;
	const bool bSelf = !Killer || Killer == Victim;
	const bool bTeamKill = !bSelf && bTeams && Killer->TeamId == Victim->TeamId;

	Victim->AddDeath();
	if (!bSelf && !bTeamKill)
	{
		Killer->AddKill();
		Killer->AddScore(KillScore);
		if (Assist && Assist != Killer && Assist != Victim)
		{
			Assist->AddAssist();
			Assist->AddScore(AssistScore);
		}
	}

	FALKFeedEvent Event;
	Event.EventId = ALKMatchGameModePrivate::KillEvent;
	Event.TargetName = Victim->GetPlayerName();
	Event.TargetTeam = Victim->TeamId;
	Event.TargetStableIndex = Victim->GetStableIndex();
	if (bSelf)
	{
		Event.Text = FText::Format(LOCTEXT("FeedSelf", "{0} was eliminated"), FText::FromString(Event.TargetName));
	}
	else
	{
		Event.ActorName = Killer->GetPlayerName();
		Event.ActorTeam = Killer->TeamId;
		Event.ActorStableIndex = Killer->GetStableIndex();
		Event.Text = FText::Format(LOCTEXT("FeedKill", "{0} eliminated {1}"), FText::FromString(Event.ActorName), FText::FromString(Event.TargetName));
	}
	LobbyState->BroadcastFeedEvent(Event);

	if (bSelf || bTeamKill)
	{
		return;
	}
	if (bTeams)
	{
		// May end the match (score limit).
		AddTeamScore(Killer->TeamId, TeamScorePerKill);
	}
	else if (UALKLobbyStatics::IsLimitReached(static_cast<float>(Killer->Kills), static_cast<float>(LobbyState->ScoreLimit)))
	{
		// Free for all: the score limit counts a player's kills.
		RequestedWinner = Killer;
		FinishMatch(INDEX_NONE);
	}
}

// ---------------------------------------------------------------- The end of the match

void AALKMatchGameMode::FinishMatch(int32 WinnerTeam)
{
	if (!IsMatchInProgress())
	{
		return;
	}
	RequestedWinnerTeam = WinnerTeam;
	// The engine's match flow: In Progress -> Waiting Post Match, which calls Handle Match Has Ended below.
	EndMatch();
}

void AALKMatchGameMode::HandleMatchHasEnded()
{
	Super::HandleMatchHasEnded();

	GetWorldTimerManager().ClearTimer(LimitsTimer);
	GetWorldTimerManager().ClearTimer(SimulationTimer);
	bMatchSimulation = false;

	AALKLobbyGameState* LobbyState = GetLobbyGameState();
	if (!LobbyState)
	{
		return;
	}
	const UALKOnlineConfig* Config = UALKDeveloperSettings::GetOnlineConfig();
	const TArray<AALKLobbyPlayerState*> Players = GetLobbyPlayers();

	int32 WinnerTeam = RequestedWinnerTeam;
	if (WinnerTeam < 0 && LobbyState->TeamCount > 0)
	{
		WinnerTeam = UALKLobbyStatics::PickWinningTeam(LobbyState->TeamScores);
	}

	// The MVP: the player who won a free for all by the score limit, else the best of the winning team (or of everyone).
	AALKLobbyPlayerState* MVP = RequestedWinner.Get();
	if (!MVP)
	{
		TArray<FALKScoreLine> Lines;
		for (const AALKLobbyPlayerState* Each : Players)
		{
			Lines.Add(Each->GetScoreLine());
		}
		const int32 MVPIndex = UALKLobbyStatics::PickMVP(Lines, WinnerTeam);
		MVP = Players.IsValidIndex(MVPIndex) ? Players[MVPIndex] : nullptr;
	}

	LobbyState->WinnerTeam = WinnerTeam;
	LobbyState->MVPPlayer = MVP;
	LobbyState->MatchEndServerTime = 0.0;
	LobbyState->PostMatchEndServerTime = Config->PostMatchSeconds > 0.f ? LobbyState->GetServerWorldTimeSeconds() + Config->PostMatchSeconds : 0.0;
	LobbyState->Phase = EALKLobbyPhase::PostMatch;
	LobbyState->MarkLobbyStateChanged();

	for (AALKLobbyPlayerState* Each : Players)
	{
		Each->SetRematchVote(false);
	}
	if (Config->PostMatchSeconds > 0.f)
	{
		GetWorldTimerManager().SetTimer(PostMatchTimer, this, &ThisClass::HandlePostMatchTimeUp, Config->PostMatchSeconds, false);
	}
	K2_OnMatchFinished(WinnerTeam, MVP);
}

void AALKMatchGameMode::HandlePostMatchTimeUp()
{
	ReturnToLobby();
}

void AALKMatchGameMode::ReturnToLobby()
{
	const FString LobbyURL = UALKDeveloperSettings::GetOnlineConfig()->GetLobbyTravelURL();
	if (LobbyURL.IsEmpty())
	{
		UE_LOG(LogALK, Warning, TEXT("Return To Lobby: set Lobby Level in your Assetpro Online Config."));
		return;
	}
	GetWorldTimerManager().ClearTimer(LimitsTimer);
	GetWorldTimerManager().ClearTimer(PostMatchTimer);
	GetWorldTimerManager().ClearTimer(SimulationTimer);
	if (UALKOnlineSubsystem* Online = GetOnline())
	{
		// Also when the match is cut short: the session is open for players again.
		Online->NotifyServerMatchEnded();
	}
	TravelToLevel(LobbyURL);
}

void AALKMatchGameMode::Rematch()
{
	const FString LevelURL = UALKDeveloperSettings::GetOnlineConfig()->GetMatchTravelURL(ServerSettings.ModeId, ServerSettings.MapId);
	if (LevelURL.IsEmpty())
	{
		return;
	}
	GetWorldTimerManager().ClearTimer(LimitsTimer);
	GetWorldTimerManager().ClearTimer(PostMatchTimer);
	GetWorldTimerManager().ClearTimer(SimulationTimer);
	TravelToLevel(LevelURL);
}

// ---------------------------------------------------------------- Requests from players

void AALKMatchGameMode::HandleVoteRematch(APlayerController* Player, bool bWantsRematch)
{
	const AALKLobbyGameState* LobbyState = GetLobbyGameState();
	AALKLobbyPlayerState* State = Player ? Player->GetPlayerState<AALKLobbyPlayerState>() : nullptr;
	if (!State || !LobbyState || LobbyState->Phase != EALKLobbyPhase::PostMatch)
	{
		return;
	}
	State->SetRematchVote(bWantsRematch);

	// Real players decide; placeholders go along.
	int32 Voters = 0;
	int32 Yes = 0;
	for (const AALKLobbyPlayerState* Each : GetLobbyPlayers())
	{
		if (!Each->bPlaceholder && !Each->bSpectating)
		{
			++Voters;
			Yes += Each->bRematchVote ? 1 : 0;
		}
	}
	if (UALKLobbyStatics::IsRematchVotePassed(Yes, Voters))
	{
		Rematch();
	}
}

void AALKMatchGameMode::HandleSetPaused(APlayerController* Host, bool bPaused)
{
	AALKLobbyGameState* LobbyState = GetLobbyGameState();
	if (!IsHost(Host) || !LobbyState)
	{
		return;
	}
	if (bPaused)
	{
		// Only this request may pause a network game (see Allow Pausing).
		bHostPauseRequest = true;
		SetPause(Host);
		bHostPauseRequest = false;
	}
	else
	{
		ClearPause();
	}
	LobbyState->bServerPaused = IsPaused();
	LobbyState->MarkLobbyStateChanged();
}

void AALKMatchGameMode::HandleReturnToLobby(APlayerController* Host)
{
	if (IsHost(Host))
	{
		ReturnToLobby();
	}
}

// ---------------------------------------------------------------- Simulation

void AALKMatchGameMode::SetMatchSimulation(bool bEnabled)
{
	if (bMatchSimulation == bEnabled)
	{
		return;
	}
	bMatchSimulation = bEnabled;
	GetWorldTimerManager().ClearTimer(SimulationTimer);
	if (bEnabled)
	{
		ScheduleSimulation();
	}
}

void AALKMatchGameMode::ScheduleSimulation()
{
	GetWorldTimerManager().SetTimer(SimulationTimer, this, &ThisClass::SimulateMatchEvent, SimulationStream.FRandRange(1.5f, 4.f), false);
}

void AALKMatchGameMode::SimulateMatchEvent()
{
	if (!bMatchSimulation)
	{
		return;
	}
	const AALKLobbyGameState* LobbyState = GetLobbyGameState();
	if (LobbyState && IsMatchInProgress())
	{
		// A placeholder eliminates someone on another team (anyone, in a free for all), through the same calls a game makes.
		TArray<AALKLobbyPlayerState*> Killers;
		for (AALKLobbyPlayerState* Each : GetLobbyPlayers())
		{
			if (Each->bPlaceholder && !Each->bSpectating)
			{
				Killers.Add(Each);
			}
		}
		if (Killers.Num() > 0)
		{
			AALKLobbyPlayerState* Killer = Killers[SimulationStream.RandRange(0, Killers.Num() - 1)];
			const bool bTeams = LobbyState->TeamCount > 0;

			TArray<AALKLobbyPlayerState*> Victims;
			TArray<AALKLobbyPlayerState*> Helpers;
			for (AALKLobbyPlayerState* Each : Killers)
			{
				if (Each == Killer)
				{
					continue;
				}
				if (!bTeams || Each->TeamId != Killer->TeamId)
				{
					Victims.Add(Each);
				}
				else
				{
					Helpers.Add(Each);
				}
			}
			if (Victims.Num() > 0)
			{
				AALKLobbyPlayerState* Victim = Victims[SimulationStream.RandRange(0, Victims.Num() - 1)];
				AALKLobbyPlayerState* Assist = Helpers.Num() > 0 && SimulationStream.RandRange(0, 2) == 0 ? Helpers[SimulationStream.RandRange(0, Helpers.Num() - 1)] : nullptr;
				ReportKillByPlayerState(Killer, Victim, Assist);
			}
			else if (bTeams)
			{
				// Nobody to eliminate (one placeholder, or all on one team): the team scores an objective point instead.
				Killer->AddScore(KillScore);
				AddTeamScore(Killer->TeamId, 1);
			}
		}
	}
	if (bMatchSimulation && IsMatchInProgress())
	{
		ScheduleSimulation();
	}
}

#undef LOCTEXT_NAMESPACE
