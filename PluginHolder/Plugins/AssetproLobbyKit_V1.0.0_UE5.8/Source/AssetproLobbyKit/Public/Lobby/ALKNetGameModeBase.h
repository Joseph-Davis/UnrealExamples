// Copyright 2026 CodeLikeMe. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/TimerHandle.h"
#include "GameFramework/GameMode.h"
#include "Math/RandomStream.h"
#include "Lobby/ALKLobbyTypes.h"
#include "Online/ALKOnlineTypes.h"
#include "ALKNetGameModeBase.generated.h"

class AALKLobbyGameState;
class AALKLobbyPlayerState;
class UALKOnlineSubsystem;
class APlayerController;

/**
 * What the ALK lobby and match game modes share: who may join (password, version, ban list, free slot), slots and
 * teams, the host, kick and ban, chat routing, placeholder players, and the travel between lobby and match.
 * Server only, like every game mode.
 *
 * Joining players are refused in Pre Login with the ALKNetReason strings ("ALK.ServerFull", "ALK.WrongPassword",
 * "ALK.WrongVersion", "ALK.Banned"); the online subsystem on their machine turns those into the matching dialog.
 *
 * Use ALK Lobby Game Mode for your lobby level and ALK Match Game Mode (or a child) for your match levels. Their default
 * classes are ALK Lobby Player Controller, ALK Lobby Player State, ALK Lobby Game State and ALK Game Session.
 */
UCLASS(Abstract, Blueprintable)
class ASSETPROLOBBYKIT_API AALKNetGameModeBase : public AGameMode
{
	GENERATED_BODY()

public:
	AALKNetGameModeBase();

	// ---------------------------------------------------------------- Reading

	/** The settings this game runs with: name, mode, map, player limit, rules (and the password). */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Lobby")
	const FALKHostSettings& GetServerSettings() const { return ServerSettings; }

	/** The ALK game state of this level, or null with a game state class of your own. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Lobby")
	AALKLobbyGameState* GetLobbyGameState() const;

	/** Whether this player runs the game: the listen server's own player, or the lobby leader on a dedicated server. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Lobby")
	bool IsHost(const APlayerController* Player) const;

	/** Every ALK player state in the game, placeholders included. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Lobby")
	TArray<AALKLobbyPlayerState*> GetLobbyPlayers() const;

	/** The controller of the player whose player state has this Player Id, or null (a placeholder has none). */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Lobby")
	APlayerController* FindControllerByPlayerId(int32 PlayerId) const;

	/** The player state with this Player Id, or null. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Lobby")
	AALKLobbyPlayerState* FindPlayerByPlayerId(int32 PlayerId) const;

	// ---------------------------------------------------------------- Host tools

	/**
	 * Removes a player: tells them why ("ALK.Kicked" or "ALK.Banned"), then closes their connection. With Ban they are
	 * also added to this machine's ban list and refused from then on. The host cannot be removed.
	 */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Lobby")
	bool KickPlayer(APlayerController* Player, bool bBan);

	/** Sends a System line to everyone's chat. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Lobby")
	void BroadcastSystemMessage(const FString& Text);

	/** Sends a line to everyone's kill feed / event feed. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Lobby")
	void BroadcastFeedEvent(const FALKFeedEvent& Event);

	// ---------------------------------------------------------------- Placeholder players

	/**
	 * Adds stand-in players: player states with no controller behind them, with a made-up name, a slot, a team and a ping,
	 * exactly like real players in the lobby, on the scoreboard and in the start checks. For testing alone, demos,
	 * screenshots, or as simple bot seats. In the lobby they ready up by themselves after a moment, except the last one
	 * (call Set Placeholders Ready). They follow the game to the match and back. A real player who needs the seat takes it.
	 * Returns how many were added (free slots permitting).
	 */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Lobby")
	int32 AddPlaceholderPlayers(int32 Count);

	/** Removes every placeholder player. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Lobby")
	void RemovePlaceholderPlayers();

	/** Readies every placeholder now (also the one that was holding out), or un-readies them all. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Lobby")
	void SetPlaceholdersReady(bool bReady);

	/** How many placeholder players are in the game. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Lobby")
	int32 GetPlaceholderCount() const;

	/** Makes a random placeholder say a line in the All chat, through the normal chat path. False with no placeholders. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Lobby")
	bool SendPlaceholderChat(const FString& Text);

	// ---------------------------------------------------------------- Blueprint events

	/** A player is in: slot and team are set. Also fired after the travel from the lobby to the match and back. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Assetpro Lobby|Lobby", meta = (DisplayName = "On Lobby Player Joined"))
	void K2_OnLobbyPlayerJoined(APlayerController* Player);

	/** A player is leaving: their slot and team are free again. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Assetpro Lobby|Lobby", meta = (DisplayName = "On Lobby Player Left"))
	void K2_OnLobbyPlayerLeft(AController* Player);

	/** How many bots the host asked for (the "Bots" rule). Spawn or remove yours here; the kit has no bot AI of its own. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Assetpro Lobby|Lobby")
	void OnBotCountChanged(int32 NewBotCount);

	// ---------------------------------------------------------------- Requests from players (the net component calls these)

	virtual void HandleSetReady(APlayerController* Player, bool bReady);
	virtual void HandleRequestTeam(APlayerController* Player, int32 TeamId);
	virtual void HandleSetLoadout(APlayerController* Player, FName LoadoutId);
	virtual void HandleSetColor(APlayerController* Player, int32 ColorIndex);
	virtual void HandleSetSpectating(APlayerController* Player, bool bSpectating);
	virtual void HandleVoteMap(APlayerController* Player, FName MapId) {}
	virtual void HandleVoteRematch(APlayerController* Player, bool bWantsRematch) {}

	/** Host only: each checks the caller again with Is Host. */
	virtual void HandleKickPlayer(APlayerController* Host, int32 PlayerId, bool bBan);
	virtual void HandleUnbanPlayer(APlayerController* Host, const FString& UserId);
	virtual void HandleMovePlayerToTeam(APlayerController* Host, int32 PlayerId, int32 TeamId);
	virtual void HandleChangeMatchSettings(APlayerController* Host, const FALKHostSettings& Settings) {}
	virtual void HandleStartMatch(APlayerController* Host, bool bForce) {}
	virtual void HandleCancelCountdown(APlayerController* Host) {}
	virtual void HandleChangeMap(APlayerController* Host, FName MapId) {}
	virtual void HandleSetPaused(APlayerController* Host, bool bPaused) {}
	virtual void HandleReturnToLobby(APlayerController* Host) {}

	// ---------------------------------------------------------------- Chat

	/** Sends a chat line to the players its channel reaches. Override to add channels or logging of your own. */
	virtual void RouteChatMessage(const FALKChatMessage& Message);

	/**
	 * The routing itself, usable with any game mode: All and System reach everyone, Team the sender's team, Party the
	 * players with the sender's party id, Whisper the target and the sender.
	 */
	static void DeliverChatMessage(UWorld* World, const FALKChatMessage& Message);

	// ---------------------------------------------------------------- AGameMode
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void InitGameState() override;
	virtual void StartPlay() override;
	virtual void PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage) override;
	virtual void Logout(AController* Exiting) override;
	virtual void GetSeamlessTravelActorList(bool bToTransition, TArray<AActor*>& ActorList) override;
	virtual bool AllowPausing(APlayerController* PC = nullptr) override;

protected:
	virtual void GenericPlayerInitialization(AController* C) override;

	/** A player is in the level (first join, or after a travel): gives them a user id, slot and team. */
	virtual void OnPlayerJoined(APlayerController* Player);

	/** A player is leaving. Their player state is still in the game state's list. */
	virtual void OnPlayerLeft(AController* Player);

	/** Sets a player's ready flag. The lobby checks the start conditions after it. */
	virtual void SetPlayerReady(AALKLobbyPlayerState* Player, bool bReady);

	/** The players, their teams or the placeholders changed. The lobby checks the start conditions here. */
	virtual void OnRosterChanged() {}

	/** Copies the server settings into the game state (everyone sees them). */
	virtual void ApplySettingsToGameState();

	/** Gives a player a free slot and a team with room: the preferred ones when they are free, else the first free slot and the smallest team. */
	void AssignSlotAndTeam(AALKLobbyPlayerState* Player, int32 PreferredSlot, int32 PreferredTeam);

	/** Puts every player on a valid team again after the team count changed. */
	void ReassignTeams();

	/** Marks the host: the listen server's own player, else the first real player. */
	void UpdateHost();

	/** Writes what the next level's game mode needs to put everyone back in place (also the placeholders). */
	void StoreRosterForTravel();

	/**
	 * Server travel to a level ("/Game/Maps/Arena" or "/Game/Maps/Arena?game=..."), with "?listen" on a listen server
	 * and the server settings as URL options. Seamless when the project and the editor allow it.
	 */
	bool TravelToLevel(const FString& LevelURL);

	UALKOnlineSubsystem* GetOnline() const;
	void SendNotice(APlayerController* Player, const FText& Text) const;
	void AnnouncePlayer(AALKLobbyPlayerState* Player, bool bJoined);

	/** With the Online Config's Bots Are Placeholders: as many placeholder players as the Bots rule asks for. */
	void SyncPlaceholderBots();

	AALKLobbyPlayerState* SpawnPlaceholder(const FString& Name, int32 PreferredSlot, int32 PreferredTeam);
	void RestorePlaceholders();
	void ReadyPlaceholder(TWeakObjectPtr<AALKLobbyPlayerState> Placeholder);
	void SchedulePlaceholderReady(AALKLobbyPlayerState* Placeholder);
	TArray<AALKLobbyPlayerState*> GetPlaceholders() const;

	/** The settings of the running game. Read from the online subsystem and the level URL in Init Game. */
	FALKHostSettings ServerSettings;

	/** Random choices of the placeholders (the same every run, so demos and screenshots repeat). */
	FRandomStream PlaceholderStream;

	/** Set Placeholders Ready (true) was called: no placeholder holds out any more. */
	bool bAllPlaceholdersReady = false;

private:
	void UpdateServerTickRate();
	void HandleDelayedHostUpdate();

	FTimerHandle TickRateTimer;
	uint64 LastFrameCounter = 0;
	double LastTickRateTime = 0.0;

protected:
	/** True only while the host's pause request is carried out: nothing else pauses a network game. */
	bool bHostPauseRequest = false;
};
