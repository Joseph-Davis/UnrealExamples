// Copyright 2026 CodeLikeMe. All Rights Reserved.

#include "Lobby/ALKPlayerNetComponent.h"
#include "Lobby/ALKLobbyPlayerState.h"
#include "Lobby/ALKLobbyStatics.h"
#include "Lobby/ALKNetGameModeBase.h"
#include "Online/ALKOnlineConfig.h"
#include "Online/ALKOnlineSubsystem.h"
#include "ALKDeveloperSettings.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ALKPlayerNetComponent)

#define LOCTEXT_NAMESPACE "ALKLobby"

namespace ALKNetComponentPrivate
{
	/** At most this many chat lines... */
	static constexpr int32 ChatBurst = 5;
	/** ...in this many seconds. */
	static constexpr double ChatWindow = 5.0;
	/** Seconds between two name changes of one player. */
	static constexpr double NameChangeInterval = 1.0;
}

UALKPlayerNetComponent::UALKPlayerNetComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

UALKPlayerNetComponent* UALKPlayerNetComponent::Get(const APlayerController* PlayerController)
{
	return PlayerController ? PlayerController->FindComponentByClass<UALKPlayerNetComponent>() : nullptr;
}

APlayerController* UALKPlayerNetComponent::GetPlayerController() const
{
	return Cast<APlayerController>(GetOwner());
}

AALKLobbyPlayerState* UALKPlayerNetComponent::GetLobbyPlayerState() const
{
	const APlayerController* Controller = GetPlayerController();
	return Controller ? Controller->GetPlayerState<AALKLobbyPlayerState>() : nullptr;
}

AALKNetGameModeBase* UALKPlayerNetComponent::GetNetGameMode() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetAuthGameMode<AALKNetGameModeBase>() : nullptr;
}

void UALKPlayerNetComponent::BeginPlay()
{
	Super::BeginPlay();
	const APlayerController* Controller = GetPlayerController();
	if (Controller && Controller->IsLocalController() && GetNetMode() != NM_Standalone)
	{
		SendLocalPlayerInfo();
	}
}

void UALKPlayerNetComponent::SendLocalPlayerInfo()
{
	if (const UALKOnlineSubsystem* Online = UALKOnlineSubsystem::Get(this))
	{
		ServerSetPlayerName(Online->GetLocalDisplayName());
		const FString PartyId = Online->GetPartyId();
		if (!PartyId.IsEmpty())
		{
			ServerSetPartyId(PartyId);
		}
	}
}

// ---------------------------------------------------------------- Any player

void UALKPlayerNetComponent::ServerSetReady_Implementation(bool bReady)
{
	APlayerController* Controller = GetPlayerController();
	if (AALKNetGameModeBase* GameMode = GetNetGameMode())
	{
		GameMode->HandleSetReady(Controller, bReady);
	}
	else if (AALKLobbyPlayerState* Player = GetLobbyPlayerState())
	{
		// A game mode of your own: the value is still kept and replicated.
		Player->SetReady(bReady);
	}
}

void UALKPlayerNetComponent::ServerRequestTeam_Implementation(int32 TeamId)
{
	if (AALKNetGameModeBase* GameMode = GetNetGameMode())
	{
		GameMode->HandleRequestTeam(GetPlayerController(), TeamId);
	}
}

void UALKPlayerNetComponent::ServerSetLoadout_Implementation(FName LoadoutId)
{
	if (AALKNetGameModeBase* GameMode = GetNetGameMode())
	{
		GameMode->HandleSetLoadout(GetPlayerController(), LoadoutId);
	}
	else if (AALKLobbyPlayerState* Player = GetLobbyPlayerState())
	{
		Player->SetLoadoutId(LoadoutId);
	}
}

void UALKPlayerNetComponent::ServerSetColor_Implementation(int32 ColorIndex)
{
	if (AALKNetGameModeBase* GameMode = GetNetGameMode())
	{
		GameMode->HandleSetColor(GetPlayerController(), ColorIndex);
	}
	else if (AALKLobbyPlayerState* Player = GetLobbyPlayerState())
	{
		Player->SetColorIndex(FMath::Clamp(ColorIndex, 0, FMath::Max(0, UALKDeveloperSettings::GetOnlineConfig()->PlayerColorCount - 1)));
	}
}

void UALKPlayerNetComponent::ServerSendChat_Implementation(EALKChatChannel Channel, const FString& Text, const FString& WhisperTargetId)
{
	APlayerController* Controller = GetPlayerController();
	const APlayerState* PlayerState = Controller ? Controller->GetPlayerState<APlayerState>() : nullptr;
	// System lines only come from the server itself.
	if (!PlayerState || Channel == EALKChatChannel::System)
	{
		return;
	}
	const UALKOnlineConfig* Config = UALKDeveloperSettings::GetOnlineConfig();
	const FString Clean = UALKLobbyStatics::SanitizeChatText(Text, Config->ChatMaxLength);
	if (Clean.IsEmpty() || (Channel == EALKChatChannel::Whisper && WhisperTargetId.IsEmpty()))
	{
		return;
	}

	// Rate limit: a few lines in a row are fine, a flood is not.
	const double Now = FPlatformTime::Seconds();
	RecentChatTimes.RemoveAll([Now](double Time) { return Time < Now - ALKNetComponentPrivate::ChatWindow; });
	if (!UALKLobbyStatics::IsChatAllowed(RecentChatTimes, Now, ALKNetComponentPrivate::ChatBurst, ALKNetComponentPrivate::ChatWindow))
	{
		ClientNotify(LOCTEXT("ChatTooFast", "You are sending messages too quickly."));
		return;
	}
	RecentChatTimes.Add(Now);

	const AALKLobbyPlayerState* Player = Cast<AALKLobbyPlayerState>(PlayerState);
	FALKChatMessage Message;
	Message.Time = FDateTime::UtcNow();
	Message.Channel = Channel;
	Message.SenderId = Player ? Player->GetUserId() : FString::FromInt(PlayerState->GetPlayerId());
	Message.SenderName = PlayerState->GetPlayerName();
	Message.TeamId = Player ? Player->TeamId : INDEX_NONE;
	Message.SenderStableIndex = Player ? Player->GetStableIndex() : INDEX_NONE;
	Message.Text = Clean;
	Message.WhisperTargetId = Channel == EALKChatChannel::Whisper ? WhisperTargetId.Left(128) : FString();

	if (AALKNetGameModeBase* GameMode = GetNetGameMode())
	{
		GameMode->RouteChatMessage(Message);
	}
	else
	{
		AALKNetGameModeBase::DeliverChatMessage(GetWorld(), Message);
	}
}

void UALKPlayerNetComponent::ServerVoteMap_Implementation(FName MapId)
{
	if (AALKNetGameModeBase* GameMode = GetNetGameMode())
	{
		GameMode->HandleVoteMap(GetPlayerController(), MapId);
	}
}

void UALKPlayerNetComponent::ServerVoteRematch_Implementation(bool bWantsRematch)
{
	if (AALKNetGameModeBase* GameMode = GetNetGameMode())
	{
		GameMode->HandleVoteRematch(GetPlayerController(), bWantsRematch);
	}
	else if (AALKLobbyPlayerState* Player = GetLobbyPlayerState())
	{
		Player->SetRematchVote(bWantsRematch);
	}
}

void UALKPlayerNetComponent::ServerSetSpectating_Implementation(bool bSpectating)
{
	if (AALKNetGameModeBase* GameMode = GetNetGameMode())
	{
		GameMode->HandleSetSpectating(GetPlayerController(), bSpectating);
	}
	else if (AALKLobbyPlayerState* Player = GetLobbyPlayerState())
	{
		Player->SetSpectating(bSpectating);
	}
}

void UALKPlayerNetComponent::ServerSetPlayerName_Implementation(const FString& Name)
{
	APlayerController* Controller = GetPlayerController();
	UWorld* World = GetWorld();
	AGameModeBase* GameMode = World ? World->GetAuthGameMode() : nullptr;
	APlayerState* PlayerState = Controller ? Controller->GetPlayerState<APlayerState>() : nullptr;
	if (!GameMode || !PlayerState || !World->GetGameState())
	{
		return;
	}
	const double Now = FPlatformTime::Seconds();
	if (Now - LastNameChangeTime < ALKNetComponentPrivate::NameChangeInterval)
	{
		return;
	}
	const FString Clean = UALKLobbyStatics::SanitizePlayerName(Name, UALKDeveloperSettings::GetOnlineConfig()->PlayerNameMaxLength);
	if (Clean.IsEmpty())
	{
		return;
	}

	// Two players with the same name are told apart: "Name (2)".
	TArray<FString> TakenNames;
	for (const APlayerState* Other : World->GetGameState()->PlayerArray)
	{
		if (Other && Other != PlayerState)
		{
			TakenNames.Add(Other->GetPlayerName());
		}
	}
	const FString Unique = UALKLobbyStatics::MakeUniquePlayerName(Clean, TakenNames);
	if (PlayerState->GetPlayerName() != Unique)
	{
		LastNameChangeTime = Now;
		GameMode->ChangeName(Controller, Unique, true);
	}
}

void UALKPlayerNetComponent::ServerSetPartyId_Implementation(const FString& PartyId)
{
	if (AALKLobbyPlayerState* Player = GetLobbyPlayerState())
	{
		Player->SetPartyId(PartyId.Left(128));
	}
}

// ---------------------------------------------------------------- Host only (the game mode checks the caller again)

void UALKPlayerNetComponent::ServerKickPlayer_Implementation(int32 PlayerId)
{
	if (AALKNetGameModeBase* GameMode = GetNetGameMode())
	{
		GameMode->HandleKickPlayer(GetPlayerController(), PlayerId, false);
	}
}

void UALKPlayerNetComponent::ServerBanPlayer_Implementation(int32 PlayerId)
{
	if (AALKNetGameModeBase* GameMode = GetNetGameMode())
	{
		GameMode->HandleKickPlayer(GetPlayerController(), PlayerId, true);
	}
}

void UALKPlayerNetComponent::ServerUnbanPlayer_Implementation(const FString& UserId)
{
	if (AALKNetGameModeBase* GameMode = GetNetGameMode())
	{
		GameMode->HandleUnbanPlayer(GetPlayerController(), UserId);
	}
}

void UALKPlayerNetComponent::ServerMovePlayerToTeam_Implementation(int32 PlayerId, int32 TeamId)
{
	if (AALKNetGameModeBase* GameMode = GetNetGameMode())
	{
		GameMode->HandleMovePlayerToTeam(GetPlayerController(), PlayerId, TeamId);
	}
}

void UALKPlayerNetComponent::ServerChangeMatchSettings_Implementation(const FALKHostSettings& Settings)
{
	if (AALKNetGameModeBase* GameMode = GetNetGameMode())
	{
		GameMode->HandleChangeMatchSettings(GetPlayerController(), Settings);
	}
}

void UALKPlayerNetComponent::ServerStartMatch_Implementation(bool bForce)
{
	if (AALKNetGameModeBase* GameMode = GetNetGameMode())
	{
		GameMode->HandleStartMatch(GetPlayerController(), bForce);
	}
}

void UALKPlayerNetComponent::ServerCancelCountdown_Implementation()
{
	if (AALKNetGameModeBase* GameMode = GetNetGameMode())
	{
		GameMode->HandleCancelCountdown(GetPlayerController());
	}
}

void UALKPlayerNetComponent::ServerChangeMap_Implementation(FName MapId)
{
	if (AALKNetGameModeBase* GameMode = GetNetGameMode())
	{
		GameMode->HandleChangeMap(GetPlayerController(), MapId);
	}
}

void UALKPlayerNetComponent::ServerSetPaused_Implementation(bool bPaused)
{
	if (AALKNetGameModeBase* GameMode = GetNetGameMode())
	{
		GameMode->HandleSetPaused(GetPlayerController(), bPaused);
	}
}

void UALKPlayerNetComponent::ServerReturnToLobby_Implementation()
{
	if (AALKNetGameModeBase* GameMode = GetNetGameMode())
	{
		GameMode->HandleReturnToLobby(GetPlayerController());
	}
}

// ---------------------------------------------------------------- From the server

void UALKPlayerNetComponent::ClientReceiveChat_Implementation(const FALKChatMessage& Message)
{
	// The online subsystem applies this player's mutes, blocks, chat switch and filter, and keeps the line for the menus.
	FALKChatMessage Shown = Message;
	if (UALKOnlineSubsystem* Online = UALKOnlineSubsystem::Get(this))
	{
		if (!Online->ReceiveChatMessage(Message, &Shown))
		{
			return;
		}
	}
	ChatHistory.Add(Shown);
	const int32 MaxLines = FMath::Max(1, UALKDeveloperSettings::GetOnlineConfig()->ChatHistorySize);
	if (ChatHistory.Num() > MaxLines)
	{
		ChatHistory.RemoveAt(0, ChatHistory.Num() - MaxLines);
	}
	OnChatReceived.Broadcast(Shown);
	OnChatReceivedNative.Broadcast(Shown);
}

void UALKPlayerNetComponent::ClientReceiveFeedEvent_Implementation(const FALKFeedEvent& Event)
{
	OnFeedEventNative.Broadcast(Event);
}

void UALKPlayerNetComponent::ClientKicked_Implementation(const FString& Reason)
{
	if (UALKOnlineSubsystem* Online = UALKOnlineSubsystem::Get(this))
	{
		Online->NotifyKicked(Reason);
	}
}

void UALKPlayerNetComponent::ClientNotify_Implementation(const FText& Text)
{
	OnServerNotice.Broadcast(Text);
	OnServerNoticeNative.Broadcast(Text);
}

#undef LOCTEXT_NAMESPACE
