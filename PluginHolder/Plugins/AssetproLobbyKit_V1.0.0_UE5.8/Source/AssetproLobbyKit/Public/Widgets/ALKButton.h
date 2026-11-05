// Copyright 2026 CodeLikeMe. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CommonButtonBase.h"
#include "ALKTypes.h"
#include "Widgets/ALKDecor.h"
#include "Widgets/ALKInputLabels.h"
#include "ALKButton.generated.h"

class UALKTheme;
class UALKIconWidget;
class UBorder;
class UTextBlock;
class UWidgetTree;
enum class ECommonInputType : uint8;

/** CommonUI button style with no visuals at all. UALKButton draws itself from the theme instead. */
UCLASS()
class ASSETPROLOBBYKIT_API UALKButtonStyle_Invisible : public UCommonButtonStyle
{
	GENERATED_BODY()

public:
	UALKButtonStyle_Invisible();
};

/**
 * Themed CommonUI button used for every button in the menu.
 *
 * Default layout (generated when there is no designer layout):
 *   Background (ALKFrame) > [KeyCap (ALKFrame) > KeyText] [LeadingIcon] [Label (ALKTitleText)] ... [Chevron]
 *
 * Menu entries light up in a rounded frame with a small accent bar when focused. A UMG child can provide its own
 * layout; name widgets Background, AccentBar, KeyCap, KeyText, LeadingIcon, Label and Chevron to keep the theme
 * colouring and focus animation (Background and KeyCap may be plain Borders), or untick Apply Theme Styles to handle
 * visuals entirely in Blueprint.
 */
UCLASS(Blueprintable)
class ASSETPROLOBBYKIT_API UALKButton : public UCommonButtonBase
{
	GENERATED_BODY()

public:
	UALKButton(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintCallable, Category = "Assetpro Menu|Button")
	void SetButtonText(const FText& InText);

	UFUNCTION(BlueprintPure, Category = "Assetpro Menu|Button")
	FText GetButtonText() const { return ButtonText; }

	UFUNCTION(BlueprintCallable, Category = "Assetpro Menu|Button")
	void SetButtonStyleType(EALKButtonStyle InStyle);

	UFUNCTION(BlueprintPure, Category = "Assetpro Menu|Button")
	EALKButtonStyle GetButtonStyleType() const { return ButtonStyleType; }

	/**
	 * Shows the button as active (current tab, selected key slot) without using CommonUI selection, so clicks keep
	 * working normally.
	 */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Menu|Button")
	void SetActiveVisual(bool bInActive);

	UFUNCTION(BlueprintPure, Category = "Assetpro Menu|Button")
	bool IsActiveVisual() const { return bActiveVisual; }

	/**
	 * Shows a key in front of the label ("[A] Confirm"). The key follows the input method in use: keyboard key text
	 * for mouse and keyboard, the controller's button for gamepads. Hidden when the hint has no key for the method.
	 */
	void SetInputHint(const FALKInputHint& InHint);

	void ClearInputHint();

	bool HasInputHint() const { return bHasInputHint; }

	/** Shows a small icon in front of the label (a tick on "Ready up", a bolt on "Find match"). None hides it. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Menu|Button")
	void SetLeadingIcon(EALKIcon InIcon);

	UFUNCTION(BlueprintPure, Category = "Assetpro Menu|Button")
	EALKIcon GetLeadingIcon() const { return LeadingIconType; }

	/** Presses and clicks the button as if the player did (respects enabled / interaction state). */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Menu|Button")
	void SimulateClick();

	/** Restyles the button from the theme. Called on construct and whenever the theme changes. */
	UFUNCTION(BlueprintNativeEvent, Category = "Assetpro Menu|Button")
	void ApplyTheme(const UALKTheme* Theme);

	/** Free identifier, e.g. the menu entry Id this button represents. */
	UPROPERTY(BlueprintReadWrite, Category = "Assetpro Menu|Button")
	FName ButtonId;

	/** Move keyboard / gamepad focus to this button when the mouse hovers it, so only one button is ever highlighted. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Assetpro Menu|Button")
	bool bFocusOnHover = true;

	/** Play the theme's Click sound when clicked. Tabs and value arrows turn it off: they play what they trigger (Tab, Change). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Assetpro Menu|Button")
	bool bPlayClickSound = true;

	/**
	 * Hint style: a clickable hint that is only a key (no label) still gets the box labelled hints have while the mouse
	 * is in use. For the first of two keys that share one label ("[Q] [E] Switch tab"), so the pair reads as one hint.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Assetpro Menu|Button")
	bool bBoxKeyOnlyHint = false;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnAddedToFocusPath(const FFocusEvent& InFocusEvent) override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply NativeOnKeyUp(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual void NativeOnHovered() override;
	virtual void NativeOnUnhovered() override;
	virtual void NativeOnPressed() override;
	virtual void NativeOnReleased() override;
	virtual void NativeOnClicked() override;
	virtual void NativeOnSelected(bool bBroadcast) override;
	virtual void NativeOnDeselected(bool bBroadcast) override;
	virtual void NativeOnEnabled() override;
	virtual void NativeOnDisabled() override;

	virtual void NativeBuildDefaultLayout(UWidgetTree& Tree);
	virtual void NativeApplyTheme(const UALKTheme& Theme);

	/**
	 * Called whenever the (animated) look changes, after the background and label are updated. Highlight blends from
	 * 0 to 1 while the button gains focus. Subclasses restyle their extra widgets here (the chapter still: its frame).
	 */
	virtual void NativeApplyVisualState(const UALKTheme& Theme, float Highlight, bool bEnabled) {}

	/** Interaction enabled and not disabled: the button can be used now. */
	bool IsUsable() const { return GetIsEnabled() && IsInteractionEnabled(); }

	/** Current, animated highlight (0 = normal, 1 = focused). */
	float GetHighlight() const { return CurrentAccent; }

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Assetpro Menu|Button", meta = (ExposeOnSpawn = true))
	FText ButtonText;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Assetpro Menu|Button", meta = (ExposeOnSpawn = true))
	EALKButtonStyle ButtonStyleType = EALKButtonStyle::Primary;

	/** Restyle from the theme. Untick in a UMG child that handles its own visuals. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Assetpro Menu|Button")
	bool bApplyThemeStyles = true;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Menu|Button", meta = (BindWidgetOptional))
	TObjectPtr<UBorder> Background;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Menu|Button", meta = (BindWidgetOptional))
	TObjectPtr<UBorder> AccentBar;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Menu|Button", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Label;

	/** Key shown in front of the label by Set Input Hint. */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Menu|Button", meta = (BindWidgetOptional))
	TObjectPtr<UBorder> KeyCap;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Menu|Button", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> KeyText;

	/** The icon in front of the label (see Set Leading Icon). */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Menu|Button", meta = (BindWidgetOptional))
	TObjectPtr<UALKIconWidget> LeadingIcon;

	/** A small arrow at the far end of a focused menu entry. */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Menu|Button", meta = (BindWidgetOptional))
	TObjectPtr<UALKIconWidget> Chevron;

private:
	void HandleThemeChanged();
	void HandleInputMethodChanged(ECommonInputType NewInputType);
	void RefreshInputHint();
	void UpdateKeyCapVisibility();
	void LayoutLabel(const UALKTheme& Theme);
	bool IsHintBoxed() const;
	void HandleFocusChanged();
	void UpdateVisualTargets(bool bInstant);
	void ApplyCurrentVisuals();
	void PlayThemeSound(EALKUISound Sound) const;
	bool IsHighlighted() const;

	FDelegateHandle ThemeChangedHandle;
	FDelegateHandle InputMethodChangedHandle;

	FALKInputHint InputHint;
	bool bHasInputHint = false;
	bool bGamepadInput = false;

	/** Label placement for the current style, set by NativeApplyTheme. */
	TEnumAsByte<EHorizontalAlignment> LabelAlignment = HAlign_Center;
	FMargin LabelPaddingValue;

	void UpdateLeadingIcon();

	EALKIcon LeadingIconType = EALKIcon::None;
	bool bHoveredVisual = false;
	bool bPressedVisual = false;
	bool bActiveVisual = false;
	bool bWasFocused = false;
	bool bEnterPressed = false;

	FLinearColor CurrentFill = FLinearColor::Transparent;
	FLinearColor TargetFill = FLinearColor::Transparent;
	FLinearColor CurrentOutline = FLinearColor::Transparent;
	FLinearColor TargetOutline = FLinearColor::Transparent;
	FLinearColor CurrentText = FLinearColor::White;
	FLinearColor TargetText = FLinearColor::White;
	float CurrentAccent = 0.f;
	float TargetAccent = 0.f;
};
