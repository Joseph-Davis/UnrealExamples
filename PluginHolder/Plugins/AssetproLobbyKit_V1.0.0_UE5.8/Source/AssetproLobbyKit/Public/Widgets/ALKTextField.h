// Copyright 2026 CodeLikeMe. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Types/SlateEnums.h"
#include "Widgets/ALKDecor.h"
#include "Widgets/ALKUserWidget.h"
#include "ALKTextField.generated.h"

class UBorder;
class UEditableText;
class UALKIconWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FALKOnTextFieldChanged, const FText&, Text);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FALKOnTextFieldCommitted, const FText&, Text, bool, bEnterPressed);

/**
 * A themed single-line text box: a server name, a password, a search, a lobby code, a chat message. It lights up in
 * the accent colour while it has the keyboard, shows a hint while empty and can hide what is typed.
 *
 * Designer widget names: Background (a Border or ALKFrame), Input (Editable Text), Icon (ALKIconWidget).
 */
UCLASS(Blueprintable)
class ASSETPROLOBBYKIT_API UALKTextField : public UALKUserWidget
{
	GENERATED_BODY()

public:
	UALKTextField(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintCallable, Category = "Assetpro Menu|Text Field")
	void SetText(const FText& InText);

	UFUNCTION(BlueprintPure, Category = "Assetpro Menu|Text Field")
	FText GetText() const;

	/** Shown in the muted colour while the field is empty. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Menu|Text Field")
	void SetHintText(const FText& InHintText);

	/** Shows dots instead of what is typed. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Menu|Text Field")
	void SetIsPassword(bool bInIsPassword);

	/** Most characters the field takes. 0 means no limit. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Menu|Text Field")
	void SetMaxLength(int32 InMaxLength);

	/** A small icon in front of the text (a magnifier in a search box). None hides it. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Menu|Text Field")
	void SetLeadingIcon(EALKIcon InIcon);

	/** Puts the keyboard in the field, ready to type. */
	UFUNCTION(BlueprintCallable, Category = "Assetpro Menu|Text Field")
	void FocusInput();

	/** True while the field has the keyboard. */
	UFUNCTION(BlueprintPure, Category = "Assetpro Menu|Text Field")
	bool IsTyping() const;

	UPROPERTY(BlueprintAssignable, Category = "Assetpro Menu|Text Field")
	FALKOnTextFieldChanged OnTextChanged;

	/** Enter was pressed (Enter Pressed true), or the field lost the keyboard or Escape was pressed (false). */
	UPROPERTY(BlueprintAssignable, Category = "Assetpro Menu|Text Field")
	FALKOnTextFieldCommitted OnTextCommitted;

	TMulticastDelegate<void(const FText&)> OnTextChangedNative;
	TMulticastDelegate<void(const FText&, ETextCommit::Type)> OnTextCommittedNative;

protected:
	virtual void NativeBuildDefaultLayout(UWidgetTree& Tree) override;
	virtual void NativeOnLayoutReady() override;
	virtual void NativeApplyTheme(const UALKTheme& Theme) override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnFocusReceived(const FGeometry& InGeometry, const FFocusEvent& InFocusEvent) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Assetpro Menu|Text Field")
	FText HintText;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Assetpro Menu|Text Field")
	bool bIsPassword = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Assetpro Menu|Text Field", meta = (ClampMin = "0"))
	int32 MaxLength = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Assetpro Menu|Text Field")
	EALKIcon LeadingIcon = EALKIcon::None;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Menu|Text Field", meta = (BindWidgetOptional))
	TObjectPtr<UBorder> Background;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Menu|Text Field", meta = (BindWidgetOptional))
	TObjectPtr<UEditableText> Input;

	UPROPERTY(BlueprintReadOnly, Category = "Assetpro Menu|Text Field", meta = (BindWidgetOptional))
	TObjectPtr<UALKIconWidget> Icon;

private:
	UFUNCTION()
	void HandleTextChanged(const FText& Text);

	UFUNCTION()
	void HandleTextCommitted(const FText& Text, ETextCommit::Type CommitMethod);

	void ApplyFocusLook(const UALKTheme& Theme, bool bFocused);

	bool bWasTyping = false;
};
