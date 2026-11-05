// Copyright 2026 CodeLikeMe. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "Lobby/ALKLobbyTypes.h"
#include "ALKLobbyPlayerState.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FALKOnPlayerStateChanged);

/**
 * A player in the lobby and in the match: slot, team, ready, host, loadout, colour, level, spectating, votes and the
 * match numbers the scoreboard shows. Everything replicates to every player and survives the travel between the lobby
 * and the match (seamless or not) and a reconnect.
 *
 * Players change their own values through the ALK Player Net Component (Server Set Ready, Server Request Team...); the
 * server-side setters here are for your game code: Add Kill, Add Death, Add Assist, Add Custom Stat feed the scoreboard.
 *
 * Use it as the Player State Class of your own game mode, or make a child of it for more replicated values.
 */
UCLASS(Blueprintable)
class ASSETPROLOBBYKIT_API AALKLobbyPlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	AALKLobbyPlayerState();

	// ---------------------------------------------------------------- Replicated

	/** The player's id on the online backend, as text: chat, mutes, bans and the roster go by it. Set by the server. */
	UPROPERTY(ReplicatedUsing = OnRep_LobbyState, BlueprintReadOnly, Category = "Assetpro Lobby|Lobby")
	FString UserId;

	/** The lobby slot, 0 to Max Players - 1. -1 until the server gave one. */
	UPROPERTY(ReplicatedUsing = OnRep_LobbyState, BlueprintReadOnly, Category = "Assetpro Lobby|Lobby")
	int32 Slot = INDEX_NONE;

	/** The team number, or -1 for no team (free for all). */
	UPROPERTY(ReplicatedUsing = OnRep_LobbyState, BlueprintReadOnly, Category = "Assetpro Lobby|Lobby")
	int32 TeamId = INDEX_NONE;

	/** Ready to start. */
	UPROPERTY(ReplicatedUsing = OnRep_LobbyState, BlueprintReadOnly, Category = "Assetpro Lobby|Lobby")
	bool bReady = false;

	/** The player who runs the lobby: starts the match, changes settings, kicks. */
	UPROPERTY(ReplicatedUsing = OnRep_LobbyState, BlueprintReadOnly, Category = "Assetpro Lobby|Lobby")
	bool bHost = false;

	/** A stand-in player with nobody behind it (Add Placeholder Players on the game mode): for testing alone, demos and simple bot seats. */
	UPROPERTY(ReplicatedUsing = OnRep_LobbyState, BlueprintReadOnly, Category = "Assetpro Lobby|Lobby")
	bool bPlaceholder = false;

	/** The chosen character or loadout, by a name your game knows. */
	UPROPERTY(ReplicatedUsing = OnRep_LobbyState, BlueprintReadOnly, Category = "Assetpro Lobby|Lobby")
	FName LoadoutId;

	/** The chosen colour, 0 to the Online Config's Player Color Count - 1. */
	UPROPERTY(ReplicatedUsing = OnRep_LobbyState, BlueprintReadOnly, Category = "Assetpro Lobby|Lobby")
	int32 ColorIndex = 0;

	/** The player's level, for the lobby card. Set it from your progression with Set Player Level. */
	UPROPERTY(ReplicatedUsing = OnRep_LobbyState, BlueprintReadOnly, Category = "Assetpro Lobby|Lobby")
	int32 PlayerLevel = 1;

	/** Watching instead of playing: not counted for teams, ready checks and the MVP. */
	UPROPERTY(ReplicatedUsing = OnRep_LobbyState, BlueprintReadOnly, Category = "Assetpro Lobby|Lobby")
	bool bSpectating = false;

	/** The map this player voted for in the lobby. None = no vote. */
	UPROPERTY(ReplicatedUsing = OnRep_LobbyState, BlueprintReadOnly, Category = "Assetpro Lobby|Lobby")
	FName VotedMapId;

	/** This player wants a rematch (after the match). */
	UPROPERTY(ReplicatedUsing = OnRep_LobbyState, BlueprintReadOnly, Category = "Assetpro Lobby|Match")
	bool bRematchVote = false;

	/** The party this player is in (its leader's user id), for the Party chat channel. Empty = none. */
	UPROPERTY(ReplicatedUsing = OnRep_LobbyState, BlueprintReadOnly, Category = "Assetpro Lobby|Lobby")
	FString PartyId;

	/** Eliminations in this match. */
	UPROPERTY(ReplicatedUsing = OnRep_LobbyState, BlueprintReadOnly, Category = "Assetpro Lobby|Match")
	int32 Kills = 0;

	/** Times eliminated in this match. */
	UPROPERTY(ReplicatedUsing = OnRep_LobbyState, BlueprintReadOnly, Category = "Assetpro Lobby|Match")
	int32 Deaths = 0;

	/** Assists in this match. */
	UPROPERTY(ReplicatedUsing = OnRep_LobbyState, BlueprintReadOnly, Category = "Assetpro Lobby|Match")
	int32 Assists = 0;

	/** Extra scoreboard numbers by name (Custom Stat score columns show them). */
	UPROPERTY(ReplicatedUsing = OnRep_LobbyState, BlueprintReadOnly, Category = "Assetpro Lobby|Match")
	TArray<FALKCustomStat> CustomStats;

	// ---------------------------------------------------------------- Reading

	/** The player's id on the backend; a placeholder id for a player the backend does not know. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Lobby")
	const FString& GetUserId() const { return UserId; }

	/** A small number that stays the same for this player in this game (the slot): streamer mode names ("Player 3") use it. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Lobby")
	int32 GetStableIndex() const;

	/** True on the machine of the player this state belongs to. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Lobby")
	bool IsLocalPlayerState() const;

	/** A named stat's value; 0 when it was never set. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Match")
	float GetCustomStat(FName StatId) const;

	/** The numbers the scoring helpers take (Pick MVP). */
	FALKScoreLine GetScoreLine() const;

	// ---------------------------------------------------------------- Server: your game code feeds the scoreboard

	/** Adds to the player's kills. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Assetpro Lobby|Match")
	void AddKill(int32 Amount = 1);

	/** Adds to the player's deaths. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Assetpro Lobby|Match")
	void AddDeath(int32 Amount = 1);

	/** Adds to the player's assists. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Assetpro Lobby|Match")
	void AddAssist(int32 Amount = 1);

	/** Adds to the player state's Score (the Score column). */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Assetpro Lobby|Match")
	void AddScore(float Amount);

	/** Adds to a named stat (made at 0 the first time). */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Assetpro Lobby|Match")
	void AddCustomStat(FName StatId, float Amount);

	/** Sets a named stat (at most 16 per player). */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Assetpro Lobby|Match")
	void SetCustomStat(FName StatId, float Value);

	/** Kills, deaths, assists, score and custom stats back to 0 (the match game mode does it when a match starts). */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Assetpro Lobby|Match")
	void ResetMatchStats();

	/** Sets the level shown on the player's lobby card. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Assetpro Lobby|Lobby")
	void SetPlayerLevel(int32 NewLevel);

	// ---------------------------------------------------------------- Server: used by the ALK game modes

	void SetUserId(const FString& NewUserId);
	void SetSlot(int32 NewSlot);
	void SetTeamId(int32 NewTeamId);
	void SetReady(bool bNewReady);
	void SetHost(bool bNewHost);
	void SetPlaceholder(bool bNewPlaceholder);
	void SetLoadoutId(FName NewLoadoutId);
	void SetColorIndex(int32 NewColorIndex);
	void SetSpectating(bool bNewSpectating);
	void SetVotedMapId(FName NewMapId);
	void SetRematchVote(bool bNewVote);
	void SetPartyId(const FString& NewPartyId);
	/** A ping to show for a player without a connection (a placeholder), in milliseconds. */
	void SetDisplayedPing(int32 PingMs);

	// ---------------------------------------------------------------- Events

	/** Any replicated value of this player changed (on every machine). */
	UPROPERTY(BlueprintAssignable, Category = "Assetpro Lobby|Lobby")
	FALKOnPlayerStateChanged OnPlayerStateChanged;
	FSimpleMulticastDelegate OnPlayerStateChangedNative;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void CopyProperties(APlayerState* PlayerState) override;
	virtual void OverrideWith(APlayerState* PlayerState) override;
	virtual void Reset() override;
	virtual void OnRep_PlayerName() override;

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnRep_LobbyState();

	/** Tells everyone listening that something changed: this state's event, and the game state's player list event. */
	void NotifyChanged();

	/** Server: marks the state as changed, so it replicates soon and local listeners hear about it. */
	void MarkChanged();

	/** Remembers the other players this machine met, for the Recent Players list. */
	void NoteAsRecentPlayer();

	/** The name this player was last noted under, so they are noted once and again only after a name change. */
	FString NotedName;
};
