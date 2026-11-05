// Copyright 2026 CodeLikeMe. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameState.h"
#include "Lobby/ALKLobbyTypes.h"
#include "Online/ALKOnlineTypes.h"
#include "Rules/ALKGameRules.h"
#include "ALKLobbyGameState.generated.h"

class AALKLobbyPlayerState;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FALKOnLobbyStateChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FALKOnLobbyPlayersChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FALKOnFeedEvent, const FALKFeedEvent&, Event);

/**
 * What every player knows about the lobby and the match: the server's name, mode, map, rules and player limit, the
 * lobby phase and countdown, the map vote, and in a match the team scores, the time left, the winner and the MVP.
 * The same class serves the lobby level and the match level.
 *
 * Menus and HUDs listen to three events: On Lobby State Changed (anything on this actor changed), On Players Changed
 * (someone joined, left, or a player's values changed) and On Feed Event (kill feed / event feed lines).
 */
UCLASS(Blueprintable)
class ASSETPROLOBBYKIT_API AALKLobbyGameState : public AGameState
{
	GENERATED_BODY()

public:
	// ---------------------------------------------------------------- Replicated: the game

	/** The game's name. */
	UPROPERTY(ReplicatedUsing = OnRep_LobbyState, BlueprintReadOnly, Category = "Assetpro Lobby|Lobby")
	FString ServerName;

	/** The game mode's Id (see the Online Config's Modes). */
	UPROPERTY(ReplicatedUsing = OnRep_LobbyState, BlueprintReadOnly, Category = "Assetpro Lobby|Lobby")
	FName ModeId;

	/** The map's Id. In the lobby it is the host's choice; the map vote can change it when the match starts. */
	UPROPERTY(ReplicatedUsing = OnRep_LobbyState, BlueprintReadOnly, Category = "Assetpro Lobby|Lobby")
	FName MapId;

	/** The match rules. Read them with Get Rule Int / Bool / Float / String. */
	UPROPERTY(ReplicatedUsing = OnRep_LobbyState, BlueprintReadOnly, Category = "Assetpro Lobby|Rules")
	FALKGameRules Rules;

	/** The player limit. */
	UPROPERTY(ReplicatedUsing = OnRep_LobbyState, BlueprintReadOnly, Category = "Assetpro Lobby|Lobby")
	int32 MaxPlayers = 8;

	/** How many teams the mode has. 0 = free for all. */
	UPROPERTY(ReplicatedUsing = OnRep_LobbyState, BlueprintReadOnly, Category = "Assetpro Lobby|Lobby")
	int32 TeamCount = 0;

	/** How many bots the host asked for. */
	UPROPERTY(ReplicatedUsing = OnRep_LobbyState, BlueprintReadOnly, Category = "Assetpro Lobby|Lobby")
	int32 BotCount = 0;

	/** A password is needed to join. */
	UPROPERTY(ReplicatedUsing = OnRep_LobbyState, BlueprintReadOnly, Category = "Assetpro Lobby|Lobby")
	bool bPasswordProtected = false;

	/** The code others type to join (empty when the game cannot be reached by address). Show it with the online subsystem's Get Display Lobby Code. */
	UPROPERTY(ReplicatedUsing = OnRep_LobbyState, BlueprintReadOnly, Category = "Assetpro Lobby|Lobby")
	FString LobbyCode;

	// ---------------------------------------------------------------- Replicated: lobby

	/** Where the lobby or match is at: waiting, countdown, starting, in match or post match. */
	UPROPERTY(ReplicatedUsing = OnRep_LobbyState, BlueprintReadOnly, Category = "Assetpro Lobby|Lobby")
	EALKLobbyPhase Phase = EALKLobbyPhase::Waiting;

	/** The server time at which the lobby countdown ends. Use Get Countdown Remaining. */
	UPROPERTY(ReplicatedUsing = OnRep_LobbyState, BlueprintReadOnly, Category = "Assetpro Lobby|Lobby")
	double CountdownEndServerTime = 0.0;

	/** The host started before everyone was ready. */
	UPROPERTY(ReplicatedUsing = OnRep_LobbyState, BlueprintReadOnly, Category = "Assetpro Lobby|Lobby")
	bool bForcedStart = false;

	/** The map vote so far, most voted first. */
	UPROPERTY(ReplicatedUsing = OnRep_LobbyState, BlueprintReadOnly, Category = "Assetpro Lobby|Lobby")
	TArray<FALKMapVote> MapVotes;

	// ---------------------------------------------------------------- Replicated: match

	/** One score per team. */
	UPROPERTY(ReplicatedUsing = OnRep_LobbyState, BlueprintReadOnly, Category = "Assetpro Lobby|Match")
	TArray<int32> TeamScores;

	/** The server time at which the match ends. 0 = no time limit. Use Get Match Time Remaining. */
	UPROPERTY(ReplicatedUsing = OnRep_LobbyState, BlueprintReadOnly, Category = "Assetpro Lobby|Match")
	double MatchEndServerTime = 0.0;

	/** The score that ends the match. 0 = no score limit. */
	UPROPERTY(ReplicatedUsing = OnRep_LobbyState, BlueprintReadOnly, Category = "Assetpro Lobby|Match")
	int32 ScoreLimit = 0;

	/** The team that won, after the match. -1 = a draw, or no teams (then the MVP is the winner). */
	UPROPERTY(ReplicatedUsing = OnRep_LobbyState, BlueprintReadOnly, Category = "Assetpro Lobby|Match")
	int32 WinnerTeam = INDEX_NONE;

	/** The match's best player, after the match. */
	UPROPERTY(ReplicatedUsing = OnRep_LobbyState, BlueprintReadOnly, Category = "Assetpro Lobby|Match")
	TObjectPtr<AALKLobbyPlayerState> MVPPlayer;

	/** The server time at which the results screen ends and everyone returns to the lobby. 0 = when the host chooses. */
	UPROPERTY(ReplicatedUsing = OnRep_LobbyState, BlueprintReadOnly, Category = "Assetpro Lobby|Match")
	double PostMatchEndServerTime = 0.0;

	/** The host paused the match. */
	UPROPERTY(ReplicatedUsing = OnRep_LobbyState, BlueprintReadOnly, Category = "Assetpro Lobby|Match")
	bool bServerPaused = false;

	/** The server's frames per second, for the net stats overlay. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Assetpro Lobby|Match")
	int32 ServerTickRate = 0;

	// ---------------------------------------------------------------- Reading

	/** Every ALK player state, by slot. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Lobby")
	TArray<AALKLobbyPlayerState*> GetLobbyPlayers() const;

	/** The players on a team (-1 = the players without a team), by slot. Spectators are left out. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Lobby")
	TArray<AALKLobbyPlayerState*> GetPlayersOnTeam(int32 TeamId) const;

	/** The players who are watching, by slot. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Lobby")
	TArray<AALKLobbyPlayerState*> GetSpectators() const;

	/** The player in a slot, or null when the slot is free. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Lobby")
	AALKLobbyPlayerState* GetPlayerInSlot(int32 Slot) const;

	/** The player with this user id, or null. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Lobby")
	AALKLobbyPlayerState* FindPlayerByUserId(const FString& UserId) const;

	/** The player running the lobby, or null. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Lobby")
	AALKLobbyPlayerState* GetHostPlayer() const;

	/** How many players take part (spectators not counted). */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Lobby")
	int32 GetActivePlayerCount() const;

	/** How many of them are ready. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Lobby")
	int32 GetReadyCount() const;

	/** Everyone who takes part is ready (and there is at least one player). */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Lobby")
	bool AreAllPlayersReady() const;

	/** Why the match cannot start yet (for the start button), or None. Force = as the host's forced start would see it. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Lobby")
	EALKStartBlocker GetStartBlocker(bool bForce) const;

	/** Seconds left of the lobby countdown. 0 when none runs. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Lobby")
	float GetCountdownRemaining() const;

	/** How many votes a map has. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Lobby")
	int32 GetMapVoteCount(FName InMapId) const;

	/** Seconds left of the match. -1 with no time limit. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Match")
	float GetMatchTimeRemaining() const;

	/** Seconds left of the results screen. -1 when the host decides. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Match")
	float GetPostMatchTimeRemaining() const;

	/** A team's score; 0 for a team that does not exist. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Match")
	int32 GetTeamScore(int32 TeamId) const;

	/** The lobby phases: waiting, countdown or starting. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Lobby")
	bool IsInLobby() const { return Phase == EALKLobbyPhase::Waiting || Phase == EALKLobbyPhase::Countdown || Phase == EALKLobbyPhase::Starting; }

	/** The match phases: in match or post match. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Match")
	bool IsInMatch() const { return Phase == EALKLobbyPhase::InMatch || Phase == EALKLobbyPhase::PostMatch; }

	// ---------------------------------------------------------------- Server

	/** Server: sends a line to everyone's kill feed / event feed. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Assetpro Lobby|Match")
	void BroadcastFeedEvent(const FALKFeedEvent& Event);

	/** Server to everyone: a feed line. Call Broadcast Feed Event instead. */
	UFUNCTION(NetMulticast, Reliable)
	void MulticastFeedEvent(const FALKFeedEvent& Event);

	/** Server: the values above were changed. Replicates them now and fires On Lobby State Changed on the server too. */
	void MarkLobbyStateChanged();

	/**
	 * Server: moves the lobby / match to a phase (waiting, countdown, starting, in match, post match). The ALK game modes
	 * do it for you; call it from a game mode of your own so the menus follow: the lobby screen shows in the lobby phases,
	 * the match overlay in In Match, the results screen in Post Match.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Assetpro Lobby|Lobby")
	void SetPhase(EALKLobbyPhase NewPhase);

	/** Fires On Players Changed (player states call it when their values change). */
	void NotifyPlayersChanged();

	// ---------------------------------------------------------------- Events

	/** Something on the game state changed: settings, phase, countdown, votes, scores, winner. */
	UPROPERTY(BlueprintAssignable, Category = "Assetpro Lobby|Lobby")
	FALKOnLobbyStateChanged OnLobbyStateChanged;
	FSimpleMulticastDelegate OnLobbyStateChangedNative;

	/** A player joined or left, or a player's values (ready, team, score...) changed. */
	UPROPERTY(BlueprintAssignable, Category = "Assetpro Lobby|Lobby")
	FALKOnLobbyPlayersChanged OnPlayersChanged;
	FSimpleMulticastDelegate OnPlayersChangedNative;

	/** A kill feed / event feed line arrived. */
	UPROPERTY(BlueprintAssignable, Category = "Assetpro Lobby|Match")
	FALKOnFeedEvent OnFeedEvent;
	TMulticastDelegate<void(const FALKFeedEvent& /*Event*/)> OnFeedEventNative;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void AddPlayerState(APlayerState* PlayerState) override;
	virtual void RemovePlayerState(APlayerState* PlayerState) override;

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnRep_LobbyState();

	void NotifyLobbyStateChanged();

	/** The phase the online subsystem was last told about. */
	EALKLobbyPhase ReportedPhase = EALKLobbyPhase::Waiting;
	bool bPhaseReported = false;
};
