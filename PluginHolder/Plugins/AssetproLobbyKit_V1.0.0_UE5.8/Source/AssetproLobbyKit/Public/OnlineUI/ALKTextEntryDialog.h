// Copyright 2026 CodeLikeMe. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Screens/ALKScreen.h"
#include "Types/SlateEnums.h"
#include "ALKTextEntryDialog.generated.h"

class UALKButton;
class UALKTextField;
class UBorder;
class UImage;
class UTextBlock;

/** Everything shown by a text entry dialog. */
USTRUCT(BlueprintType)
struct ASSETPROLOBBYKIT_API FALKTextEntryDescriptor
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Text Entry")
	FText Title;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Text Entry", meta = (MultiLine = true))
	FText Message;

	/** Shown in the field while it is empty. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Text Entry")
	FText HintText;

	/** What the field starts with. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Text Entry")
	FString InitialText;

	/** Leave empty for "OK". */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Text Entry")
	FText ConfirmText;

	/** Leave empty for "Cancel". */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Text Entry")
	FText CancelText;

	/** Hide what is typed in the field (a password). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Text Entry")
	bool bIsPassword = false;

	/** Most characters the field takes. 0 means no limit. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Text Entry", meta = (ClampMin = "0"))
	int32 MaxLength = 0;

	/** The dialog can be confirmed with nothing typed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Text Entry")
	bool bAllowEmpty = false;

	/** Shows a second field under the first (a password next to a lobby code). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Text Entry")
	bool bSecondField = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Text Entry", meta = (EditCondition = "bSecondField"))
	FText SecondHintText;

	/** Hide what is typed in the second field. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Text Entry", meta = (EditCondition = "bSecondField"))
	bool bSecondIsPassword = true;
};

DECLARE_DYNAMIC_DELEGATE_ThreeParams(FALKTextEntryResultDelegate, bool, bConfirmed, const FString&, Text, const FString&, SecondText);

/**
 * A modal dialog that asks for a line of text: a lobby code, a password, a name for a rules preset, a note for a
 * report. Enter confirms, Back cancels. Open it with Show Text Entry on the Online Flow subsystem.
 * Designer widget names: DimImage, Panel, TitleText, MessageText, EntryField, SecondField, ConfirmButton, CancelButton.
 */
UCLASS(Blueprintable)
class ASSETPROLOBBYKIT_API UALKTextEntryDialog : public UALKScreen
{
	GENERATED_BODY()

public:
	UALKTextEntryDialog(const FObjectInitializer& ObjectInitializer);

	void SetupDialog(const FALKTextEntryDescriptor& InDescriptor, TFunction<void(bool, const FString&, const FString&)> InOnResult);

	/** Closes the dialog and reports the result. Confirming with an empty field does nothing unless the descriptor allows it. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Lobby|Dialog")
	void FinishDialog(bool bConfirmed);

protected:
	virtual void NativeBuildDefaultLayout(UWidgetTree& Tree) override;
	virtual void NativeOnLayoutReady() override;
	virtual void NativeApplyTheme(const UALKTheme& Theme) override;
	virtual void NativeOnActivated() override;
	virtual bool OnBackRequested_Implementation() override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Dialog", meta = (BindWidgetOptional))
	TObjectPtr<UImage> DimImage;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Dialog", meta = (BindWidgetOptional))
	TObjectPtr<UBorder> Panel;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Dialog", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TitleText;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Dialog", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> MessageText;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Dialog", meta = (BindWidgetOptional))
	TObjectPtr<UALKTextField> EntryField;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Dialog", meta = (BindWidgetOptional))
	TObjectPtr<UALKTextField> SecondField;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Dialog", meta = (BindWidgetOptional))
	TObjectPtr<UALKButton> ConfirmButton;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Dialog", meta = (BindWidgetOptional))
	TObjectPtr<UALKButton> CancelButton;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Lobby|Dialog")
	FALKTextEntryDescriptor Descriptor;

private:
	void HandleEntryCommitted(const FText& Text, ETextCommit::Type CommitMethod);
	void HandleSecondCommitted(const FText& Text, ETextCommit::Type CommitMethod);
	void RefreshConfirmEnabled();

	TFunction<void(bool, const FString&, const FString&)> OnResult;
};
