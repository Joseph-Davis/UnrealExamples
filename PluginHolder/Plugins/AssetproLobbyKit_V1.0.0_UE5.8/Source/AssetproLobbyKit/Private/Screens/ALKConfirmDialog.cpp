// Copyright 2026 CodeLikeMe. All Rights Reserved.

#include "Screens/ALKConfirmDialog.h"
#include "Widgets/ALKFrame.h"
#include "Widgets/ALKTitleText.h"
#include "ALKTheme.h"
#include "Widgets/ALKButton.h"
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

#include UE_INLINE_GENERATED_CPP_BY_NAME(ALKConfirmDialog)

#define LOCTEXT_NAMESPACE "AssetproLobbyKit"

UALKConfirmDialog::UALKConfirmDialog(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bIsModal = true;
}

void UALKConfirmDialog::NativeBuildDefaultLayout(UWidgetTree& Tree)
{
	UOverlay* Root = FALKWidgetBuilder::Make<UOverlay>(Tree, TEXT("Root"));
	Tree.RootWidget = Root;

	DimImage = FALKWidgetBuilder::Make<UImage>(Tree, TEXT("DimImage"));
	FALKWidgetBuilder::AddToOverlay(Root, DimImage);

	// A panel over the dimmed screen: label, title, message and a row of buttons.
	Panel = FALKWidgetBuilder::Make<UALKFrame>(Tree, TEXT("Panel"));
	Panel->SetPadding(FMargin(48.f, 54.f, 48.f, 40.f));
	FALKWidgetBuilder::AddToOverlay(Root, FALKWidgetBuilder::WrapInSizeBox(Tree, Panel, 640.f, 0.f), HAlign_Center, VAlign_Center);

	UVerticalBox* Column = FALKWidgetBuilder::Make<UVerticalBox>(Tree, TEXT("PanelColumn"));
	Panel->SetContent(Column);

	SystemLabelText = FALKWidgetBuilder::MakeText(Tree, TEXT("SystemLabelText"), MakeSystemLabel(9, LOCTEXT("DialogLabel", "Confirm")));
	FALKWidgetBuilder::AddToVerticalBox(Column, SystemLabelText, FMargin(0.f, 0.f, 0.f, 8.f), HAlign_Left);

	UALKTitleText* Title = FALKWidgetBuilder::Make<UALKTitleText>(Tree, TEXT("TitleText"));
	TitleText = Title;
	FALKWidgetBuilder::AddToVerticalBox(Column, TitleText, FMargin(), HAlign_Left);

	MessageText = FALKWidgetBuilder::MakeText(Tree, TEXT("MessageText"));
	MessageText->SetAutoWrapText(true);
	FALKWidgetBuilder::AddToVerticalBox(Column, MessageText, FMargin(0.f, 20.f, 0.f, 0.f), HAlign_Fill);

	UHorizontalBox* Buttons = FALKWidgetBuilder::Make<UHorizontalBox>(Tree, TEXT("ButtonRow"));
	FALKWidgetBuilder::AddToVerticalBox(Column, Buttons, FMargin(0.f, 38.f, 0.f, 0.f), HAlign_Right);

	// Gamepad: the accept button's glyph follows focus, Back always cancels.
	FALKInputHint AcceptHint(FText::GetEmpty(), FPlatformInput::GetGamepadAcceptKey(), FText::GetEmpty());
	AcceptHint.bOnlyWhenFocused = true;

	ConfirmButton = CreateMenuButton(FText::GetEmpty(), EALKButtonStyle::Solid);
	ConfirmButton->SetInputHint(AcceptHint);
	FALKWidgetBuilder::AddToHorizontalBox(Buttons, ConfirmButton);

	AlternateButton = CreateMenuButton(FText::GetEmpty(), EALKButtonStyle::Secondary);
	AlternateButton->SetInputHint(AcceptHint);
	FALKWidgetBuilder::AddToHorizontalBox(Buttons, AlternateButton, FMargin(16.f, 0.f, 0.f, 0.f));

	CancelButton = CreateMenuButton(FText::GetEmpty(), EALKButtonStyle::Secondary);
	CancelButton->SetInputHint(FALKInputHint(LOCTEXT("KeyEsc", "Esc"), FPlatformInput::GetGamepadBackKey(), FText::GetEmpty()));
	FALKWidgetBuilder::AddToHorizontalBox(Buttons, CancelButton, FMargin(16.f, 0.f, 0.f, 0.f));
}

void UALKConfirmDialog::NativeOnLayoutReady()
{
	Super::NativeOnLayoutReady();

	if (ConfirmButton)
	{
		ConfirmButton->OnClicked().AddUObject(this, &ThisClass::FinishDialog, EALKDialogResult::Confirmed);
	}
	if (CancelButton)
	{
		CancelButton->OnClicked().AddUObject(this, &ThisClass::FinishDialog, EALKDialogResult::Cancelled);
	}
	if (AlternateButton)
	{
		AlternateButton->OnClicked().AddUObject(this, &ThisClass::FinishDialog, EALKDialogResult::Alternate);
	}
}

void UALKConfirmDialog::SetupDialog(const FALKDialogDescriptor& InDescriptor, TFunction<void(EALKDialogResult)> InOnResult)
{
	Descriptor = InDescriptor;
	OnResult = MoveTemp(InOnResult);

	if (Descriptor.ConfirmText.IsEmpty())
	{
		Descriptor.ConfirmText = LOCTEXT("OK", "OK");
	}

	if (TitleText)
	{
		TitleText->SetText(Descriptor.Title);
	}
	if (MessageText)
	{
		MessageText->SetText(Descriptor.Message);
	}
	if (ConfirmButton)
	{
		ConfirmButton->SetButtonText(Descriptor.ConfirmText);
		// The main answer is the filled button; a destructive one is drawn in the danger colour.
		ConfirmButton->SetButtonStyleType(Descriptor.bDestructive ? EALKButtonStyle::Danger : EALKButtonStyle::Solid);
	}
	if (CancelButton)
	{
		CancelButton->SetButtonText(Descriptor.CancelText);
		CancelButton->SetVisibility(Descriptor.CancelText.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	}
	if (AlternateButton)
	{
		AlternateButton->SetButtonText(Descriptor.AlternateText);
		AlternateButton->SetVisibility(Descriptor.AlternateText.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	}

	// Destructive dialogs start on Cancel so a stray Enter never quits the game.
	const bool bFocusCancel = Descriptor.bDestructive && CancelButton && !Descriptor.CancelText.IsEmpty();
	DefaultFocusWidget = bFocusCancel ? static_cast<UWidget*>(CancelButton.Get()) : static_cast<UWidget*>(ConfirmButton.Get());

	TimeRemaining = Descriptor.TimeoutSeconds;
	LastShownSeconds = -1;
	UpdateCountdownText();

	OnDialogSetup(Descriptor);
}

void UALKConfirmDialog::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (TimeRemaining > 0.f && IsActivated())
	{
		// Slate delta time keeps counting while the game is paused.
		TimeRemaining -= FMath::Min(InDeltaTime, 0.1f);
		if (TimeRemaining <= 0.f)
		{
			TimeRemaining = 0.f;
			FinishDialog(EALKDialogResult::Cancelled);
			return;
		}
		UpdateCountdownText();
	}
}

void UALKConfirmDialog::UpdateCountdownText()
{
	if (!CancelButton || Descriptor.CancelText.IsEmpty())
	{
		return;
	}

	if (TimeRemaining <= 0.f)
	{
		CancelButton->SetButtonText(Descriptor.CancelText);
		return;
	}

	const int32 Seconds = FMath::CeilToInt(TimeRemaining);
	if (Seconds != LastShownSeconds)
	{
		LastShownSeconds = Seconds;
		CancelButton->SetButtonText(FText::Format(LOCTEXT("CancelCountdown", "{0} ({1})"), Descriptor.CancelText, FText::AsNumber(Seconds)));
	}
}

void UALKConfirmDialog::NativeApplyTheme(const UALKTheme& Theme)
{
	Super::NativeApplyTheme(Theme);

	if (DimImage)
	{
		DimImage->SetBrush(UALKTheme::MakeSolidBrush(Theme.ScreenDim));
	}
	if (Panel)
	{
		if (UALKFrame* Frame = Cast<UALKFrame>(Panel))
		{
			Frame->SetFrameStyle(Theme.MakeDialogFrame());
		}
		else
		{
			Panel->SetBrush(UALKTheme::MakeSolidBrush(FLinearColor::Transparent));
		}
		if (USizeBox* PanelSize = Cast<USizeBox>(Panel->GetParent()))
		{
			PanelSize->SetWidthOverride(Theme.DialogWidth);
		}
	}
	if (TitleText)
	{
		FALKWidgetBuilder::StyleText(TitleText, Theme, EALKTextRole::Heading, Theme.TextPrimary);
		FSlateFontInfo Font = Theme.HeadingFont;
		Font.Size = FMath::Max(12.f, FMath::RoundToFloat(Font.Size * 0.78f));
		FALKWidgetBuilder::SetTextFont(TitleText, Font);
	}
	FALKWidgetBuilder::StyleText(SystemLabelText, Theme, EALKTextRole::Data, Theme.Accent);
	if (SystemLabelText)
	{
		SystemLabelText->SetVisibility(SystemLabelText->GetText().IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
	FALKWidgetBuilder::StyleText(MessageText, Theme, EALKTextRole::Body);
	if (MessageText)
	{
		MessageText->SetLineHeightPercentage(1.25f);
	}
}

void UALKConfirmDialog::FinishDialog(EALKDialogResult Result)
{
	// Take the callback first: it may open another dialog that reuses this pooled widget.
	TFunction<void(EALKDialogResult)> Callback = MoveTemp(OnResult);
	OnResult = nullptr;
	TimeRemaining = 0.f;

	if (Result == EALKDialogResult::Cancelled)
	{
		PlayThemeSound(EALKUISound::Back);
	}
	CloseScreen();

	if (Callback)
	{
		Callback(Result);
	}
}

bool UALKConfirmDialog::OnBackRequested_Implementation()
{
	FinishDialog(EALKDialogResult::Cancelled);
	return true;
}

#undef LOCTEXT_NAMESPACE
