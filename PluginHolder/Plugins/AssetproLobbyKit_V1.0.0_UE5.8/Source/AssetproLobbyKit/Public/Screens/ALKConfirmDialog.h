// Copyright 2026 CodeLikeMe. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Screens/ALKScreen.h"
#include "ALKConfirmDialog.generated.h"

class UImage;
class UBorder;
class UTextBlock;
class UALKButton;

/**
 * Modal "are you sure?" dialog with up to three buttons. Back counts as Cancel.
 * Designer widget names: DimImage, Panel, SystemLabelText, TitleText, MessageText, ConfirmButton, CancelButton, AlternateButton.
 */
UCLASS(Blueprintable)
class ASSETPROLOBBYKIT_API UALKConfirmDialog : public UALKScreen
{
	GENERATED_BODY()

public:
	UALKConfirmDialog(const FObjectInitializer& ObjectInitializer);

	void SetupDialog(const FALKDialogDescriptor& InDescriptor, TFunction<void(EALKDialogResult)> InOnResult);

	/** Closes the dialog and reports Result. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Menu|Dialog")
	void FinishDialog(EALKDialogResult Result);

	/** Blueprint hook after the dialog is set up (for custom visuals). */
	UFUNCTION(BlueprintImplementableEvent, Category = "Assetpro Menu|Dialog")
	void OnDialogSetup(const FALKDialogDescriptor& InDescriptor);

protected:
	virtual void NativeBuildDefaultLayout(UWidgetTree& Tree) override;
	virtual void NativeOnLayoutReady() override;
	virtual void NativeApplyTheme(const UALKTheme& Theme) override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual bool OnBackRequested_Implementation() override;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Menu|Dialog", meta = (BindWidgetOptional))
	TObjectPtr<UImage> DimImage;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Menu|Dialog", meta = (BindWidgetOptional))
	TObjectPtr<UBorder> Panel;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Menu|Dialog", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TitleText;

	/** The optional small label over the title (see System Label). */
	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Menu|Dialog", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> SystemLabelText;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Menu|Dialog", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> MessageText;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Menu|Dialog", meta = (BindWidgetOptional))
	TObjectPtr<UALKButton> ConfirmButton;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Menu|Dialog", meta = (BindWidgetOptional))
	TObjectPtr<UALKButton> CancelButton;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Menu|Dialog", meta = (BindWidgetOptional))
	TObjectPtr<UALKButton> AlternateButton;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Menu|Dialog")
	FALKDialogDescriptor Descriptor;

private:
	void UpdateCountdownText();

	TFunction<void(EALKDialogResult)> OnResult;
	float TimeRemaining = 0.f;
	int32 LastShownSeconds = -1;
};
