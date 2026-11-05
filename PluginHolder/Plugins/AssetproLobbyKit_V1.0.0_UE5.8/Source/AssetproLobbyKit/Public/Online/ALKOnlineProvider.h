// Copyright 2026 CodeLikeMe. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Online/ALKOnlineTypes.h"
#include "ALKOnlineProvider.generated.h"

class UGameInstance;
class UALKOnlineConfig;

/**
 * The online backend behind the multiplayer menus. The online subsystem owns one provider and calls it for everything
 * that needs a service: sessions, matchmaking, party, friends, profiles, voice, leaderboards.
 *
 * To plug in your own backend (a platform service, a plugin, your servers), make a child of this class in C++ or
 * Blueprint, override the operations you support and select the class in the Online Config. Every operation has a safe
 * default: it does nothing, or reports a clean failure.
 *
 * Operations that take time answer through the Notify functions: Host Session ends with Notify Host Complete, Find
 * Sessions with Notify Find Complete, Join Session with Notify Join Complete, and so on. Call the matching Notify once
 * for every request, also when it fails.
 *
 * Ready-made children: ALK Sessions Online Provider (engine Online Subsystem sessions; LAN out of the box) and ALK
 * Sample Online Provider (the same, plus made-up friends, party and matchmaking to try every screen).
 */
UCLASS(Abstract, Blueprintable, BlueprintType)
class ASSETPROLOBBYKIT_API UALKOnlineProvider : public UObject
{
	GENERATED_BODY()

public:
	virtual UWorld* GetWorld() const override;

	/** The game instance this provider serves. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Online")
	UGameInstance* GetGameInstance() const { return GameInstance.Get(); }

	/** The active Online Config. Never null. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Online")
	const UALKOnlineConfig* GetOnlineConfig() const;

	// ---------------------------------------------------------------- Lifecycle and identity

	/** Called once when the game starts. Log in and bind your backend's events here. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Assetpro Lobby|Online|Provider")
	void InitializeProvider(UGameInstance* InGameInstance);
	virtual void InitializeProvider_Implementation(UGameInstance* InGameInstance);

	/** Called when the game ends. Unbind everything. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Assetpro Lobby|Online|Provider")
	void ShutdownProvider();
	virtual void ShutdownProvider_Implementation();

	/** The local player's id on the backend. Chat, mutes and bans go by it. */
	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "Assetpro Lobby|Online|Provider")
	FString GetLocalUserId() const;
	virtual FString GetLocalUserId_Implementation() const;

	/** The local player's name on the backend. */
	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "Assetpro Lobby|Online|Provider")
	FString GetLocalDisplayName() const;
	virtual FString GetLocalDisplayName_Implementation() const;

	/** The backend can be used now (logged in, service reachable). */
	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "Assetpro Lobby|Online|Provider")
	bool IsOnline() const;
	virtual bool IsOnline_Implementation() const;

	// ---------------------------------------------------------------- Sessions

	/** Creates the session for a game this player hosts. Answer with Notify Host Complete. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Assetpro Lobby|Online|Provider")
	void HostSession(const FALKHostSettings& Settings);
	virtual void HostSession_Implementation(const FALKHostSettings& Settings);

	/** Looks for games. Answer with Notify Find Complete (every game found; the subsystem applies the filter again). */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Assetpro Lobby|Online|Provider")
	void FindSessions(const FALKSessionFilter& Filter);
	virtual void FindSessions_Implementation(const FALKSessionFilter& Filter);

	/** Stops a running search. No answer is expected. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Assetpro Lobby|Online|Provider")
	void CancelFindSessions();
	virtual void CancelFindSessions_Implementation();

	/** Joins a session found earlier. Answer with Notify Join Complete and the address to travel to. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Assetpro Lobby|Online|Provider")
	void JoinSession(const FALKSessionInfo& Session, const FString& Password);
	virtual void JoinSession_Implementation(const FALKSessionInfo& Session, const FString& Password);

	/** Leaves (client) or destroys (host) the current session. Answer with Notify Session Destroyed. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Assetpro Lobby|Online|Provider")
	void LeaveSession();
	virtual void LeaveSession_Implementation();

	/** The match begins: mark the session as in progress. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Assetpro Lobby|Online|Provider")
	void StartSession();
	virtual void StartSession_Implementation();

	/** The match ended: mark the session as open again. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Assetpro Lobby|Online|Provider")
	void EndSession();
	virtual void EndSession_Implementation();

	/** The host changed the game's settings: advertise the new ones. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Assetpro Lobby|Online|Provider")
	void UpdateSession(const FALKHostSettings& Settings);
	virtual void UpdateSession_Implementation(const FALKHostSettings& Settings);

	/** The session this player is in. False when there is none. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Assetpro Lobby|Online|Provider")
	bool GetCurrentSession(FALKSessionInfo& OutSession) const;
	virtual bool GetCurrentSession_Implementation(FALKSessionInfo& OutSession) const;

	/** The address of a found session, when the backend knows it without joining (kept with favourites). */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Assetpro Lobby|Online|Provider")
	bool GetSessionConnectString(const FALKSessionInfo& Session, FString& OutConnectString) const;
	virtual bool GetSessionConnectString_Implementation(const FALKSessionInfo& Session, FString& OutConnectString) const;

	/** A code for the hosted game from your backend. Empty (the default) = the subsystem makes one from the host's address. */
	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "Assetpro Lobby|Online|Provider")
	FString GetLobbyCode() const;
	virtual FString GetLobbyCode_Implementation() const;

	/**
	 * Joins by a code from your backend. Return true when you take the code (then answer with Notify Join Complete);
	 * false (the default) lets the subsystem read it as an address code.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Assetpro Lobby|Online|Provider")
	bool JoinByCode(const FString& Code, const FString& Password);
	virtual bool JoinByCode_Implementation(const FString& Code, const FString& Password);

	// ---------------------------------------------------------------- Matchmaking

	/**
	 * Starts looking for a match in a playlist. Then, in order: Notify Match Found (optional accept step), Notify Match
	 * Accept Progress, and one of Notify Match Ready, Notify Match Host Requested or Notify Matchmaking Failed.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Assetpro Lobby|Online|Provider")
	void StartMatchmaking(FName PlaylistId);
	virtual void StartMatchmaking_Implementation(FName PlaylistId);

	/** Leaves the queue. Answer with Notify Matchmaking Failed (Cancelled). */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Assetpro Lobby|Online|Provider")
	void CancelMatchmaking();
	virtual void CancelMatchmaking_Implementation();

	/** The expected wait for a playlist, in seconds. Negative (the default) = unknown. */
	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "Assetpro Lobby|Online|Provider")
	float GetEstimatedWaitSeconds(FName PlaylistId) const;
	virtual float GetEstimatedWaitSeconds_Implementation(FName PlaylistId) const;

	/** The player accepted or declined the found match. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Assetpro Lobby|Online|Provider")
	void RespondToMatch(bool bAccept);
	virtual void RespondToMatch_Implementation(bool bAccept);

	// ---------------------------------------------------------------- Party

	/** The party, the local player included. Empty or one member = not in a party. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Assetpro Lobby|Online|Provider")
	TArray<FALKPartyMember> GetPartyMembers() const;
	virtual TArray<FALKPartyMember> GetPartyMembers_Implementation() const;

	/** Invites a player to the local player's party. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Assetpro Lobby|Online|Provider")
	void InviteToParty(const FString& UserId);
	virtual void InviteToParty_Implementation(const FString& UserId);

	/** The player answered a party invite (Notify Party Invite Received). */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Assetpro Lobby|Online|Provider")
	void RespondToPartyInvite(const FString& FromUserId, bool bAccept);
	virtual void RespondToPartyInvite_Implementation(const FString& FromUserId, bool bAccept);

	/** Removes a member from the party (the party leader only). */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Assetpro Lobby|Online|Provider")
	void KickFromParty(const FString& UserId);
	virtual void KickFromParty_Implementation(const FString& UserId);

	/** Makes another member the party leader. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Assetpro Lobby|Online|Provider")
	void PromotePartyMember(const FString& UserId);
	virtual void PromotePartyMember_Implementation(const FString& UserId);

	/** Leaves the party. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Assetpro Lobby|Online|Provider")
	void LeaveParty();
	virtual void LeaveParty_Implementation();

	/** Marks the local player ready or not ready in the party. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Assetpro Lobby|Online|Provider")
	void SetPartyReady(bool bReady);
	virtual void SetPartyReady_Implementation(bool bReady);

	/** Sends a line to the party. Echo it back with Notify Party Chat Received so the sender sees it too. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Assetpro Lobby|Online|Provider")
	void SendPartyChat(const FString& Text);
	virtual void SendPartyChat_Implementation(const FString& Text);

	/** How many players a party holds. */
	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "Assetpro Lobby|Online|Provider")
	int32 GetPartyMaxSize() const;
	virtual int32 GetPartyMaxSize_Implementation() const;

	// ---------------------------------------------------------------- Friends

	/** Asks the backend for the friends list. Answer with Notify Friends Changed. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Assetpro Lobby|Online|Provider")
	void QueryFriends();
	virtual void QueryFriends_Implementation();

	/** The friends list as last read. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Assetpro Lobby|Online|Provider")
	TArray<FALKFriend> GetFriends() const;
	virtual TArray<FALKFriend> GetFriends_Implementation() const;

	/** Recently met players the backend keeps. The subsystem adds the ones it remembers itself. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Assetpro Lobby|Online|Provider")
	TArray<FALKFriend> GetRecentPlayers() const;
	virtual TArray<FALKFriend> GetRecentPlayers_Implementation() const;

	/** Invites a friend to the game the local player is in. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Assetpro Lobby|Online|Provider")
	void InviteFriendToGame(const FString& UserId);
	virtual void InviteFriendToGame_Implementation(const FString& UserId);

	/** Joins the game a friend is in. Answer with Notify Join Requested (the session to join), or nothing when there is none. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Assetpro Lobby|Online|Provider")
	void JoinFriend(const FString& UserId);
	virtual void JoinFriend_Implementation(const FString& UserId);

	/** The player answered a game invite (Notify Game Invite Received). On accept, answer with Notify Join Requested. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Assetpro Lobby|Online|Provider")
	void RespondToGameInvite(const FString& FromUserId, bool bAccept);
	virtual void RespondToGameInvite_Implementation(const FString& FromUserId, bool bAccept);

	/** Opens the platform's own invite window, when it has one. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Assetpro Lobby|Online|Provider")
	void ShowInviteOverlay();
	virtual void ShowInviteOverlay_Implementation();

	/** Sends a private line to a player who is not in the same game (in-game whispers go through the game server). */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Assetpro Lobby|Online|Provider")
	void SendWhisper(const FString& UserId, const FString& Text);
	virtual void SendWhisper_Implementation(const FString& UserId, const FString& Text);

	// ---------------------------------------------------------------- Profile and social

	/** A player's profile card. False when the backend has none. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Assetpro Lobby|Online|Provider")
	bool GetPlayerProfile(const FString& UserId, FALKPlayerProfile& OutProfile) const;
	virtual bool GetPlayerProfile_Implementation(const FString& UserId, FALKPlayerProfile& OutProfile) const;

	/** Forwards a player report to your backend. The subsystem's On Player Reported event fires either way. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Assetpro Lobby|Online|Provider")
	void ReportPlayer(const FString& UserId, FName ReasonId, const FString& Comment);
	virtual void ReportPlayer_Implementation(const FString& UserId, FName ReasonId, const FString& Comment);

	/** Mutes a player's voice. Text chat is muted by the subsystem. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Assetpro Lobby|Online|Provider")
	void SetPlayerMuted(const FString& UserId, bool bMuted);
	virtual void SetPlayerMuted_Implementation(const FString& UserId, bool bMuted);

	/** Blocks or unblocks a player on the backend. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Assetpro Lobby|Online|Provider")
	void SetPlayerBlocked(const FString& UserId, bool bBlocked);
	virtual void SetPlayerBlocked_Implementation(const FString& UserId, bool bBlocked);

	// ---------------------------------------------------------------- Voice

	/** Off, push to talk or open microphone. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Assetpro Lobby|Online|Provider")
	void SetVoiceMode(EALKVoiceMode Mode);
	virtual void SetVoiceMode_Implementation(EALKVoiceMode Mode);

	/** The push-to-talk key went down or up. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Assetpro Lobby|Online|Provider")
	void SetPushToTalkActive(bool bActive);
	virtual void SetPushToTalkActive_Implementation(bool bActive);

	/** The microphones to choose from. Empty hides the setting. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Assetpro Lobby|Online|Provider")
	TArray<FALKOnlineOption> GetAudioInputDevices() const;
	virtual TArray<FALKOnlineOption> GetAudioInputDevices_Implementation() const;

	/** The speakers / headsets to choose from for voice. Empty hides the setting. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Assetpro Lobby|Online|Provider")
	TArray<FALKOnlineOption> GetAudioOutputDevices() const;
	virtual TArray<FALKOnlineOption> GetAudioOutputDevices_Implementation() const;

	/** Uses this microphone (an Id from Get Audio Input Devices). */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Assetpro Lobby|Online|Provider")
	void SetAudioInputDevice(const FString& DeviceId);
	virtual void SetAudioInputDevice_Implementation(const FString& DeviceId);

	/** Plays voices on this device (an Id from Get Audio Output Devices). */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Assetpro Lobby|Online|Provider")
	void SetAudioOutputDevice(const FString& DeviceId);
	virtual void SetAudioOutputDevice_Implementation(const FString& DeviceId);

	/** 0 to 1. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Assetpro Lobby|Online|Provider")
	void SetMicVolume(float Volume);
	virtual void SetMicVolume_Implementation(float Volume);

	/** How loud other players' voices are, 0 to 1. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Assetpro Lobby|Online|Provider")
	void SetVoiceVolume(float Volume);
	virtual void SetVoiceVolume_Implementation(float Volume);

	// ---------------------------------------------------------------- Reconnect

	/** True when the backend knows a match the player dropped out of and can go back to. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Assetpro Lobby|Online|Provider")
	bool GetRejoinInfo(FALKSessionInfo& OutSession) const;
	virtual bool GetRejoinInfo_Implementation(FALKSessionInfo& OutSession) const;

	/** Rejoins that match. Answer with Notify Join Complete. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Assetpro Lobby|Online|Provider")
	void RejoinSession();
	virtual void RejoinSession_Implementation();

	// ---------------------------------------------------------------- Leaderboards and achievements

	/** Reads the top rows of a leaderboard. Answer with Notify Leaderboard Received. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Assetpro Lobby|Online|Provider")
	void QueryLeaderboard(FName BoardId, int32 MaxRows);
	virtual void QueryLeaderboard_Implementation(FName BoardId, int32 MaxRows);

	/** Sends the local player's score to a leaderboard. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Assetpro Lobby|Online|Provider")
	void SubmitScore(FName BoardId, double Score);
	virtual void SubmitScore_Implementation(FName BoardId, double Score);

	/** Mirrors an achievement to the platform. Progress is 0 to 1; 1 unlocks it. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Assetpro Lobby|Online|Provider")
	void SetAchievementProgress(FName AchievementId, float Progress);
	virtual void SetAchievementProgress_Implementation(FName AchievementId, float Progress);

	// ---------------------------------------------------------------- Regions, crossplay, chat filter

	/** The regions to choose from. Empty hides the setting. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Assetpro Lobby|Online|Provider")
	TArray<FALKOnlineOption> GetRegions() const;
	virtual TArray<FALKOnlineOption> GetRegions_Implementation() const;

	/** Searches and hosts in this region (an Id from Get Regions; empty = automatic). */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Assetpro Lobby|Online|Provider")
	void SetRegion(const FString& RegionId);
	virtual void SetRegion_Implementation(const FString& RegionId);

	/** Allows or refuses games with players of other platforms. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Assetpro Lobby|Online|Provider")
	void SetCrossplayEnabled(bool bEnabled);
	virtual void SetCrossplayEnabled_Implementation(bool bEnabled);

	/**
	 * Cleans a chat line before it is shown. The default masks the Online Config's Filtered Words with asterisks (whole
	 * words, any case). Override to call your platform's text filter.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Assetpro Lobby|Online|Provider")
	FString FilterChatText(const FString& Text) const;
	virtual FString FilterChatText_Implementation(const FString& Text) const;

	// ---------------------------------------------------------------- Results (call these from your provider)

	/** Host Session finished. On failure give the reason (Network Error when there is no better one). */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Online|Provider|Notify")
	void NotifyHostComplete(bool bSuccess, EALKConnectionError Error);

	/** Find Sessions finished: every game found. Give each one a Session Id (or Search Result Index) Join Session can find it by. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Online|Provider|Notify")
	void NotifyFindComplete(bool bSuccess, const TArray<FALKSessionInfo>& Results);

	/** Connect String is the address the game travels to ("192.168.1.20:7777" or your platform's address form). */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Online|Provider|Notify")
	void NotifyJoinComplete(bool bSuccess, EALKConnectionError Error, const FString& ConnectString);

	/** Leave Session finished. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Online|Provider|Notify")
	void NotifySessionDestroyed();

	/** The platform asks to join a game: an accepted invite, Join Friend. The subsystem leaves the current game and joins. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Online|Provider|Notify")
	void NotifyJoinRequested(const FALKSessionInfo& Session);

	/** A match was found and waits for the players to accept. Skip this when your matchmaking has no accept step. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Online|Provider|Notify")
	void NotifyMatchFound(const FALKSessionInfo& Session, float AcceptSeconds, int32 PlayersAccepted, int32 PlayersNeeded);

	/** More players accepted the found match. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Online|Provider|Notify")
	void NotifyMatchAcceptProgress(int32 PlayersAccepted, int32 PlayersNeeded);

	/** The search ended without a match. Use Cancelled when the player (or a declined accept) stopped it. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Online|Provider|Notify")
	void NotifyMatchmakingFailed(EALKConnectionError Error);

	/** The match is ready: give the address to travel to, or leave it empty and give the session to join. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Online|Provider|Notify")
	void NotifyMatchReady(const FString& ConnectString, const FALKSessionInfo& Session);

	/** No game was found and this player should host one with these settings. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Online|Provider|Notify")
	void NotifyMatchHostRequested(const FALKHostSettings& Settings);

	/** The party's members, leader or ready flags changed. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Online|Provider|Notify")
	void NotifyPartyChanged();

	/** Someone invited the local player to their party. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Online|Provider|Notify")
	void NotifyPartyInviteReceived(const FString& FromUserId, const FString& FromName);

	/** A party chat line arrived (also the local player's own, as an echo). */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Online|Provider|Notify")
	void NotifyPartyChatReceived(const FString& FromUserId, const FString& FromName, const FString& Text);

	/** The friends list or someone's presence changed. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Online|Provider|Notify")
	void NotifyFriendsChanged();

	/** Someone invited the local player to their game. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Online|Provider|Notify")
	void NotifyGameInviteReceived(const FString& FromUserId, const FString& FromName, const FALKSessionInfo& Session);

	/** A private line arrived. To User Id is who it was for: the local player, or the receiver when echoing a line the local player sent. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Online|Provider|Notify")
	void NotifyWhisperReceived(const FString& FromUserId, const FString& FromName, const FString& ToUserId, const FString& Text);

	/** A player started or stopped talking. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Online|Provider|Notify")
	void NotifyPlayerTalking(const FString& UserId, bool bTalking);

	/** Query Leaderboard finished: the rows, best first. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Online|Provider|Notify")
	void NotifyLeaderboardReceived(FName BoardId, const TArray<FALKLeaderboardRow>& Rows);

	// ---------------------------------------------------------------- What the online subsystem listens to

	TMulticastDelegate<void(bool /*bSuccess*/, EALKConnectionError /*Error*/)> OnHostComplete;
	TMulticastDelegate<void(bool /*bSuccess*/, const TArray<FALKSessionInfo>& /*Results*/)> OnFindComplete;
	TMulticastDelegate<void(bool /*bSuccess*/, EALKConnectionError /*Error*/, const FString& /*ConnectString*/)> OnJoinComplete;
	FSimpleMulticastDelegate OnSessionDestroyed;
	TMulticastDelegate<void(const FALKSessionInfo& /*Session*/)> OnJoinRequested;
	TMulticastDelegate<void(const FALKSessionInfo& /*Session*/, float /*AcceptSeconds*/, int32 /*Accepted*/, int32 /*Needed*/)> OnMatchFound;
	TMulticastDelegate<void(int32 /*Accepted*/, int32 /*Needed*/)> OnMatchAcceptProgress;
	TMulticastDelegate<void(EALKConnectionError /*Error*/)> OnMatchmakingFailed;
	TMulticastDelegate<void(const FString& /*ConnectString*/, const FALKSessionInfo& /*Session*/)> OnMatchReady;
	TMulticastDelegate<void(const FALKHostSettings& /*Settings*/)> OnMatchHostRequested;
	FSimpleMulticastDelegate OnPartyChanged;
	TMulticastDelegate<void(const FString& /*FromUserId*/, const FString& /*FromName*/)> OnPartyInviteReceived;
	TMulticastDelegate<void(const FString& /*FromUserId*/, const FString& /*FromName*/, const FString& /*Text*/)> OnPartyChatReceived;
	FSimpleMulticastDelegate OnFriendsChanged;
	TMulticastDelegate<void(const FString& /*FromUserId*/, const FString& /*FromName*/, const FALKSessionInfo& /*Session*/)> OnGameInviteReceived;
	TMulticastDelegate<void(const FString& /*FromUserId*/, const FString& /*FromName*/, const FString& /*ToUserId*/, const FString& /*Text*/)> OnWhisperReceived;
	TMulticastDelegate<void(const FString& /*UserId*/, bool /*bTalking*/)> OnPlayerTalking;
	TMulticastDelegate<void(FName /*BoardId*/, const TArray<FALKLeaderboardRow>& /*Rows*/)> OnLeaderboardReceived;

protected:
	/** Set by Initialize Provider. */
	TWeakObjectPtr<UGameInstance> GameInstance;
};
