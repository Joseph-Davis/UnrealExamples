// Copyright 2026 CodeLikeMe. All Rights Reserved.

#include "Screens/ALKCreditsScreen.h"
#include "Widgets/ALKTitleText.h"
#include "ALKMenuConfig.h"
#include "ALKTheme.h"
#include "Widgets/ALKButton.h"
#include "Widgets/ALKWidgetBuilder.h"
#include "Blueprint/WidgetTree.h"
#include "Components/HorizontalBox.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "HAL/PlatformInput.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ALKCreditsScreen)

#define LOCTEXT_NAMESPACE "AssetproLobbyKit"

UALKCreditsScreen::UALKCreditsScreen(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bDimScene = true;
}

void UALKCreditsScreen::NativeBuildDefaultLayout(UWidgetTree& Tree)
{
	UOverlay* Root = FALKWidgetBuilder::Make<UOverlay>(Tree, TEXT("Root"));
	Tree.RootWidget = Root;

	BackgroundImage = FALKWidgetBuilder::AddBackgroundLayers(Tree, Root);

	CreditsScroll = FALKWidgetBuilder::Make<UScrollBox>(Tree, TEXT("CreditsScroll"));
	CreditsScroll->SetScrollBarVisibility(ESlateVisibility::Collapsed);
	CreditsScroll->SetConsumeMouseWheel(EConsumeMouseWheel::Never);
	FALKWidgetBuilder::AddToOverlay(Root, CreditsScroll);

	CreditsBox = FALKWidgetBuilder::Make<UVerticalBox>(Tree, TEXT("CreditsBox"));
	CreditsScroll->AddChild(CreditsBox);
	if (UScrollBoxSlot* BoxSlot = Cast<UScrollBoxSlot>(CreditsBox->Slot))
	{
		BoxSlot->SetHorizontalAlignment(HAlign_Center);
		BoxSlot->SetPadding(FMargin(0.f, 560.f, 0.f, 560.f));
	}

	// Label and title in the top left corner.
	UVerticalBox* Header = FALKWidgetBuilder::Make<UVerticalBox>(Tree, TEXT("Header"));
	FALKWidgetBuilder::AddToOverlay(Root, Header, HAlign_Left, VAlign_Top);
	SystemLabelText = FALKWidgetBuilder::MakeText(Tree, TEXT("SystemLabelText"), MakeSystemLabel(11, LOCTEXT("CreditsTitle", "Credits")));
	FALKWidgetBuilder::AddToVerticalBox(Header, SystemLabelText, FMargin(0.f, 0.f, 0.f, 6.f), HAlign_Left);
	UALKTitleText* Title = FALKWidgetBuilder::Make<UALKTitleText>(Tree, TEXT("TitleText"));
	Title->SetText(LOCTEXT("CreditsTitle", "Credits"));
	TitleText = Title;
	FALKWidgetBuilder::AddToVerticalBox(Header, TitleText, FMargin(), HAlign_Left);

	// Footer (kept as the root's last child, see NativeApplyTheme): [Esc] Back  [Up / Down] Scroll
	UHorizontalBox* Footer = FALKWidgetBuilder::Make<UHorizontalBox>(Tree, TEXT("Footer"));
	FALKWidgetBuilder::AddToOverlay(Root, Footer, HAlign_Left, VAlign_Bottom);

	// The Back hint is clickable, so the screen keeps keyboard focus and Up / Down always scroll.
	UHorizontalBox* Hints = BuildInputHints(Tree, {
		FALKInputHint(LOCTEXT("KeyEsc", "Esc"), FPlatformInput::GetGamepadBackKey(), LOCTEXT("HintBack", "Back"))
			.WithAction(FSimpleDelegate::CreateWeakLambda(this, [this]() { OnBackRequested(); })),
		FALKInputHint(LOCTEXT("KeyUpDown", "Up / Down"), LOCTEXT("PadUpDown", "D-Pad"), LOCTEXT("HintScroll", "Scroll"))
	});
	FALKWidgetBuilder::AddToHorizontalBox(Footer, Hints, FMargin(), HAlign_Left, VAlign_Center);
}

void UALKCreditsScreen::NativeOnLayoutReady()
{
	Super::NativeOnLayoutReady();
	RebuildCredits();
	if (BackButton)
	{
		BackButton->OnClicked().AddWeakLambda(this, [this]() { OnBackRequested(); });
	}
}

void UALKCreditsScreen::RebuildCredits()
{
	if (!CreditsBox || !WidgetTree)
	{
		return;
	}

	CreditsBox->ClearChildren();
	HeadingTexts.Reset();
	LineTexts.Reset();

	const UALKMenuConfig* Config = GetMenuConfig();

	UALKTitleText* GameTitle = WidgetTree->ConstructWidget<UALKTitleText>();
	GameTitle->SetText(Config->GameTitle);
	GameTitle->SetJustification(ETextJustify::Center);
	FALKWidgetBuilder::AddToVerticalBox(CreditsBox, GameTitle, FMargin(0.f, 0.f, 0.f, 80.f), HAlign_Center);
	HeadingTexts.Add(GameTitle);

	for (const FALKCreditsSection& Section : Config->Credits)
	{
		UTextBlock* Heading = FALKWidgetBuilder::MakeText(*WidgetTree, NAME_None, Section.Heading);
		Heading->SetJustification(ETextJustify::Center);
		FALKWidgetBuilder::AddToVerticalBox(CreditsBox, Heading, FMargin(0.f, 0.f, 0.f, 14.f), HAlign_Center);
		HeadingTexts.Add(Heading);

		for (int32 Index = 0; Index < Section.Lines.Num(); ++Index)
		{
			UTextBlock* Line = FALKWidgetBuilder::MakeText(*WidgetTree, NAME_None, Section.Lines[Index]);
			Line->SetJustification(ETextJustify::Center);
			const bool bLast = Index == Section.Lines.Num() - 1;
			FALKWidgetBuilder::AddToVerticalBox(CreditsBox, Line, FMargin(0.f, 0.f, 0.f, bLast ? 64.f : 8.f), HAlign_Center);
			LineTexts.Add(Line);
		}
	}

	if (const UALKTheme* Theme = GetTheme())
	{
		NativeApplyTheme(*Theme);
	}
}

void UALKCreditsScreen::NativeApplyTheme(const UALKTheme& Theme)
{
	Super::NativeApplyTheme(Theme);

	if (WidgetTree)
	{
		StyleScreenBackground(Theme);
	}
	if (TitleText)
	{
		FALKWidgetBuilder::StyleText(TitleText, Theme, EALKTextRole::Heading);
	}
	FALKWidgetBuilder::StyleText(SystemLabelText, Theme, EALKTextRole::Data, Theme.Accent);
	if (SystemLabelText)
	{
		SystemLabelText->SetVisibility(SystemLabelText->GetText().IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
	if (UWidget* Header = WidgetTree ? WidgetTree->FindWidget(TEXT("Header")) : nullptr)
	{
		if (UOverlaySlot* HeaderSlot = Cast<UOverlaySlot>(Header->Slot))
		{
			HeaderSlot->SetPadding(FMargin(Theme.ScreenPadding.Left, Theme.ScreenPadding.Top, 0.f, 0.f));
		}
	}
	if (UWidget* Hints = WidgetTree ? WidgetTree->FindWidget(TEXT("Root")) : nullptr)
	{
		if (UOverlay* Root = Cast<UOverlay>(Hints))
		{
			if (UWidget* HintRow = Root->GetChildAt(Root->GetChildrenCount() - 1))
			{
				if (UOverlaySlot* HintSlot = Cast<UOverlaySlot>(HintRow->Slot))
				{
					HintSlot->SetPadding(FMargin(Theme.ScreenPadding.Left, 0.f, 0.f, Theme.ScreenPadding.Bottom));
				}
			}
		}
	}

	for (int32 Index = 0; Index < HeadingTexts.Num(); ++Index)
	{
		UTextBlock* Heading = HeadingTexts[Index];
		if (Index == 0)
		{
			FALKWidgetBuilder::StyleText(Heading, Theme, EALKTextRole::Title);
			continue;
		}
		// "GAME DESIGN" in the data font and the accent text colour.
		FALKWidgetBuilder::StyleText(Heading, Theme, EALKTextRole::Data, Theme.AccentAlt);
		Heading->SetTextTransformPolicy(ETextTransformPolicy::ToUpper);
	}
	for (UTextBlock* Line : LineTexts)
	{
		FALKWidgetBuilder::StyleText(Line, Theme, EALKTextRole::Subheading, Theme.TextPrimary);
	}
}

void UALKCreditsScreen::NativeOnActivated()
{
	Super::NativeOnActivated();
	ScrollOffset = 0.f;
	ManualScrollCooldown = 0.f;
	EndHoldTime = 0.f;
	if (CreditsScroll)
	{
		CreditsScroll->SetScrollOffset(0.f);
	}
}

void UALKCreditsScreen::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!CreditsScroll || !IsActivated())
	{
		return;
	}

	const float Delta = FMath::Min(InDeltaTime, 0.05f);
	if (ManualScrollCooldown > 0.f)
	{
		ManualScrollCooldown -= Delta;
		return;
	}

	const float EndOffset = CreditsScroll->GetScrollOffsetOfEnd();
	if (EndOffset <= 0.f)
	{
		return;
	}

	if (ScrollOffset >= EndOffset)
	{
		EndHoldTime += Delta;
		if (EndHoldTime > 1.5f)
		{
			ScrollOffset = 0.f;
			EndHoldTime = 0.f;
		}
	}
	else
	{
		ScrollOffset = FMath::Min(EndOffset, ScrollOffset + GetTheme()->CreditsScrollSpeed * Delta);
	}
	CreditsScroll->SetScrollOffset(ScrollOffset);
}

bool UALKCreditsScreen::NativeHandleMenuKey(const FKey& Key)
{
	float Direction = 0.f;
	if (Key == EKeys::Up || Key == EKeys::Gamepad_DPad_Up || Key == EKeys::W)
	{
		Direction = -1.f;
	}
	else if (Key == EKeys::Down || Key == EKeys::Gamepad_DPad_Down || Key == EKeys::S)
	{
		Direction = 1.f;
	}

	if (Direction != 0.f && CreditsScroll)
	{
		ScrollOffset = FMath::Clamp(ScrollOffset + Direction * 160.f, 0.f, CreditsScroll->GetScrollOffsetOfEnd());
		CreditsScroll->SetScrollOffset(ScrollOffset);
		ManualScrollCooldown = 3.f;
		return true;
	}
	return false;
}

FReply UALKCreditsScreen::NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (CreditsScroll)
	{
		ScrollOffset = FMath::Clamp(ScrollOffset - InMouseEvent.GetWheelDelta() * 80.f, 0.f, CreditsScroll->GetScrollOffsetOfEnd());
		CreditsScroll->SetScrollOffset(ScrollOffset);
		ManualScrollCooldown = 3.f;
	}
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
