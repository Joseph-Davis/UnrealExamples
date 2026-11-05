// Copyright 2026 CodeLikeMe. All Rights Reserved.

#include "OnlineUI/ALKTextEntryDialog.h"
#include "ALKTheme.h"
#include "Widgets/ALKButton.h"
#include "Widgets/ALKFrame.h"
#include "Widgets/ALKInputLabels.h"
#include "Widgets/ALKTextField.h"
#include "Widgets/ALKTitleText.h"
#include "Widgets/ALKWidgetBuilder.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "HAL/PlatformInput.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ALKTextEntryDialog)

#define LOCTEXT_NAMESPACE "AssetproLobbyKit"

UALKTextEntryDialog::UALKTextEntryDialog(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bIsModal = true;
}

void UALKTextEntryDialog::NativeBuildDefaultLayout(UWidgetTree& Tree)
{
	UOverlay* Root = FALKWidgetBuilder::Make<UOverlay>(Tree, TEXT("Root"));
	Tree.RootWidget = Root;

	DimImage = FALKWidgetBuilder::Make<UImage>(Tree, TEXT("DimImage"));
	FALKWidgetBuilder::AddToOverlay(Root, DimImage);

	// A panel over the dimmed screen: title, message, one or two fields and the buttons.
	Panel = FALKWidgetBuilder::Make<UALKFrame>(Tree, TEXT("Panel"));
	Panel->SetPadding(FMargin(44.f, 44.f, 44.f, 36.f));
	FALKWidgetBuilder::AddToOverlay(Root, FALKWidgetBuilder::WrapInSizeBox(Tree, Panel, 640.f, 0.f), HAlign_Center, VAlign_Center);

	UVerticalBox* Column = FALKWidgetBuilder::Make<UVerticalBox>(Tree, TEXT("PanelColumn"));
	Panel->SetContent(Column);

	TitleText = FALKWidgetBuilder::Make<UALKTitleText>(Tree, TEXT("TitleText"));
	FALKWidgetBuilder::AddToVerticalBox(Column, TitleText, FMargin(), HAlign_Left);

	MessageText = FALKWidgetBuilder::MakeText(Tree, TEXT("MessageText"));
	MessageText->SetAutoWrapText(true);
	FALKWidgetBuilder::AddToVerticalBox(Column, MessageText, FMargin(0.f, 14.f, 0.f, 0.f), HAlign_Fill);

	EntryField = Tree.ConstructWidget<UALKTextField>(UALKTextField::StaticClass(), TEXT("EntryField"));
	FALKWidgetBuilder::AddToVerticalBox(Column, EntryField, FMargin(0.f, 24.f, 0.f, 0.f), HAlign_Fill);

	SecondField = Tree.ConstructWidget<UALKTextField>(UALKTextField::StaticClass(), TEXT("SecondField"));
	SecondField->SetVisibility(ESlateVisibility::Collapsed);
	FALKWidgetBuilder::AddToVerticalBox(Column, SecondField, FMargin(0.f, 12.f, 0.f, 0.f), HAlign_Fill);

	UHorizontalBox* Buttons = FALKWidgetBuilder::Make<UHorizontalBox>(Tree, TEXT("ButtonRow"));
	FALKWidgetBuilder::AddToVerticalBox(Column, Buttons, FMargin(0.f, 32.f, 0.f, 0.f), HAlign_Right);

	ConfirmButton = CreateMenuButton(FText::GetEmpty(), EALKButtonStyle::Solid);
	FALKWidgetBuilder::AddToHorizontalBox(Buttons, ConfirmButton);

	CancelButton = CreateMenuButton(FText::GetEmpty(), EALKButtonStyle::Secondary);
	CancelButton->SetInputHint(FALKInputHint(LOCTEXT("KeyEsc", "Esc"), FPlatformInput::GetGamepadBackKey(), FText::GetEmpty()));
	FALKWidgetBuilder::AddToHorizontalBox(Buttons, CancelButton, FMargin(16.f, 0.f, 0.f, 0.f));
}

void UALKTextEntryDialog::NativeOnLayoutReady()
{
	Super::NativeOnLayoutReady();
	if (ConfirmButton)
	{
		ConfirmButton->OnClicked().AddUObject(this, &ThisClass::FinishDialog, true);
	}
	if (CancelButton)
	{
		CancelButton->OnClicked().AddUObject(this, &ThisClass::FinishDialog, false);
	}
	if (EntryField)
	{
		EntryField->OnTextCommittedNative.AddUObject(this, &ThisClass::HandleEntryCommitted);
		EntryField->OnTextChangedNative.AddWeakLambda(this, [this](const FText&) { RefreshConfirmEnabled(); });
	}
	if (SecondField)
	{
		SecondField->OnTextCommittedNative.AddUObject(this, &ThisClass::HandleSecondCommitted);
	}
}

void UALKTextEntryDialog::SetupDialog(const FALKTextEntryDescriptor& InDescriptor, TFunction<void(bool, const FString&, const FString&)> InOnResult)
{
	Descriptor = InDescriptor;
	OnResult = MoveTemp(InOnResult);

	if (TitleText)
	{
		TitleText->SetText(Descriptor.Title);
	}
	if (MessageText)
	{
		MessageText->SetText(Descriptor.Message);
		MessageText->SetVisibility(Descriptor.Message.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
	if (EntryField)
	{
		EntryField->SetHintText(Descriptor.HintText);
		EntryField->SetIsPassword(Descriptor.bIsPassword);
		EntryField->SetMaxLength(Descriptor.MaxLength);
		EntryField->SetText(FText::FromString(Descriptor.InitialText));
	}
	if (SecondField)
	{
		SecondField->SetHintText(Descriptor.SecondHintText);
		SecondField->SetIsPassword(Descriptor.bSecondIsPassword);
		SecondField->SetText(FText::GetEmpty());
		SecondField->SetVisibility(Descriptor.bSecondField ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (ConfirmButton)
	{
		ConfirmButton->SetButtonText(Descriptor.ConfirmText.IsEmpty() ? LOCTEXT("OK", "OK") : Descriptor.ConfirmText);
	}
	if (CancelButton)
	{
		CancelButton->SetButtonText(Descriptor.CancelText.IsEmpty() ? LOCTEXT("Cancel", "Cancel") : Descriptor.CancelText);
	}
	RefreshConfirmEnabled();
}

void UALKTextEntryDialog::NativeOnActivated()
{
	Super::NativeOnActivated();
	// Ready to type at once.
	if (EntryField)
	{
		EntryField->FocusInput();
	}
}

UWidget* UALKTextEntryDialog::NativeGetDesiredFocusTarget() const
{
	return EntryField ? static_cast<UWidget*>(EntryField.Get()) : Super::NativeGetDesiredFocusTarget();
}

void UALKTextEntryDialog::NativeApplyTheme(const UALKTheme& Theme)
{
	Super::NativeApplyTheme(Theme);

	if (DimImage)
	{
		DimImage->SetBrush(UALKTheme::MakeSolidBrush(Theme.ScreenDim));
	}
	if (UALKFrame* Frame = Cast<UALKFrame>(Panel))
	{
		Frame->SetFrameStyle(Theme.MakeDialogFrame());
	}
	if (USizeBox* PanelSize = Panel ? Cast<USizeBox>(Panel->GetParent()) : nullptr)
	{
		PanelSize->SetWidthOverride(Theme.DialogWidth);
	}
	if (TitleText)
	{
		FALKWidgetBuilder::StyleText(TitleText, Theme, EALKTextRole::Heading, Theme.TextPrimary);
		FSlateFontInfo Font = Theme.HeadingFont;
		Font.Size = FMath::Max(12.f, FMath::RoundToFloat(Font.Size * 0.78f));
		FALKWidgetBuilder::SetTextFont(TitleText, Font);
	}
	FALKWidgetBuilder::StyleText(MessageText, Theme, EALKTextRole::Body);
	if (MessageText)
	{
		MessageText->SetLineHeightPercentage(1.25f);
	}
}

void UALKTextEntryDialog::HandleEntryCommitted(const FText& Text, ETextCommit::Type CommitMethod)
{
	if (CommitMethod != ETextCommit::OnEnter)
	{
		return;
	}
	// Enter in the first field moves on to the second, or confirms.
	if (Descriptor.bSecondField && SecondField)
	{
		SecondField->FocusInput();
	}
	else
	{
		FinishDialog(true);
	}
}

void UALKTextEntryDialog::HandleSecondCommitted(const FText& Text, ETextCommit::Type CommitMethod)
{
	if (CommitMethod == ETextCommit::OnEnter)
	{
		FinishDialog(true);
	}
}

void UALKTextEntryDialog::RefreshConfirmEnabled()
{
	if (ConfirmButton)
	{
		ConfirmButton->SetIsEnabled(Descriptor.bAllowEmpty || (EntryField && !EntryField->GetText().ToString().TrimStartAndEnd().IsEmpty()));
	}
}

void UALKTextEntryDialog::FinishDialog(bool bConfirmed)
{
	const FString Text = EntryField ? EntryField->GetText().ToString().TrimStartAndEnd() : FString();
	const FString SecondText = SecondField && Descriptor.bSecondField ? SecondField->GetText().ToString() : FString();
	if (bConfirmed && Text.IsEmpty() && !Descriptor.bAllowEmpty)
	{
		PlayThemeSound(EALKUISound::Locked);
		return;
	}

	// Take the callback first: it may open another dialog that reuses this pooled widget.
	TFunction<void(bool, const FString&, const FString&)> Callback = MoveTemp(OnResult);
	OnResult = nullptr;
	if (!bConfirmed)
	{
		PlayThemeSound(EALKUISound::Back);
	}
	CloseScreen();
	if (Callback)
	{
		Callback(bConfirmed, Text, SecondText);
	}
}

bool UALKTextEntryDialog::OnBackRequested_Implementation()
{
	FinishDialog(false);
	return true;
}

#undef LOCTEXT_NAMESPACE
