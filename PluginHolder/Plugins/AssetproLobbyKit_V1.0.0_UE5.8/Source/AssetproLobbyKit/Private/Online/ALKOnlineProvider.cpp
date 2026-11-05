// Copyright 2026 CodeLikeMe. All Rights Reserved.

#include "Online/ALKOnlineProvider.h"
#include "Online/ALKOnlineConfig.h"
#include "Lobby/ALKLobbyStatics.h"
#include "ALKDeveloperSettings.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/PlatformProcess.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ALKOnlineProvider)

UWorld* UALKOnlineProvider::GetWorld() const
{
	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		return nullptr;
	}
	const UGameInstance* Instance = GameInstance.Get();
	return Instance ? Instance->GetWorld() : nullptr;
}

const UALKOnlineConfig* UALKOnlineProvider::GetOnlineConfig() const
{
	return UALKDeveloperSettings::GetOnlineConfig();
}

// ---------------------------------------------------------------- Lifecycle and identity

void UALKOnlineProvider::InitializeProvider_Implementation(UGameInstance* InGameInstance)
{
	GameInstance = InGameInstance;
}

void UALKOnlineProvider::ShutdownProvider_Implementation()
{
}

FString UALKOnlineProvider::GetLocalUserId_Implementation() const
{
	return TEXT("LocalPlayer");
}

FString UALKOnlineProvider::GetLocalDisplayName_Implementation() const
{
	return FString(FPlatformProcess::UserName());
}

bool UALKOnlineProvider::IsOnline_Implementation() const
{
	return false;
}

// ---------------------------------------------------------------- Sessions

void UALKOnlineProvider::HostSession_Implementation(const FALKHostSettings& Settings)
{
	NotifyHostComplete(false, EALKConnectionError::NetworkError);
}

void UALKOnlineProvider::FindSessions_Implementation(const FALKSessionFilter& Filter)
{
	NotifyFindComplete(false, TArray<FALKSessionInfo>());
}

void UALKOnlineProvider::CancelFindSessions_Implementation()
{
}

void UALKOnlineProvider::JoinSession_Implementation(const FALKSessionInfo& Session, const FString& Password)
{
	NotifyJoinComplete(false, EALKConnectionError::SessionNotFound, FString());
}

void UALKOnlineProvider::LeaveSession_Implementation()
{
	NotifySessionDestroyed();
}

void UALKOnlineProvider::StartSession_Implementation()
{
}

void UALKOnlineProvider::EndSession_Implementation()
{
}

void UALKOnlineProvider::UpdateSession_Implementation(const FALKHostSettings& Settings)
{
}

bool UALKOnlineProvider::GetCurrentSession_Implementation(FALKSessionInfo& OutSession) const
{
	return false;
}

bool UALKOnlineProvider::GetSessionConnectString_Implementation(const FALKSessionInfo& Session, FString& OutConnectString) const
{
	return false;
}

FString UALKOnlineProvider::GetLobbyCode_Implementation() const
{
	return FString();
}

bool UALKOnlineProvider::JoinByCode_Implementation(const FString& Code, const FString& Password)
{
	return false;
}

// ---------------------------------------------------------------- Matchmaking

void UALKOnlineProvider::StartMatchmaking_Implementation(FName PlaylistId)
{
	NotifyMatchmakingFailed(EALKConnectionError::SessionNotFound);
}

void UALKOnlineProvider::CancelMatchmaking_Implementation()
{
	NotifyMatchmakingFailed(EALKConnectionError::Cancelled);
}

float UALKOnlineProvider::GetEstimatedWaitSeconds_Implementation(FName PlaylistId) const
{
	return -1.f;
}

void UALKOnlineProvider::RespondToMatch_Implementation(bool bAccept)
{
}

// ---------------------------------------------------------------- Party

TArray<FALKPartyMember> UALKOnlineProvider::GetPartyMembers_Implementation() const
{
	return TArray<FALKPartyMember>();
}

void UALKOnlineProvider::InviteToParty_Implementation(const FString& UserId)
{
}

void UALKOnlineProvider::RespondToPartyInvite_Implementation(const FString& FromUserId, bool bAccept)
{
}

void UALKOnlineProvider::KickFromParty_Implementation(const FString& UserId)
{
}

void UALKOnlineProvider::PromotePartyMember_Implementation(const FString& UserId)
{
}

void UALKOnlineProvider::LeaveParty_Implementation()
{
}

void UALKOnlineProvider::SetPartyReady_Implementation(bool bReady)
{
}

void UALKOnlineProvider::SendPartyChat_Implementation(const FString& Text)
{
}

int32 UALKOnlineProvider::GetPartyMaxSize_Implementation() const
{
	return 4;
}

// ---------------------------------------------------------------- Friends

void UALKOnlineProvider::QueryFriends_Implementation()
{
	NotifyFriendsChanged();
}

TArray<FALKFriend> UALKOnlineProvider::GetFriends_Implementation() const
{
	return TArray<FALKFriend>();
}

TArray<FALKFriend> UALKOnlineProvider::GetRecentPlayers_Implementation() const
{
	return TArray<FALKFriend>();
}

void UALKOnlineProvider::InviteFriendToGame_Implementation(const FString& UserId)
{
}

void UALKOnlineProvider::JoinFriend_Implementation(const FString& UserId)
{
}

void UALKOnlineProvider::RespondToGameInvite_Implementation(const FString& FromUserId, bool bAccept)
{
}

void UALKOnlineProvider::ShowInviteOverlay_Implementation()
{
}

void UALKOnlineProvider::SendWhisper_Implementation(const FString& UserId, const FString& Text)
{
}

// ---------------------------------------------------------------- Profile and social

bool UALKOnlineProvider::GetPlayerProfile_Implementation(const FString& UserId, FALKPlayerProfile& OutProfile) const
{
	return false;
}

void UALKOnlineProvider::ReportPlayer_Implementation(const FString& UserId, FName ReasonId, const FString& Comment)
{
}

void UALKOnlineProvider::SetPlayerMuted_Implementation(const FString& UserId, bool bMuted)
{
}

void UALKOnlineProvider::SetPlayerBlocked_Implementation(const FString& UserId, bool bBlocked)
{
}

// ---------------------------------------------------------------- Voice

void UALKOnlineProvider::SetVoiceMode_Implementation(EALKVoiceMode Mode)
{
}

void UALKOnlineProvider::SetPushToTalkActive_Implementation(bool bActive)
{
}

TArray<FALKOnlineOption> UALKOnlineProvider::GetAudioInputDevices_Implementation() const
{
	return TArray<FALKOnlineOption>();
}

TArray<FALKOnlineOption> UALKOnlineProvider::GetAudioOutputDevices_Implementation() const
{
	return TArray<FALKOnlineOption>();
}

void UALKOnlineProvider::SetAudioInputDevice_Implementation(const FString& DeviceId)
{
}

void UALKOnlineProvider::SetAudioOutputDevice_Implementation(const FString& DeviceId)
{
}

void UALKOnlineProvider::SetMicVolume_Implementation(float Volume)
{
}

void UALKOnlineProvider::SetVoiceVolume_Implementation(float Volume)
{
}

// ---------------------------------------------------------------- Reconnect

bool UALKOnlineProvider::GetRejoinInfo_Implementation(FALKSessionInfo& OutSession) const
{
	return false;
}

void UALKOnlineProvider::RejoinSession_Implementation()
{
	NotifyJoinComplete(false, EALKConnectionError::SessionNotFound, FString());
}

// ---------------------------------------------------------------- Leaderboards and achievements

void UALKOnlineProvider::QueryLeaderboard_Implementation(FName BoardId, int32 MaxRows)
{
	NotifyLeaderboardReceived(BoardId, TArray<FALKLeaderboardRow>());
}

void UALKOnlineProvider::SubmitScore_Implementation(FName BoardId, double Score)
{
}

void UALKOnlineProvider::SetAchievementProgress_Implementation(FName AchievementId, float Progress)
{
}

// ---------------------------------------------------------------- Regions, crossplay, chat filter

TArray<FALKOnlineOption> UALKOnlineProvider::GetRegions_Implementation() const
{
	return TArray<FALKOnlineOption>();
}

void UALKOnlineProvider::SetRegion_Implementation(const FString& RegionId)
{
}

void UALKOnlineProvider::SetCrossplayEnabled_Implementation(bool bEnabled)
{
}

FString UALKOnlineProvider::FilterChatText_Implementation(const FString& Text) const
{
	return UALKLobbyStatics::MaskFilteredWords(Text, GetOnlineConfig()->FilteredWords);
}

// ---------------------------------------------------------------- Results

void UALKOnlineProvider::NotifyHostComplete(bool bSuccess, EALKConnectionError Error)
{
	OnHostComplete.Broadcast(bSuccess, bSuccess ? EALKConnectionError::None : Error);
}

void UALKOnlineProvider::NotifyFindComplete(bool bSuccess, const TArray<FALKSessionInfo>& Results)
{
	OnFindComplete.Broadcast(bSuccess, Results);
}

void UALKOnlineProvider::NotifyJoinComplete(bool bSuccess, EALKConnectionError Error, const FString& ConnectString)
{
	OnJoinComplete.Broadcast(bSuccess, bSuccess ? EALKConnectionError::None : Error, ConnectString);
}

void UALKOnlineProvider::NotifySessionDestroyed()
{
	OnSessionDestroyed.Broadcast();
}

void UALKOnlineProvider::NotifyJoinRequested(const FALKSessionInfo& Session)
{
	OnJoinRequested.Broadcast(Session);
}

void UALKOnlineProvider::NotifyMatchFound(const FALKSessionInfo& Session, float AcceptSeconds, int32 PlayersAccepted, int32 PlayersNeeded)
{
	OnMatchFound.Broadcast(Session, AcceptSeconds, PlayersAccepted, PlayersNeeded);
}

void UALKOnlineProvider::NotifyMatchAcceptProgress(int32 PlayersAccepted, int32 PlayersNeeded)
{
	OnMatchAcceptProgress.Broadcast(PlayersAccepted, PlayersNeeded);
}

void UALKOnlineProvider::NotifyMatchmakingFailed(EALKConnectionError Error)
{
	OnMatchmakingFailed.Broadcast(Error);
}

void UALKOnlineProvider::NotifyMatchReady(const FString& ConnectString, const FALKSessionInfo& Session)
{
	OnMatchReady.Broadcast(ConnectString, Session);
}

void UALKOnlineProvider::NotifyMatchHostRequested(const FALKHostSettings& Settings)
{
	OnMatchHostRequested.Broadcast(Settings);
}

void UALKOnlineProvider::NotifyPartyChanged()
{
	OnPartyChanged.Broadcast();
}

void UALKOnlineProvider::NotifyPartyInviteReceived(const FString& FromUserId, const FString& FromName)
{
	OnPartyInviteReceived.Broadcast(FromUserId, FromName);
}

void UALKOnlineProvider::NotifyPartyChatReceived(const FString& FromUserId, const FString& FromName, const FString& Text)
{
	OnPartyChatReceived.Broadcast(FromUserId, FromName, Text);
}

void UALKOnlineProvider::NotifyFriendsChanged()
{
	OnFriendsChanged.Broadcast();
}

void UALKOnlineProvider::NotifyGameInviteReceived(const FString& FromUserId, const FString& FromName, const FALKSessionInfo& Session)
{
	OnGameInviteReceived.Broadcast(FromUserId, FromName, Session);
}

void UALKOnlineProvider::NotifyWhisperReceived(const FString& FromUserId, const FString& FromName, const FString& ToUserId, const FString& Text)
{
	OnWhisperReceived.Broadcast(FromUserId, FromName, ToUserId, Text);
}

void UALKOnlineProvider::NotifyPlayerTalking(const FString& UserId, bool bTalking)
{
	OnPlayerTalking.Broadcast(UserId, bTalking);
}

void UALKOnlineProvider::NotifyLeaderboardReceived(FName BoardId, const TArray<FALKLeaderboardRow>& Rows)
{
	OnLeaderboardReceived.Broadcast(BoardId, Rows);
}
