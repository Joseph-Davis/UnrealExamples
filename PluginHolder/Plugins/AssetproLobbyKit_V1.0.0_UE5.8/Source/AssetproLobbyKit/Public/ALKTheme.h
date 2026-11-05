// Copyright 2026 CodeLikeMe. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Fonts/SlateFontInfo.h"
#include "Styling/SlateBrush.h"
#include "Layout/Margin.h"
#include "ALKTypes.h"
#include "Widgets/ALKFrame.h"
#include "ALKTheme.generated.h"

class USoundBase;
class UTexture2D;

/**
 * The single source of truth for how the whole menu looks.
 *
 * Create a data asset of this class (or a Blueprint child of it), tweak colours / fonts / sizes, and assign it in
 * Project Settings > Plugins > Assetpro Lobby Kit. Every widget in the plugin reads from the active theme, so one
 * asset re-skins every screen. Editing the asset while playing in editor updates open menus live.
 */
UCLASS(BlueprintType, Blueprintable)
class ASSETPROLOBBYKIT_API UALKTheme : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UALKTheme();

	// ---------------------------------------------------------------- Palette

	/** Top colour of the full screen background gradient. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Palette")
	FLinearColor BackgroundTop;

	/** Bottom colour of the full screen background gradient. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Palette")
	FLinearColor BackgroundBottom;

	/** Tint drawn behind dialogs and popups. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Palette")
	FLinearColor ScreenDim;

	/** Tint over the game behind the pause menu (under its blur). Alpha sets the strength. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Palette")
	FLinearColor GameDim;

	/**
	 * Darkening over the scene behind screens full of text: settings pages, the controller layout, credits and the
	 * chapter select. Alpha sets the strength; a light colour brightens instead (light themes).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Palette")
	FLinearColor PageDim;

	/** Fill of panels (pause menu, dialogs, cards, lists). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Palette")
	FLinearColor Panel;

	/** Fill of things that sit on a panel: player slots, chips, input boxes, the focused card. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Palette")
	FLinearColor PanelRaised;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Palette")
	FLinearColor PanelOutline;

	/** Highlight colour: the focused item, selected tabs, sliders, switches, primary buttons. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Palette")
	FLinearColor Accent;

	/** A second, calmer highlight for information that is not a call to action (links, "in game" presence). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Palette")
	FLinearColor AccentAlt;

	/** Text drawn on top of the accent colour. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Palette")
	FLinearColor AccentContrast;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Palette")
	FLinearColor TextPrimary;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Palette")
	FLinearColor TextSecondary;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Palette")
	FLinearColor TextMuted;

	/** Destructive actions and errors: leave, kick, bad ping. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Palette")
	FLinearColor Danger;

	/** Good news: online, ready, low ping, talking. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Palette")
	FLinearColor Success;

	/** Attention: the host's crown, favourites, medium ping, notices. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Palette")
	FLinearColor Warning;

	/** Shadow under panels (Panel Style: Raised or Chunky). Alpha sets the strength. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Palette")
	FLinearColor Shadow;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Palette|Buttons")
	FLinearColor ButtonNormal;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Palette|Buttons")
	FLinearColor ButtonHovered;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Palette|Buttons")
	FLinearColor ButtonPressed;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Palette|Buttons")
	FLinearColor ButtonDisabled;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Palette|Settings")
	FLinearColor RowNormal;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Palette|Settings")
	FLinearColor RowFocused;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Palette|Settings")
	FLinearColor SliderTrack;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Palette|Settings")
	FLinearColor Separator;

	/** Fill of key caps in input hints and key binding rows ("Esc", "Enter"). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Palette|Keys")
	FLinearColor KeyFill;

	/** Outline of key caps. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Palette|Keys")
	FLinearColor KeyOutline;

	/** Text on key caps. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Palette|Keys")
	FLinearColor KeyText;

	// ---------------------------------------------------------------- Typography

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Typography")
	FSlateFontInfo TitleFont;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Typography")
	FSlateFontInfo HeadingFont;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Typography")
	FSlateFontInfo SubheadingFont;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Typography")
	FSlateFontInfo BodyFont;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Typography")
	FSlateFontInfo ButtonFont;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Typography")
	FSlateFontInfo CaptionFont;

	/** Small labels and values: column headers, key caps, timers, counts and the HUD's small print. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Typography")
	FSlateFontInfo DataFont;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Typography")
	bool bUppercaseTitles = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Typography")
	bool bUppercaseButtons = false;

	/** Drop shadow behind titles. Set alpha to 0 to disable. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Typography")
	FLinearColor TitleShadowColor;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Typography")
	FVector2D TitleShadowOffset = FVector2D(0.0, 3.0);

	// ---------------------------------------------------------------- Shape

	/** Corner radius of buttons, rows, cards and panels. 0 gives sharp corners. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shape", meta = (ClampMin = "0"))
	float CornerRadius = 10.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shape", meta = (ClampMin = "0"))
	float OutlineWidth = 1.f;

	/** How panels and cards are drawn: flat, outlined, raised on a soft shadow, or chunky with a hard shadow. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shape")
	EALKPanelStyle PanelStyle = EALKPanelStyle::Raised;

	/** How far the shadow of panels is moved (Panel Style: Raised or Chunky). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shape")
	FVector2D ShadowOffset = FVector2D(0.0, 8.0);

	/** How far the soft shadow of raised panels spreads. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shape", meta = (ClampMin = "0"))
	float ShadowBlur = 16.f;

	/** How the focused item is marked: a tint of the accent colour, or filled with it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shape")
	EALKFocusStyle FocusStyle = EALKFocusStyle::Tinted;

	/** Width of the small bar on the left of the focused menu entry (Focus Style: Tinted). 0 hides it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shape", meta = (ClampMin = "0"))
	float AccentBarWidth = 4.f;

	/** Width of the ring round focused cards, tiles and primary buttons. 0 hides it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shape", meta = (ClampMin = "0"))
	float FocusRingWidth = 2.f;

	// ---------------------------------------------------------------- Depth
	// What makes panels read as lit glass floating over the backdrop instead of flat boxes. Each value can be turned down
	// (Panel Opacity up to 1, the others to 0) for the flat look; the Arcade skin does exactly that.

	/**
	 * How solid panels and cards are: 1 is opaque, lower lets the blurred backdrop tint them like dark (or frosted)
	 * glass. Keep it above 0.75 so text stays easy to read.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Depth", meta = (ClampMin = "0.3", ClampMax = "1"))
	float PanelOpacity = 1.f;

	/**
	 * Strength of the soft top-to-bottom gradient on panels, cards and boxed buttons: a little lighter at the top, a
	 * little darker at the bottom. 0 gives flat fills; the main action button uses about three times this.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Depth", meta = (ClampMin = "0", ClampMax = "0.5"))
	float PanelGradient = 0.f;

	/** The one pixel lit line along the top edge of panels, cards and boxed buttons. Alpha sets the strength; 0 hides it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Depth")
	FLinearColor PanelTopEdge;

	/** Strength of the soft glow in the accent colour round the focused row, card and main button. 0 turns it off. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Depth", meta = (ClampMin = "0", ClampMax = "1"))
	float FocusGlow = 0.f;

	/** How far the focus glow spreads, in pixels. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Depth", meta = (ClampMin = "0", ClampMax = "64"))
	float FocusGlowSize = 16.f;

	/**
	 * How much of a focused row's tint is a gradient that starts strong at its leading edge, instead of one flat tint
	 * (Focus Style: Tinted). 0 keeps the flat tint, 1 is the full gradient.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Depth", meta = (ClampMin = "0", ClampMax = "1"))
	float FocusGradient = 0.f;

	/**
	 * How much a focused card grows, so it lifts off the screen: 1.02 is two percent. 1 keeps cards still, and so does
	 * the Reduce Effects setting.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Depth", meta = (ClampMin = "1", ClampMax = "1.1"))
	float CardFocusScale = 1.f;

	/**
	 * How strongly the lower part of a picture fades into the panel under it, where a title sits on or right under the
	 * picture (cards, the lobby's match card, the server details). 0 shows pictures with a hard lower edge.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Depth", meta = (ClampMin = "0", ClampMax = "1"))
	float PictureVeil = 0.f;

	// ---------------------------------------------------------------- Layout

	/** Padding between the screen edge and the menu content. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout")
	FMargin ScreenPadding = FMargin(96.f, 64.f);

	/** Where the main menu content sits horizontally. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout")
	TEnumAsByte<EHorizontalAlignment> MainMenuAlignment = HAlign_Left;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout", meta = (ClampMin = "0"))
	float MenuButtonWidth = 380.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout", meta = (ClampMin = "0"))
	float MenuButtonHeight = 54.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout", meta = (ClampMin = "0"))
	float MenuButtonSpacing = 6.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout")
	FMargin ButtonContentPadding = FMargin(22.f, 8.f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout", meta = (ClampMin = "0"))
	float SecondaryButtonHeight = 44.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout", meta = (ClampMin = "0"))
	float PauseMenuWidth = 460.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout", meta = (ClampMin = "0"))
	float DialogWidth = 640.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout", meta = (ClampMin = "0"))
	float SettingRowHeight = 42.f;

	/** Largest width of the setting description on the right of the settings list. 0 hides it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout", meta = (ClampMin = "0"))
	float SettingsDescriptionWidth = 460.f;

	// ---------------------------------------------------------------- Background

	/** Use Background Brush (a texture or material) instead of the colour gradient. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Background")
	bool bUseBackgroundBrush = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Background", meta = (EditCondition = "bUseBackgroundBrush"))
	FSlateBrush BackgroundBrush;

	/**
	 * Lower this to let your menu level (a 3D scene, a camera fly-through...) show behind the main menu. The gradient,
	 * glows, pattern and picture layers fade; the vignette stays.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Background", meta = (ClampMin = "0", ClampMax = "1"))
	float MainMenuBackgroundOpacity = 1.f;

	/** Blur of the game behind the pause menu and the menus opened from it. 0 turns it off. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Background", meta = (ClampMin = "0", ClampMax = "100"))
	float PauseBlur = 6.f;

	/**
	 * How much colour drains from the game while the pause menu is open: 0 keeps it, 1 turns it black and white. Done
	 * with a camera modifier on the player's view (ALKCameraModifier_Drain), so it fades in and out.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Background", meta = (ClampMin = "0", ClampMax = "1"))
	float PauseDesaturation = 0.f;

	/**
	 * Blur of the backdrop scene behind screens full of text (settings pages, the controller layout, credits and the
	 * chapter select): with Page Dim, the scene steps back out of focus behind them. 0 turns it off.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Background", meta = (ClampMin = "0", ClampMax = "100"))
	float PageBlur = 6.f;

	/** Strength of a film grain over menu backgrounds. 0 (the default) turns it off. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Background", meta = (ClampMin = "0", ClampMax = "1"))
	float BackgroundGrain = 0.f;

	/**
	 * How many times a second the grain changes to a new pattern, like film running through a projector (24). 0 keeps it
	 * still, a texture over the scene.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Background", meta = (ClampMin = "0", ClampMax = "30"))
	float GrainSpeed = 24.f;

	/** Colour of the large soft glows behind the menus (the presets use the accent colour). Alpha sets the strength; 0 turns them off. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Background")
	FLinearColor BackgroundGlow;

	/** How fast the glows drift round: 1 is the calm default, 0 keeps them still. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Background", meta = (ClampMin = "0", ClampMax = "10"))
	float GlowDriftSpeed = 1.f;

	/** Colour of the faint dot pattern over the background. Alpha sets the strength; 0 turns it off. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Background")
	FLinearColor PatternColor;

	/** Pixels (at 1080p) between the dots of the pattern. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Background", meta = (ClampMin = "8", ClampMax = "200"))
	float PatternSpacing = 38.f;

	/** Size (pixels at 1080p) of each dot. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Background", meta = (ClampMin = "0.5", ClampMax = "20"))
	float PatternDotSize = 2.4f;

	/** How much the background darkens towards the screen edges. 0 turns it off. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Background", meta = (ClampMin = "0", ClampMax = "1"))
	float BackgroundVignette = 0.3f;

	// ---------------------------------------------------------------- Backdrop
	// Drawn behind every menu by UALKAtmosphere, all in code: the glows and the dot pattern above, your own picture
	// layers, a darkening behind the menu and the vignette. It keeps moving while the game is paused.

	/**
	 * The picture layers of the backdrop, far to near. Each skin puts its own picture here as the first layer (a full
	 * picture, Behind Glows) when the plugin's Backdrops folder has it; applying another skin swaps that one layer and
	 * keeps yours. Add layers of your own (white shapes with transparency drawn in their Color, or full pictures), lower
	 * the first layer's Color to dim the picture, or remove it for the plain gradient with glows. The menu config's Main
	 * Menu Background Image replaces the backdrop on the main menu with key art.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Backdrop")
	TArray<FALKBackdropLayer> BackdropLayers;

	/** How far (pixels at 1080p) the nearest layer moves with the pointer. 0 keeps the backdrop still. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Backdrop", meta = (ClampMin = "0"))
	float ParallaxAmount = 14.f;

	/**
	 * Darkening behind the menu, from the side the menu sits on (Main Menu Alignment), so its text reads over key art
	 * or picture layers. Alpha sets the strength; a light colour brightens instead (for dark text on a light theme).
	 * Drawn on the menus that show the backdrop clearly (main menu, settings menu, title screen); screens full of
	 * panels use Page Dim instead.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Backdrop")
	FLinearColor MenuScrim;

	// ---------------------------------------------------------------- HUD
	// The HUD essentials (UALKHUDSubsystem): interaction prompts, subtitles, notifications, chapter titles and the saving
	// indicator. They use the palette and frames above.

	/** Font of subtitles (the Subtitle Size setting scales it). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HUD")
	FSlateFontInfo SubtitleFont;

	/** Widest a subtitle line gets before it wraps (pixels at 1080p). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HUD", meta = (ClampMin = "300"))
	float SubtitleMaxWidth = 1200.f;

	/** Distance of the HUD pieces from the screen edges (pixels at 1080p). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HUD")
	FMargin HudMargin = FMargin(70.f, 60.f);

	/** Smallest width of notifications (pixels at 1080p); a long title widens one up to 1.5 times this. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HUD", meta = (ClampMin = "200"))
	float NotificationWidth = 440.f;

	// ---------------------------------------------------------------- Chapters

	/** Width of the chapter still in the chapter select, as a share of the screen's width. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chapters", meta = (ClampMin = "0.2", ClampMax = "0.9"))
	float ChapterStillWidth = 0.5f;

	/** Darkening laid over the stills of locked chapters. Alpha sets the strength. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chapters")
	FLinearColor LockedChapterDim;

	/** Icon shown on locked chapters. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chapters")
	TSoftObjectPtr<UTexture2D> LockIcon;

	// ---------------------------------------------------------------- Motion

	/** Seconds a screen takes to fade in. 0 disables the animation. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Motion", meta = (ClampMin = "0"))
	float ScreenFadeInTime = 0.22f;

	/** How far (pixels) screens slide while fading in. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Motion")
	float ScreenSlideOffset = 28.f;

	/** How quickly buttons blend between normal and hovered colours. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Motion", meta = (ClampMin = "0"))
	float HoverTransitionSpeed = 16.f;

	/** Pixels per second the credits roll. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Motion", meta = (ClampMin = "0"))
	float CreditsScrollSpeed = 55.f;

	// ---------------------------------------------------------------- Sounds
	// Subtle interface sounds ship with the plugin (Content/Sounds) and play at the Interface volume setting. Pick your
	// own sounds, or clear a slot for silence.

	/** Focus moves to another item. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sounds")
	TObjectPtr<USoundBase> HoverSound;

	/** An item is picked. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sounds")
	TObjectPtr<USoundBase> ClickSound;

	/** Back or cancel. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sounds")
	TObjectPtr<USoundBase> BackSound;

	/** A screen or dialog opens. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sounds")
	TObjectPtr<USoundBase> ScreenOpenSound;

	/** Another settings tab is shown, or the chapter select moves to another chapter. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sounds")
	TObjectPtr<USoundBase> TabSound;

	/** A setting's value changes. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sounds")
	TObjectPtr<USoundBase> ChangeSound;

	/** A locked chapter (one not reached yet) is picked. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sounds")
	TObjectPtr<USoundBase> LockedSound;

	/** The player readies up in a lobby. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sounds")
	TObjectPtr<USoundBase> ReadySound;

	/** Matchmaking found a match. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sounds")
	TObjectPtr<USoundBase> MatchFoundSound;

	/** A chat message, an invite or a notice arrives. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sounds")
	TObjectPtr<USoundBase> NotifySound;

	/** Each second of a countdown. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sounds")
	TObjectPtr<USoundBase> CountdownSound;

	/** A connection problem or a refused action. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sounds")
	TObjectPtr<USoundBase> ErrorSound;

	// ---------------------------------------------------------------- Skins and presets

	/**
	 * Pick a skin and a colour preset, then press Apply Skin And Preset to overwrite the palette, shape and backdrop.
	 * Fonts, sizes, sounds and picture layers are kept.
	 */
	UPROPERTY(EditAnywhere, Category = "Skins and Presets")
	EALKThemeSkin SkinToApply = EALKThemeSkin::Default;

	UPROPERTY(EditAnywhere, Category = "Skins and Presets")
	EALKThemePreset PresetToApply = EALKThemePreset::Mint;

	UFUNCTION(CallInEditor, Category = "Skins and Presets", meta = (DisplayName = "Apply Skin And Preset"))
	void ApplySelectedPreset();

	/** Overwrites the palette, shape and backdrop colours with one of the built-in skins in one of the five accent colours. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Menu|Theme")
	void ApplySkin(EALKThemeSkin Skin, EALKThemePreset Preset);

	/** Changes the accent colour of the current skin. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Menu|Theme")
	void ApplyPreset(EALKThemePreset Preset);

	/** The skin and preset applied last. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Skins and Presets")
	EALKThemeSkin CurrentSkin = EALKThemeSkin::Default;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Skins and Presets")
	EALKThemePreset CurrentPreset = EALKThemePreset::Mint;

	/** The accent colour of a preset. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Menu|Theme")
	static FLinearColor GetPresetAccent(EALKThemePreset Preset);

	// ---------------------------------------------------------------- Helpers

	/** The theme's sound for an interface event (None when the slot is empty). */
	UFUNCTION(BlueprintPure, Category = "Assetpro Menu|Theme")
	USoundBase* GetUISound(EALKUISound Sound) const;

	/**
	 * Plays one of the active theme's interface sounds as a 2D UI sound, at the Interface volume setting (when the sound
	 * uses the Menu Config's Interface sound class, the settings' sound mix scales it instead). One action plays one
	 * sound: sounds asked for in the same frame play only the most important one (Match Found, Error, Back, Click, Ready
	 * or Locked, Tab, Change, Open, Notify or Countdown, Hover), and focus that moves by itself (a screen opening, a click,
	 * a tab switch) plays no Hover.
	 */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Menu|Theme", meta = (WorldContext = "WorldContextObject"))
	static void PlayUISound(const UObject* WorldContextObject, EALKUISound Sound);

	/** Fires whenever an interface sound plays. */
	static TMulticastDelegate<void(EALKUISound)>& OnUISoundPlayed();

	UFUNCTION(BlueprintPure, Category = "Assetpro Menu|Theme")
	FSlateFontInfo GetFont(EALKTextRole Role) const;

	UFUNCTION(BlueprintPure, Category = "Assetpro Menu|Theme")
	FLinearColor GetTextColor(EALKTextRole Role) const;

	/** A panel (pause menu, dialogs, settings, lists) in the theme's panel colour and style. bWithShadow false leaves the shadow out (panels inside panels). */
	FALKFrameStyle MakePanelFrame(bool bWithShadow = true) const;

	/**
	 * The panel of a dialog: the panel frame, but solid whatever Panel Opacity says. A dialog lies over a whole screen
	 * that is only dimmed, and that screen's text must not show through the dialog's own.
	 */
	FALKFrameStyle MakeDialogFrame() const;

	/** A card that sits on a panel (player slot, tile, save slot) in the raised colour; Highlight (0-1) blends in the focus ring. */
	FALKFrameStyle MakeCardFrame(float Highlight = 0.f) const;

	/**
	 * The frame of a focusable item (menu entry, row, list line) blended by Highlight from rest (0) to focused (1): the
	 * focus fill and outline fade in, with the small accent bar on the left when bWithTick.
	 */
	FALKFrameStyle MakeItemFrame(float Highlight, bool bWithTick = true) const;

	/** A small solid chip, such as a key cap, a tag or a selected tab. */
	FALKFrameStyle MakeChipFrame(const FLinearColor& Fill, const FLinearColor& Outline) const;

	/**
	 * A filled button: the accent colour for the main action (bDanger: the danger colour), with the focus ring and glow
	 * blended in by Highlight, a gradient and a lit top edge.
	 */
	FALKFrameStyle MakeSolidButtonFrame(float Highlight, bool bDanger = false) const;

	/**
	 * A band in a colour behind a title (the results title on the winning team's colour): a tint that starts strong at
	 * the leading edge on skins with gradients, a flat tint with the panel outline on the others.
	 */
	FALKFrameStyle MakeBandFrame(const FLinearColor& Color) const;

	/**
	 * The flat part of a focused item's tint. Focus Gradient moves a share of the tint into the gradient from the leading
	 * edge (the Wash of MakeItemFrame), so widgets that set an item frame's Fill themselves pass their fill through this.
	 */
	FLinearColor GetFocusFill(const FLinearColor& FlatFill) const;

	/** The Panel colour as panels are drawn: with Panel Opacity applied. Use it for a fill that must match a panel. */
	FLinearColor GetPanelFill() const;

	/** The colour of text on a focused item: the contrast colour when the focus is a solid fill, else the primary text colour. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Menu|Theme")
	FLinearColor GetFocusTextColor() const;

	/** An accent colour that reads as text on panels (darker on light skins). */
	UFUNCTION(BlueprintPure, Category = "Assetpro Menu|Theme")
	FLinearColor GetAccentTextColor() const;

	/** Green, amber or red for a ping in milliseconds (under 60, under 120, above). */
	UFUNCTION(BlueprintPure, Category = "Assetpro Menu|Theme")
	FLinearColor GetPingColor(int32 PingMs) const;

	/** A rounded box brush using this theme's corner radius. */
	FSlateBrush MakeRoundedBrush(const FLinearColor& Fill, const FLinearColor& Outline = FLinearColor::Transparent, float InOutlineWidth = 0.f, float RadiusScale = 1.f) const;

	static FSlateBrush MakeSolidBrush(const FLinearColor& Fill);

	/** A brush with fully rounded ends: a circle when square, a pill when wider (key glyphs, badges). */
	static FSlateBrush MakePillBrush(const FLinearColor& Fill, const FLinearColor& Outline = FLinearColor::Transparent, float InOutlineWidth = 0.f);

	/** Fired when any theme asset is edited or the active theme changes. Open widgets restyle themselves. */
	static FSimpleMulticastDelegate& OnAnyThemeChanged();

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};

/** The Light skin as a ready-made theme class: pick it as Theme Class in Project Settings, or base a data asset on it. */
UCLASS()
class ASSETPROLOBBYKIT_API UALKTheme_Light : public UALKTheme
{
	GENERATED_BODY()

public:
	UALKTheme_Light();
};

/** The Arcade skin as a ready-made theme class: pick it as Theme Class in Project Settings, or base a data asset on it. */
UCLASS()
class ASSETPROLOBBYKIT_API UALKTheme_Arcade : public UALKTheme
{
	GENERATED_BODY()

public:
	UALKTheme_Arcade();
};
