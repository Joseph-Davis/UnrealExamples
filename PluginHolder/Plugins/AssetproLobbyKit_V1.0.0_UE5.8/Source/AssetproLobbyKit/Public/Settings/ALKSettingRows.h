// Copyright 2026 CodeLikeMe. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/ALKUserWidget.h"
#include "Fonts/SlateFontInfo.h"
#include "ALKTypes.h"
#include "ALKSettingRows.generated.h"

class UALKSetting;
class UALKTheme;
class UALKButton;
class UALKSwitch;
class UBorder;
class UTextBlock;
class UHorizontalBox;
class USlider;
class UProgressBar;
class UALKMenuSubsystem;

/**
 * Base row of the settings list: a focusable line with the setting name on the left and a value control on the
 * right. Left / Right (keys, d-pad, stick) change the value; Accept activates it.
 *
 * Designer layouts can bind RowBackground and NameText; subclasses document their own value widgets.
 */
UCLASS(Abstract, Blueprintable)
class ASSETPROLOBBYKIT_API UALKSettingRow : public UALKUserWidget
{
	GENERATED_BODY()

public:
	UALKSettingRow(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintCallable, Category = "Assetpro Menu|Settings")
	void SetSetting(UALKSetting* InSetting);

	UFUNCTION(BlueprintPure, Category = "Assetpro Menu|Settings")
	UALKSetting* GetSetting() const { return Setting; }

	/** Re-reads the value from the setting. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Menu|Settings")
	void Refresh();

	/** Blueprint hook after the row refreshed its value. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Assetpro Menu|Settings")
	void OnRefreshed(UALKSetting* InSetting);

	UFUNCTION(BlueprintPure, Category = "Assetpro Menu|Settings")
	bool IsRowHighlighted() const;

	/** Runs the row's Accept action (cycle an option, rebind a key, run an action), as the gamepad A button does. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Menu|Settings")
	void HandleGamepadAccept();

	/** Fired when the row gains focus (used for the description panel and scrolling). */
	TMulticastDelegate<void(UALKSettingRow*, bool /*bFromNavigation*/)> OnRowFocused;

protected:
	virtual void NativeBuildDefaultLayout(UWidgetTree& Tree) override;
	virtual void NativeApplyTheme(const UALKTheme& Theme) override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void NativeOnAddedToFocusPath(const FFocusEvent& InFocusEvent) override;
	virtual void NativeOnRemovedFromFocusPath(const FFocusEvent& InFocusEvent) override;
	virtual void NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseLeave(const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FNavigationReply NativeOnNavigation(const FGeometry& MyGeometry, const FNavigationEvent& InNavigationEvent, const FNavigationReply& InDefaultReply) override;

	/** Subclasses add their value widgets to ValueBox (right half of the row). */
	virtual void NativeBuildValueArea(UWidgetTree& Tree, UHorizontalBox& ValueBox) {}

	/** Update value widgets from the setting. */
	virtual void NativeRefresh() {}

	/** Restyle value widgets for the (animated) focus highlight: 0 = normal, 1 = focused. */
	virtual void NativeApplyHighlight(const UALKTheme& Theme, float InHighlight, bool bEditable) {}

	/** Font of names and values in rows: the button font, smaller. */
	static FSlateFontInfo GetRowFont(const UALKTheme& Theme);

	/** Gives a row's name or value text the row font, in capitals when the theme's buttons are. */
	static void StyleRowText(UTextBlock* Text, const UALKTheme& Theme);

	/** Current, animated highlight (0 = normal, 1 = focused). */
	float GetHighlight() const { return Highlight; }

	/** Left (-1) / Right (+1). Return true if handled. */
	virtual bool NativeOnAdjust(int32 Direction) { return false; }

	/** Runs NativeOnAdjust and plays the theme's Change sound when the shown value changed. */
	bool AdjustValue(int32 Direction);

	/** Plays the theme's Change sound when the setting now shows another value than Before. */
	void PlayChangeSoundIfChanged(const FText& Before) const;

	/** Accept pressed (Enter, Space, gamepad A). */
	virtual void NativeOnAccept() {}

	/** Extra key handling (e.g. clear binding). Return true if handled. */
	virtual bool NativeOnRowKey(const FKey& Key) { return false; }

	UALKMenuSubsystem* GetMenuSubsystem() const;
	UALKButton* CreateRowButton(UWidgetTree& Tree, const FText& Text, EALKButtonStyle Style);

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Menu|Settings", meta = (BindWidgetOptional))
	TObjectPtr<UBorder> RowBackground;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Menu|Settings", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> NameText;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Assetpro Menu|Settings")
	TObjectPtr<UALKSetting> Setting;

	/** True while CommonUI turns the gamepad accept button into a simulated mouse click. */
	bool IsGamepadSimulatedClick() const;

private:
	void ApplyHighlight();
	void UpdateGamepadHitTesting();

	FDelegateHandle InputMethodChangedHandle;
	bool bMouseOver = false;
	float Highlight = 0.f;
	float TargetHighlight = 0.f;
};

/** Options row: [<] Value [>]; the arrows show while the row has focus. Binds LeftButton, RightButton, ValueText, PipBox (optional position pips). */
UCLASS(Blueprintable)
class ASSETPROLOBBYKIT_API UALKSettingRow_Options : public UALKSettingRow
{
	GENERATED_BODY()

protected:
	virtual void NativeBuildValueArea(UWidgetTree& Tree, UHorizontalBox& ValueBox) override;
	virtual void NativeOnLayoutReady() override;
	virtual void NativeApplyTheme(const UALKTheme& Theme) override;
	virtual void NativeRefresh() override;
	virtual void NativeApplyHighlight(const UALKTheme& Theme, float InHighlight, bool bEditable) override;
	virtual bool NativeOnAdjust(int32 Direction) override;
	virtual void NativeOnAccept() override;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Menu|Settings", meta = (BindWidgetOptional))
	TObjectPtr<UALKButton> LeftButton;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Menu|Settings", meta = (BindWidgetOptional))
	TObjectPtr<UALKButton> RightButton;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Menu|Settings", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ValueText;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Menu|Settings", meta = (BindWidgetOptional))
	TObjectPtr<UHorizontalBox> PipBox;

	/** Shown instead of the arrows for Off / On settings (see ALKSetting_Options: Is Toggle). */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Menu|Settings", meta = (BindWidgetOptional))
	TObjectPtr<UALKSwitch> ToggleSwitch;

	/** Invisible button over the switch, so the mouse can flip it. */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Menu|Settings", meta = (BindWidgetOptional))
	TObjectPtr<UALKButton> ToggleButton;

private:
	void RebuildPips();

	UPROPERTY(Transient)
	TArray<TObjectPtr<UBorder>> Pips;
};

/**
 * Slider row: a rounded bar with a knob and the value. Binds Slider (input), ValueBar (the bar), SliderFill (a
 * progress bar kept for designer layouts; hidden when a ValueBar is present) and ValueText.
 */
UCLASS(Blueprintable)
class ASSETPROLOBBYKIT_API UALKSettingRow_Slider : public UALKSettingRow
{
	GENERATED_BODY()

protected:
	virtual void NativeBuildValueArea(UWidgetTree& Tree, UHorizontalBox& ValueBox) override;
	virtual void NativeOnLayoutReady() override;
	virtual void NativeApplyTheme(const UALKTheme& Theme) override;
	virtual void NativeRefresh() override;
	virtual void NativeApplyHighlight(const UALKTheme& Theme, float InHighlight, bool bEditable) override;
	virtual bool NativeOnAdjust(int32 Direction) override;

	UFUNCTION()
	void HandleSliderValueChanged(float Value);

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Menu|Settings", meta = (BindWidgetOptional))
	TObjectPtr<USlider> Slider;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Menu|Settings", meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> SliderFill;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Menu|Settings", meta = (BindWidgetOptional))
	TObjectPtr<class UALKValueBar> ValueBar;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Menu|Settings", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ValueText;

private:
	bool bUpdatingSlider = false;
};

/** Key binding row: two key slots. Left / Right picks a slot, Accept rebinds it, Delete / gamepad X clears it. */
UCLASS(Blueprintable)
class ASSETPROLOBBYKIT_API UALKSettingRow_KeyBinding : public UALKSettingRow
{
	GENERATED_BODY()

protected:
	virtual void NativeBuildValueArea(UWidgetTree& Tree, UHorizontalBox& ValueBox) override;
	virtual void NativeOnLayoutReady() override;
	virtual void NativeRefresh() override;
	virtual void NativeApplyHighlight(const UALKTheme& Theme, float InHighlight, bool bEditable) override;
	virtual bool NativeOnAdjust(int32 Direction) override;
	virtual void NativeOnAccept() override;
	virtual bool NativeOnRowKey(const FKey& Key) override;
	virtual void NativeOnAddedToFocusPath(const FFocusEvent& InFocusEvent) override;
	virtual void NativeOnRemovedFromFocusPath(const FFocusEvent& InFocusEvent) override;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Menu|Settings", meta = (BindWidgetOptional))
	TObjectPtr<UALKButton> PrimaryKeyButton;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Menu|Settings", meta = (BindWidgetOptional))
	TObjectPtr<UALKButton> SecondaryKeyButton;

private:
	void BeginCapture(int32 SlotIndex);
	void UpdateSlotSelection();

	int32 SelectedSlot = 0;
};

/** Action row: a single button on the right (e.g. Auto-Detect, Reset). Binds ActionButton. */
UCLASS(Blueprintable)
class ASSETPROLOBBYKIT_API UALKSettingRow_Action : public UALKSettingRow
{
	GENERATED_BODY()

protected:
	virtual void NativeBuildValueArea(UWidgetTree& Tree, UHorizontalBox& ValueBox) override;
	virtual void NativeOnLayoutReady() override;
	virtual void NativeRefresh() override;
	virtual void NativeOnAccept() override;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Menu|Settings", meta = (BindWidgetOptional))
	TObjectPtr<UALKButton> ActionButton;
};
