// Copyright 2026 CodeLikeMe. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Screens/ALKScreen.h"
#include "ALKKeyCaptureDialog.generated.h"

class UImage;
class UBorder;
class UTextBlock;

/**
 * "Press a key" dialog used when rebinding. Captures keyboard keys, mouse buttons and the mouse wheel (or gamepad
 * buttons when rebinding a gamepad row). Escape cancels.
 * Designer widget names: DimImage, Panel, TitleText, MessageText, PromptText.
 */
UCLASS(Blueprintable)
class ASSETPROLOBBYKIT_API UALKKeyCaptureDialog : public UALKScreen
{
	GENERATED_BODY()

public:
	UALKKeyCaptureDialog(const FObjectInitializer& ObjectInitializer);

	void SetupCapture(const FText& InActionName, bool bInGamepad, TFunction<void(FKey)> InOnKeyCaptured);

protected:
	virtual void NativeBuildDefaultLayout(UWidgetTree& Tree) override;
	virtual void NativeApplyTheme(const UALKTheme& Theme) override;
	virtual void NativeOnActivated() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual bool NativeHandleMenuKey(const FKey& Key) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual bool OnBackRequested_Implementation() override;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Menu|Dialog", meta = (BindWidgetOptional))
	TObjectPtr<UImage> DimImage;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Menu|Dialog", meta = (BindWidgetOptional))
	TObjectPtr<UBorder> Panel;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Menu|Dialog", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TitleText;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Menu|Dialog", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> MessageText;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Menu|Dialog", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> PromptText;

private:
	bool TryCaptureKey(const FKey& Key);
	void Finish(const FKey& Key);

	TFunction<void(FKey)> OnKeyCaptured;
	bool bGamepad = false;
	float ArmTime = 0.f;
	float PulseTime = 0.f;
};
