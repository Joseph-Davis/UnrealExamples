// Copyright 2026 CodeLikeMe. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/TimerHandle.h"
#include "Math/RandomStream.h"
#include "Lobby/ALKNetGameModeBase.h"
#include "ALKMatchGameMode.generated.h"

/**
 * A ready base for match game modes (yours can be a child of it, in C++ or Blueprint; it is optional). It picks the
 * match up where the lobby left it and runs the frame around your gameplay:
 *
 * - the mode, map and rules arrive in the level URL and are put in the game state for everyone to read;
 * - players keep the slot and team they had in the lobby (a late joiner gets a free slot and the smaller team);
 * - the "TimeLimit" (minutes) and "ScoreLimit" rules end the match; so does Finish Match;
 * - Add Team Score and Report Kill keep the scores, the players' kills / deaths / assists and the kill feed;
 * - at the end: winner and MVP in the game state, the results phase, a rematch vote, then Return To Lobby;
 * - the host can pause.
 *
 * Your gameplay calls Report Kill (or Add Team Score / the player state's Add Score) and reads the rules with
 * Get Rule Int / Bool / Float on the game state's Rules.
 */
UCLASS(Blueprintable)
class ASSETPROLOBBYKIT_API AALKMatchGameMode : public AALKNetGameModeBase
{
	GENERATED_BODY()

public:
	AALKMatchGameMode();

	/** Points a kill adds to the killer's Score. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Assetpro Lobby|Match")
	float KillScore = 100.f;

	/** Points an assist adds to the helper's Score. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Assetpro Lobby|Match")
	float AssistScore = 50.f;

	/** What a kill adds to the killer's team score (team modes). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Assetpro Lobby|Match")
	int32 TeamScorePerKill = 1;

	/** Adds to a team's score. Reaching the score limit ends the match with that team as the winner. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Match")
	void AddTeamScore(int32 TeamId, int32 Amount = 1);

	/**
	 * A player was eliminated: the victim gets a death, the killer a kill and Kill Score (not for a team kill or a
	 * self-elimination), the killer's team its point, the assisting player an assist, and everyone a kill feed line.
	 * Killer and Assist may be empty.
	 */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Match")
	void ReportKill(AController* Killer, AController* Victim, AController* Assist);

	/** The same by player state (placeholder players have no controller). */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Match")
	void ReportKillByPlayerState(AALKLobbyPlayerState* Killer, AALKLobbyPlayerState* Victim, AALKLobbyPlayerState* Assist);

	/**
	 * Ends the match now. Winner Team is the winning team, or -1 to let the scores decide (the highest team score; in a
	 * free for all the best player). Then: results phase, rematch vote, return to the lobby.
	 */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Match")
	void FinishMatch(int32 WinnerTeam = -1);

	/** Ends the session's match and brings everyone back to the lobby level. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Match")
	void ReturnToLobby();

	/** Plays the same mode and map again, with fresh scores. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Match")
	void Rematch();

	/**
	 * While on, placeholder players "score" every few seconds through Report Kill and Add Team Score, so the kill feed,
	 * the scoreboard and the end of the match have something to show without anyone playing. For demos and screenshots.
	 */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Match")
	void SetMatchSimulation(bool bEnabled);

	/** The match simulation is running. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Match")
	bool IsMatchSimulationOn() const { return bMatchSimulation; }

	/** The match ended: winner and MVP are in the game state. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Assetpro Lobby|Match", meta = (DisplayName = "On Match Finished"))
	void K2_OnMatchFinished(int32 WinnerTeam, AALKLobbyPlayerState* MVP);

	// ---- AALKNetGameModeBase
	virtual void InitGameState() override;
	virtual void HandleVoteRematch(APlayerController* Player, bool bWantsRematch) override;
	virtual void HandleSetPaused(APlayerController* Host, bool bPaused) override;
	virtual void HandleReturnToLobby(APlayerController* Host) override;

protected:
	virtual void OnPlayerJoined(APlayerController* Player) override;
	virtual void HandleMatchHasStarted() override;
	virtual void HandleMatchHasEnded() override;

	/** Ends the match when the time is up. */
	void CheckMatchLimits();
	void HandlePostMatchTimeUp();
	void SimulateMatchEvent();
	void ScheduleSimulation();

	FTimerHandle LimitsTimer;
	FTimerHandle PostMatchTimer;
	FTimerHandle SimulationTimer;

	/** The winner Finish Match was given, until the match has ended. -1 = let the scores decide. */
	int32 RequestedWinnerTeam = INDEX_NONE;
	/** A free for all was won by reaching the score limit: this player. */
	TWeakObjectPtr<AALKLobbyPlayerState> RequestedWinner;

	bool bMatchSimulation = false;
	FRandomStream SimulationStream;
};
