// Copyright 2026 CodeLikeMe. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "Containers/Ticker.h"
#include "InputCoreTypes.h"
#include "ALKTypes.h"
#include "ALKMenuSubsystem.generated.h"

class UALKRootLayout;
class UALKScreen;
class UALKPressStartScreen;
class UALKPauseMenuScreen;
class UALKSettingsMenuScreen;
class UALKSettingsScreen;
class UALKCreditsScreen;
class UALKChapterSelectScreen;
class UALKControllerLayoutScreen;
class UALKSaveSlotScreen;
class UALKSaveSubsystem;
class UALKConfirmDialog;
class UALKKeyCaptureDialog;
class UCommonActivatableWidget;
class UCommonUIActionRouterBase;
class APlayerController;
enum class ECommonInputMode : uint8;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FALKOnMenuAction, FName, EntryId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FALKOnPauseMenuChanged, bool, bIsOpen);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FALKOnChapterStarted, FName, ChapterId);

/**
 * Per-player entry point for the menu system. Get it from Blueprint with "Get Assetpro Menu Subsystem" or
 * LocalPlayer->GetSubsystem<UALKMenuSubsystem>() in C++.
 */
UCLASS()
class ASSETPROLOBBYKIT_API UALKMenuSubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** The root layout for the current world, created on first use. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Menu")
	UALKRootLayout* GetRootLayout();

	/** Push any activatable widget onto a layer. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Menu", meta = (DeterminesOutputType = "ScreenClass"))
	UCommonActivatableWidget* PushScreen(TSubclassOf<UCommonActivatableWidget> ScreenClass, EALKLayer Layer = EALKLayer::Menu);

	/** Shows the press start screen or the main menu, depending on the menu config. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Menu")
	void ShowStartupMenu();

	/** Shows the main menu (ALKMainMenuScreen unless the menu config names another class), or returns it if it is open. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Menu")
	UALKScreen* ShowMainMenu();

	UFUNCTION(BlueprintCallable, Category = "Assetpro Menu")
	UALKPressStartScreen* ShowPressStartScreen();

	UFUNCTION(BlueprintCallable, Category = "Assetpro Menu")
	UALKPauseMenuScreen* ShowPauseMenu();

	/** Closes the pause menu and anything opened on top of it. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Menu")
	void HidePauseMenu();

	/** Bind this to your pause input (the plugin's ALKPlayerController already does). */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Menu")
	void TogglePauseMenu();

	UFUNCTION(BlueprintPure, Category = "Assetpro Menu")
	bool IsPauseMenuOpen() const;

	/** Opens the settings menu: the list of settings pages from the menu config's Settings Menu Entries. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Menu")
	UALKSettingsMenuScreen* OpenSettings();

	/** Opens every settings category on one screen, a tab each (Q / E or LB / RB switch tabs), instead of the menu of pages. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Menu")
	UALKSettingsScreen* OpenSettingsTabs();

	/**
	 * Opens one settings page: the settings of these categories in this order, under Title, with no tabs. What the
	 * settings menu's entries open. Empty Categories shows every category.
	 */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Menu")
	UALKSettingsScreen* OpenSettingsPage(const FText& Title, const TArray<EALKSettingsCategory>& Categories);

	UFUNCTION(BlueprintCallable, Category = "Assetpro Menu")
	UALKCreditsScreen* OpenCredits();

	/** Opens the chapter select with the menu config's Chapters. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Menu")
	UALKChapterSelectScreen* OpenChapterSelect();

	/** Opens the gamepad diagram that shows what every button does. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Menu")
	UALKControllerLayoutScreen* OpenControllerLayout();

	/** Opens the Load Game screen: every save, newest first. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Menu")
	UALKSaveSlotScreen* OpenLoadGame();

	/** Opens the Save Game screen (while playing): a new save, or a manual slot to overwrite. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Menu")
	UALKSaveSlotScreen* OpenSaveGame();

	/** The save slots (Assetpro Save Subsystem of this player's game instance). */
	UFUNCTION(BlueprintPure, Category = "Assetpro Menu")
	UALKSaveSubsystem* GetSaveSubsystem() const;

	/**
	 * Starts a chapter: broadcasts On Chapter Started, then opens the chapter's Level (if it has one). Locked chapters
	 * do not start. Returns true when the chapter started.
	 */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Menu|Chapters")
	bool StartChapter(FName ChapterId);

	/** Makes a chapter playable in the chapter select screen. Saved right away (menu config: Chapter Progress Slot). */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Menu|Chapters")
	void UnlockChapter(FName ChapterId);

	/** True when the chapter is Unlocked By Default or was unlocked with Unlock Chapter. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Menu|Chapters")
	bool IsChapterUnlocked(FName ChapterId) const;

	/** Locks every chapter again, except those unlocked by default, and saves. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Menu|Chapters")
	void ResetChapterProgress();

	/** Broadcast when a chapter is unlocked or progress is reset, so an open chapter select updates. */
	FSimpleMulticastDelegate OnChapterProgressChanged;

	/** Broadcast when Unlock Chapter unlocks a chapter that was locked (the HUD announces it). */
	TMulticastDelegate<void(FName /*ChapterId*/)> OnChapterUnlockedNative;

	/** Shows a modal confirmation dialog. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Menu", meta = (AutoCreateRefTerm = "OnResult"))
	UALKConfirmDialog* ShowConfirmDialog(const FALKDialogDescriptor& Descriptor, const FALKDialogResultDelegate& OnResult);

	UALKConfirmDialog* ShowConfirmDialog(const FALKDialogDescriptor& Descriptor, TFunction<void(EALKDialogResult)> OnResult);

	/** Shows the "press a key" dialog used for rebinding. The callback gets the key, or an invalid key if cancelled. */
	UALKKeyCaptureDialog* ShowKeyCaptureDialog(const FText& ActionName, bool bGamepad, TFunction<void(FKey)> OnKeyCaptured);

	/** Runs a menu entry (asks for confirmation first if the entry wants it) and broadcasts On Menu Action. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Menu")
	void ExecuteMenuEntry(const FALKMenuEntry& Entry);

	/**
	 * Turns a menu entry on or off by Id, for example Continue when your game keeps its saves its own way. Replaces the
	 * entry's Required Save Slot check until Clear Menu Entry Enabled is called; open menus update right away.
	 */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Menu")
	void SetMenuEntryEnabled(FName EntryId, bool bEnabled);

	/** Hands a menu entry back to its Required Save Slot check. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Menu")
	void ClearMenuEntryEnabled(FName EntryId);

	/** Whether a menu entry can be used now: Set Menu Entry Enabled if it was called for it, else whether its Required Save Slot has a save. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Menu")
	bool IsMenuEntryEnabled(const FALKMenuEntry& Entry) const;

	/** Broadcast when Set / Clear Menu Entry Enabled changes an entry, so open menu lists update. */
	FSimpleMulticastDelegate OnMenuEntriesChanged;

	UFUNCTION(BlueprintCallable, Category = "Assetpro Menu")
	void StartNewGame();

	UFUNCTION(BlueprintCallable, Category = "Assetpro Menu")
	void OpenMenuLevel(TSoftObjectPtr<UWorld> Level);

	UFUNCTION(BlueprintCallable, Category = "Assetpro Menu")
	void RestartLevel();

	UFUNCTION(BlueprintCallable, Category = "Assetpro Menu")
	void ReturnToMainMenu();

	UFUNCTION(BlueprintCallable, Category = "Assetpro Menu")
	void QuitGame();

	/** Closes every menu, popup and dialog. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Menu")
	void CloseAllMenus();

	UFUNCTION(BlueprintPure, Category = "Assetpro Menu")
	bool IsAnyMenuOpen() const;

	/** Broadcast after any menu entry runs, with the entry's Id. Use it for Custom entries (Continue, Load Game...). */
	UPROPERTY(BlueprintAssignable, Category = "Assetpro Menu")
	FALKOnMenuAction OnMenuAction;

	UPROPERTY(BlueprintAssignable, Category = "Assetpro Menu")
	FALKOnPauseMenuChanged OnPauseMenuChanged;

	/** Broadcast when a chapter starts, before its level opens. Set up the chapter's state here (checkpoint, loadout...). */
	UPROPERTY(BlueprintAssignable, Category = "Assetpro Menu|Chapters")
	FALKOnChapterStarted OnChapterStarted;

private:
	UALKSaveSlotScreen* OpenSaveSlots(bool bSave);
	APlayerController* GetPlayerController() const;
	void RunMenuAction(const FALKMenuEntry& Entry);
	void HandleLayersChanged();

	/** Drains the colour from the game behind the pause menu (theme: Pause Desaturation), or brings it back. */
	void UpdateGameDrain(bool bPauseOpen);
	bool TickGameDrain(float DeltaTime);
	void HandleWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources);
	void HandlePlayerControllerChanged(APlayerController* NewController);
	void ApplyGameInputMode();
	void BindInputModeChanged();
	void HandleInputModeChanged(ECommonInputMode NewMode);

	UPROPERTY(Transient)
	TObjectPtr<UALKRootLayout> RootLayout;

	TWeakObjectPtr<UALKPauseMenuScreen> PauseMenu;
	bool bPauseMenuWasOpen = false;

	/** Menu entries turned on or off by Set Menu Entry Enabled. */
	TMap<FName, bool> MenuEntryOverrides;

	void LoadChapterProgress() const;
	void SaveChapterProgress() const;

	/** Chapters unlocked with Unlock Chapter, read from the save slot on first use. */
	mutable TSet<FName> UnlockedChapters;
	mutable bool bChapterProgressLoaded = false;

	FDelegateHandle WorldCleanupHandle;
	FDelegateHandle ControllerChangedHandle;
	FTSTicker::FDelegateHandle GameInputTickerHandle;
	FTSTicker::FDelegateHandle DrainTickerHandle;

	TWeakObjectPtr<UCommonUIActionRouterBase> InputModeRouter;
	FDelegateHandle InputModeChangedHandle;
};
