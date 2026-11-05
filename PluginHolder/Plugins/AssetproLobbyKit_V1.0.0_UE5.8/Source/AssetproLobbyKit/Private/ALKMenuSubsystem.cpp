// Copyright 2026 CodeLikeMe. All Rights Reserved.

#include "ALKMenuSubsystem.h"
#include "OnlineUI/ALKOnlineFlowSubsystem.h"
#include "Online/ALKOnlineSubsystem.h"
#include "AssetproLobbyKit.h"
#include "ALKDeveloperSettings.h"
#include "ALKMenuConfig.h"
#include "Widgets/ALKRootLayout.h"
#include "Screens/ALKScreen.h"
#include "Screens/ALKMainMenuScreen.h"
#include "Screens/ALKSettingsMenuScreen.h"
#include "Screens/ALKPressStartScreen.h"
#include "Screens/ALKPauseMenuScreen.h"
#include "Screens/ALKSettingsScreen.h"
#include "Screens/ALKCreditsScreen.h"
#include "Screens/ALKChapterSelectScreen.h"
#include "Screens/ALKControllerLayoutScreen.h"
#include "Game/ALKChapterProgressSaveGame.h"
#include "Save/ALKSaveSubsystem.h"
#include "Screens/ALKSaveSlotScreen.h"
#include "Game/ALKCameraModifier_Drain.h"
#include "ALKTheme.h"
#include "Camera/PlayerCameraManager.h"
#include "Screens/ALKConfirmDialog.h"
#include "Screens/ALKKeyCaptureDialog.h"
#include "Settings/ALKSettingsSubsystem.h"
#include "CommonActivatableWidget.h"
#include "CommonInputModeTypes.h"
#include "Input/CommonUIActionRouterBase.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "UObject/Package.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ALKMenuSubsystem)

#define LOCTEXT_NAMESPACE "AssetproLobbyKit"

void UALKMenuSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	WorldCleanupHandle = FWorldDelegates::OnWorldCleanup.AddUObject(this, &ThisClass::HandleWorldCleanup);

	if (ULocalPlayer* LocalPlayer = GetLocalPlayer<ULocalPlayer>())
	{
		ControllerChangedHandle = LocalPlayer->OnPlayerControllerChanged().AddUObject(this, &ThisClass::HandlePlayerControllerChanged);
	}
	BindInputModeChanged();
}

void UALKMenuSubsystem::Deinitialize()
{
	if (GameInputTickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(GameInputTickerHandle);
		GameInputTickerHandle.Reset();
	}
	if (DrainTickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(DrainTickerHandle);
		DrainTickerHandle.Reset();
	}
	FWorldDelegates::OnWorldCleanup.Remove(WorldCleanupHandle);
	if (ULocalPlayer* LocalPlayer = GetLocalPlayer<ULocalPlayer>())
	{
		LocalPlayer->OnPlayerControllerChanged().Remove(ControllerChangedHandle);
	}
	if (UCommonUIActionRouterBase* Router = InputModeRouter.Get())
	{
		Router->OnActiveInputModeChanged().Remove(InputModeChangedHandle);
	}
	InputModeRouter.Reset();
	InputModeChangedHandle.Reset();

	if (RootLayout)
	{
		RootLayout->RemoveFromParent();
		RootLayout = nullptr;
	}

	Super::Deinitialize();
}

void UALKMenuSubsystem::BindInputModeChanged()
{
	// CommonUI moves the cursor to the middle of the screen when it switches to menu input, which can be long after the menu
	// screen activated (at startup the level loads behind the loading screen). Buttons would take that for the mouse moving,
	// so every input mode change restarts the moment in which they ignore mouse moves.
	const ULocalPlayer* LocalPlayer = GetLocalPlayer<ULocalPlayer>();
	UCommonUIActionRouterBase* Router = LocalPlayer ? LocalPlayer->GetSubsystem<UCommonUIActionRouterBase>() : nullptr;
	if (Router && Router != InputModeRouter.Get())
	{
		if (UCommonUIActionRouterBase* OldRouter = InputModeRouter.Get())
		{
			OldRouter->OnActiveInputModeChanged().Remove(InputModeChangedHandle);
		}
		InputModeRouter = Router;
		InputModeChangedHandle = Router->OnActiveInputModeChanged().AddUObject(this, &ThisClass::HandleInputModeChanged);
	}
}

void UALKMenuSubsystem::HandleInputModeChanged(ECommonInputMode NewMode)
{
	UALKScreen::StartActivationGracePeriod();
}

APlayerController* UALKMenuSubsystem::GetPlayerController() const
{
	const ULocalPlayer* LocalPlayer = GetLocalPlayer<ULocalPlayer>();
	return LocalPlayer ? LocalPlayer->GetPlayerController(LocalPlayer->GetWorld()) : nullptr;
}

UALKRootLayout* UALKMenuSubsystem::GetRootLayout()
{
	APlayerController* PlayerController = GetPlayerController();
	if (!PlayerController)
	{
		UE_LOG(LogALK, Warning, TEXT("GetRootLayout: the local player has no player controller yet."));
		return nullptr;
	}

	if (RootLayout && (RootLayout->GetWorld() != PlayerController->GetWorld() || !RootLayout->IsInViewport()))
	{
		RootLayout->RemoveFromParent();
		RootLayout = nullptr;
	}

	BindInputModeChanged();
	if (!RootLayout)
	{
		TSubclassOf<UALKRootLayout> LayoutClass = UALKDeveloperSettings::GetMenuConfig()->RootLayoutClass;
		if (!LayoutClass)
		{
			LayoutClass = UALKRootLayout::StaticClass();
		}

		RootLayout = CreateWidget<UALKRootLayout>(PlayerController, LayoutClass);
		if (RootLayout)
		{
			RootLayout->AddToPlayerScreen(1000);
			RootLayout->OnLayersChanged.AddUObject(this, &ThisClass::HandleLayersChanged);
		}
	}

	return RootLayout;
}

UCommonActivatableWidget* UALKMenuSubsystem::PushScreen(TSubclassOf<UCommonActivatableWidget> ScreenClass, EALKLayer Layer)
{
	if (!ScreenClass)
	{
		return nullptr;
	}
	UALKRootLayout* Layout = GetRootLayout();
	return Layout ? Layout->PushWidget(Layer, ScreenClass) : nullptr;
}

void UALKMenuSubsystem::ShowStartupMenu()
{
	if (UALKDeveloperSettings::GetMenuConfig()->bShowPressStartScreen)
	{
		ShowPressStartScreen();
	}
	else
	{
		ShowMainMenu();
	}
}

UALKScreen* UALKMenuSubsystem::ShowMainMenu()
{
	UALKRootLayout* Layout = GetRootLayout();
	if (!Layout)
	{
		return nullptr;
	}

	TSubclassOf<UALKScreen> ScreenClass = UALKDeveloperSettings::GetMenuConfig()->MainMenuScreenClass;
	if (!ScreenClass)
	{
		ScreenClass = UALKMainMenuScreen::StaticClass();
	}

	if (UCommonActivatableWidgetContainerBase* MenuLayer = Layout->GetLayer(EALKLayer::Menu))
	{
		for (UCommonActivatableWidget* Widget : MenuLayer->GetWidgetList())
		{
			if (Widget && Widget->IsA(ScreenClass))
			{
				return Cast<UALKScreen>(Widget);
			}
		}
	}

	return Cast<UALKScreen>(PushScreen(ScreenClass, EALKLayer::Menu));
}

UALKPressStartScreen* UALKMenuSubsystem::ShowPressStartScreen()
{
	TSubclassOf<UALKPressStartScreen> ScreenClass = UALKDeveloperSettings::GetMenuConfig()->PressStartScreenClass;
	return Cast<UALKPressStartScreen>(PushScreen(ScreenClass ? ScreenClass : TSubclassOf<UALKPressStartScreen>(UALKPressStartScreen::StaticClass()), EALKLayer::Menu));
}

UALKPauseMenuScreen* UALKMenuSubsystem::ShowPauseMenu()
{
	if (PauseMenu.IsValid() && PauseMenu->IsActivated())
	{
		return PauseMenu.Get();
	}

	TSubclassOf<UALKPauseMenuScreen> ScreenClass = UALKDeveloperSettings::GetMenuConfig()->PauseMenuScreenClass;
	PauseMenu = Cast<UALKPauseMenuScreen>(PushScreen(ScreenClass ? ScreenClass : TSubclassOf<UALKPauseMenuScreen>(UALKPauseMenuScreen::StaticClass()), EALKLayer::GameMenu));
	HandleLayersChanged();
	return PauseMenu.Get();
}

void UALKMenuSubsystem::HidePauseMenu()
{
	if (RootLayout)
	{
		RootLayout->ClearMenuLayers();
	}
	PauseMenu.Reset();
	HandleLayersChanged();
}

void UALKMenuSubsystem::TogglePauseMenu()
{
	if (IsPauseMenuOpen())
	{
		HidePauseMenu();
	}
	else if (!IsAnyMenuOpen())
	{
		ShowPauseMenu();
	}
}

bool UALKMenuSubsystem::IsPauseMenuOpen() const
{
	return PauseMenu.IsValid() && PauseMenu->IsActivated();
}

UALKSettingsMenuScreen* UALKMenuSubsystem::OpenSettings()
{
	TSubclassOf<UALKSettingsMenuScreen> ScreenClass = UALKDeveloperSettings::GetMenuConfig()->SettingsMenuScreenClass;
	return Cast<UALKSettingsMenuScreen>(PushScreen(ScreenClass ? ScreenClass : TSubclassOf<UALKSettingsMenuScreen>(UALKSettingsMenuScreen::StaticClass()), EALKLayer::Menu));
}

UALKSettingsScreen* UALKMenuSubsystem::OpenSettingsTabs()
{
	UALKRootLayout* Layout = GetRootLayout();
	if (!Layout)
	{
		return nullptr;
	}

	TSubclassOf<UALKSettingsScreen> ScreenClass = UALKDeveloperSettings::GetMenuConfig()->SettingsScreenClass;
	if (!ScreenClass)
	{
		ScreenClass = UALKSettingsScreen::StaticClass();
	}
	// Screens are pooled, so one used as a page before is turned back into the tabbed screen.
	return Layout->PushWidget<UALKSettingsScreen>(EALKLayer::Menu, ScreenClass, [](UALKSettingsScreen& Screen)
	{
		Screen.SetTabbed();
	});
}

UALKSettingsScreen* UALKMenuSubsystem::OpenSettingsPage(const FText& Title, const TArray<EALKSettingsCategory>& Categories)
{
	UALKRootLayout* Layout = GetRootLayout();
	if (!Layout)
	{
		return nullptr;
	}

	TSubclassOf<UALKSettingsScreen> ScreenClass = UALKDeveloperSettings::GetMenuConfig()->SettingsScreenClass;
	if (!ScreenClass)
	{
		ScreenClass = UALKSettingsScreen::StaticClass();
	}
	return Layout->PushWidget<UALKSettingsScreen>(EALKLayer::Menu, ScreenClass, [&Title, &Categories](UALKSettingsScreen& Screen)
	{
		Screen.SetPage(Title, Categories);
	});
}

UALKCreditsScreen* UALKMenuSubsystem::OpenCredits()
{
	TSubclassOf<UALKCreditsScreen> ScreenClass = UALKDeveloperSettings::GetMenuConfig()->CreditsScreenClass;
	return Cast<UALKCreditsScreen>(PushScreen(ScreenClass ? ScreenClass : TSubclassOf<UALKCreditsScreen>(UALKCreditsScreen::StaticClass()), EALKLayer::Menu));
}

UALKChapterSelectScreen* UALKMenuSubsystem::OpenChapterSelect()
{
	TSubclassOf<UALKChapterSelectScreen> ScreenClass = UALKDeveloperSettings::GetMenuConfig()->ChapterSelectScreenClass;
	return Cast<UALKChapterSelectScreen>(PushScreen(ScreenClass ? ScreenClass : TSubclassOf<UALKChapterSelectScreen>(UALKChapterSelectScreen::StaticClass()), EALKLayer::Menu));
}

UALKSaveSlotScreen* UALKMenuSubsystem::OpenSaveSlots(bool bSave)
{
	UALKRootLayout* Layout = GetRootLayout();
	if (!Layout)
	{
		return nullptr;
	}
	TSubclassOf<UALKSaveSlotScreen> ScreenClass = UALKDeveloperSettings::GetMenuConfig()->SaveSlotScreenClass;
	if (!ScreenClass)
	{
		ScreenClass = UALKSaveSlotScreen::StaticClass();
	}
	// Screens are pooled: the same screen serves both, set to the mode asked for.
	return Layout->PushWidget<UALKSaveSlotScreen>(EALKLayer::Menu, ScreenClass, [bSave](UALKSaveSlotScreen& Screen)
	{
		Screen.SetSaveMode(bSave);
	});
}

UALKSaveSlotScreen* UALKMenuSubsystem::OpenLoadGame()
{
	return OpenSaveSlots(false);
}

UALKSaveSlotScreen* UALKMenuSubsystem::OpenSaveGame()
{
	return OpenSaveSlots(true);
}

UALKSaveSubsystem* UALKMenuSubsystem::GetSaveSubsystem() const
{
	const ULocalPlayer* LocalPlayer = GetLocalPlayer<ULocalPlayer>();
	const UGameInstance* GameInstance = LocalPlayer ? LocalPlayer->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<UALKSaveSubsystem>() : nullptr;
}

UALKControllerLayoutScreen* UALKMenuSubsystem::OpenControllerLayout()
{
	TSubclassOf<UALKControllerLayoutScreen> ScreenClass = UALKDeveloperSettings::GetMenuConfig()->ControllerLayoutScreenClass;
	return Cast<UALKControllerLayoutScreen>(PushScreen(ScreenClass ? ScreenClass : TSubclassOf<UALKControllerLayoutScreen>(UALKControllerLayoutScreen::StaticClass()), EALKLayer::Menu));
}

void UALKMenuSubsystem::LoadChapterProgress() const
{
	if (bChapterProgressLoaded)
	{
		return;
	}
	bChapterProgressLoaded = true;
	UnlockedChapters.Reset();

	const FString& Slot = UALKDeveloperSettings::GetMenuConfig()->ChapterProgressSlot;
	if (!Slot.IsEmpty() && UGameplayStatics::DoesSaveGameExist(Slot, 0))
	{
		if (const UALKChapterProgressSaveGame* Save = Cast<UALKChapterProgressSaveGame>(UGameplayStatics::LoadGameFromSlot(Slot, 0)))
		{
			UnlockedChapters.Append(Save->UnlockedChapters);
		}
	}
}

void UALKMenuSubsystem::SaveChapterProgress() const
{
	const FString& Slot = UALKDeveloperSettings::GetMenuConfig()->ChapterProgressSlot;
	if (Slot.IsEmpty())
	{
		return;
	}
	UALKChapterProgressSaveGame* Save = NewObject<UALKChapterProgressSaveGame>();
	Save->UnlockedChapters = UnlockedChapters.Array();
	UGameplayStatics::SaveGameToSlot(Save, Slot, 0);
}

bool UALKMenuSubsystem::IsChapterUnlocked(FName ChapterId) const
{
	const FALKChapter* Chapter = UALKDeveloperSettings::GetMenuConfig()->FindChapter(ChapterId);
	if (!Chapter)
	{
		return false;
	}
	if (Chapter->bUnlockedByDefault)
	{
		return true;
	}
	LoadChapterProgress();
	return UnlockedChapters.Contains(ChapterId);
}

void UALKMenuSubsystem::UnlockChapter(FName ChapterId)
{
	if (!UALKDeveloperSettings::GetMenuConfig()->FindChapter(ChapterId))
	{
		UE_LOG(LogALK, Warning, TEXT("Unlock Chapter: no chapter '%s' in the menu config."), *ChapterId.ToString());
		return;
	}
	LoadChapterProgress();
	bool bAlreadyUnlocked = false;
	UnlockedChapters.Add(ChapterId, &bAlreadyUnlocked);
	if (!bAlreadyUnlocked)
	{
		SaveChapterProgress();
		OnChapterProgressChanged.Broadcast();
		OnChapterUnlockedNative.Broadcast(ChapterId);
	}
}

void UALKMenuSubsystem::ResetChapterProgress()
{
	LoadChapterProgress();
	UnlockedChapters.Reset();
	SaveChapterProgress();
	OnChapterProgressChanged.Broadcast();
}

bool UALKMenuSubsystem::StartChapter(FName ChapterId)
{
	const FALKChapter* Chapter = UALKDeveloperSettings::GetMenuConfig()->FindChapter(ChapterId);
	if (!Chapter)
	{
		UE_LOG(LogALK, Warning, TEXT("Start Chapter: no chapter '%s' in the menu config."), *ChapterId.ToString());
		return false;
	}
	if (!IsChapterUnlocked(ChapterId))
	{
		return false;
	}

	// Copy what is needed first: listeners may change the config.
	const TSoftObjectPtr<UWorld> Level = Chapter->Level;
	if (UALKSaveSubsystem* Saves = GetSaveSubsystem())
	{
		Saves->BeginNewGame(ChapterId);
	}
	OnChapterStarted.Broadcast(ChapterId);
	if (!Level.IsNull())
	{
		OpenMenuLevel(Level);
	}
	return true;
}

UALKConfirmDialog* UALKMenuSubsystem::ShowConfirmDialog(const FALKDialogDescriptor& Descriptor, const FALKDialogResultDelegate& OnResult)
{
	return ShowConfirmDialog(Descriptor, [OnResult](EALKDialogResult Result)
	{
		OnResult.ExecuteIfBound(Result);
	});
}

UALKConfirmDialog* UALKMenuSubsystem::ShowConfirmDialog(const FALKDialogDescriptor& Descriptor, TFunction<void(EALKDialogResult)> OnResult)
{
	UALKRootLayout* Layout = GetRootLayout();
	if (!Layout)
	{
		return nullptr;
	}

	TSubclassOf<UALKConfirmDialog> DialogClass = UALKDeveloperSettings::GetMenuConfig()->ConfirmDialogClass;
	if (!DialogClass)
	{
		DialogClass = UALKConfirmDialog::StaticClass();
	}

	return Layout->PushWidget<UALKConfirmDialog>(EALKLayer::Modal, DialogClass, [&Descriptor, &OnResult](UALKConfirmDialog& Dialog)
	{
		Dialog.SetupDialog(Descriptor, MoveTemp(OnResult));
	});
}

UALKKeyCaptureDialog* UALKMenuSubsystem::ShowKeyCaptureDialog(const FText& ActionName, bool bGamepad, TFunction<void(FKey)> OnKeyCaptured)
{
	UALKRootLayout* Layout = GetRootLayout();
	if (!Layout)
	{
		return nullptr;
	}

	TSubclassOf<UALKKeyCaptureDialog> DialogClass = UALKDeveloperSettings::GetMenuConfig()->KeyCaptureDialogClass;
	if (!DialogClass)
	{
		DialogClass = UALKKeyCaptureDialog::StaticClass();
	}

	return Layout->PushWidget<UALKKeyCaptureDialog>(EALKLayer::Modal, DialogClass, [&ActionName, bGamepad, &OnKeyCaptured](UALKKeyCaptureDialog& Dialog)
	{
		Dialog.SetupCapture(ActionName, bGamepad, MoveTemp(OnKeyCaptured));
	});
}

void UALKMenuSubsystem::SetMenuEntryEnabled(FName EntryId, bool bEnabled)
{
	MenuEntryOverrides.Add(EntryId, bEnabled);
	OnMenuEntriesChanged.Broadcast();
}

void UALKMenuSubsystem::ClearMenuEntryEnabled(FName EntryId)
{
	if (MenuEntryOverrides.Remove(EntryId) > 0)
	{
		OnMenuEntriesChanged.Broadcast();
	}
}

bool UALKMenuSubsystem::IsMenuEntryEnabled(const FALKMenuEntry& Entry) const
{
	if (const bool* Override = MenuEntryOverrides.Find(Entry.Id))
	{
		return *Override;
	}
	// Nothing to choose from without chapters.
	if (Entry.Action == EALKMenuAction::OpenChapterSelect && UALKDeveloperSettings::GetMenuConfig()->Chapters.IsEmpty())
	{
		return false;
	}
	// Save slots: Continue and Load Game need a save, Save Game a game that may be saved now.
	const UALKSaveSubsystem* Saves = GetSaveSubsystem();
	switch (Entry.Action)
	{
	case EALKMenuAction::ContinueGame:
	case EALKMenuAction::OpenLoadGame:
		return Saves && Saves->HasAnySave();
	case EALKMenuAction::OpenSaveGame:
	case EALKMenuAction::QuickSave:
		return Saves && Saves->IsSavingAllowed();
	case EALKMenuAction::QuickLoad:
	{
		FALKSaveSlotInfo Quick;
		return Saves && Saves->FindSave(UALKSaveSubsystem::MakeSlotName(EALKSaveSlotType::Quick, 1), Quick);
	}
	default:
		break;
	}
	return Entry.RequiredSaveSlot.IsEmpty() || UGameplayStatics::DoesSaveGameExist(Entry.RequiredSaveSlot, 0);
}

void UALKMenuSubsystem::ExecuteMenuEntry(const FALKMenuEntry& Entry)
{
	if (!IsMenuEntryEnabled(Entry))
	{
		return;
	}

	if (!Entry.bRequiresConfirmation)
	{
		RunMenuAction(Entry);
		return;
	}

	FALKDialogDescriptor Descriptor;
	Descriptor.Title = Entry.ConfirmTitle.IsEmpty() ? Entry.Label : Entry.ConfirmTitle;
	Descriptor.Message = Entry.ConfirmMessage.IsEmpty() ? LOCTEXT("ConfirmGeneric", "Are you sure?") : Entry.ConfirmMessage;
	Descriptor.ConfirmText = Entry.Label;
	Descriptor.CancelText = LOCTEXT("Cancel", "Cancel");
	Descriptor.bDestructive = Entry.Action == EALKMenuAction::QuitGame || Entry.Action == EALKMenuAction::ReturnToMainMenu || Entry.Action == EALKMenuAction::RestartLevel;

	TWeakObjectPtr<UALKMenuSubsystem> WeakThis(this);
	const FALKMenuEntry EntryCopy = Entry;
	ShowConfirmDialog(Descriptor, [WeakThis, EntryCopy](EALKDialogResult Result)
	{
		if (Result == EALKDialogResult::Confirmed && WeakThis.IsValid())
		{
			WeakThis->RunMenuAction(EntryCopy);
		}
	});
}

void UALKMenuSubsystem::RunMenuAction(const FALKMenuEntry& Entry)
{
	switch (Entry.Action)
	{
	case EALKMenuAction::StartGame:
		StartNewGame();
		break;
	case EALKMenuAction::OpenLevel:
		OpenMenuLevel(Entry.Level);
		break;
	case EALKMenuAction::OpenSettings:
		OpenSettings();
		break;
	case EALKMenuAction::OpenCredits:
		OpenCredits();
		break;
	case EALKMenuAction::OpenSettingsPage:
		OpenSettingsPage(Entry.Label, Entry.SettingsCategories);
		break;
	case EALKMenuAction::OpenChapterSelect:
		OpenChapterSelect();
		break;
	case EALKMenuAction::OpenControllerLayout:
		OpenControllerLayout();
		break;
	case EALKMenuAction::ContinueGame:
		if (UALKSaveSubsystem* Saves = GetSaveSubsystem())
		{
			Saves->LoadNewestSave();
		}
		break;
	case EALKMenuAction::OpenLoadGame:
		OpenLoadGame();
		break;
	case EALKMenuAction::OpenSaveGame:
		OpenSaveGame();
		break;
	case EALKMenuAction::QuickSave:
		if (UALKSaveSubsystem* Saves = GetSaveSubsystem())
		{
			HidePauseMenu();
			Saves->QuickSave();
		}
		break;
	case EALKMenuAction::QuickLoad:
		if (UALKSaveSubsystem* Saves = GetSaveSubsystem())
		{
			Saves->QuickLoad();
		}
		break;
	case EALKMenuAction::OpenScreen:
		if (UClass* ScreenClass = Entry.ScreenClass.LoadSynchronous())
		{
			PushScreen(ScreenClass, EALKLayer::Menu);
		}
		else
		{
			UE_LOG(LogALK, Warning, TEXT("Menu entry '%s' uses Open Screen but has no Screen Class."), *Entry.Id.ToString());
		}
		break;
	case EALKMenuAction::Resume:
		HidePauseMenu();
		break;
	case EALKMenuAction::OpenPlayOnline:
	case EALKMenuAction::OpenHostGame:
	case EALKMenuAction::OpenServerBrowser:
	case EALKMenuAction::OpenJoinByCode:
	case EALKMenuAction::OpenSocial:
	case EALKMenuAction::LeaveOnlineGame:
	case EALKMenuAction::OpenAdminPanel:
	case EALKMenuAction::OpenLeaderboards:
	case EALKMenuAction::OpenLoadout:
		if (UALKOnlineFlowSubsystem* Flow = GetLocalPlayer() ? GetLocalPlayer()->GetSubsystem<UALKOnlineFlowSubsystem>() : nullptr)
		{
			switch (Entry.Action)
			{
			case EALKMenuAction::OpenPlayOnline: Flow->OpenPlayHub(); break;
			case EALKMenuAction::OpenHostGame: Flow->OpenHostGame(false); break;
			case EALKMenuAction::OpenServerBrowser: Flow->OpenServerBrowser(); break;
			case EALKMenuAction::OpenJoinByCode: Flow->OpenJoinByCode(); break;
			case EALKMenuAction::OpenSocial: Flow->OpenSocial(); break;
			case EALKMenuAction::LeaveOnlineGame: Flow->LeaveGameWithConfirm(); break;
			case EALKMenuAction::OpenAdminPanel: Flow->OpenAdminPanel(); break;
			case EALKMenuAction::OpenLeaderboards: Flow->OpenLeaderboards(NAME_None); break;
			default: Flow->OpenLoadout(); break;
			}
		}
		break;
	case EALKMenuAction::RestartLevel:
		RestartLevel();
		break;
	case EALKMenuAction::ReturnToMainMenu:
		ReturnToMainMenu();
		break;
	case EALKMenuAction::QuitGame:
		QuitGame();
		break;
	case EALKMenuAction::Custom:
	default:
		break;
	}

	OnMenuAction.Broadcast(Entry.Id);
}

void UALKMenuSubsystem::StartNewGame()
{
	const UALKMenuConfig* Config = UALKDeveloperSettings::GetMenuConfig();
	if (Config->NewGameLevel.IsNull())
	{
		UE_LOG(LogALK, Warning, TEXT("Start Game: set New Game Level in your Assetpro Menu Config."));
		return;
	}
	if (UALKSaveSubsystem* Saves = GetSaveSubsystem())
	{
		Saves->BeginNewGame(Config->Chapters.Num() > 0 ? Config->Chapters[0].Id : NAME_None);
	}
	OpenMenuLevel(Config->NewGameLevel);
}

void UALKMenuSubsystem::OpenMenuLevel(TSoftObjectPtr<UWorld> Level)
{
	if (Level.IsNull())
	{
		UE_LOG(LogALK, Warning, TEXT("OpenMenuLevel called with an empty level."));
		return;
	}

	CloseAllMenus();
	UGameplayStatics::OpenLevelBySoftObjectPtr(GetPlayerController(), Level);
}

void UALKMenuSubsystem::RestartLevel()
{
	APlayerController* PlayerController = GetPlayerController();
	UWorld* World = PlayerController ? PlayerController->GetWorld() : nullptr;
	if (!World)
	{
		return;
	}
	// Use the full package path (e.g. /Game/Maps/Level) so levels with duplicate short names reopen correctly.
	const FString LevelName = UWorld::RemovePIEPrefix(World->GetOutermost()->GetName());
	CloseAllMenus();
	UGameplayStatics::OpenLevel(PlayerController, FName(*LevelName));
}

void UALKMenuSubsystem::ReturnToMainMenu()
{
	// Out of an online game: the online subsystem closes the session (for a host: for everyone) and brings this player
	// back to the main menu level itself.
	const ULocalPlayer* LocalPlayer = GetLocalPlayer<ULocalPlayer>();
	const UGameInstance* GameInstance = LocalPlayer ? LocalPlayer->GetGameInstance() : nullptr;
	if (UALKOnlineSubsystem* Online = GameInstance ? GameInstance->GetSubsystem<UALKOnlineSubsystem>() : nullptr)
	{
		if (Online->IsInGame())
		{
			CloseAllMenus();
			Online->LeaveGame();
			return;
		}
	}

	const UALKMenuConfig* Config = UALKDeveloperSettings::GetMenuConfig();
	if (Config->MainMenuLevel.IsNull())
	{
		UE_LOG(LogALK, Warning, TEXT("Return to Main Menu: set Main Menu Level in your Assetpro Menu Config."));
		return;
	}
	OpenMenuLevel(Config->MainMenuLevel);
}

void UALKMenuSubsystem::QuitGame()
{
	APlayerController* PlayerController = GetPlayerController();
	UKismetSystemLibrary::QuitGame(PlayerController, PlayerController, EQuitPreference::Quit, false);
}

void UALKMenuSubsystem::CloseAllMenus()
{
	if (RootLayout)
	{
		RootLayout->ClearMenuLayers();
	}
	PauseMenu.Reset();
	HandleLayersChanged();
}

bool UALKMenuSubsystem::IsAnyMenuOpen() const
{
	return RootLayout && RootLayout->IsAnyMenuOpen();
}

void UALKMenuSubsystem::HandleLayersChanged()
{
	const bool bPauseOpen = IsPauseMenuOpen();
	if (bPauseOpen != bPauseMenuWasOpen)
	{
		bPauseMenuWasOpen = bPauseOpen;
		// The view for a save made from the pause menu, taken before the game behind it drains and blurs.
		if (bPauseOpen)
		{
			if (UALKSaveSubsystem* Saves = GetSaveSubsystem())
			{
				Saves->CaptureThumbnail();
			}
		}
		UpdateGameDrain(bPauseOpen);
		OnPauseMenuChanged.Broadcast(bPauseOpen);
	}

	// Screens are often swapped (close one, open the next), which empties the layers for a moment. Wait a frame
	// before handing input back to the game so the cursor does not flicker in and out of capture.
	if (!IsAnyMenuOpen() && !GameInputTickerHandle.IsValid())
	{
		TWeakObjectPtr<UALKMenuSubsystem> WeakThis(this);
		GameInputTickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakThis](float)
		{
			if (UALKMenuSubsystem* Self = WeakThis.Get())
			{
				Self->GameInputTickerHandle.Reset();
				if (!Self->IsAnyMenuOpen())
				{
					Self->ApplyGameInputMode();
				}
			}
			return false;
		}));
	}
}

void UALKMenuSubsystem::UpdateGameDrain(bool bPauseOpen)
{
	APlayerController* Controller = GetPlayerController();
	APlayerCameraManager* Camera = Controller ? Controller->PlayerCameraManager.Get() : nullptr;
	if (!Camera)
	{
		return;
	}
	const UALKTheme* Theme = UALKDeveloperSettings::GetTheme();
	const float Amount = bPauseOpen && Theme ? Theme->PauseDesaturation : 0.f;
	UALKCameraModifier_Drain* Drain = Cast<UALKCameraModifier_Drain>(Camera->FindCameraModifierByClass(UALKCameraModifier_Drain::StaticClass()));
	if (!Drain)
	{
		if (Amount <= 0.f)
		{
			return;
		}
		Drain = Cast<UALKCameraModifier_Drain>(Camera->AddNewCameraModifier(UALKCameraModifier_Drain::StaticClass()));
	}
	if (!Drain)
	{
		return;
	}
	Drain->FadeTo(Amount);

	// A paused world does not update the view, so keep it updated (with no time passing: nothing else moves) until the
	// fade is done.
	if (!DrainTickerHandle.IsValid())
	{
		DrainTickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &ThisClass::TickGameDrain));
	}
}

bool UALKMenuSubsystem::TickGameDrain(float DeltaTime)
{
	APlayerController* Controller = GetPlayerController();
	APlayerCameraManager* Camera = Controller ? Controller->PlayerCameraManager.Get() : nullptr;
	const UALKCameraModifier_Drain* Drain = Camera ? Cast<UALKCameraModifier_Drain>(Camera->FindCameraModifierByClass(UALKCameraModifier_Drain::StaticClass())) : nullptr;
	if (!Drain)
	{
		DrainTickerHandle.Reset();
		return false;
	}
	const UWorld* World = Controller->GetWorld();
	if (World && World->IsPaused() && !Controller->ShouldPerformFullTickWhenPaused())
	{
		Camera->UpdateCamera(0.f);
	}
	if (!Drain->IsFading())
	{
		DrainTickerHandle.Reset();
		return false;
	}
	return true;
}

void UALKMenuSubsystem::ApplyGameInputMode()
{
	ULocalPlayer* LocalPlayer = GetLocalPlayer<ULocalPlayer>();
	if (!LocalPlayer || !GetPlayerController())
	{
		return;
	}

	if (UCommonUIActionRouterBase* ActionRouter = LocalPlayer->GetSubsystem<UCommonUIActionRouterBase>())
	{
		ActionRouter->SetActiveUIInputConfig(FUIInputConfig(ECommonInputMode::Game, EMouseCaptureMode::CapturePermanently_IncludingInitialMouseDown), this);
	}
}

void UALKMenuSubsystem::HandleWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources)
{
	if (RootLayout && RootLayout->GetWorld() == World)
	{
		RootLayout->RemoveFromParent();
		RootLayout = nullptr;
		PauseMenu.Reset();
		bPauseMenuWasOpen = false;
	}
}

void UALKMenuSubsystem::HandlePlayerControllerChanged(APlayerController* NewController)
{
	// Saved per-player settings (FOV, vibration) reach every controller class, not only ALKPlayerController.
	UGameInstance* GameInstance = NewController ? NewController->GetGameInstance() : nullptr;
	if (UALKSettingsSubsystem* Settings = GameInstance ? GameInstance->GetSubsystem<UALKSettingsSubsystem>() : nullptr)
	{
		Settings->ApplyToPlayerController(NewController);
	}

	// A new level starts in gameplay input mode. Menus shown at BeginPlay switch it back to menu mode.
	if (NewController && !IsAnyMenuOpen())
	{
		ApplyGameInputMode();
	}
}

#undef LOCTEXT_NAMESPACE
