// Copyright 2026 CodeLikeMe. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Online/ALKOnlineProvider.h"
#include "ALKSessionsOnlineProvider.generated.h"

struct FALKSessionsProviderImpl;
class FOnlineSession;
class FOnlineSessionSearchResult;

/**
 * The default online backend: the engine's Online Subsystem session interface (create, find, join, destroy, start, end
 * and update session), with friends, invites, identity and voice when the active online service has them.
 *
 * - Works out of the box on the local network with the engine's built-in "Null" service (Default Platform Service = Null).
 * - Works with a platform service by config only: enable its Online Subsystem plugin and set it as the Default Platform
 *   Service in DefaultEngine.ini. No platform SDK is included or linked by this plugin.
 * - Advertises the server name, mode, map, password flag, build version, region and the match rules with the session and
 *   reads them back for the server browser.
 * - Quick match: finds games of the playlist's mode, joins the best one (not full, same version, lowest ping), or hosts
 *   one when the Online Config allows.
 *
 * Party, leaderboards, profiles and whispers are left to your own child class (or see ALK Sample Online Provider).
 */
UCLASS(Blueprintable)
class ASSETPROLOBBYKIT_API UALKSessionsOnlineProvider : public UALKOnlineProvider
{
	GENERATED_BODY()

public:
	/** The engine online service in use ("NULL" for the built-in local network one). Empty when none is loaded. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Online|Provider")
	FString GetServiceName() const;

	/** True when games can only be found on the local network (no service, or the built-in local network one). */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Online|Provider")
	bool IsLANOnly() const;

	// ---- UALKOnlineProvider
	virtual void InitializeProvider_Implementation(UGameInstance* InGameInstance) override;
	virtual void ShutdownProvider_Implementation() override;
	virtual FString GetLocalUserId_Implementation() const override;
	virtual FString GetLocalDisplayName_Implementation() const override;
	virtual bool IsOnline_Implementation() const override;

	virtual void HostSession_Implementation(const FALKHostSettings& Settings) override;
	virtual void FindSessions_Implementation(const FALKSessionFilter& Filter) override;
	virtual void CancelFindSessions_Implementation() override;
	virtual void JoinSession_Implementation(const FALKSessionInfo& Session, const FString& Password) override;
	virtual void LeaveSession_Implementation() override;
	virtual void StartSession_Implementation() override;
	virtual void EndSession_Implementation() override;
	virtual void UpdateSession_Implementation(const FALKHostSettings& Settings) override;
	virtual bool GetCurrentSession_Implementation(FALKSessionInfo& OutSession) const override;
	virtual bool GetSessionConnectString_Implementation(const FALKSessionInfo& Session, FString& OutConnectString) const override;

	virtual void StartMatchmaking_Implementation(FName PlaylistId) override;
	virtual void CancelMatchmaking_Implementation() override;

	virtual void QueryFriends_Implementation() override;
	virtual TArray<FALKFriend> GetFriends_Implementation() const override;
	virtual void InviteFriendToGame_Implementation(const FString& UserId) override;
	virtual void JoinFriend_Implementation(const FString& UserId) override;
	virtual void RespondToGameInvite_Implementation(const FString& FromUserId, bool bAccept) override;
	virtual void ShowInviteOverlay_Implementation() override;

	virtual void SetPlayerMuted_Implementation(const FString& UserId, bool bMuted) override;
	virtual void SetVoiceMode_Implementation(EALKVoiceMode Mode) override;
	virtual void SetPushToTalkActive_Implementation(bool bActive) override;

	virtual bool GetRejoinInfo_Implementation(FALKSessionInfo& OutSession) const override;
	virtual void RejoinSession_Implementation() override;

	virtual void SetRegion_Implementation(const FString& RegionId) override;

protected:
	/**
	 * The quick match of this provider: finds games of the playlist's mode, then answers with Notify Match Ready (the
	 * best game), Notify Match Host Requested (none found, hosting allowed) or Notify Matchmaking Failed. Start
	 * Matchmaking calls it; a child with its own queue or accept step can call it when that is done.
	 */
	void BeginQuickMatchSearch(FName PlaylistId);

	/** Stops a quick match search without answering. True when one was running. */
	bool StopQuickMatchSearch();

	bool IsQuickMatchSearching() const { return bFindPending && bFindForMatchmaking; }

	/**
	 * Called with what a Find Sessions search found, just before Notify Find Complete: a child can add to the list or
	 * change it (the sample provider adds made-up games). Not called for the quick match's own search.
	 */
	virtual void AugmentFindResults(const FALKSessionFilter& Filter, TArray<FALKSessionInfo>& Results) {}

	/** The region set with Set Region; advertised with hosted games. */
	FString CurrentRegion;

private:
	void BindServiceEvents();
	void UnbindServiceEvents();

	void CreatePendingSession();
	void HandleCreateComplete(FName SessionName, bool bWasSuccessful);
	void FinishHost(bool bSuccess, EALKConnectionError Error);

	void DestroyCurrentSession();
	void HandleDestroyComplete(FName SessionName, bool bWasSuccessful);

	void BeginFind(const FALKSessionFilter& Filter, bool bForMatchmaking);
	void HandleFindComplete(bool bWasSuccessful);
	/** Ends the running search and gives back what it found. */
	TArray<FALKSessionInfo> EndFind();
	void FinishQuickMatch(const TArray<FALKSessionInfo>& Results);

	void JoinPendingSession();
	void HandleJoinComplete(FName SessionName, int32 Result);
	void FinishJoin(bool bSuccess, EALKConnectionError Error, const FString& ConnectString);

	/** Keeps a session that did not come from a search (an invite, a friend's game) so it can be joined later. */
	FALKSessionInfo RememberDirectResult(const FOnlineSessionSearchResult& Result);
	const FOnlineSessionSearchResult* FindSearchResult(const FALKSessionInfo& Session) const;
	FALKSessionInfo MakeSessionInfo(const FOnlineSession& Session, int32 PingMs, int32 SearchResultIndex) const;
	void RebuildFriends();

	/** The engine online service objects, kept out of this header so it needs no Online Subsystem include. */
	TSharedPtr<FALKSessionsProviderImpl> Impl;

	FALKHostSettings PendingHostSettings;
	FALKSessionInfo PendingJoinSession;
	FALKSessionFilter ActiveFilter;
	FName QuickMatchPlaylist;

	/** Friends as last read from the service. */
	TArray<FALKFriend> CachedFriends;

	/** Game invites that arrived and were not answered yet, by sender. */
	TMap<FString, FALKSessionInfo> PendingInvites;

	EALKVoiceMode VoiceMode = EALKVoiceMode::Off;

	bool bEventsBound = false;
	bool bCreatePending = false;
	bool bDestroyPending = false;
	bool bJoinPending = false;
	bool bFindPending = false;
	bool bFindForMatchmaking = false;
	/** What to do once a stale session is gone. */
	bool bHostAfterDestroy = false;
	bool bJoinAfterDestroy = false;
	bool bNotifyAfterDestroy = false;
	bool bHasRejoinSession = false;
	FALKSessionInfo RejoinSessionInfo;
};
