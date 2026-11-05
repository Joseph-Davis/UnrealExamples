// Copyright 2026 CodeLikeMe. All Rights Reserved.

#include "Widgets/ALKTextField.h"
#include "ALKTheme.h"
#include "Widgets/ALKFrame.h"
#include "Widgets/ALKWidgetBuilder.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/EditableText.h"
#include "Components/HorizontalBox.h"
#include "Styling/SlateTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ALKTextField)

UALKTextField::UALKTextField(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsFocusable(true);
}

void UALKTextField::NativeBuildDefaultLayout(UWidgetTree& Tree)
{
	UALKFrame* Frame = FALKWidgetBuilder::Make<UALKFrame>(Tree, TEXT("Background"));
	Frame->SetPadding(FMargin(14.f, 9.f, 14.f, 10.f));
	Frame->SetVerticalAlignment(VAlign_Center);
	Background = Frame;
	Tree.RootWidget = Frame;

	UHorizontalBox* Row = FALKWidgetBuilder::Make<UHorizontalBox>(Tree, TEXT("FieldRow"));
	Frame->SetContent(Row);

	Icon = FALKWidgetBuilder::MakeIcon(Tree, TEXT("Icon"), LeadingIcon, 18.f);
	FALKWidgetBuilder::AddToHorizontalBox(Row, Icon, FMargin(0.f, 0.f, 10.f, 0.f), HAlign_Left, VAlign_Center);

	Input = FALKWidgetBuilder::Make<UEditableText>(Tree, TEXT("Input"));
	Input->SetClearKeyboardFocusOnCommit(false);
	Input->SetRevertTextOnEscape(false);
	FALKWidgetBuilder::AddToHorizontalBox(Row, Input, FMargin(), HAlign_Fill, VAlign_Center, 1.f);
}

void UALKTextField::NativeOnLayoutReady()
{
	Super::NativeOnLayoutReady();
	if (Input)
	{
		Input->SetHintText(HintText);
		Input->SetIsPassword(bIsPassword);
		Input->OnTextChanged.AddDynamic(this, &ThisClass::HandleTextChanged);
		Input->OnTextCommitted.AddDynamic(this, &ThisClass::HandleTextCommitted);
	}
	if (Icon)
	{
		Icon->SetVisibility(LeadingIcon == EALKIcon::None ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
}

void UALKTextField::NativeApplyTheme(const UALKTheme& Theme)
{
	Super::NativeApplyTheme(Theme);
	if (Input)
	{
		FEditableTextStyle Style;
		Style.SetFont(Theme.BodyFont);
		Style.SetColorAndOpacity(FSlateColor(Theme.TextPrimary));
		Style.SetBackgroundImageSelected(UALKTheme::MakeSolidBrush(Theme.Accent.CopyWithNewOpacity(0.35f)));
		Style.SetCaretImage(UALKTheme::MakeSolidBrush(Theme.TextPrimary));
		Input->SetWidgetStyle(Style);
		Input->SetFont(Theme.BodyFont);
	}
	ApplyFocusLook(Theme, IsTyping());
}

void UALKTextField::ApplyFocusLook(const UALKTheme& Theme, bool bFocused)
{
	if (UALKFrame* Frame = Cast<UALKFrame>(Background))
	{
		FALKFrameStyle Style = Theme.MakeChipFrame(Theme.PanelRaised, bFocused ? Theme.Accent : Theme.PanelOutline);
		Style.Radius = Theme.CornerRadius;
		Frame->SetFrameStyle(Style);
	}
	else if (Background)
	{
		Background->SetBrush(Theme.MakeRoundedBrush(Theme.PanelRaised, bFocused ? Theme.Accent : Theme.PanelOutline, Theme.OutlineWidth));
	}
	if (Icon)
	{
		Icon->SetIcon(LeadingIcon, bFocused ? Theme.TextPrimary : Theme.TextMuted);
	}
}

void UALKTextField::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	const bool bTyping = IsTyping();
	if (bTyping != bWasTyping)
	{
		bWasTyping = bTyping;
		if (const UALKTheme* Theme = GetTheme(); Theme && bApplyThemeStyles)
		{
			ApplyFocusLook(*Theme, bTyping);
		}
	}
}

FReply UALKTextField::NativeOnFocusReceived(const FGeometry& InGeometry, const FFocusEvent& InFocusEvent)
{
	// Focus on the field itself goes straight to its text, so typing works right away.
	FocusInput();
	return FReply::Handled();
}

void UALKTextField::SetText(const FText& InText)
{
	if (Input)
	{
		Input->SetText(InText);
	}
}

FText UALKTextField::GetText() const
{
	return Input ? Input->GetText() : FText::GetEmpty();
}

void UALKTextField::SetHintText(const FText& InHintText)
{
	HintText = InHintText;
	if (Input)
	{
		Input->SetHintText(HintText);
	}
}

void UALKTextField::SetIsPassword(bool bInIsPassword)
{
	bIsPassword = bInIsPassword;
	if (Input)
	{
		Input->SetIsPassword(bIsPassword);
	}
}

void UALKTextField::SetMaxLength(int32 InMaxLength)
{
	MaxLength = FMath::Max(0, InMaxLength);
}

void UALKTextField::SetLeadingIcon(EALKIcon InIcon)
{
	LeadingIcon = InIcon;
	if (Icon)
	{
		Icon->SetVisibility(LeadingIcon == EALKIcon::None ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
		Icon->SetIcon(LeadingIcon, Icon->Color);
	}
}

void UALKTextField::FocusInput()
{
	if (Input)
	{
		Input->SetKeyboardFocus();
	}
}

bool UALKTextField::IsTyping() const
{
	return Input && Input->HasKeyboardFocus();
}

void UALKTextField::HandleTextChanged(const FText& Text)
{
	if (MaxLength > 0 && Text.ToString().Len() > MaxLength && Input)
	{
		// Setting the text raises this event again with the shortened text.
		Input->SetText(FText::FromString(Text.ToString().Left(MaxLength)));
		return;
	}
	OnTextChangedNative.Broadcast(Text);
	OnTextChanged.Broadcast(Text);
}

void UALKTextField::HandleTextCommitted(const FText& Text, ETextCommit::Type CommitMethod)
{
	OnTextCommittedNative.Broadcast(Text, CommitMethod);
	OnTextCommitted.Broadcast(Text, CommitMethod == ETextCommit::OnEnter);
}
