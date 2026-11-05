// Copyright 2026 CodeLikeMe. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Online/ALKOnlineTypes.h"
#include "ALKPlayerNetComponent.generated.h"

class APlayerController;
class AALKLobbyPlayerState;
class AALKNetGameModeBase;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FALKOnChatReceived, const FALKChatMessage&, Message);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FALKOnServerNotice, const FText&, Text);

/**
 * How a player talks to the server in the lobby and in the match. It lives on the player controller (ALK Lobby Player
 * Controller has one; add it to your own controller class to use the lobby with it) and carries every request:
 *
 * - any player: ready, team, loadout, colour, chat, map vote, rematch vote, spectate;
 * - the host only (checked again on the server): kick, ban, unban, move a player, change the settings, start the match,
 *   cancel the countdown, change the map, pause, return to the lobby.
 *
 * The server answers through the replicated player states and game state; chat lines, feed events and notices come back
 * here. Get the local player's component with Get Local Player Net Component.
 */
UCLASS(ClassGroup = "Assetpro Lobby", meta = (BlueprintSpawnableComponent))
class ASSETPROLOBBYKIT_API UALKPlayerNetComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UALKPlayerNetComponent();

	/** The net component of a player controller, or null. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Lobby")
	static UALKPlayerNetComponent* Get(const APlayerController* PlayerController);

	/** The controller this component is on. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Lobby")
	APlayerController* GetPlayerController() const;

	/** This player's ALK player state, or null. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Lobby")
	AALKLobbyPlayerState* GetLobbyPlayerState() const;

	// ---------------------------------------------------------------- Any player

	/** Marks this player ready or not ready (in the lobby). */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Assetpro Lobby|Lobby")
	void ServerSetReady(bool bReady);

	/** Asks to move to a team. Refused when the team is full or team choice is turned off. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Assetpro Lobby|Lobby")
	void ServerRequestTeam(int32 TeamId);

	/** Picks a character or loadout by name. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Assetpro Lobby|Lobby")
	void ServerSetLoadout(FName LoadoutId);

	/** Picks a colour by its number. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Assetpro Lobby|Lobby")
	void ServerSetColor(int32 ColorIndex);

	/**
	 * Sends a chat line. All: everyone. Team: the sender's team. Party: the sender's party members in this game. Whisper:
	 * one player, by user id. Prefer the online subsystem's Send Chat, which also reaches party members and friends who
	 * are not in this game.
	 */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Assetpro Lobby|Lobby")
	void ServerSendChat(EALKChatChannel Channel, const FString& Text, const FString& WhisperTargetId);

	/** Votes for a map (None takes the vote back). */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Assetpro Lobby|Lobby")
	void ServerVoteMap(FName MapId);

	/** Votes for or against a rematch (after the match). */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Assetpro Lobby|Match")
	void ServerVoteRematch(bool bWantsRematch);

	/** Watches instead of playing, or comes back to play. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Assetpro Lobby|Lobby")
	void ServerSetSpectating(bool bSpectating);

	/** Sets the name others see. The online subsystem sends it by itself when the player joins. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Assetpro Lobby|Lobby")
	void ServerSetPlayerName(const FString& Name);

	/** Tells the server which party this player is in, for the Party chat channel. Sent by the online subsystem. */
	UFUNCTION(Server, Reliable)
	void ServerSetPartyId(const FString& PartyId);

	// ---------------------------------------------------------------- Host only

	/** Removes a player, by the Player Id of their player state. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Assetpro Lobby|Lobby|Host")
	void ServerKickPlayer(int32 PlayerId);

	/** Removes a player and keeps them out (the host's ban list). */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Assetpro Lobby|Lobby|Host")
	void ServerBanPlayer(int32 PlayerId);

	/** Takes a player off the host's ban list. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Assetpro Lobby|Lobby|Host")
	void ServerUnbanPlayer(const FString& UserId);

	/** Moves a player to a team that has room. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Assetpro Lobby|Lobby|Host")
	void ServerMovePlayerToTeam(int32 PlayerId, int32 TeamId);

	/** Changes the game's settings in the lobby: name, mode, map, player limit, password, rules. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Assetpro Lobby|Lobby|Host")
	void ServerChangeMatchSettings(const FALKHostSettings& Settings);

	/** Starts the countdown. Force starts even when not everyone is ready (when the Online Config allows it). */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Assetpro Lobby|Lobby|Host")
	void ServerStartMatch(bool bForce);

	/** Stops the countdown. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Assetpro Lobby|Lobby|Host")
	void ServerCancelCountdown();

	/** Changes the map in the lobby. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Assetpro Lobby|Lobby|Host")
	void ServerChangeMap(FName MapId);

	/** Pauses or resumes the match for everyone. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Assetpro Lobby|Match|Host")
	void ServerSetPaused(bool bPaused);

	/** Ends the match (when one is running) and brings everyone back to the lobby. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Assetpro Lobby|Match|Host")
	void ServerReturnToLobby();

	// ---------------------------------------------------------------- From the server

	/** Server to this player: a chat line. */
	UFUNCTION(Client, Reliable)
	void ClientReceiveChat(const FALKChatMessage& Message);

	/** A feed line for this player only (the game state's feed goes to everyone). */
	UFUNCTION(Client, Reliable)
	void ClientReceiveFeedEvent(const FALKFeedEvent& Event);

	/** The server removes this player. Reason is one of the ALKNetReason strings; the connection closes right after. */
	UFUNCTION(Client, Reliable)
	void ClientKicked(const FString& Reason);

	/** A short message from the server for this player ("The team is full"...). */
	UFUNCTION(Client, Reliable)
	void ClientNotify(const FText& Text);

	// ---------------------------------------------------------------- Chat on this machine

	/** The chat lines received in this lobby / match, oldest first, as shown (filtered, muted players left out). */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Lobby")
	const TArray<FALKChatMessage>& GetChatHistory() const { return ChatHistory; }

	/** A chat line arrived for this player (already filtered; lines of muted and blocked players never get here). */
	UPROPERTY(BlueprintAssignable, Category = "Assetpro Lobby|Lobby")
	FALKOnChatReceived OnChatReceived;
	TMulticastDelegate<void(const FALKChatMessage& /*Message*/)> OnChatReceivedNative;

	/** A feed line arrived for this player only. */
	TMulticastDelegate<void(const FALKFeedEvent& /*Event*/)> OnFeedEventNative;

	/** The server sent this player a notice. */
	UPROPERTY(BlueprintAssignable, Category = "Assetpro Lobby|Lobby")
	FALKOnServerNotice OnServerNotice;
	TMulticastDelegate<void(const FText& /*Text*/)> OnServerNoticeNative;

	/** Server: when this player last sent chat lines, for the rate limit. */
	TArray<double> RecentChatTimes;

	/** Server: when this player last changed their name. */
	double LastNameChangeTime = -100.0;

protected:
	virtual void BeginPlay() override;

private:
	/** The ALK game mode, on the server. Null on clients and with a game mode of your own. */
	AALKNetGameModeBase* GetNetGameMode() const;

	/** Sends the things the server should know about this player once they are in: display name, party. */
	void SendLocalPlayerInfo();

	TArray<FALKChatMessage> ChatHistory;
};
