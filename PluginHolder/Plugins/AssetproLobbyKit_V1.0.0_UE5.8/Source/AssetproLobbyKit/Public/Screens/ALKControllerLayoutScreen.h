// Copyright 2026 CodeLikeMe. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Screens/ALKScreen.h"
#include "Widgets/ALKGamepadDiagram.h"
#include "ALKControllerLayoutScreen.generated.h"

class UImage;
class UTextBlock;

/**
 * Shows a gamepad and what every button does: the gamepad bindings of the player's rebindable actions, plus the menu
 * config's Controller Layout Labels for inputs that are not rebindable (sticks, the pause button). Y opens the
 * Controls page to change the bindings.
 *
 * Designer widget names: BackgroundImage, TitleText, Diagram (ALKGamepadDiagram).
 */
UCLASS(Blueprintable)
class ASSETPROLOBBYKIT_API UALKControllerLayoutScreen : public UALKScreen
{
	GENERATED_BODY()

public:
	UALKControllerLayoutScreen(const FObjectInitializer& ObjectInitializer);

	/** Reads the bindings again. Runs whenever the screen is shown. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Menu|Controller Layout")
	void RefreshLayout();

	/** What the diagram shows. Override in a Blueprint child to add or rename callouts. */
	UFUNCTION(BlueprintNativeEvent, Category = "Assetpro Menu|Controller Layout")
	TArray<FALKGamepadCallout> GetCallouts() const;

	/** Which diagram input a gamepad key belongs to (d-pad directions share one). False for keys the diagram does not show. */
	static bool GetInputForKey(const FKey& Key, EALKGamepadInput& OutInput);

protected:
	virtual void NativeBuildDefaultLayout(UWidgetTree& Tree) override;
	virtual void NativeApplyTheme(const UALKTheme& Theme) override;
	virtual void NativeOnActivated() override;
	virtual bool NativeHandleMenuKey(const FKey& Key) override;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Menu|Controller Layout", meta = (BindWidgetOptional))
	TObjectPtr<UImage> BackgroundImage;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Menu|Controller Layout", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TitleText;

	/** The optional small label over the title (see System Label). */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Menu|Controller Layout", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> SystemLabelText;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Menu|Controller Layout", meta = (BindWidgetOptional))
	TObjectPtr<UALKGamepadDiagram> Diagram;

private:
	void OpenBindings();
};
