// Copyright 2026 CodeLikeMe. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Screens/ALKScreen.h"
#include "ALKPressStartScreen.generated.h"

class UImage;
class UTextBlock;
class UBorder;

/**
 * Title screen: game title with a pulsing "press any key" prompt. Any key, button or click opens the main menu.
 * Designer widget names: BackgroundImage, TitleText, SubtitleText, TitleAccent, PromptText.
 */
UCLASS(Blueprintable)
class ASSETPROLOBBYKIT_API UALKPressStartScreen : public UALKScreen
{
	GENERATED_BODY()

public:
	UALKPressStartScreen(const FObjectInitializer& ObjectInitializer);

	/** Closes this screen and shows the main menu. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Menu|Press Start")
	void Continue();

protected:
	virtual void NativeBuildDefaultLayout(UWidgetTree& Tree) override;
	virtual void NativeOnLayoutReady() override;
	virtual void NativeApplyTheme(const UALKTheme& Theme) override;
	virtual void NativeOnActivated() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual bool NativeHandleMenuKey(const FKey& Key) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Menu|Press Start", meta = (BindWidgetOptional))
	TObjectPtr<UImage> BackgroundImage;

	/** Shown instead of Title Text when the menu config has a Game Logo. */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Menu|Press Start", meta = (BindWidgetOptional))
	TObjectPtr<UImage> LogoImage;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Menu|Press Start", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TitleText;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Menu|Press Start", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> SubtitleText;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Menu|Press Start", meta = (BindWidgetOptional))
	TObjectPtr<UBorder> TitleAccent;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Menu|Press Start", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> PromptText;

private:
	float PulseTime = 0.f;
	bool bContinued = false;
};
