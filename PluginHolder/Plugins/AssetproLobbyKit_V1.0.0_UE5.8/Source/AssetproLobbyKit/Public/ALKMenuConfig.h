// Copyright 2026 CodeLikeMe. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "InputCoreTypes.h"
#include "Styling/SlateBrush.h"
#include "ALKTypes.h"
#include "OnlineUI/ALKOnlineUITypes.h"
#include "ALKMenuConfig.generated.h"

class UALKRootLayout;
class UALKScreen;
class UALKPressStartScreen;
class UALKPauseMenuScreen;
class UALKChapterSelectScreen;
class UALKControllerLayoutScreen;
class UALKSettingsMenuScreen;
class UALKSettingsScreen;
class UALKCreditsScreen;
class UALKConfirmDialog;
class UALKKeyCaptureDialog;
class UALKButton;
class UALKSettingRow;
class UALKHUDLayer;
class UALKSaveGame;
class UALKSaveSlotScreen;
class UALKSaveSlotCard;
class UTexture2D;
class USoundClass;
class UWorld;

/**
 * What the menu contains and how it behaves: the main menu, settings menu and pause menu entries, chapters, levels,
 * credits, settings tabs, custom settings and the widget classes used for every screen. Pair it with a UALKTheme (the
 * look) in Project Settings > Plugins > Assetpro Lobby Kit.
 *
 * To re-skin a screen's layout, make a Blueprint child of its class (for example ALKMainMenuScreen), design it in UMG
 * using the documented widget names, and select it below.
 */
UCLASS(BlueprintType, Blueprintable)
class ASSETPROLOBBYKIT_API UALKMenuConfig : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UALKMenuConfig();

	// ---------------------------------------------------------------- Game

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Game")
	FText GameTitle;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Game")
	FText GameSubtitle;

	/** Optional logo shown instead of the title text on the press start screen. Set Image Size to the size you want it drawn. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Game")
	FSlateBrush GameLogo;

	/** Shown in the main menu footer. Leave empty to use the Project Version from Project Settings. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Game")
	FText VersionText;

	/** Level opened by "Return to Main Menu". */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Game")
	TSoftObjectPtr<UWorld> MainMenuLevel;

	/** Level opened by entries using the Start Game action. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Game")
	TSoftObjectPtr<UWorld> NewGameLevel;

	// ---------------------------------------------------------------- Main menu

	/** Show a "press any key" title screen before the main menu. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Main Menu")
	bool bShowPressStartScreen = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Main Menu", meta = (EditCondition = "bShowPressStartScreen"))
	FText PressStartText;

	/** The main menu's entries, top to bottom. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Main Menu", meta = (TitleProperty = "Label"))
	TArray<FALKMenuEntry> MainMenuEntries;

	/**
	 * Key art behind the main menu (and the title screen) instead of the theme's backdrop, scaled to cover the screen.
	 * The theme's Menu Scrim, vignette and grain are still drawn over it, so the entries read over a busy picture. The
	 * other screens keep the theme's backdrop. Leave empty to show the theme's backdrop on the main menu too.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Main Menu")
	TSoftObjectPtr<UTexture2D> MainMenuBackgroundImage;

	/** Pressing Back (Escape / B) on the main menu asks whether to quit the game. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Main Menu")
	bool bBackOnMainMenuOffersQuit = true;

	// ---------------------------------------------------------------- Settings menu

	/**
	 * The settings menu, opened by entries with the Open Settings action (main menu, pause menu). Each entry opens a
	 * settings page (Open Settings Page with the categories to show), the controller layout, the credits...
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings Menu", meta = (TitleProperty = "Label"))
	TArray<FALKMenuEntry> SettingsMenuEntries;

	// ---------------------------------------------------------------- Chapters

	/** Levels offered by the chapter select screen, in order. Entries with the Open Chapter Select action open it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chapters", meta = (TitleProperty = "Name"))
	TArray<FALKChapter> Chapters;

	/** Show "Chapter 3" style numbers above chapter names. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chapters")
	bool bNumberChapters = true;

	/** Save slot (user index 0) that remembers which chapters are unlocked. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chapters")
	FString ChapterProgressSlot = TEXT("ALKChapterProgress");

	// ---------------------------------------------------------------- Save slots

	/**
	 * Save and load slots: Continue loads the newest save, Load Game and Save Game open the slot screens, quick save and
	 * quick load keys, checkpoint autosaves. Turn off if your game keeps its saves another way (then Continue entries
	 * use Required Save Slot or Set Menu Entry Enabled as before).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Save Slots")
	bool bEnableSaveSlots = true;

	/** The save written to each slot: ALKSaveGame or your child of it with your game's own variables. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Save Slots", meta = (EditCondition = "bEnableSaveSlots"))
	TSubclassOf<UALKSaveGame> SaveGameClass;

	/** Start of every save file's name (ALK_Manual_01, ALK_Auto_1, ALK_Quick, ALK_SaveIndex), so they never clash with your own. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Save Slots", meta = (EditCondition = "bEnableSaveSlots"))
	FString SaveSlotPrefix = TEXT("ALK");

	/** How many slots the player can save to. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Save Slots", meta = (EditCondition = "bEnableSaveSlots", ClampMin = "1", ClampMax = "99"))
	int32 ManualSlots = 10;

	/** Autosaves take turns through this many slots (the oldest is replaced). 0 turns autosaves off. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Save Slots", meta = (EditCondition = "bEnableSaveSlots", ClampMin = "0", ClampMax = "10"))
	int32 AutosaveSlots = 3;

	/** Checkpoint Volumes autosave when the player reaches them. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Save Slots", meta = (EditCondition = "bEnableSaveSlots"))
	bool bAutosaveAtCheckpoints = true;

	/** Every save keeps the player's position and view, and loading puts the player back there. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Save Slots", meta = (EditCondition = "bEnableSaveSlots"))
	bool bSavePlayerTransform = true;

	/** Quick save and quick load keys (ALKPlayerController). Leave a key empty to turn it off. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Save Slots", meta = (EditCondition = "bEnableSaveSlots"))
	FKey QuickSaveKey = EKeys::F5;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Save Slots", meta = (EditCondition = "bEnableSaveSlots"))
	FKey QuickLoadKey = EKeys::F9;

	/** Loading a save while playing asks first: progress since the last save is lost. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Save Slots", meta = (EditCondition = "bEnableSaveSlots"))
	bool bConfirmLoadInGame = true;

	// ---------------------------------------------------------------- Controller layout

	/**
	 * Labels for the controller layout diagram. Gamepad buttons bound to a rebindable action show the action's name on
	 * their own; add inputs here that are not rebindable rows, such as the sticks or the menu button.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Controller Layout")
	TMap<FKey, FText> ControllerLayoutLabels;

	// ---------------------------------------------------------------- Pause menu

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pause Menu")
	FText PauseTitle;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pause Menu", meta = (TitleProperty = "Label"))
	TArray<FALKMenuEntry> PauseMenuEntries;

	/** Pause the game while the pause menu is open. Online games never pause: the match keeps running behind the menu. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pause Menu")
	bool bPauseGameWhileOpen = true;

	/**
	 * The pause menu while in an online game (a lobby or a match). The game keeps running, so there is no save, load or
	 * restart here; Leave Game asks first, and the host also gets the Host Panel.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pause Menu", meta = (TitleProperty = "Label"))
	TArray<FALKMenuEntry> OnlinePauseMenuEntries;

	/** The pause menu's title while in an online game (the game is not paused there). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pause Menu")
	FText OnlinePauseTitle;

	/** Shown under the pause menu's title while in an online game. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pause Menu")
	FText OnlinePauseNote;

	// ---------------------------------------------------------------- Credits

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Credits", meta = (TitleProperty = "Heading"))
	TArray<FALKCreditsSection> Credits;

	// ---------------------------------------------------------------- Loading screen

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loading Screen")
	FText LoadingText;

	/** One tip is picked at random for each loading screen. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loading Screen", meta = (MultiLine = true))
	TArray<FText> LoadingTips;

	// ---------------------------------------------------------------- Settings

	/** Tabs in display order. Hide a tab or rename it here. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings")
	TArray<FALKSettingsTab> SettingsTabs;

	/**
	 * Ids of built-in settings to hide, for example "Difficulty" or "ColorBlindMode".
	 * The Id of every setting is listed in the documentation and printed by the console command ALK.ListSettings.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings")
	TArray<FName> HiddenSettings;

	/** Extra game specific settings. Read them with Get Custom Setting Value. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings", meta = (TitleProperty = "DisplayName"))
	TArray<FALKCustomSettingDefinition> CustomSettings;

	/**
	 * Apply the Field of View setting to the player's view (see ALKCameraModifier_FieldOfView). A camera at 90 degrees
	 * shows exactly the chosen value. Turn this off if your game applies Get Field Of View to its camera itself.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings")
	bool bApplyFieldOfViewToCamera = true;

	/**
	 * Languages offered by the Language setting, as culture codes ("en", "de", "pt-BR", "zh-Hans"...).
	 * Leave empty to offer every language your project is localized into. The row is hidden with fewer than two.
	 * The plugin's own text ships in: en, de, es, fr, it, ja, ko, pl, pt-BR, ru, zh-Hans.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings")
	TArray<FString> Languages;

	/** Sound class driven by the Music Volume setting. Without one the value is still saved for your own use. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings|Audio")
	TSoftObjectPtr<USoundClass> MusicSoundClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings|Audio")
	TSoftObjectPtr<USoundClass> EffectsSoundClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings|Audio")
	TSoftObjectPtr<USoundClass> VoiceSoundClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings|Audio")
	TSoftObjectPtr<USoundClass> InterfaceSoundClass;

	// ---------------------------------------------------------------- Input hints

	/** Show key / button hints ("Esc Back") in screen footers. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input Hints")
	bool bShowInputHints = true;

	/** Gamepad button names in hints and key binding rows. Auto follows the controller being used. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input Hints")
	EALKGamepadLabels GamepadLabels = EALKGamepadLabels::Auto;

	// ---------------------------------------------------------------- HUD

	/** Starting a chapter shows its number and name as a title card once its level has loaded. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HUD")
	bool bShowChapterTitleOnStart = true;

	/** Unlock Chapter shows a "Chapter Unlocked" notification with the chapter's name. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HUD")
	bool bNotifyChapterUnlocks = true;

	/** Most notifications shown at once; older ones leave early to make room. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HUD", meta = (ClampMin = "1", ClampMax = "6"))
	int32 MaxNotifications = 3;

	/** Most subtitle lines shown at once; the oldest leaves when a new one arrives. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HUD", meta = (ClampMin = "1", ClampMax = "4"))
	int32 MaxSubtitleLines = 2;

	/** The HUD layer: ALKHUDLayer or a Blueprint child of it (see its designer widget names). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Widget Classes")
	TSubclassOf<UALKHUDLayer> HUDLayerClass;

	// ---------------------------------------------------------------- Widget classes

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Widget Classes")
	TSubclassOf<UALKRootLayout> RootLayoutClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Widget Classes")
	TSubclassOf<UALKPressStartScreen> PressStartScreenClass;

	/**
	 * What the main menu shows in its right column: the party and the friends who are online (ALK Social Strip) by default,
	 * or any widget of your own. Clear it for a main menu without a side column. The strip hides itself while online play
	 * is turned off. Right and the Party key move the focus into the column when it is the strip (or a child of it); a
	 * widget of another kind is reached with the mouse or by its own navigation.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Widget Classes")
	TSoftClassPtr<UUserWidget> MainMenuSideWidgetClass;

	/** The main menu: a list of entries (ALKMainMenuScreen), a Blueprint child of it or any screen of your own. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Widget Classes")
	TSubclassOf<UALKScreen> MainMenuScreenClass;

	/** The settings menu: the list of settings pages. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Widget Classes")
	TSubclassOf<UALKSettingsMenuScreen> SettingsMenuScreenClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Widget Classes")
	TSubclassOf<UALKPauseMenuScreen> PauseMenuScreenClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Widget Classes")
	TSubclassOf<UALKSettingsScreen> SettingsScreenClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Widget Classes")
	TSubclassOf<UALKCreditsScreen> CreditsScreenClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Widget Classes")
	TSubclassOf<UALKChapterSelectScreen> ChapterSelectScreenClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Widget Classes")
	TSubclassOf<UALKControllerLayoutScreen> ControllerLayoutScreenClass;

	/** The Load Game and Save Game screens (one class, two modes). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Widget Classes")
	TSubclassOf<UALKSaveSlotScreen> SaveSlotScreenClass;

	/** One save on the slot screens. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Widget Classes")
	TSubclassOf<UALKSaveSlotCard> SaveSlotCardClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Widget Classes")
	TSubclassOf<UALKConfirmDialog> ConfirmDialogClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Widget Classes")
	TSubclassOf<UALKKeyCaptureDialog> KeyCaptureDialogClass;

	/** Used for every menu entry, dialog choice and footer hint. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Widget Classes")
	TSubclassOf<UALKButton> ButtonClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Widget Classes|Setting Rows")
	TSubclassOf<UALKSettingRow> OptionRowClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Widget Classes|Setting Rows")
	TSubclassOf<UALKSettingRow> SliderRowClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Widget Classes|Setting Rows")
	TSubclassOf<UALKSettingRow> KeyBindingRowClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Widget Classes|Setting Rows")
	TSubclassOf<UALKSettingRow> ActionRowClass;

	/** The screens of the multiplayer front end (play hub, server browser, lobby, scoreboard...). Swap any of them for your own. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Widget Classes|Online")
	FALKOnlineScreenClasses OnlineScreens;

	/**
	 * The pictures players get when your platform or profile service gives none: every player list (lobby, friends,
	 * party, scoreboard, results, leaderboards, profile) picks one by the player's name, always the same one for the
	 * same name. The plugin's sixteen emblems by default; put your own here, or empty the list for the plain coloured
	 * silhouette. Streamer mode always shows the silhouette.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Online")
	TArray<TSoftObjectPtr<UTexture2D>> DefaultAvatars;

	/** The keys of the match overlay: scoreboard, chat, spectating. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Online")
	FALKOnlineKeys OnlineKeys;

	/**
	 * For projects whose menus come from another Assetpro menu plugin (or any menu system with a local player subsystem
	 * that fires "On Menu Action (Entry Id)"): when that menu's entry with one of these Ids is picked, the kit runs the
	 * action beside it. Give that menu a Custom entry with the Id "PlayOnline" and it opens the kit's play hub: no code
	 * and no dependency between the two plugins. Empty turns the link off.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Online")
	TMap<FName, EALKMenuAction> OtherMenuEntries;

	/** Display name for a tab, using the override from Settings Tabs when set. */
	FText GetTabDisplayName(EALKSettingsCategory Category) const;

	/** The Version Text, or the project version when it is empty. */
	FText GetResolvedVersionText() const;

	/**
	 * The default picture for a player name (see Default Avatars): loaded on demand, null when the list is empty, the
	 * name is empty or the picture is missing.
	 */
	UFUNCTION(BlueprintPure, Category = "Assetpro Lobby|Players")
	UTexture2D* GetDefaultAvatar(const FString& PlayerName) const;

	/** A chapter by Id, or null. */
	const FALKChapter* FindChapter(FName ChapterId) const;

	/** True when Game Logo has a texture or material assigned. */
	bool HasGameLogo() const { return GameLogo.GetResourceObject() != nullptr; }
};
