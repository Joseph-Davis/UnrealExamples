// Copyright 2026 CodeLikeMe. All Rights Reserved.

#include "OnlineUI/ALKOnlineFlowSubsystem.h"
#include "OnlineUI/ALKMatchHUDBase.h"
#include "ALKDeveloperSettings.h"
#include "ALKMenuConfig.h"
#include "ALKMenuSubsystem.h"
#include "ALKTheme.h"
#include "AssetproLobbyKit.h"
#include "HUD/ALKHUDSubsystem.h"
#include "Lobby/ALKLobbyGameState.h"
#include "Lobby/ALKLobbyPlayerState.h"
#include "Lobby/ALKLobbyStatics.h"
#include "Lobby/ALKPlayerNetComponent.h"
#include "Online/ALKOnlineSubsystem.h"
#include "Screens/ALKConfirmDialog.h"
#include "Settings/ALKSettingsSubsystem.h"
#include "Widgets/ALKDecor.h"
#include "Widgets/ALKRootLayout.h"
#include "Blueprint/UserWidget.h"
#include "CommonActivatableWidget.h"
#include "Widgets/CommonActivatableWidgetContainer.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Widgets/SWindow.h"
#include "UObject/UnrealType.h"
#include "UObject/UObjectIterator.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ALKOnlineFlowSubsystem)

#define LOCTEXT_NAMESPACE "AssetproLobbyKit"

namespace ALKOnlineFlowPrivate
{
	/** How often the flow checks that the right screen is up. */
	const float SyncInterval = 0.1f;
	/** The match overlay sits under the HUD notifications (500) and the menus (1000). */
	const int32 MatchHUDZOrder = 400;
	/** Room the match overlay's feed takes in the top right corner (five lines). */
	const float FeedHeight = 190.f;
	/** Seconds an invite dialog waits for an answer before it declines. */
	const float InviteTimeout = 30.f;
}

/** Hands every key press to the flow before the focused widget or the game sees it. */
class FALKOnlineInputProcessor : public IInputProcessor
{
public:
	explicit FALKOnlineInputProcessor(UALKOnlineFlowSubsystem* InOwner)
		: Owner(InOwner)
	{
	}

	virtual void Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor) override {}

	virtual bool HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) override
	{
		UALKOnlineFlowSubsystem* Flow = Owner.Get();
		return Flow && Flow->HandleKey(InKeyEvent.GetKey(), true, InKeyEvent.IsRepeat());
	}

	virtual bool HandleKeyUpEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) override
	{
		UALKOnlineFlowSubsystem* Flow = Owner.Get();
		return Flow && Flow->HandleKey(InKeyEvent.GetKey(), false, false);
	}

	virtual const TCHAR* GetDebugName() const override { return TEXT("ALKOnlineFlow"); }

private:
	TWeakObjectPtr<UALKOnlineFlowSubsystem> Owner;
};

void UALKOnlineFlowSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Collection.InitializeDependency<UALKMenuSubsystem>();
	Collection.InitializeDependency<UALKHUDSubsystem>();

	if (UALKOnlineSubsystem* Online = GetOnline())
	{
		Online->OnStateChangedNative.AddUObject(this, &ThisClass::HandleOnlineStateChanged);
		Online->OnConnectionErrorNative.AddUObject(this, &ThisClass::HandleConnectionError);
		Online->OnMatchmakingChangedNative.AddUObject(this, &ThisClass::HandleMatchmakingChanged);
		Online->OnInviteReceivedNative.AddUObject(this, &ThisClass::HandleInviteReceived);
	}

	TickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &ThisClass::TickFlow), ALKOnlineFlowPrivate::SyncInterval);

	if (FSlateApplication::IsInitialized())
	{
		InputProcessor = MakeShared<FALKOnlineInputProcessor>(this);
		FSlateApplication::Get().RegisterInputPreProcessor(InputProcessor);
	}
}

void UALKOnlineFlowSubsystem::Deinitialize()
{
	if (InputProcessor.IsValid())
	{
		if (FSlateApplication::IsInitialized())
		{
			FSlateApplication::Get().UnregisterInputPreProcessor(InputProcessor);
		}
		InputProcessor.Reset();
	}
	if (TickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
		TickerHandle.Reset();
	}
	if (UALKOnlineSubsystem* Online = GetOnline())
	{
		Online->OnStateChangedNative.RemoveAll(this);
		Online->OnConnectionErrorNative.RemoveAll(this);
		Online->OnMatchmakingChangedNative.RemoveAll(this);
		Online->OnInviteReceivedNative.RemoveAll(this);
	}
	if (UALKPlayerNetComponent* NetComponent = BoundNetComponent.Get())
	{
		NetComponent->OnServerNoticeNative.Remove(NoticeHandle);
	}
	BoundNetComponent.Reset();
	RemoveMatchHUD();
	Super::Deinitialize();
}

UALKOnlineFlowSubsystem* UALKOnlineFlowSubsystem::Get(const UObject* WorldContextObject)
{
	const ULocalPlayer* LocalPlayer = nullptr;
	if (const UUserWidget* Widget = Cast<UUserWidget>(WorldContextObject))
	{
		LocalPlayer = Widget->GetOwningLocalPlayer();
	}
	if (!LocalPlayer)
	{
		const UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
		const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
		LocalPlayer = GameInstance ? GameInstance->GetFirstGamePlayer() : nullptr;
	}
	return LocalPlayer ? LocalPlayer->GetSubsystem<UALKOnlineFlowSubsystem>() : nullptr;
}

UALKOnlineSubsystem* UALKOnlineFlowSubsystem::GetOnline() const
{
	const ULocalPlayer* LocalPlayer = GetLocalPlayer();
	const UGameInstance* GameInstance = LocalPlayer ? LocalPlayer->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<UALKOnlineSubsystem>() : nullptr;
}

UALKMenuSubsystem* UALKOnlineFlowSubsystem::GetMenu() const
{
	const ULocalPlayer* LocalPlayer = GetLocalPlayer();
	return LocalPlayer ? LocalPlayer->GetSubsystem<UALKMenuSubsystem>() : nullptr;
}

const FALKOnlineScreenClasses& UALKOnlineFlowSubsystem::GetClasses() const
{
	return UALKDeveloperSettings::GetMenuConfig()->OnlineScreens;
}

bool UALKOnlineFlowSubsystem::IsFlowOwner() const
{
	const ULocalPlayer* LocalPlayer = GetLocalPlayer();
	return LocalPlayer && LocalPlayer->IsPrimaryPlayer();
}

APlayerController* UALKOnlineFlowSubsystem::GetReadyController() const
{
	const ULocalPlayer* LocalPlayer = GetLocalPlayer();
	UWorld* World = LocalPlayer ? LocalPlayer->GetWorld() : nullptr;
	APlayerController* Controller = World ? LocalPlayer->GetPlayerController(World) : nullptr;
	const FWorldContext* Context = Controller && GEngine ? GEngine->GetWorldContextFromWorld(World) : nullptr;
	if (!Context || !World->HasBegunPlay() || World->bIsTearingDown || !Context->TravelURL.IsEmpty() || Context->PendingNetGame != nullptr || World->IsInSeamlessTravel())
	{
		return nullptr;
	}
	return Controller;
}

UCommonActivatableWidget* UALKOnlineFlowSubsystem::Push(const TSoftClassPtr<UCommonActivatableWidget>& ScreenClass, EALKLayer Layer, const TCHAR* What)
{
	UALKMenuSubsystem* Menu = GetMenu();
	UClass* Class = ScreenClass.LoadSynchronous();
	if (!Menu || !Class)
	{
		if (!ReportedMissingClasses.Contains(ScreenClass.ToSoftObjectPath()))
		{
			ReportedMissingClasses.Add(ScreenClass.ToSoftObjectPath());
			UE_LOG(LogALK, Warning, TEXT("Online flow: no widget class for %s (menu config: Online Screens)."), What);
		}
		return nullptr;
	}
	return Menu->PushScreen(Class, Layer);
}

UCommonActivatableWidget* UALKOnlineFlowSubsystem::FindOpenScreen(const TSoftClassPtr<UCommonActivatableWidget>& ScreenClass, EALKLayer Layer) const
{
	UALKMenuSubsystem* Menu = GetMenu();
	const UClass* Class = ScreenClass.Get();
	const UALKRootLayout* Layout = Menu && Class && GetReadyController() ? Menu->GetRootLayout() : nullptr;
	const UCommonActivatableWidgetContainerBase* Container = Layout ? Layout->GetLayer(Layer) : nullptr;
	if (Container)
	{
		for (UCommonActivatableWidget* Widget : Container->GetWidgetList())
		{
			if (Widget && Widget->IsA(Class))
			{
				return Widget;
			}
		}
	}
	return nullptr;
}

void UALKOnlineFlowSubsystem::SetPlayerContext(const FString& UserId, const FString& DisplayName)
{
	ContextUserId = UserId;
	ContextDisplayName = DisplayName;
}

// ---------------------------------------------------------------- Screens

UCommonActivatableWidget* UALKOnlineFlowSubsystem::OpenPlayHub()
{
	return Push(GetClasses().PlayHub, EALKLayer::Menu, TEXT("the play hub"));
}

UCommonActivatableWidget* UALKOnlineFlowSubsystem::OpenHostGame(bool bEditCurrentGame)
{
	bEditingCurrentGame = bEditCurrentGame;
	return Push(GetClasses().HostGame, EALKLayer::Menu, TEXT("the host game screen"));
}

UCommonActivatableWidget* UALKOnlineFlowSubsystem::OpenServerBrowser()
{
	return Push(GetClasses().ServerBrowser, EALKLayer::Menu, TEXT("the server browser"));
}

UCommonActivatableWidget* UALKOnlineFlowSubsystem::OpenSocial()
{
	return Push(GetClasses().Social, EALKLayer::Menu, TEXT("the friends and party screen"));
}

UCommonActivatableWidget* UALKOnlineFlowSubsystem::OpenLobby()
{
	return Push(GetClasses().Lobby, EALKLayer::Menu, TEXT("the lobby"));
}

UCommonActivatableWidget* UALKOnlineFlowSubsystem::OpenResults()
{
	return Push(GetClasses().Results, EALKLayer::Menu, TEXT("the results screen"));
}

UCommonActivatableWidget* UALKOnlineFlowSubsystem::OpenLeaderboards(FName BoardId)
{
	ContextBoardId = BoardId;
	return Push(GetClasses().Leaderboards, EALKLayer::Menu, TEXT("the leaderboards"));
}

UCommonActivatableWidget* UALKOnlineFlowSubsystem::OpenLoadout()
{
	return Push(GetClasses().Loadout, EALKLayer::Menu, TEXT("the loadout screen"));
}

UCommonActivatableWidget* UALKOnlineFlowSubsystem::OpenAdminPanel()
{
	return Push(GetClasses().AdminPanel, EALKLayer::Menu, TEXT("the host's panel"));
}

UCommonActivatableWidget* UALKOnlineFlowSubsystem::ShowPlayerOptions(const FString& UserId, const FString& DisplayName)
{
	SetPlayerContext(UserId, DisplayName);
	return Push(GetClasses().PlayerOptionsDialog, EALKLayer::Modal, TEXT("the player options dialog"));
}

UCommonActivatableWidget* UALKOnlineFlowSubsystem::ShowProfile(const FString& UserId, const FString& DisplayName)
{
	SetPlayerContext(UserId, DisplayName);
	return Push(GetClasses().ProfileDialog, EALKLayer::Modal, TEXT("the profile dialog"));
}

UCommonActivatableWidget* UALKOnlineFlowSubsystem::ShowReport(const FString& UserId, const FString& DisplayName)
{
	SetPlayerContext(UserId, DisplayName);
	return Push(GetClasses().ReportDialog, EALKLayer::Modal, TEXT("the report dialog"));
}

UALKTextEntryDialog* UALKOnlineFlowSubsystem::ShowTextEntry(const FALKTextEntryDescriptor& Descriptor, TFunction<void(bool, const FString&, const FString&)> OnResult)
{
	UALKMenuSubsystem* Menu = GetMenu();
	UALKRootLayout* Layout = Menu ? Menu->GetRootLayout() : nullptr;
	if (!Layout)
	{
		return nullptr;
	}
	TSubclassOf<UCommonActivatableWidget> DialogClass = GetClasses().TextEntryDialog.LoadSynchronous();
	if (!DialogClass || !DialogClass->IsChildOf(UALKTextEntryDialog::StaticClass()))
	{
		DialogClass = UALKTextEntryDialog::StaticClass();
	}
	return Layout->PushWidget<UALKTextEntryDialog>(EALKLayer::Modal, DialogClass, [&Descriptor, &OnResult](UALKTextEntryDialog& Dialog)
	{
		Dialog.SetupDialog(Descriptor, MoveTemp(OnResult));
	});
}

UALKTextEntryDialog* UALKOnlineFlowSubsystem::K2_ShowTextEntry(const FALKTextEntryDescriptor& Descriptor, const FALKTextEntryResultDelegate& OnResult)
{
	return ShowTextEntry(Descriptor, [OnResult](bool bConfirmed, const FString& Text, const FString& SecondText)
	{
		OnResult.ExecuteIfBound(bConfirmed, Text, SecondText);
	});
}

void UALKOnlineFlowSubsystem::OpenJoinByCode()
{
	FALKTextEntryDescriptor Descriptor;
	Descriptor.Title = LOCTEXT("JoinByCodeTitle", "Join by Code");
	Descriptor.Message = LOCTEXT("JoinByCodeMessage", "Enter the lobby code your host shared with you, or the address of the game.");
	Descriptor.HintText = LOCTEXT("JoinByCodeHint", "Lobby code or address");
	Descriptor.ConfirmText = LOCTEXT("JoinByCodeConfirm", "Join");
	Descriptor.MaxLength = 64;
	Descriptor.bSecondField = true;
	Descriptor.SecondHintText = LOCTEXT("JoinByCodePassword", "Password (only if the game has one)");
	Descriptor.bSecondIsPassword = true;

	TWeakObjectPtr<UALKOnlineFlowSubsystem> WeakThis(this);
	ShowTextEntry(Descriptor, [WeakThis](bool bConfirmed, const FString& Text, const FString& Password)
	{
		UALKOnlineSubsystem* Online = WeakThis.IsValid() ? WeakThis->GetOnline() : nullptr;
		if (bConfirmed && Online)
		{
			Online->JoinByCode(Text, Password);
		}
	});
}

void UALKOnlineFlowSubsystem::JoinGameWithPrompt(const FALKSessionInfo& Session)
{
	UALKOnlineSubsystem* Online = GetOnline();
	if (!Online)
	{
		return;
	}
	if (!Session.bPasswordProtected)
	{
		Online->JoinGame(Session, FString());
		return;
	}

	FALKTextEntryDescriptor Descriptor;
	Descriptor.Title = LOCTEXT("PasswordTitle", "Password Needed");
	Descriptor.Message = FText::Format(LOCTEXT("PasswordMessage", "{0} is protected by a password."), FText::FromString(Session.ServerName));
	Descriptor.HintText = LOCTEXT("PasswordHint", "Password");
	Descriptor.ConfirmText = LOCTEXT("JoinByCodeConfirm", "Join");
	Descriptor.bIsPassword = true;
	Descriptor.MaxLength = 64;

	TWeakObjectPtr<UALKOnlineFlowSubsystem> WeakThis(this);
	ShowTextEntry(Descriptor, [WeakThis, Session](bool bConfirmed, const FString& Password, const FString&)
	{
		UALKOnlineSubsystem* OnlineNow = WeakThis.IsValid() ? WeakThis->GetOnline() : nullptr;
		if (bConfirmed && OnlineNow)
		{
			OnlineNow->JoinGame(Session, Password);
		}
	});
}

void UALKOnlineFlowSubsystem::LeaveGameWithConfirm()
{
	UALKMenuSubsystem* Menu = GetMenu();
	UALKOnlineSubsystem* Online = GetOnline();
	if (!Menu || !Online)
	{
		return;
	}

	FALKDialogDescriptor Descriptor;
	Descriptor.Title = LOCTEXT("LeaveGameTitle", "Leave the Game?");
	Descriptor.Message = Online->IsHost()
		? LOCTEXT("LeaveGameHostMessage", "You are the host: leaving closes the game for everyone.")
		: LOCTEXT("LeaveGameMessage", "You will return to the main menu.");
	Descriptor.ConfirmText = LOCTEXT("LeaveGameConfirm", "Leave");
	Descriptor.CancelText = LOCTEXT("Cancel", "Cancel");
	Descriptor.bDestructive = true;

	TWeakObjectPtr<UALKOnlineFlowSubsystem> WeakThis(this);
	Menu->ShowConfirmDialog(Descriptor, [WeakThis](EALKDialogResult Result)
	{
		UALKOnlineSubsystem* OnlineNow = WeakThis.IsValid() ? WeakThis->GetOnline() : nullptr;
		if (Result == EALKDialogResult::Confirmed && OnlineNow)
		{
			// The player chose to go: nothing to rejoin.
			OnlineNow->ClearRejoin();
			OnlineNow->LeaveGame();
		}
	});
}

void UALKOnlineFlowSubsystem::CopyToClipboard(const FString& Text, const FText& Notice)
{
	FPlatformApplicationMisc::ClipboardCopy(*Text);
	const ULocalPlayer* LocalPlayer = GetLocalPlayer();
	if (UALKHUDSubsystem* HUD = LocalPlayer ? LocalPlayer->GetSubsystem<UALKHUDSubsystem>() : nullptr)
	{
		HUD->ShowNotification(Notice.IsEmpty() ? LOCTEXT("CopiedNotice", "Copied") : Notice, FText::GetEmpty(), EALKIcon::Copy, 2.f);
	}
}

void UALKOnlineFlowSubsystem::ShowNotice(const FText& Title, const FText& Detail)
{
	const ULocalPlayer* LocalPlayer = GetLocalPlayer();
	if (UALKHUDSubsystem* HUD = LocalPlayer ? LocalPlayer->GetSubsystem<UALKHUDSubsystem>() : nullptr)
	{
		HUD->ShowNotification(Title, Detail, EALKIcon::Info, 3.5f);
	}
}

// ---------------------------------------------------------------- Keeping the right screen up

EALKOnlineStage UALKOnlineFlowSubsystem::ComputeStage(const UWorld* World) const
{
	const UALKOnlineSubsystem* Online = GetOnline();
	const AALKLobbyGameState* GameState = World ? World->GetGameState<AALKLobbyGameState>() : nullptr;
	if (!Online || !GameState || !Online->IsInGame() || World->GetNetMode() == NM_Standalone)
	{
		return EALKOnlineStage::None;
	}
	switch (GameState->Phase)
	{
	case EALKLobbyPhase::InMatch: return EALKOnlineStage::Match;
	case EALKLobbyPhase::PostMatch: return EALKOnlineStage::Results;
	default: return EALKOnlineStage::Lobby;
	}
}

bool UALKOnlineFlowSubsystem::TickFlow(float DeltaTime)
{
	if (!IsFlowOwner())
	{
		return true;
	}
	APlayerController* Controller = GetReadyController();
	if (!Controller)
	{
		return true;
	}

	UWorld* World = Controller->GetWorld();
	const EALKOnlineStage NewStage = ComputeStage(World);
	if (NewStage != Stage || World != StageWorld.Get())
	{
		SetStage(NewStage, World);
	}
	if (bAutomaticScreens)
	{
		MaintainStage(Controller);
		ShowQueuedInvite();
	}
	BindNetComponent();
	return true;
}

void UALKOnlineFlowSubsystem::SetStage(EALKOnlineStage NewStage, UWorld* World)
{
	const EALKOnlineStage OldStage = Stage;
	const bool bSameWorld = World == StageWorld.Get();
	Stage = NewStage;
	StageWorld = World;
	PendingChatKey = FKey();
	if (!bSameWorld)
	{
		BindOtherMenus();
	}

	if (bAutomaticScreens)
	{
		// A match that starts or ends under an open menu (the pause menu, the lobby of the same level) clears it.
		if (bSameWorld && (NewStage == EALKOnlineStage::Match || NewStage == EALKOnlineStage::Results))
		{
			if (UALKMenuSubsystem* Menu = GetMenu())
			{
				Menu->CloseAllMenus();
			}
		}
		if (NewStage != EALKOnlineStage::Match)
		{
			RemoveMatchHUD();
		}
		// The match overlay's feed has the top right corner: the HUD's notifications go under it meanwhile.
		const ULocalPlayer* LocalPlayer = GetLocalPlayer();
		if (UALKHUDSubsystem* HUD = LocalPlayer ? LocalPlayer->GetSubsystem<UALKHUDSubsystem>() : nullptr)
		{
			HUD->SetNotificationDrop(NewStage == EALKOnlineStage::Match ? ALKOnlineFlowPrivate::FeedHeight : 0.f);
		}
	}

	if (NewStage != OldStage)
	{
		OnStageChanged.Broadcast(NewStage, OldStage);
		OnStageChangedNative.Broadcast(NewStage, OldStage);
	}
}

void UALKOnlineFlowSubsystem::MaintainStage(APlayerController* Controller)
{
	switch (Stage)
	{
	case EALKOnlineStage::Lobby:
		if (!FindOpenScreen(GetClasses().Lobby, EALKLayer::Menu))
		{
			GetClasses().Lobby.LoadSynchronous();
			if (!FindOpenScreen(GetClasses().Lobby, EALKLayer::Menu))
			{
				OpenLobby();
			}
		}
		break;
	case EALKOnlineStage::Match:
		EnsureMatchHUD(Controller);
		break;
	case EALKOnlineStage::Results:
		if (!FindOpenScreen(GetClasses().Results, EALKLayer::Menu))
		{
			GetClasses().Results.LoadSynchronous();
			if (!FindOpenScreen(GetClasses().Results, EALKLayer::Menu))
			{
				OpenResults();
			}
		}
		break;
	default:
		break;
	}
}

void UALKOnlineFlowSubsystem::EnsureMatchHUD(APlayerController* Controller)
{
	if (MatchHUD && MatchHUD->GetWorld() == Controller->GetWorld() && MatchHUD->IsInViewport())
	{
		return;
	}
	RemoveMatchHUD();

	const TSoftClassPtr<UUserWidget>& HUDClass = GetClasses().MatchHUD;
	UClass* Class = HUDClass.LoadSynchronous();
	if (!Class)
	{
		if (!ReportedMissingClasses.Contains(HUDClass.ToSoftObjectPath()))
		{
			ReportedMissingClasses.Add(HUDClass.ToSoftObjectPath());
			UE_LOG(LogALK, Warning, TEXT("Online flow: no widget class for the match overlay (menu config: Online Screens)."));
		}
		return;
	}
	MatchHUD = CreateWidget<UUserWidget>(Controller, Class);
	if (MatchHUD)
	{
		MatchHUD->AddToPlayerScreen(ALKOnlineFlowPrivate::MatchHUDZOrder);
	}
}

void UALKOnlineFlowSubsystem::RemoveMatchHUD()
{
	if (MatchHUD)
	{
		MatchHUD->RemoveFromParent();
		MatchHUD = nullptr;
	}
}

void UALKOnlineFlowSubsystem::BindNetComponent()
{
	const UALKOnlineSubsystem* Online = GetOnline();
	UALKPlayerNetComponent* NetComponent = Online ? Online->GetLocalNetComponent() : nullptr;
	if (NetComponent == BoundNetComponent.Get())
	{
		return;
	}
	if (UALKPlayerNetComponent* Old = BoundNetComponent.Get())
	{
		Old->OnServerNoticeNative.Remove(NoticeHandle);
	}
	BoundNetComponent = NetComponent;
	NoticeHandle.Reset();
	if (NetComponent)
	{
		NoticeHandle = NetComponent->OnServerNoticeNative.AddUObject(this, &ThisClass::HandleServerNotice);
	}
}

// ---------------------------------------------------------------- The online subsystem's events

void UALKOnlineFlowSubsystem::HandleOnlineStateChanged(EALKOnlineState NewState, EALKOnlineState OldState)
{
	if (!IsFlowOwner() || !bAutomaticScreens)
	{
		return;
	}
	// Hosting, joining and leaving take a moment: a dialog says so and offers Cancel. It closes itself.
	const bool bBusy = NewState == EALKOnlineState::Hosting || NewState == EALKOnlineState::Joining || NewState == EALKOnlineState::Leaving;
	if (bBusy && GetReadyController())
	{
		GetClasses().ConnectingDialog.LoadSynchronous();
		if (!FindOpenScreen(GetClasses().ConnectingDialog, EALKLayer::Modal))
		{
			Push(GetClasses().ConnectingDialog, EALKLayer::Modal, TEXT("the connecting dialog"));
		}
	}
}

void UALKOnlineFlowSubsystem::HandleConnectionError(EALKConnectionError Error, const FText& Title, const FText& Message)
{
	UALKOnlineSubsystem* Online = GetOnline();
	UALKMenuSubsystem* Menu = GetMenu();
	if (!IsFlowOwner() || !bAutomaticScreens || !Online || !Menu)
	{
		return;
	}
	// Taking the error marks it as shown; a menu of the game's own that took it first keeps it.
	EALKConnectionError TakenError = EALKConnectionError::None;
	FText TakenTitle;
	FText TakenMessage;
	if (!Online->ConsumePendingConnectionError(TakenError, TakenTitle, TakenMessage) || TakenError == EALKConnectionError::Cancelled)
	{
		return;
	}

	const bool bCanRejoin = Online->CanRejoinMatch();
	FALKDialogDescriptor Descriptor;
	Descriptor.Title = TakenTitle;
	Descriptor.Message = bCanRejoin
		? FText::Format(LOCTEXT("ErrorRejoinMessage", "{0}\n\nThe match is still running: you can rejoin it."), TakenMessage)
		: TakenMessage;
	Descriptor.ConfirmText = bCanRejoin ? LOCTEXT("Rejoin", "Rejoin") : LOCTEXT("OK", "OK");
	if (bCanRejoin)
	{
		Descriptor.CancelText = LOCTEXT("Close", "Close");
	}

	TWeakObjectPtr<UALKOnlineFlowSubsystem> WeakThis(this);
	Menu->ShowConfirmDialog(Descriptor, [WeakThis, bCanRejoin](EALKDialogResult Result)
	{
		UALKOnlineSubsystem* OnlineNow = WeakThis.IsValid() ? WeakThis->GetOnline() : nullptr;
		if (bCanRejoin && Result == EALKDialogResult::Confirmed && OnlineNow)
		{
			OnlineNow->RejoinMatch();
		}
	});
	UALKTheme::PlayUISound(this, EALKUISound::Error);
}

void UALKOnlineFlowSubsystem::HandleMatchmakingChanged(EALKMatchmakingState NewState)
{
	const EALKMatchmakingState OldState = LastMatchmakingState;
	LastMatchmakingState = NewState;
	if (!IsFlowOwner() || !bAutomaticScreens || NewState == OldState || !GetReadyController())
	{
		return;
	}
	if (NewState == EALKMatchmakingState::MatchFound)
	{
		GetClasses().MatchFoundDialog.LoadSynchronous();
		if (!FindOpenScreen(GetClasses().MatchFoundDialog, EALKLayer::Modal))
		{
			Push(GetClasses().MatchFoundDialog, EALKLayer::Modal, TEXT("the match found dialog"));
		}
	}
	else if (NewState == EALKMatchmakingState::Failed)
	{
		const UALKOnlineSubsystem* Online = GetOnline();
		FText Title;
		FText Message;
		UALKLobbyStatics::GetConnectionErrorText(Online ? Online->GetMatchmakingError() : EALKConnectionError::Unknown, Title, Message);
		ShowNotice(LOCTEXT("MatchmakingFailed", "No match found"), Message);
		UALKTheme::PlayUISound(this, EALKUISound::Error);
	}
}

void UALKOnlineFlowSubsystem::HandleInviteReceived(const FALKInvite& Invite)
{
	if (!IsFlowOwner() || !bAutomaticScreens)
	{
		return;
	}
	QueuedInvites.Add(Invite);
	UALKTheme::PlayUISound(this, EALKUISound::Notify);
	if (Stage == EALKOnlineStage::Match)
	{
		// Not in the middle of a match: the question waits for the results screen.
		ShowNotice(FText::Format(LOCTEXT("InviteWaiting", "Invite from {0}"), FText::FromString(Invite.FromName)), LOCTEXT("InviteWaitingDetail", "You can answer after the match"));
	}
}

void UALKOnlineFlowSubsystem::ShowQueuedInvite()
{
	if (QueuedInvites.Num() == 0 || Stage == EALKOnlineStage::Match || InviteDialog.IsValid())
	{
		return;
	}
	UALKMenuSubsystem* Menu = GetMenu();
	UALKOnlineSubsystem* Online = GetOnline();
	if (!Menu || !Online)
	{
		return;
	}
	const FALKInvite Invite = QueuedInvites[0];
	QueuedInvites.RemoveAt(0);

	const FText From = FText::FromString(Invite.FromName);
	FALKDialogDescriptor Descriptor;
	if (Invite.bPartyInvite)
	{
		Descriptor.Title = LOCTEXT("PartyInviteTitle", "Party Invite");
		Descriptor.Message = FText::Format(LOCTEXT("PartyInviteMessage", "{0} invited you to their party."), From);
	}
	else
	{
		Descriptor.Title = LOCTEXT("GameInviteTitle", "Game Invite");
		Descriptor.Message = Invite.Session.ServerName.IsEmpty()
			? FText::Format(LOCTEXT("GameInviteMessage", "{0} invited you to their game."), From)
			: FText::Format(LOCTEXT("GameInviteNamedMessage", "{0} invited you to {1}."), From, FText::FromString(Invite.Session.ServerName));
		if (Online->IsInGame())
		{
			Descriptor.Message = FText::Format(LOCTEXT("GameInviteLeaves", "{0}\n\nAccepting leaves the game you are in."), Descriptor.Message);
		}
	}
	Descriptor.ConfirmText = LOCTEXT("Accept", "Accept");
	Descriptor.CancelText = LOCTEXT("Decline", "Decline");
	Descriptor.TimeoutSeconds = ALKOnlineFlowPrivate::InviteTimeout;

	TWeakObjectPtr<UALKOnlineFlowSubsystem> WeakThis(this);
	InviteDialog = Menu->ShowConfirmDialog(Descriptor, [WeakThis, Invite](EALKDialogResult Result)
	{
		UALKOnlineFlowSubsystem* Self = WeakThis.Get();
		if (UALKOnlineSubsystem* OnlineNow = Self ? Self->GetOnline() : nullptr)
		{
			OnlineNow->RespondToInvite(Invite, Result == EALKDialogResult::Confirmed);
		}
		if (Self)
		{
			Self->InviteDialog.Reset();
		}
	});
}

void UALKOnlineFlowSubsystem::HandleServerNotice(const FText& Text)
{
	if (IsFlowOwner() && bAutomaticScreens && !Text.IsEmpty())
	{
		ShowNotice(Text, FText::GetEmpty());
	}
}

// ---------------------------------------------------------------- Beside another menu system

void UALKOnlineFlowSubsystem::BindOtherMenus()
{
	const ULocalPlayer* LocalPlayer = GetLocalPlayer();
	if (!LocalPlayer || UALKDeveloperSettings::GetMenuConfig()->OtherMenuEntries.Num() == 0)
	{
		return;
	}
	BoundOtherMenus.RemoveAll([](const TWeakObjectPtr<UObject>& Each) { return !Each.IsValid(); });

	// Any other local player subsystem with an "On Menu Action (Entry Id)" event: the Assetpro menu plugins all have one.
	// Found by name and signature, so nothing here depends on those plugins.
	static const FName EventName(TEXT("OnMenuAction"));
	for (TObjectIterator<ULocalPlayerSubsystem> It; It; ++It)
	{
		ULocalPlayerSubsystem* Subsystem = *It;
		if (Subsystem == this || Subsystem->IsA<UALKMenuSubsystem>() || Subsystem->GetOuter() != LocalPlayer || BoundOtherMenus.Contains(Subsystem))
		{
			continue;
		}
		const FMulticastDelegateProperty* Event = FindFProperty<FMulticastDelegateProperty>(Subsystem->GetClass(), EventName);
		const UFunction* Signature = Event ? Event->SignatureFunction.Get() : nullptr;
		if (!Signature || Signature->NumParms != 1 || !CastField<FNameProperty>(Signature->PropertyLink))
		{
			continue;
		}
		FScriptDelegate Delegate;
		Delegate.BindUFunction(this, GET_FUNCTION_NAME_CHECKED(UALKOnlineFlowSubsystem, HandleOtherMenuAction));
		Event->AddDelegate(MoveTemp(Delegate), Subsystem);
		BoundOtherMenus.Add(Subsystem);
		UE_LOG(LogALK, Log, TEXT("Online flow: listening to the menu actions of %s."), *Subsystem->GetClass()->GetName());
	}
}

void UALKOnlineFlowSubsystem::HandleOtherMenuAction(FName EntryId)
{
	const EALKMenuAction* Action = UALKDeveloperSettings::GetMenuConfig()->OtherMenuEntries.Find(EntryId);
	UALKMenuSubsystem* Menu = GetMenu();
	if (Action && Menu && IsFlowOwner())
	{
		Menu->ExecuteMenuEntry(FALKMenuEntry(EntryId, FText::GetEmpty(), *Action));
	}
}

// ---------------------------------------------------------------- Keys

bool UALKOnlineFlowSubsystem::IsTypingText() const
{
	if (!FSlateApplication::IsInitialized())
	{
		return false;
	}
	const TSharedPtr<SWidget> Focused = FSlateApplication::Get().GetKeyboardFocusedWidget();
	if (!Focused.IsValid())
	{
		return false;
	}
	static const FName EditableText(TEXT("SEditableText"));
	static const FName MultiLineEditableText(TEXT("SMultiLineEditableText"));
	const FName Type = Focused->GetType();
	return Type == EditableText || Type == MultiLineEditableText;
}

bool UALKOnlineFlowSubsystem::IsOurWindowActive() const
{
	// Several games can share one process (play in editor with two players): each reads the keys of its own window only.
	const ULocalPlayer* LocalPlayer = GetLocalPlayer();
	UGameViewportClient* Viewport = LocalPlayer ? LocalPlayer->ViewportClient.Get() : nullptr;
	if (!Viewport || !FSlateApplication::IsInitialized())
	{
		return false;
	}
	const TSharedPtr<SWindow> Active = FSlateApplication::Get().GetActiveTopLevelWindow();
	const TSharedPtr<SWindow> Ours = Viewport->GetWindow();
	return !Active.IsValid() || !Ours.IsValid() || Active == Ours;
}

bool UALKOnlineFlowSubsystem::HandleKey(const FKey& Key, bool bDown, bool bRepeat)
{
	UALKOnlineSubsystem* Online = GetOnline();
	if (!Online || !Online->IsInGame() || !IsFlowOwner() || !IsOurWindowActive())
	{
		return false;
	}
	const bool bTyping = IsTypingText();
	const FALKOnlineKeys& Keys = UALKDeveloperSettings::GetMenuConfig()->OnlineKeys;

	// Push to talk works in the lobby and in the match, but never takes a letter from a chat line.
	if (Online->GetVoiceMode() == EALKVoiceMode::PushToTalk)
	{
		const ULocalPlayer* LocalPlayer = GetLocalPlayer();
		const UGameInstance* GameInstance = LocalPlayer ? LocalPlayer->GetGameInstance() : nullptr;
		const UALKSettingsSubsystem* Settings = GameInstance ? GameInstance->GetSubsystem<UALKSettingsSubsystem>() : nullptr;
		const bool bTalkKey = (Settings && Key.IsValid() && Key == Settings->GetPushToTalkKey()) || (Keys.PushToTalkGamepad.IsValid() && Key == Keys.PushToTalkGamepad);
		if (bTalkKey && !bRepeat && (!bTyping || !bDown))
		{
			Online->SetPushToTalkActive(bDown);
			return false;
		}
	}

	// The match keys: only while the game itself has the screen.
	UALKMatchHUDBase* HUD = Cast<UALKMatchHUDBase>(MatchHUD);
	const UALKMenuSubsystem* Menu = GetMenu();
	if (!HUD || Stage != EALKOnlineStage::Match || !bAutomaticScreens || (Menu && Menu->IsAnyMenuOpen()))
	{
		PendingChatKey = FKey();
		return false;
	}
	if (bTyping)
	{
		// The chat line has the keyboard. A chat key that went down before it opened no longer counts.
		PendingChatKey = FKey();
		return false;
	}

	if (Key == Keys.Scoreboard || Key == Keys.ScoreboardGamepad)
	{
		if (!bRepeat)
		{
			HUD->SetScoreboardVisible(bDown);
		}
		return true;
	}

	const bool bChatKey = Key == Keys.Chat || Key == Keys.ChatAlternate || Key == Keys.TeamChat;
	if (bChatKey && Online->IsTextChatEnabled())
	{
		// The chat opens when the key comes up, so the key's own letter is not typed into the line.
		if (bDown)
		{
			if (!bRepeat)
			{
				PendingChatKey = Key;
			}
		}
		else if (PendingChatKey == Key)
		{
			PendingChatKey = FKey();
			HUD->OpenChat(Key == Keys.TeamChat);
		}
		return false;
	}

	const AALKLobbyPlayerState* LocalState = UALKLobbyStatics::GetLocalPlayerState(this);
	if (LocalState && LocalState->bSpectating && bDown && !bRepeat)
	{
		if (Key == Keys.SpectatePrevious || Key == Keys.SpectatePreviousGamepad)
		{
			HUD->SpectateStep(-1);
			return true;
		}
		if (Key == Keys.SpectateNext || Key == Keys.SpectateNextGamepad)
		{
			HUD->SpectateStep(1);
			return true;
		}
	}
	return false;
}

#undef LOCTEXT_NAMESPACE
