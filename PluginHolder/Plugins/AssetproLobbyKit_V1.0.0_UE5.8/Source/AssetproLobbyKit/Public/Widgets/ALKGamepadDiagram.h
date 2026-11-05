// Copyright 2026 CodeLikeMe. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ALKGamepadDiagram.generated.h"

/** The buttons and sticks labelled on the gamepad diagram. */
UENUM(BlueprintType)
enum class EALKGamepadInput : uint8
{
	LeftTrigger,
	LeftBumper,
	LeftStick,
	LeftStickPress,
	View,
	DPad,
	RightTrigger,
	RightBumper,
	Menu,
	FaceTop,
	FaceRight,
	FaceLeft,
	FaceBottom,
	RightStick,
	RightStickPress
};

/** What one gamepad input does in the game. */
USTRUCT(BlueprintType)
struct ASSETPROLOBBYKIT_API FALKGamepadCallout
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gamepad Diagram")
	EALKGamepadInput Input = EALKGamepadInput::FaceBottom;

	/** Shown next to the button. Empty draws the button as unassigned. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gamepad Diagram")
	FText Action;

	FALKGamepadCallout() = default;
	FALKGamepadCallout(EALKGamepadInput InInput, const FText& InAction) : Input(InInput), Action(InAction) {}
};

/**
 * Draws a gamepad with a line and a label from every button to what it does, in the theme's colours. The drawing
 * keeps its proportions inside whatever size it is given (wrap it in a Size Box). Button names follow the controller
 * in use (A / Cross / B...).
 */
UCLASS(Blueprintable)
class ASSETPROLOBBYKIT_API UALKGamepadDiagram : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Sets what the buttons do. Inputs without a callout are drawn as unassigned. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Menu|Gamepad Diagram")
	void SetCallouts(const TArray<FALKGamepadCallout>& InCallouts);

	UFUNCTION(BlueprintPure, Category = "Assetpro Menu|Gamepad Diagram")
	const TArray<FALKGamepadCallout>& GetCallouts() const { return Callouts; }

	/** The short name printed on the controller for an input ("LT", "L3", "Menu"...). */
	FText GetInputLabel(EALKGamepadInput Input) const;

protected:
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

private:
	TArray<FALKGamepadCallout> Callouts;
};
