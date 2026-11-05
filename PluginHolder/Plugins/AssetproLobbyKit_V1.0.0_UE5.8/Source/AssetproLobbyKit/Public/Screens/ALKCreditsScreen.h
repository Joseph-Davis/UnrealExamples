// Copyright 2026 CodeLikeMe. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Screens/ALKScreen.h"
#include "ALKCreditsScreen.generated.h"

class UImage;
class UTextBlock;
class UScrollBox;
class UVerticalBox;
class UALKButton;

/**
 * Auto-scrolling credits built from the menu config's Credits sections. The mouse wheel or up / down take over
 * scrolling; the roll loops when it reaches the end.
 * Designer widget names: BackgroundImage, TitleText, CreditsScroll (Scroll Box), CreditsBox (Vertical Box), BackButton.
 */
UCLASS(Blueprintable)
class ASSETPROLOBBYKIT_API UALKCreditsScreen : public UALKScreen
{
	GENERATED_BODY()

public:
	UALKCreditsScreen(const FObjectInitializer& ObjectInitializer);

	/** Rebuilds the credits text from the menu config. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Menu|Credits")
	void RebuildCredits();

protected:
	virtual void NativeBuildDefaultLayout(UWidgetTree& Tree) override;
	virtual void NativeOnLayoutReady() override;
	virtual void NativeApplyTheme(const UALKTheme& Theme) override;
	virtual void NativeOnActivated() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual bool NativeHandleMenuKey(const FKey& Key) override;
	virtual FReply NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Menu|Credits", meta = (BindWidgetOptional))
	TObjectPtr<UImage> BackgroundImage;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Menu|Credits", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TitleText;

	/** The optional small label over the title (see System Label). */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Menu|Credits", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> SystemLabelText;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Menu|Credits", meta = (BindWidgetOptional))
	TObjectPtr<UScrollBox> CreditsScroll;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Menu|Credits", meta = (BindWidgetOptional))
	TObjectPtr<UVerticalBox> CreditsBox;

	/** Closes the credits with the mouse. Keyboard and gamepad players use Back (Escape / B). */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Menu|Credits", meta = (BindWidgetOptional))
	TObjectPtr<UALKButton> BackButton;

private:
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> HeadingTexts;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> LineTexts;

	float ScrollOffset = 0.f;
	float ManualScrollCooldown = 0.f;
	float EndHoldTime = 0.f;
};
