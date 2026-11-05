// Copyright 2026 CodeLikeMe. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Engine/EngineBaseTypes.h"
#include "Net/Core/Connection/NetEnums.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Lobby/ALKLobbyTypes.h"
#include "Online/ALKOnlineSaveGame.h"
#include "Online/ALKOnlineTypes.h"
#include "ALKOnlineSubsystem.generated.h"

class UALKOnlineProvider;
class UALKOnlineConfig;
class UALKPlayerNetComponent;
class APlayerController;
class UNetDriver;
class UWorld;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FALKOnOnlineStateChanged, EALKOnlineState, NewState, EALKOnlineState, OldState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FALKOnSessionsFound, const TArray<FALKSessionInfo>&, Sessions);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FALKOnConnectionError, EALKConnectionError, Error, const FText&, Title, const FText&, Message);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FALKOnMatchmakingChanged, EALKMatchmakingState, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FALKOnMatchFound, const FALKSessionInfo&, Session, float, AcceptSeconds, int32, PlayersAccepted, int32, PlayersNeeded);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FALKOnPartyChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FALKOnFriendsChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FALKOnChatMessage, const FALKChatMessage&, Message);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FALKOnInviteReceived, const FALKInvite&, Invite);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FALKOnPlayerTalking, const FString&, UserId, bool, bTalking);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FALKOnPlayerReported, const FString&, UserId, FName, ReasonId, const FString&, Comment);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FALKOnLeaderboardReceived, FName, BoardId, const TArray<FALKLeaderboardRow>&, Rows);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FALKOnOnlineListsChanged);

/**
 * The one place the multiplayer menus talk to. It owns the online provider (the backend), keeps the state of what the
 * player is doing (hosting, searching, joining, in a lobby, in a match), travels to and from games, turns network
 * failures into a ready dialog, and remembers favourites, recent servers and players, mutes, bans and rule presets.
 *
 * Get it with Get Game Instance Subsystem (ALK Online Subsystem), "Get Assetpro Online Subsystem" or
 * UALKOnlineSubsystem::Get(WorldContext) in C++.
 *
 * Every event exists twice: a Blueprint event (On State Changed) and a C++ one with "Native" at the end
 * (OnStateChangedNative) for AddUObject / AddLambda.
 *
 * Connection errors: when a connection fails or ends (timed out, full, wrong version or password, host left, kicked,
 * banned...), the subsystem cleans up, goes back to the main menu level if the game left it, and only then fires On
 * Connection Error. The error stays "pending" until a menu takes it with Consume Pending Connection Error, so a menu that
 * was not there for the event (it was created after the level loaded) still shows the dialog:
 *     on activation, and in On Connection Error:  if (Consume Pending Connection Error) -> show the dialog.
 */
UCLASS()
class ASSETPROLOBBYKIT_API UALKOnlineSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** The online subsystem of the game a world context object belongs to. Null outside a game. */
	static UALKOnlineSubsystem* Get(const UObject* WorldContextObject);

	// ---------------------------------------------------------------- Provider and state

	/** The online backend. Null when the Online Config turns online off. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Online")
	UALKOnlineProvider* GetProvider() const { return Provider; }

	/** Online is turned on in the Online Config and a provider exists. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Online")
	bool IsOnlineEnabled() const { return Provider != nullptr; }

	/** What the subsystem is doing: idle, hosting, searching, joining, in a lobby, in a match or leaving. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Online")
	EALKOnlineState GetState() const { return State; }

	/** In a lobby or a match. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Online")
	bool IsInGame() const { return State == EALKOnlineState::InLobby || State == EALKOnlineState::InMatch; }

	/** This player hosts the current game (the listen server, or the lobby leader on a dedicated server). */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Online")
	bool IsHost() const;

	/** The local player's id on the backend. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Online")
	FString GetLocalUserId() const;

	/** The name others see: the one set with Set Player Name, else the backend's. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Online")
	FString GetLocalDisplayName() const;

	/** Sets the name others see (kept between runs; empty = the backend's name). Applies in the current game right away. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Online")
	void SetPlayerName(const FString& Name);

	// ---------------------------------------------------------------- Host, find, join, leave

	/**
	 * Hosts a game: creates the session, then opens the Online Config's Lobby Level as a listen server with the server
	 * name, mode, map, player limit and rules in the URL options. False when it cannot start (busy, no lobby level).
	 */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Online")
	bool HostGame(const FALKHostSettings& Settings);

	/** Looks for games. On Sessions Found fires with the games that pass the filter (empty when none). */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Online")
	void FindGames(const FALKSessionFilter& Filter);

	/** Stops a running search. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Online")
	void CancelFindGames();

	/** The games of the last search that pass the filter it was started with. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Online")
	TArray<FALKSessionInfo> GetLastSearchResults() const;

	/** Every game of the last search, filtered and sorted another way, without searching again. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Online")
	TArray<FALKSessionInfo> GetSearchResults(const FALKSessionFilter& Filter, EALKSessionSort SortBy, bool bAscending) const;

	/**
	 * Joins a game from the browser. False (with a connection error) when it cannot: full, another version, a password
	 * protected game with no password given.
	 */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Online")
	bool JoinGame(const FALKSessionInfo& Session, const FString& Password);

	/** Joins by lobby code ("7K2-M9Q-4XD") or by address ("192.168.1.20", "192.168.1.20:7778"). */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Online")
	bool JoinByCode(const FString& CodeOrAddress, const FString& Password);

	/** Joins a favourite or recent server: the live game when the last search found it, else its remembered address. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Online")
	bool JoinBookmark(const FALKServerBookmark& Bookmark, const FString& Password);

	/** Leaves the game (a host closes it for everyone), removes the session and returns to the main menu level. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Online")
	void LeaveGame();

	/** The settings of the game this player hosts (or last hosted). */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Online")
	FALKHostSettings GetCurrentHostSettings() const { return ServerSettings; }

	/** The game this player is in, hosting or joining. Session Id is empty when there is none. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Online")
	FALKSessionInfo GetCurrentSession() const;

	/** The code others type to join the current game. Empty when the game cannot be reached by address. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Online")
	FString GetLobbyCode() const;

	/** The lobby code as it should be shown: hidden ("***-***-***") in streamer mode. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Online")
	FString GetDisplayLobbyCode() const;

	// ---------------------------------------------------------------- Quick match

	/** Joins the queue of a playlist. Watch On Matchmaking Changed and On Match Found. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Online")
	bool StartQuickMatch(FName PlaylistId);

	/** Leaves the quick match queue. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Online")
	void CancelQuickMatch();

	/** Accepts or declines the found match. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Online")
	void RespondToMatch(bool bAccept);

	/** Where the quick match is at: idle, searching, match found, accepted, joining or failed. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Online")
	EALKMatchmakingState GetMatchmakingState() const { return MatchmakingState; }

	/** The playlist of the running (or last) quick match. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Online")
	FName GetQueuePlaylist() const { return QueuePlaylist; }

	/** Seconds since the quick match started. 0 when not in the queue. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Online")
	float GetQueueElapsedSeconds() const;

	/** The backend's estimate for a playlist, in seconds. Negative = unknown. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Online")
	float GetEstimatedWaitSeconds(FName PlaylistId) const;

	/** The match that was found (while Match Found or Accepted). */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Online")
	FALKSessionInfo GetFoundMatch() const { return FoundMatch; }

	/** Seconds left to accept the found match. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Online")
	float GetMatchAcceptRemaining() const;

	/** How many players accepted the found match so far. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Online")
	int32 GetMatchPlayersAccepted() const { return MatchPlayersAccepted; }

	/** How many players have to accept the found match. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Online")
	int32 GetMatchPlayersNeeded() const { return MatchPlayersNeeded; }

	/** Why the last quick match ended in Failed: Session Not Found (no game), Timed Out (not accepted in time)... */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Online")
	EALKConnectionError GetMatchmakingError() const { return LastMatchmakingError; }

	// ---------------------------------------------------------------- Connection errors and reconnect

	/**
	 * Takes the pending connection error, if there is one: call it when a menu becomes visible and in On Connection
	 * Error, and show the dialog when it returns true. The error is cleared, so the dialog shows once.
	 */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Online")
	bool ConsumePendingConnectionError(EALKConnectionError& OutError, FText& OutTitle, FText& OutMessage);

	/** A connection error is waiting to be shown. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Online")
	bool HasPendingConnectionError() const { return PendingError != EALKConnectionError::None; }

	/**
	 * Raises a connection error exactly as a real one would be: the game is left, the main menu comes back and the error
	 * is announced. For trying the dialogs (also the console command ALK.Online.Error).
	 */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Online")
	void SimulateConnectionError(EALKConnectionError Error);

	/** The game is on its way back to the main menu level (after Leave Game or a lost connection). */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Online")
	bool IsReturningToMenu() const { return bReturnToMenuPending; }

	/** A match this player dropped out of can still be rejoined (within the Online Config's Reconnect Window). */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Online")
	bool CanRejoinMatch() const;

	/** The name of the game Rejoin Match goes back to. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Online")
	FString GetRejoinServerName() const;

	/** Goes back to the match this player dropped out of. Give the password again for a protected game after a restart. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Online")
	bool RejoinMatch(const FString& Password = TEXT(""));

	/** Forgets the match to rejoin ("No thanks"). */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Online")
	void ClearRejoin();

	// ---------------------------------------------------------------- Party

	/** The party, the local player included. One member = not in a party. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Online")
	TArray<FALKPartyMember> GetPartyMembers() const;

	/** The party has more members than the local player. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Online")
	bool IsInParty() const;

	/** The local player leads the party (also true when not in one). */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Online")
	bool IsPartyLeader() const;

	/** How many players a party holds. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Online")
	int32 GetPartyMaxSize() const;

	/** What tells this party from others: its leader's user id. Empty when not in a party. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Online")
	FString GetPartyId() const;

	/** Invites a player to the local player's party. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Online")
	void InviteToParty(const FString& UserId);

	/** Removes a member from the party (the party leader only). */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Online")
	void KickFromParty(const FString& UserId);

	/** Makes another member the party leader. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Online")
	void PromotePartyMember(const FString& UserId);

	/** Leaves the party. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Online")
	void LeaveParty();

	/** Marks the local player ready or not ready in the party. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Online")
	void SetPartyReady(bool bReady);

	// ---------------------------------------------------------------- Friends and invites

	/** Refreshes the friends list. On Friends Changed fires when it is there. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Online")
	void QueryFriends();

	/** The friends list as last read (call Query Friends to refresh it). */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Online")
	TArray<FALKFriend> GetFriends() const;

	/** Players met in lobbies and matches, newest first: the ones this machine remembers, then the backend's. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Online")
	TArray<FALKFriend> GetRecentPlayers() const;

	/** Invites a friend to the current game. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Online")
	void InviteFriendToGame(const FString& UserId);

	/** Joins the game a friend is in. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Online")
	void JoinFriend(const FString& UserId);

	/** Opens the platform's own invite window, when it has one. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Online")
	void ShowInviteOverlay();

	/** Accepts or declines an invite from On Invite Received. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Online")
	void RespondToInvite(const FALKInvite& Invite, bool bAccept);

	// ---------------------------------------------------------------- Profiles, reports, mutes

	/** A player's profile card from the backend. False when it has none. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Online")
	bool GetPlayerProfile(const FString& UserId, FALKPlayerProfile& OutProfile) const;

	/** Reports a player: fires On Player Reported (handle it in Blueprint or C++) and tells the backend. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Online")
	void ReportPlayer(const FString& UserId, FName ReasonId, const FString& Comment);

	/** Hides a player's chat and silences their voice. Kept between runs. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Online")
	void SetPlayerMuted(const FString& UserId, bool bMuted);

	/** Whether the player is muted. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Online")
	bool IsPlayerMuted(const FString& UserId) const;

	/** Blocks a player: muted, and their invites and whispers are dropped. Kept between runs. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Online")
	void SetPlayerBlocked(const FString& UserId, bool bBlocked);

	/** Whether the player is blocked. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Online")
	bool IsPlayerBlocked(const FString& UserId) const;

	/** The user ids of every muted player. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Online")
	TArray<FString> GetMutedPlayers() const;

	/** The user ids of every blocked player. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Online")
	TArray<FString> GetBlockedPlayers() const;

	// ---------------------------------------------------------------- Chat

	/**
	 * Sends a chat line. All and Team go to the lobby or match; Party to the party (the backend's, or the party members in
	 * the same game); Whisper to one player (through the game when they are in it, else the backend). False when it could
	 * not be sent (text chat off, empty, not in a game).
	 */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Online")
	bool SendChat(EALKChatChannel Channel, const FString& Text, const FString& WhisperTargetId);

	/**
	 * The chat lines kept on this machine, oldest first, up to the Online Config's Chat History Size. Hosting or joining a
	 * game forgets the last game's lines (All, Team and System); Party and Whisper lines stay.
	 */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Online")
	const TArray<FALKChatMessage>& GetChatHistory() const { return ChatHistory; }

	/** Forgets the chat lines kept so far. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Online")
	void ClearChatHistory();

	/** Adds a System line to the chat on this machine only. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Online")
	void AddSystemMessage(const FText& Text);

	/** A chat line as it should be shown: filtered by the provider when the chat filter is on. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Online")
	FString FilterChatText(const FString& Text) const;

	/**
	 * A chat line arrived (the player net component and the provider call this). Applies mutes, blocks, the text chat
	 * switch and the filter, keeps the line and fires On Chat Message. False when the line was dropped. Out Shown gets the
	 * line as it is shown (filtered).
	 */
	bool ReceiveChatMessage(const FALKChatMessage& Message, FALKChatMessage* OutShown = nullptr);

	// ---------------------------------------------------------------- Favourites, recent servers, presets

	/** Whether the server is one of the player's favourites. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Online")
	bool IsFavourite(const FALKSessionInfo& Session) const;

	/** Marks or unmarks a server as a favourite (kept between runs). */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Online")
	void SetFavourite(const FALKSessionInfo& Session, bool bFavourite);

	/** The favourite servers, as last seen. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Online")
	TArray<FALKServerBookmark> GetFavouriteServers() const;

	/** Newest first. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Online")
	TArray<FALKServerBookmark> GetRecentServers() const;

	/** Forgets the recently played servers. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Online")
	void ClearRecentServers();

	/** The rule sets the player saved. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Rules")
	TArray<FALKRulePreset> GetRulePresets() const;

	/** Saves rules under a name, replacing a preset of the same name. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Rules")
	void SaveRulePreset(const FString& Name, FName ModeId, const FALKGameRules& Rules);

	/** Removes a saved rule set. False when there is none of that name. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Rules")
	bool DeleteRulePreset(const FString& Name);

	/** What the player last hosted with, or the Online Config's defaults the first time. For the Host Game screen. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Online")
	FALKHostSettings GetLastHostSettings() const;

	// ---------------------------------------------------------------- Leaderboards

	/** Reads a leaderboard. On Leaderboard Received fires with its rows. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Online")
	void QueryLeaderboard(FName BoardId, int32 MaxRows = 20);

	/** Sends the local player's score to a leaderboard. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Online")
	void SubmitScore(FName BoardId, double Score);

	// ---------------------------------------------------------------- Player preferences (the settings menu sets these)

	/** Streamer mode hides other players' names ("Player 3") and the lobby code. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Online")
	void SetStreamerMode(bool bEnabled);

	/** Streamer mode is on. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Online")
	bool IsStreamerMode() const { return bStreamerMode; }

	/** Turns the chat filter on or off (it masks the Online Config's Filtered Words, or whatever your provider filters). */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Online")
	void SetChatFilterEnabled(bool bEnabled);

	/** The chat filter is on. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Online")
	bool IsChatFilterEnabled() const { return bChatFilterEnabled; }

	/** Off: no chat lines are shown or sent (system lines still are). */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Online")
	void SetTextChatEnabled(bool bEnabled);

	/** Text chat is on. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Online")
	bool IsTextChatEnabled() const { return bTextChatEnabled; }

	/** Shows or hides ping numbers and the net stats overlay. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Online")
	void SetShowPing(bool bEnabled);

	/** Whether the net stats overlay and ping columns should be shown. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Online")
	bool IsShowPing() const { return bShowPing; }

	/** Searches and hosts in this region (an Id from Get Regions; empty = automatic). */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Online")
	void SetRegion(const FString& RegionId);

	/** The chosen region's Id; empty = automatic. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Online")
	FString GetRegion() const { return Region; }

	/** The regions the backend offers. Empty hides the setting. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Online")
	TArray<FALKOnlineOption> GetRegions() const;

	/** Allows or refuses games with players of other platforms (a hook for your provider). */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Online")
	void SetCrossplayEnabled(bool bEnabled);

	/** Crossplay is allowed. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Online")
	bool IsCrossplayEnabled() const { return bCrossplayEnabled; }

	/** Off, push to talk or open microphone. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Online")
	void SetVoiceMode(EALKVoiceMode Mode);

	/** The voice mode in use. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Online")
	EALKVoiceMode GetVoiceMode() const { return VoiceMode; }

	/** Call with true while the push-to-talk key is held. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Online")
	void SetPushToTalkActive(bool bActive);

	/** The microphones the backend offers. Empty hides the setting. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Online")
	TArray<FALKOnlineOption> GetAudioInputDevices() const;

	/** The speakers and headsets the backend offers for voice. Empty hides the setting. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Online")
	TArray<FALKOnlineOption> GetAudioOutputDevices() const;

	/** Uses this microphone (an Id from Get Audio Input Devices). */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Online")
	void SetAudioInputDevice(const FString& DeviceId);

	/** Plays voices on this device (an Id from Get Audio Output Devices). */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Online")
	void SetAudioOutputDevice(const FString& DeviceId);

	/** 0 to 1. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Online")
	void SetMicVolume(float Volume);

	/** How loud other players' voices are, 0 to 1. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Online")
	void SetVoiceVolume(float Volume);

	/**
	 * The name to show for another player: their real name, or "Player N" in streamer mode (N = Stable Index + 1; use the
	 * player state's Get Stable Index). The local player's own name is never hidden: pass it straight to the screen.
	 */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Online")
	FString GetDisplayName(const FString& RealName, int32 StableIndex) const;

	/** Ping, packet loss and traffic of the current connection. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Online")
	FALKNetStats GetNetStats() const;

	// ---------------------------------------------------------------- Host tools: ban list

	/** Everyone on this machine's ban list. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Online")
	TArray<FALKBanEntry> GetBanList() const;

	/** Adds a player to this machine's ban list (kept between runs). The ALK game modes refuse banned players in Pre Login. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Online")
	void AddBan(const FString& UserId, const FString& DisplayName, const FString& Address);

	/** Takes a player off the ban list. False when they were not on it. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Online")
	bool RemoveBan(const FString& UserId);

	/** True when the user id, or the address (the port is ignored), is on the ban list. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Online")
	bool IsBanned(const FString& UserId, const FString& Address) const;

	// ---------------------------------------------------------------- For game modes and the lobby classes

	/** Server: the settings of the running game, password included. Game modes read and update them here. */
	const FALKHostSettings& GetServerSettings() const { return ServerSettings; }

	/** Server: replaces the running game's settings and advertises them with the session. */
	void SetServerSettings(const FALKHostSettings& Settings, bool bUpdateSession);

	/**
	 * Dedicated server: creates the session for a game that is already running in this process (no level is opened).
	 * The ALK lobby game mode calls it when a dedicated server starts in the lobby level.
	 */
	void RegisterDedicatedServer(const FALKHostSettings& Settings);

	/** Server: the match starts / ended (the provider marks the session). */
	void NotifyServerMatchStarted();
	void NotifyServerMatchEnded();

	/** Server: what to give players back after a level change that is not seamless. */
	void StoreRoster(const TArray<FALKRosterEntry>& Entries) { Roster = Entries; }
	const TArray<FALKRosterEntry>& GetRoster() const { return Roster; }
	bool FindRosterEntry(const FString& UserId, FALKRosterEntry& OutEntry) const;

	/** The ALK game state reports its phase (every machine): keeps In Lobby / In Match up to date. */
	void NotifyLobbyPhase(EALKLobbyPhase Phase);

	/** The server told this client it is being removed, with an ALKNetReason. */
	void NotifyKicked(const FString& Reason);

	/** A player was seen in the lobby or match: remembered for Recent Players. */
	void NoteRecentPlayer(const FString& UserId, const FString& DisplayName);

	/** The local player's net component in the current world, or null. */
	UALKPlayerNetComponent* GetLocalNetComponent() const;

	// ---------------------------------------------------------------- Events

	/** The state changed: idle, hosting, searching, joining, in a lobby, in a match or leaving. */
	UPROPERTY(BlueprintAssignable, Category = "Assetpro Lobby|Online")
	FALKOnOnlineStateChanged OnStateChanged;
	TMulticastDelegate<void(EALKOnlineState /*NewState*/, EALKOnlineState /*OldState*/)> OnStateChangedNative;

	/** A search finished: the games that pass its filter. */
	UPROPERTY(BlueprintAssignable, Category = "Assetpro Lobby|Online")
	FALKOnSessionsFound OnSessionsFound;
	TMulticastDelegate<void(const TArray<FALKSessionInfo>& /*Sessions*/)> OnSessionsFoundNative;

	/** A connection failed or ended and a menu can show it now. Take it with Consume Pending Connection Error. */
	UPROPERTY(BlueprintAssignable, Category = "Assetpro Lobby|Online")
	FALKOnConnectionError OnConnectionError;
	TMulticastDelegate<void(EALKConnectionError /*Error*/, const FText& /*Title*/, const FText& /*Message*/)> OnConnectionErrorNative;

	/** The quick match moved on: its state, or how many players accepted. */
	UPROPERTY(BlueprintAssignable, Category = "Assetpro Lobby|Online")
	FALKOnMatchmakingChanged OnMatchmakingChanged;
	TMulticastDelegate<void(EALKMatchmakingState /*NewState*/)> OnMatchmakingChangedNative;

	/** A quick match found a game: accept or decline with Respond To Match before Accept Seconds run out. */
	UPROPERTY(BlueprintAssignable, Category = "Assetpro Lobby|Online")
	FALKOnMatchFound OnMatchFound;
	TMulticastDelegate<void(const FALKSessionInfo& /*Session*/, float /*AcceptSeconds*/, int32 /*Accepted*/, int32 /*Needed*/)> OnMatchFoundNative;

	/** The party's members, leader or ready flags changed. */
	UPROPERTY(BlueprintAssignable, Category = "Assetpro Lobby|Online")
	FALKOnPartyChanged OnPartyChanged;
	FSimpleMulticastDelegate OnPartyChangedNative;

	/** The friends list or someone's presence changed. */
	UPROPERTY(BlueprintAssignable, Category = "Assetpro Lobby|Online")
	FALKOnFriendsChanged OnFriendsChanged;
	FSimpleMulticastDelegate OnFriendsChangedNative;

	/** A chat line to show: lobby and match chat, party chat, whispers and system lines, already filtered. */
	UPROPERTY(BlueprintAssignable, Category = "Assetpro Lobby|Online")
	FALKOnChatMessage OnChatMessage;
	TMulticastDelegate<void(const FALKChatMessage& /*Message*/)> OnChatMessageNative;

	/** A party or game invite arrived. Answer with Respond To Invite. */
	UPROPERTY(BlueprintAssignable, Category = "Assetpro Lobby|Online")
	FALKOnInviteReceived OnInviteReceived;
	TMulticastDelegate<void(const FALKInvite& /*Invite*/)> OnInviteReceivedNative;

	/** A player started or stopped talking (muted players are left out). */
	UPROPERTY(BlueprintAssignable, Category = "Assetpro Lobby|Online")
	FALKOnPlayerTalking OnPlayerTalking;
	TMulticastDelegate<void(const FString& /*UserId*/, bool /*bTalking*/)> OnPlayerTalkingNative;

	/** The player sent a report from the report dialog. Send it to your moderation tools here. */
	UPROPERTY(BlueprintAssignable, Category = "Assetpro Lobby|Online")
	FALKOnPlayerReported OnPlayerReported;
	TMulticastDelegate<void(const FString& /*UserId*/, FName /*ReasonId*/, const FString& /*Comment*/)> OnPlayerReportedNative;

	/** A leaderboard's rows arrived, best first. */
	UPROPERTY(BlueprintAssignable, Category = "Assetpro Lobby|Online")
	FALKOnLeaderboardReceived OnLeaderboardReceived;
	TMulticastDelegate<void(FName /*BoardId*/, const TArray<FALKLeaderboardRow>& /*Rows*/)> OnLeaderboardReceivedNative;

	/** Favourites, recent servers, recent players, mutes, blocks, bans or rule presets changed. */
	UPROPERTY(BlueprintAssignable, Category = "Assetpro Lobby|Online")
	FALKOnOnlineListsChanged OnListsChanged;
	FSimpleMulticastDelegate OnListsChangedNative;

private:
	void SetState(EALKOnlineState NewState);
	void SetMatchmakingState(EALKMatchmakingState NewState);
	void BroadcastMatchmakingChanged();

	// Provider answers
	void HandleHostComplete(bool bSuccess, EALKConnectionError Error);
	void HandleFindComplete(bool bSuccess, const TArray<FALKSessionInfo>& Results);
	void HandleJoinComplete(bool bSuccess, EALKConnectionError Error, const FString& ConnectString);
	void HandleSessionDestroyed();
	void HandleJoinRequested(const FALKSessionInfo& Session);
	void HandleMatchFound(const FALKSessionInfo& Session, float AcceptSeconds, int32 Accepted, int32 Needed);
	void HandleMatchAcceptProgress(int32 Accepted, int32 Needed);
	void HandleMatchmakingFailed(EALKConnectionError Error);
	void HandleMatchReady(const FString& ConnectString, const FALKSessionInfo& Session);
	void HandleMatchHostRequested(const FALKHostSettings& Settings);
	void HandlePartyChanged();
	void HandlePartyInvite(const FString& FromUserId, const FString& FromName);
	void HandlePartyChat(const FString& FromUserId, const FString& FromName, const FString& Text);
	void HandleFriendsChanged();
	void HandleGameInvite(const FString& FromUserId, const FString& FromName, const FALKSessionInfo& Session);
	void HandleWhisper(const FString& FromUserId, const FString& FromName, const FString& ToUserId, const FString& Text);
	void HandlePlayerTalking(const FString& UserId, bool bTalking);
	void HandleLeaderboard(FName BoardId, const TArray<FALKLeaderboardRow>& Rows);

	// Engine events
	void HandleNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString);
	void HandleTravelFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& ErrorString);
	void HandlePreClientTravel(const FString& PendingURL, ETravelType TravelType, bool bIsSeamlessTravel);
	void HandlePostLoadMap(UWorld* World);
	bool Tick(float DeltaTime);

	/** Opens the lobby level as a listen server with the server settings in the URL. */
	bool OpenLobbyAsHost();
	/** Travels to a server address, with this client's version and password in the URL. */
	bool TravelToServer(const FString& Address);
	/** Records the error and brings the game back to a menu; the error is announced once a menu can show it. */
	void FailWithError(EALKConnectionError Error, bool bReturnToMenu);
	void RequestMenuTravel();
	FString GetMenuLevelPath() const;
	bool IsMenuWorld(const UWorld* World) const;
	bool IsOurWorld(const UWorld* World, const UNetDriver* NetDriver) const;
	bool CanStartOnlineAction() const;
	void ResetGameState();
	void RememberJoinedServer();
	void TouchRejoinInfo(bool bSaveNow);
	FString MakeHostLobbyCode() const;

	void LoadSave();
	void SaveNow();
	static FString GetSaveSlotName();

	UWorld* GetGameWorld() const;
	APlayerController* GetLocalController() const;
	const UALKOnlineConfig* GetConfig() const;

	UPROPERTY(Transient)
	TObjectPtr<UALKOnlineProvider> Provider;

	UPROPERTY(Transient)
	TObjectPtr<UALKOnlineSaveGame> Save;

	EALKOnlineState State = EALKOnlineState::Idle;
	EALKMatchmakingState MatchmakingState = EALKMatchmakingState::Idle;
	EALKConnectionError LastMatchmakingError = EALKConnectionError::None;
	/** When the state was entered: a host or join the provider never answers is given up. */
	double StateEnterTime = 0.0;

	/** Host: the game's settings, password included. Also what the server-side game modes read. */
	FALKHostSettings ServerSettings;
	bool bIsHost = false;
	/** Hosting a game that already runs in this process (a dedicated server): no lobby level to open. */
	bool bDedicatedHost = false;

	/** Client: the game being joined or played. */
	FALKSessionInfo JoinedSession;
	FString JoinedConnectString;
	FString JoinPassword;
	/** The password of the last game joined in this run, for Rejoin Match. Never written to disk. */
	FString LastJoinPassword;
	bool bRejoining = false;
	/** The joined game was written to the recent servers and the rejoin info. */
	bool bJoinRemembered = false;
	/** A game to join once the current one is left (an accepted invite while playing). */
	FALKSessionInfo QueuedJoin;
	bool bHasQueuedJoin = false;

	/** The last search: everything found, and the filter it was asked with. */
	TArray<FALKSessionInfo> SearchResults;
	FALKSessionFilter SearchFilter;

	FName QueuePlaylist;
	double QueueStartTime = 0.0;
	FALKSessionInfo FoundMatch;
	double MatchAcceptDeadline = 0.0;
	int32 MatchPlayersAccepted = 0;
	int32 MatchPlayersNeeded = 0;

	EALKConnectionError PendingError = EALKConnectionError::None;
	/** The pending error still has to be announced with On Connection Error. */
	bool bAnnounceErrorPending = false;
	/** Why the server said this client is being removed (kicked, banned, host left), until the connection closes. */
	EALKConnectionError KickError = EALKConnectionError::None;
	double KickDeadline = 0.0;

	bool bReturnToMenuPending = false;
	double LastMenuTravelRequest = 0.0;
	double LastMapLoadTime = 0.0;
	double LastRejoinTouch = 0.0;
	/** The save has changes that are not on disk yet (written a moment later, so a burst of changes is one write). */
	bool bSaveDirty = false;
	double LastSaveTime = 0.0;

	TArray<FALKChatMessage> ChatHistory;
	TArray<FALKRosterEntry> Roster;

	bool bStreamerMode = false;
	bool bChatFilterEnabled = true;
	bool bTextChatEnabled = true;
	bool bShowPing = true;
	bool bCrossplayEnabled = true;
	FString Region;
	EALKVoiceMode VoiceMode = EALKVoiceMode::Off;

	FDelegateHandle NetworkFailureHandle;
	FDelegateHandle TravelFailureHandle;
	FDelegateHandle PreClientTravelHandle;
	FDelegateHandle PostLoadMapHandle;
	FTSTicker::FDelegateHandle TickerHandle;
};
