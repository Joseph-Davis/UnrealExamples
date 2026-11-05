// Copyright 2026 CodeLikeMe. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Math/RandomStream.h"
#include "Online/ALKSessionsOnlineProvider.h"
#include "ALKSampleOnlineProvider.generated.h"

/**
 * A provider to try every multiplayer screen without a backend. Hosting, finding and joining games are real (the parent
 * class: local network out of the box). Everything social is made up and runs on this machine only:
 *
 * - eight friends whose presence changes now and then, and four recent players;
 * - a party: invited friends join after a moment and ready up, party chat gets short answers;
 * - whispers that are answered;
 * - a quick match queue: a few seconds of searching, a found match with an accept countdown, other players accepting one
 *   by one, then the parent's real quick match (join the best local game, or host one);
 * - a dozen made-up games in the server browser, next to the real ones found on the network (joining one fails after a
 *   moment, like a game that just closed);
 * - leaderboard rows and profile cards.
 *
 * The same Simulation Seed always plays out the same way, so screenshots and tests repeat.
 * Use it as a reference when you write your own provider; do not ship it.
 */
UCLASS(Blueprintable)
class ASSETPROLOBBYKIT_API UALKSampleOnlineProvider : public UALKSessionsOnlineProvider
{
	GENERATED_BODY()

public:
	/** Decides the friends' starting presence and every made-up choice after it. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sample")
	int32 SimulationSeed = 2026;

	/** Friends change presence on their own. Turn off for a still picture. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Sample")
	bool bSimulateActivity = true;

	/** Seconds between presence changes. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Sample", meta = (ClampMin = "1"))
	float PresenceChangeInterval = 9.f;

	/** Seconds a quick match spends searching before a match is found. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Sample", meta = (ClampMin = "0.5"))
	float QueueSeconds = 4.f;

	/** Adds a dozen made-up games to every search, to see a full server browser on one machine. They cannot be joined. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Sample")
	bool bSimulateSessions = true;

	/** Seconds until an invited friend joins the party. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Sample", meta = (ClampMin = "0.1"))
	float PartyJoinDelay = 1.5f;

	/** Back to the starting state of the seed: friends, an empty party, no queue. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Online|Sample")
	void ResetSimulation();

	/** Freezes everything that happens on its own (presence changes, answers, the queue). */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Online|Sample")
	void SetSimulationPaused(bool bPaused);

	/** Makes a friend invite the player to their party, to see the invite dialog. Empty = the first online friend. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Online|Sample")
	void SimulatePartyInvite(const FString& FromUserId);

	/** Makes a friend invite the player to a game, to see the invite dialog. Empty = the first friend in a game. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Online|Sample")
	void SimulateGameInvite(const FString& FromUserId);

	/** Makes a friend whisper to the player. Empty = the first online friend. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Online|Sample")
	void SimulateWhisper(const FString& FromUserId, const FString& Text);

	/** Makes a player start or stop talking, to see the voice indicators. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Online|Sample")
	void SimulateTalking(const FString& UserId, bool bTalking);

	// ---- UALKOnlineProvider
	virtual void InitializeProvider_Implementation(UGameInstance* InGameInstance) override;
	virtual void ShutdownProvider_Implementation() override;
	virtual bool IsOnline_Implementation() const override;

	virtual void JoinSession_Implementation(const FALKSessionInfo& Session, const FString& Password) override;

	virtual void StartMatchmaking_Implementation(FName PlaylistId) override;
	virtual void CancelMatchmaking_Implementation() override;
	virtual float GetEstimatedWaitSeconds_Implementation(FName PlaylistId) const override;
	virtual void RespondToMatch_Implementation(bool bAccept) override;

	virtual TArray<FALKPartyMember> GetPartyMembers_Implementation() const override;
	virtual void InviteToParty_Implementation(const FString& UserId) override;
	virtual void RespondToPartyInvite_Implementation(const FString& FromUserId, bool bAccept) override;
	virtual void KickFromParty_Implementation(const FString& UserId) override;
	virtual void PromotePartyMember_Implementation(const FString& UserId) override;
	virtual void LeaveParty_Implementation() override;
	virtual void SetPartyReady_Implementation(bool bReady) override;
	virtual void SendPartyChat_Implementation(const FString& Text) override;
	virtual int32 GetPartyMaxSize_Implementation() const override;

	virtual void QueryFriends_Implementation() override;
	virtual TArray<FALKFriend> GetFriends_Implementation() const override;
	virtual TArray<FALKFriend> GetRecentPlayers_Implementation() const override;
	virtual void InviteFriendToGame_Implementation(const FString& UserId) override;
	virtual void JoinFriend_Implementation(const FString& UserId) override;
	virtual void RespondToGameInvite_Implementation(const FString& FromUserId, bool bAccept) override;
	virtual void SendWhisper_Implementation(const FString& UserId, const FString& Text) override;

	virtual bool GetPlayerProfile_Implementation(const FString& UserId, FALKPlayerProfile& OutProfile) const override;
	virtual void QueryLeaderboard_Implementation(FName BoardId, int32 MaxRows) override;
	virtual void SubmitScore_Implementation(FName BoardId, double Score) override;

	virtual TArray<FALKOnlineOption> GetAudioInputDevices_Implementation() const override;
	virtual TArray<FALKOnlineOption> GetAudioOutputDevices_Implementation() const override;
	virtual TArray<FALKOnlineOption> GetRegions_Implementation() const override;

protected:
	virtual void AugmentFindResults(const FALKSessionFilter& Filter, TArray<FALKSessionInfo>& Results) override;

private:
	/** Something the simulation does later. */
	enum class ESampleEvent : uint8
	{
		FriendsLoaded,
		PresenceChange,
		PartyJoin,
		PartyReady,
		PartyChatReply,
		WhisperReply,
		MatchFound,
		MatchPlayerAccepted,
		MatchAcceptTimeout,
		LeaderboardLoaded,
		SampleJoinFailed
	};

	struct FSampleEvent
	{
		double Time = 0.0;
		ESampleEvent Type = ESampleEvent::FriendsLoaded;
		FString UserId;
		FString Text;
		int32 Value = 0;
	};

	bool TickSimulation(float DeltaTime);
	void Schedule(float Delay, ESampleEvent Type, const FString& UserId = FString(), const FString& Text = FString(), int32 Value = 0);
	void CancelEvents(ESampleEvent Type);
	void RunEvent(const FSampleEvent& Event);

	void BuildFriends();
	FALKFriend* FindFriend(const FString& UserId);
	const FALKFriend* FindFriend(const FString& UserId) const;
	FString FindNameFor(const FString& UserId) const;
	void SetFriendPresence(FALKFriend& Friend, EALKPresence Presence);
	void ChangeSomeonesPresence();
	bool IsInParty(const FString& UserId) const;
	bool IsLocalPartyLeader() const;
	void StopMatchSimulation();
	void CheckMatchAccepted();
	FString PickLine(const TArray<FText>& Lines);

	TArray<FALKFriend> Friends;
	TArray<FALKFriend> RecentPlayers;

	/** The other party members; the local player is added by Get Party Members. */
	TArray<FALKPartyMember> PartyOthers;
	bool bLocalPartyReady = false;
	/** Empty = the local player leads the party. */
	FString PartyLeaderId;

	TArray<FSampleEvent> Events;
	double SimulationTime = 0.0;
	bool bSimulationPaused = false;
	FRandomStream Stream;
	FTSTicker::FDelegateHandle TickerHandle;

	FName MatchPlaylist;
	bool bMatchQueueActive = false;
	bool bMatchFound = false;
	bool bLocalAccepted = false;
	int32 MatchAccepted = 0;
	int32 MatchNeeded = 0;

	/** Scores sent with Submit Score, by board. */
	TMap<FName, double> SubmittedScores;
};
