// Copyright 2026 CodeLikeMe. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/TimerHandle.h"
#include "Lobby/ALKNetGameModeBase.h"
#include "ALKLobbyGameMode.generated.h"

/**
 * The game mode of the lobby level. Set it as the GameMode Override of your lobby level (or leave the Online Config's
 * Lobby Level empty and the engine's empty level is used with this game mode).
 *
 * - A joining player gets the first free slot and the smaller team; leaving frees both.
 * - When everyone is ready the countdown starts (Online Config: Lobby Countdown Seconds); it stops if someone is not
 *   ready any more. The host can force the start and cancel a countdown.
 * - Players vote for the map; the most voted one is played, the host's map when nobody voted.
 * - The host can change the settings (mode, map, player limit, rules, name, password), move players between teams,
 *   kick and ban.
 * - At the end of the countdown the session is marked as started and everyone travels to the map's level, with the mode
 *   and rules in the URL options ("?ALKMode=..?ALKMap=..?ALKRules=..").
 *
 * Players have no pawn here: the lobby is a screen. Give it a pawn class in a Blueprint child if yours is a place to walk around.
 */
UCLASS(Blueprintable)
class ASSETPROLOBBYKIT_API AALKLobbyGameMode : public AALKNetGameModeBase
{
	GENERATED_BODY()

public:
	AALKLobbyGameMode();

	/**
	 * Starts the countdown to the match. With Force it also starts when not everyone is ready (never without the minimum
	 * players or a map). Returns why it could not, or None.
	 */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Lobby")
	EALKStartBlocker StartCountdown(bool bForce);

	/** Stops a running countdown and goes back to waiting. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Lobby")
	void CancelCountdown();

	/** Skips the countdown: travels to the match now (when it can start; Force as in Start Countdown). */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Lobby")
	EALKStartBlocker StartMatchNow(bool bForce);

	/** Changes the game's settings, as the host's Server Change Match Settings does. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Lobby")
	void ChangeMatchSettings(const FALKHostSettings& Settings);

	/** The match is about to start: the map is decided and the travel begins right after this. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Assetpro Lobby|Lobby", meta = (DisplayName = "On Match Starting"))
	void K2_OnMatchStarting(FName ModeId, FName MapId);

	// ---- AALKNetGameModeBase
	virtual void InitGameState() override;
	virtual void HandleVoteMap(APlayerController* Player, FName MapId) override;
	virtual void HandleChangeMatchSettings(APlayerController* Host, const FALKHostSettings& Settings) override;
	virtual void HandleStartMatch(APlayerController* Host, bool bForce) override;
	virtual void HandleCancelCountdown(APlayerController* Host) override;
	virtual void HandleChangeMap(APlayerController* Host, FName MapId) override;

protected:
	virtual void OnPlayerJoined(APlayerController* Player) override;
	virtual void SetPlayerReady(AALKLobbyPlayerState* Player, bool bReady) override;
	virtual void OnRosterChanged() override;

	/** Starts the countdown when everyone is ready, stops it when that is no longer so. */
	void EvaluateStart();

	/** Counts the players' map votes into the game state. */
	void RefreshMapVotes();

	void BeginCountdown(bool bForced);
	void FinishCountdown();

	/** Decides the map, marks the session as started and travels to the match. False when there is nowhere to go. */
	bool TravelToMatch();

	FTimerHandle CountdownTimer;
};
