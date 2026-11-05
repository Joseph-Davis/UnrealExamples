// Copyright 2026 CodeLikeMe. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ALKTypes.generated.h"

class UCommonActivatableWidget;
class UTexture2D;
class UWorld;

/** The stacked layers of the root layout, drawn bottom to top. */
UENUM(BlueprintType)
enum class EALKLayer : uint8
{
	/** In-game HUD style widgets. */
	Game,
	/** Menus shown on top of gameplay, such as the pause menu. */
	GameMenu,
	/** Full screen menus: main menu, settings, credits. */
	Menu,
	/** Dialogs and popups that block everything below them. */
	Modal
};

/** Visual variant of a menu button. Every variant is drawn by the active theme. */
UENUM(BlueprintType)
enum class EALKButtonStyle : uint8
{
	/** A menu entry: large text, dimmed until it has focus. */
	Primary,
	/** Smaller text button (dialog choices, settings footer). */
	Secondary,
	/** Secondary button that turns the theme's danger colour when it has focus. */
	Danger,
	/** Settings tab: a chip that fills with the accent colour while its tab is shown. */
	Tab,
	/** The < and > of option rows. */
	Arrow,
	/** An input hint ("[Esc] Back") in a footer: a key cap and a label; hints with an action can be clicked. */
	Hint,
	/** The main action of a screen, filled with the accent colour ("Ready up", "Join game", "Accept"). */
	Solid,
	/** Text only, no box until it has focus ("Leave party", "Copy address"). */
	Ghost
};

/** Text roles map to fonts and colours in the theme. */
UENUM(BlueprintType)
enum class EALKTextRole : uint8
{
	Title,
	Heading,
	Subheading,
	Body,
	Button,
	Caption,
	/** Small labels and values: column headers, key caps, timers, counts. */
	Data
};

/** The five accent colours every skin comes in. Apply one to a theme asset with one click. */
UENUM(BlueprintType)
enum class EALKThemePreset : uint8
{
	/** Mint green (the default). */
	Mint,
	/** Clear blue. */
	Azure,
	/** Warm orange. */
	Ember,
	/** Soft violet. */
	Orchid,
	/** Golden yellow. */
	Gold
};

/** The three skins that ship with the kit. A skin sets the neutral colours, the corner radius and the panel style. */
UENUM(BlueprintType)
enum class EALKThemeSkin : uint8
{
	/** Neutral dark panels with thin outlines and soft shadows. */
	Default,
	/** White panels on a light grey background. */
	Light,
	/** Bold and chunky: thick dark outlines, hard shadows, capitals, a solid highlight. */
	Arcade
};

/** How panels and cards are drawn. */
UENUM(BlueprintType)
enum class EALKPanelStyle : uint8
{
	/** A filled panel, no outline or shadow. */
	Flat,
	/** A filled panel with a thin outline. */
	Outlined,
	/** An outlined panel with a soft shadow under it. */
	Raised,
	/** A thick outline and a hard shadow, like a printed sticker. */
	Chunky
};

/** How the focused item is marked. */
UENUM(BlueprintType)
enum class EALKFocusStyle : uint8
{
	/** A light tint of the accent colour, an accent outline and a small bar. */
	Tinted,
	/** Filled with the accent colour; the text switches to the contrast colour. */
	Solid
};

/** The theme's interface sounds (see UALKTheme::PlayUISound). */
UENUM(BlueprintType)
enum class EALKUISound : uint8
{
	/** Focus moves to another item (keyboard, gamepad or mouse). */
	Hover,
	/** An item is picked. */
	Click,
	/** Back or cancel. */
	Back,
	/** A screen or dialog opens. */
	ScreenOpen,
	/** Another settings tab is shown, or the chapter select moves to another chapter. */
	Tab,
	/** A setting's value changes. */
	Change,
	/** A locked chapter is picked. */
	Locked,
	/** The player readies up in a lobby. */
	Ready,
	/** Matchmaking found a match. */
	MatchFound,
	/** A chat message, an invite or a notice arrives. */
	Notify,
	/** Each second of a countdown (the lobby's start, the match found timer). */
	Countdown,
	/** A connection problem or a refused action. */
	Error
};

/** What a menu entry does when clicked. */
UENUM(BlueprintType)
enum class EALKMenuAction : uint8
{
	/** Opens the menu config's New Game Level. */
	StartGame,
	/** Opens the level set on the entry. */
	OpenLevel,
	/** Opens the settings menu: the list of settings pages (menu config Settings Menu Entries). */
	OpenSettings,
	OpenCredits,
	/** Pushes the entry's Screen Class onto the menu layer. */
	OpenScreen,
	/** Closes the pause menu. */
	Resume,
	RestartLevel,
	ReturnToMainMenu,
	QuitGame,
	/** Does nothing by itself; broadcasts On Menu Action with the entry Id so game code can react. */
	Custom,
	/** Opens one settings page: the settings of the entry's Settings Categories. */
	OpenSettingsPage,
	/** Opens the chapter select. */
	OpenChapterSelect,
	/** Opens the gamepad layout diagram. */
	OpenControllerLayout,
	/** Loads the newest save (manual, auto or quick). Disabled while there is none. */
	ContinueGame,
	/** Opens the Load Game screen. Disabled while there are no saves. */
	OpenLoadGame,
	/** Opens the Save Game screen (in game). Disabled while saving is not allowed. */
	OpenSaveGame,
	/** Saves to the quick save slot. */
	QuickSave,
	/** Loads the quick save. */
	QuickLoad,
	/** Opens the play hub: quick match, host a game, browse games, join by code, party. */
	OpenPlayOnline,
	/** Opens the host game screen. */
	OpenHostGame,
	/** Opens the server browser. */
	OpenServerBrowser,
	/** Asks for a lobby code or an address and joins. */
	OpenJoinByCode,
	/** Opens friends, party and invites. */
	OpenSocial,
	/** Leaves the online game (asks first) and returns to the main menu. */
	LeaveOnlineGame,
	/** Opens the host's panel: kick, ban list, change map, pause. Only the host sees the entry. */
	OpenAdminPanel,
	/** Opens the leaderboards. */
	OpenLeaderboards,
	/** Opens the loadout screen. */
	OpenLoadout
};

/** Settings screen tabs. */
UENUM(BlueprintType)
enum class EALKSettingsCategory : uint8
{
	Video,
	Graphics,
	Audio,
	Controls,
	Gameplay,
	Accessibility,
	/** Region, crossplay, ping and network stats, text chat, streamer mode and voice chat. */
	Multiplayer
};

/** One entry of a menu list: the main menu, the settings menu or the pause menu. */
USTRUCT(BlueprintType)
struct ASSETPROLOBBYKIT_API FALKMenuEntry
{
	GENERATED_BODY()

	/** Identifier passed to On Menu Action. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu Entry")
	FName Id;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu Entry")
	FText Label;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu Entry")
	EALKMenuAction Action = EALKMenuAction::Custom;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu Entry", meta = (EditCondition = "Action == EALKMenuAction::OpenLevel", EditConditionHides))
	TSoftObjectPtr<UWorld> Level;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu Entry", meta = (EditCondition = "Action == EALKMenuAction::OpenScreen", EditConditionHides))
	TSoftClassPtr<UCommonActivatableWidget> ScreenClass;

	/** The settings shown on the page, in this order. Empty shows every settings category. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu Entry", meta = (EditCondition = "Action == EALKMenuAction::OpenSettingsPage", EditConditionHides))
	TArray<EALKSettingsCategory> SettingsCategories;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu Entry")
	EALKButtonStyle Style = EALKButtonStyle::Primary;

	/** Ask "are you sure?" before running the action. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu Entry")
	bool bRequiresConfirmation = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu Entry", meta = (EditCondition = "bRequiresConfirmation", EditConditionHides))
	FText ConfirmTitle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu Entry", meta = (EditCondition = "bRequiresConfirmation", EditConditionHides, MultiLine = true))
	FText ConfirmMessage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu Entry")
	bool bVisible = true;

	/**
	 * For entries that need a saved game (Continue, Load Game): the save slot to look for. While the slot has no save
	 * (user index 0) the entry is shown disabled. Leave empty when the entry is always available. Games that keep their
	 * saves another way can decide instead with Set Menu Entry Enabled.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu Entry")
	FString RequiredSaveSlot;

	FALKMenuEntry() = default;

	FALKMenuEntry(FName InId, const FText& InLabel, EALKMenuAction InAction, EALKButtonStyle InStyle = EALKButtonStyle::Primary)
		: Id(InId), Label(InLabel), Action(InAction), Style(InStyle)
	{
	}
};

/** A heading plus a list of names in the credits roll. */
USTRUCT(BlueprintType)
struct ASSETPROLOBBYKIT_API FALKCreditsSection
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Credits")
	FText Heading;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Credits")
	TArray<FText> Lines;
};

/** A settings tab and how it is shown. */
USTRUCT(BlueprintType)
struct ASSETPROLOBBYKIT_API FALKSettingsTab
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings Tab")
	EALKSettingsCategory Category = EALKSettingsCategory::Video;

	/** Leave empty to use the default name. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings Tab")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings Tab")
	bool bVisible = true;

	FALKSettingsTab() = default;
	explicit FALKSettingsTab(EALKSettingsCategory InCategory) : Category(InCategory) {}
};

/** How slider values are printed. */
UENUM(BlueprintType)
enum class EALKValueFormat : uint8
{
	/** 0..1 printed as 0%..100% */
	Percent,
	Integer,
	OneDecimal,
	TwoDecimals
};

UENUM(BlueprintType)
enum class EALKCustomSettingType : uint8
{
	Toggle,
	Options,
	Slider
};

/**
 * A game specific setting defined purely in data. Its value is saved with the other settings
 * and can be read with Get Custom Setting Value.
 */
USTRUCT(BlueprintType)
struct ASSETPROLOBBYKIT_API FALKCustomSettingDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Custom Setting")
	FName Id;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Custom Setting")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Custom Setting", meta = (MultiLine = true))
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Custom Setting")
	EALKSettingsCategory Category = EALKSettingsCategory::Gameplay;

	/** Optional group heading shown above the setting. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Custom Setting")
	FText Section;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Custom Setting")
	EALKCustomSettingType Type = EALKCustomSettingType::Toggle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Custom Setting", meta = (EditCondition = "Type == EALKCustomSettingType::Options", EditConditionHides))
	TArray<FText> Options;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Custom Setting", meta = (EditCondition = "Type == EALKCustomSettingType::Slider", EditConditionHides))
	float MinValue = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Custom Setting", meta = (EditCondition = "Type == EALKCustomSettingType::Slider", EditConditionHides))
	float MaxValue = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Custom Setting", meta = (EditCondition = "Type == EALKCustomSettingType::Slider", EditConditionHides))
	float StepSize = 0.05f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Custom Setting", meta = (EditCondition = "Type == EALKCustomSettingType::Slider", EditConditionHides))
	EALKValueFormat Format = EALKValueFormat::Percent;

	/** Toggle: 0 or 1. Options: the option index. Slider: the value. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Custom Setting")
	float DefaultValue = 0.f;
};

/** Which button names to show for gamepads in hints and key binding rows. */
UENUM(BlueprintType)
enum class EALKGamepadLabels : uint8
{
	/** Detect from the controller in use (falls back to Xbox names). */
	Auto,
	Xbox,
	PlayStation,
	Nintendo
};

/** Result of a confirmation dialog. */
UENUM(BlueprintType)
enum class EALKDialogResult : uint8
{
	Confirmed,
	Cancelled,
	/** The optional third button. */
	Alternate
};

/** Everything shown by a confirmation dialog. */
USTRUCT(BlueprintType)
struct ASSETPROLOBBYKIT_API FALKDialogDescriptor
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialog")
	FText Title;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialog", meta = (MultiLine = true))
	FText Message;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialog")
	FText ConfirmText;

	/** Leave empty for a single button dialog. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialog")
	FText CancelText;

	/** Optional third button. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialog")
	FText AlternateText;

	/** Colours the confirm button with the danger colour. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialog")
	bool bDestructive = false;

	/** Seconds before the dialog closes itself with the Cancelled result (0 = never). The countdown shows on the Cancel button. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialog", meta = (ClampMin = "0"))
	float TimeoutSeconds = 0.f;
};

DECLARE_DYNAMIC_DELEGATE_OneParam(FALKDialogResultDelegate, EALKDialogResult, Result);

/** A level the player can start from the chapter select. */
USTRUCT(BlueprintType)
struct ASSETPROLOBBYKIT_API FALKChapter
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chapter")
	FName Id;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chapter")
	FText Name;

	/** Still shown large while the chapter is picked (16:9 looks best). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chapter")
	TSoftObjectPtr<UTexture2D> Image;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chapter")
	TSoftObjectPtr<UWorld> Level;

	/** Playable from the start. The others open with Unlock Chapter on the menu subsystem, which is saved for you. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chapter")
	bool bUnlockedByDefault = false;
};

/**
 * One picture layer of the menu backdrop, such as a skyline or shapes close to the camera. The texture's transparency
 * is the shape and Color paints it, so one set of layers suits every theme preset.
 */
USTRUCT(BlueprintType)
struct ASSETPROLOBBYKIT_API FALKBackdropLayer
{
	GENERATED_BODY()

	/** The layer's shape: a white texture with transparency, stretched to cover the screen. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Backdrop Layer")
	TSoftObjectPtr<UTexture2D> Texture;

	/** Colour and opacity the shape is drawn in. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Backdrop Layer")
	FLinearColor Color = FLinearColor::Black;

	/** How much the layer moves with the pointer: 0 stays still, 1 moves the most. Near layers move more. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Backdrop Layer", meta = (ClampMin = "0", ClampMax = "1"))
	float Depth = 0.5f;

	/** Slow sideways drift in pixels per second; the texture repeats sideways. 0 keeps the layer still. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Backdrop Layer")
	float DriftSpeed = 0.f;

	/** Drawn in front of the dot pattern instead of behind it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Backdrop Layer")
	bool bInFront = false;

	/**
	 * For a full picture (a photo or a painting, drawn with a white Color) rather than a shape: it is drawn first, under
	 * the accent glows, so the glows and the dot pattern lie over it. The skins' own backdrop pictures use this.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Backdrop Layer")
	bool bBehindGlows = false;

	FALKBackdropLayer() = default;

	FALKBackdropLayer(const TSoftObjectPtr<UTexture2D>& InTexture, const FLinearColor& InColor, float InDepth, bool bInIsInFront = false)
		: Texture(InTexture), Color(InColor), Depth(InDepth), bInFront(bInIsInFront)
	{
	}
};
