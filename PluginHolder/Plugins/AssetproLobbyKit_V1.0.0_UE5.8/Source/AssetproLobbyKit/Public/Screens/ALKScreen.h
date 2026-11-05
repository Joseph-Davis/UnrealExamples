// Copyright 2026 CodeLikeMe. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "InputCoreTypes.h"
#include "ALKTypes.h"
#include "Widgets/ALKInputLabels.h"
#include "ALKScreen.generated.h"

class UALKTheme;
class UALKMenuConfig;
class UALKMenuSubsystem;
class UALKButton;
class UTexture2D;
class UWidgetTree;
class UPanelWidget;
class UHorizontalBox;
enum class ECommonInputType : uint8;

/**
 * Base class of every menu screen and dialog.
 *
 * - Builds a default layout from the theme when the class has no designer layout (so it works with zero assets).
 * - Restyles itself whenever the theme changes.
 * - Handles Back (Escape, gamepad B / Circle, Android back) without any CommonUI input data setup.
 * - Requests menu input mode (visible cursor, UI focus) while active.
 * - Fades and slides in when activated.
 */
UCLASS(Abstract, Blueprintable)
class ASSETPROLOBBYKIT_API UALKScreen : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	UALKScreen(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintNativeEvent, Category = "Assetpro Menu|Screen")
	void ApplyTheme(const UALKTheme* Theme);

	UFUNCTION(BlueprintPure, Category = "Assetpro Menu|Screen")
	const UALKTheme* GetTheme() const;

	UFUNCTION(BlueprintPure, Category = "Assetpro Menu|Screen")
	const UALKMenuConfig* GetMenuConfig() const;

	UFUNCTION(BlueprintPure, Category = "Assetpro Menu|Screen")
	UALKMenuSubsystem* GetMenuSubsystem() const;

	/** Called when the player presses Back. Return true if handled. The default closes the screen. */
	UFUNCTION(BlueprintNativeEvent, Category = "Assetpro Menu|Screen")
	bool OnBackRequested();

	/** Closes (deactivates) this screen. The owning stack removes it. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Menu|Screen")
	void CloseScreen();

	UFUNCTION(BlueprintPure, Category = "Assetpro Menu|Screen")
	bool IsUsingDefaultLayout() const { return bUsingDefaultLayout; }

	/**
	 * True for about a third of a second of frames after any screen activates or the menu input mode changes (a hitch
	 * counts as one short frame). CommonUI re-centres the cursor when it switches to menu input, which looks like mouse
	 * movement; widgets ignore mouse moves during this window so focus stays on the default item.
	 */
	static bool IsInActivationGracePeriod();

	/** Starts the window in which widgets ignore mouse moves (see Is In Activation Grace Period). */
	static void StartActivationGracePeriod();

	/** True while the current mouse event is CommonUI's simulated click for the gamepad accept button. */
	bool IsGamepadSimulatedClick() const;

	/** True when the player last used the mouse or keyboard (not a gamepad or touch). */
	bool IsUsingMouseAndKeyboard() const;

	/** True while the pause menu is open: full screen menus opened from it show the paused game behind them. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Menu|Screen")
	bool IsShownOverGame() const;

	/** The small label over the screen's title: System Label when you set one, else empty (no label). */
	UFUNCTION(BlueprintPure, Category = "Assetpro Menu|Screen")
	FText MakeSystemLabel(int32 Code, const FText& Name) const;

	/** An optional small label shown over the screen's title (a section name, a breadcrumb). Empty shows none. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Assetpro Menu|Screen")
	FText SystemLabel;

	/** Creates a themed button using the menu config's button class. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Menu|Screen")
	UALKButton* CreateMenuButton(const FText& Text, EALKButtonStyle Style = EALKButtonStyle::Primary);

	/**
	 * Called once after the code generated layout is built (not for designer layouts). Tweak it here from a Blueprint
	 * child: add a logo, change a brush, hide a widget, without redesigning the screen.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Assetpro Menu|Screen")
	void OnDefaultLayoutBuilt();

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;
	virtual bool NativeOnHandleBackAction() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FNavigationReply NativeOnNavigation(const FGeometry& MyGeometry, const FNavigationEvent& InNavigationEvent, const FNavigationReply& InDefaultReply) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnFocusReceived(const FGeometry& InGeometry, const FFocusEvent& InFocusEvent) override;
	virtual void NativeOnFocusChanging(const FWeakWidgetPath& PreviousFocusPath, const FWidgetPath& NewWidgetPath, const FFocusEvent& InFocusEvent) override;

	/**
	 * The button or row of this screen that had focus last, while it is still on the screen and usable. Focus
	 * goes back there when the screen asks for its focus target again: after a screen or dialog on top closes, and
	 * when the player picks up a gamepad (CommonUI refocuses then).
	 */
	UWidget* GetLastFocusedWidget() const;

	/**
	 * Keeps the screen's "MainColumn" (the code generated layout's full screen column) from stretching on ultrawide screens:
	 * once the screen is wider than MaxWidth (in screen units, 1920 at 16:9) the extra width goes into the side padding, so the
	 * content keeps the width it has at 16:9. Call from NativeTick with the screen's geometry.
	 */
	void CapContentWidth(const FGeometry& ScreenGeometry, float MaxWidth = 1920.f);

	/** Build the code generated layout. Only called when the class has no designer layout. Must set Tree.RootWidget. */
	virtual void NativeBuildDefaultLayout(UWidgetTree& Tree) {}

	/** Called after the layout exists (designer or generated). Bind events to named widgets here. */
	virtual void NativeOnLayoutReady() {}

	virtual void NativeApplyTheme(const UALKTheme& Theme) {}

	/** Return true to consume the key. Called before the built-in back handling. */
	virtual bool NativeHandleMenuKey(const FKey& Key) { return false; }

	/** Is this key a "back" key (Escape, gamepad B, Android back)? */
	static bool IsBackKey(const FKey& Key);

	/**
	 * Styles the background built by FALKWidgetBuilder::AddBackgroundLayers: the theme's scene, or the paused game
	 * behind a blur while the pause menu is open. Screens restyle by themselves when that changes. Opacity fades the
	 * scene (a main menu over a 3D level).
	 */
	void StyleScreenBackground(const UALKTheme& Theme, float Opacity = 1.f);

	/** The screen always shows the game behind it (the pause menu). */
	bool bAlwaysOverGame = false;

	/** The theme's Page Dim darkens the scene behind this screen (screens full of text). */
	bool bDimScene = false;

	/**
	 * The menu config's Main Menu Background Image (key art) replaces the theme's backdrop behind this screen: the main
	 * menu and the title screen. Every other screen sits on the theme's backdrop.
	 */
	bool bShowKeyArt = false;

	/** Plays one of the theme's sounds as a UI sound. */
	void PlayThemeSound(EALKUISound Sound) const;

	/**
	 * Builds a row of input hints like "[Esc] Back". Each hint is a small button (see FALKInputHint): its key follows the
	 * input method and hints with an Action can be clicked. Hidden when the menu config turns input hints off.
	 */
	UHorizontalBox* BuildInputHints(UWidgetTree& Tree, const TArray<FALKInputHint>& Hints, TArray<UALKButton*>* OutButtons = nullptr);

	/**
	 * Runs when the screen activates with Animate In on, after the screen's own fade has started: play extra intro
	 * animations of your widgets here (bFirstTime: the screen was just opened, not uncovered).
	 */
	virtual void PlayIntroEffects(bool bFirstTime);

	/** Called when the player switches between mouse and keyboard, gamepad and touch. */
	virtual void NativeOnInputMethodChanged(bool bUsingGamepad) {}

	bool IsUsingGamepad() const;

	/** Widget focused when the screen activates. Set this from your layout (or override Get Desired Focus Target). */
	UPROPERTY(BlueprintReadWrite, Category = "Assetpro Menu|Screen")
	TObjectPtr<UWidget> DefaultFocusWidget;

	/** Can Back close this screen? */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Assetpro Menu|Screen")
	bool bCloseOnBack = true;

	/** Restyle from the theme. Untick in a UMG child if you styled everything in the designer. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Assetpro Menu|Screen")
	bool bApplyThemeStyles = true;

	/** Play the fade / slide in animation from the theme on activation. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Assetpro Menu|Screen")
	bool bAnimateIn = true;

	/** Keeps runtime generated textures (background gradient) alive. */
	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> BackgroundTexture;

	/** Keeps the generated background grain, glow and vignette textures alive. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTexture2D>> OverlayTextures;

	/** The buttons created by BuildInputHints. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UALKButton>> HintButtons;

private:
	void HandleThemeChanged();
	void HandleInputMethodChanged(ECommonInputType NewInputType);

	FDelegateHandle ThemeChangedHandle;
	FDelegateHandle InputMethodChangedHandle;
	TWeakObjectPtr<UWidget> LastFocusedWidget;
	bool bUsingDefaultLayout = false;
	/** Whether the background was last styled over the game (see StyleScreenBackground). */
	bool bStyledOverGame = false;
	/** Opening a screen plays the Open sound, coming back to it when the screen above closes does not. */
	bool bOpenSoundPlayed = false;
	float AnimInElapsed = -1.f;
};
