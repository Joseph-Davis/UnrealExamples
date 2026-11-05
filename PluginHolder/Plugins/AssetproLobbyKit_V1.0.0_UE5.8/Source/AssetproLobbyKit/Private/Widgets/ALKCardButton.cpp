// Copyright 2026 CodeLikeMe. All Rights Reserved.

#include "Widgets/ALKCardButton.h"
#include "ALKTheme.h"
#include "Widgets/ALKDecor.h"
#include "Widgets/ALKFrame.h"
#include "Widgets/ALKPaint.h"
#include "Widgets/ALKThumbnail.h"
#include "Widgets/ALKTitleText.h"
#include "Widgets/ALKWidgetBuilder.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/Overlay.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ALKCardButton)

UALKCardButton::UALKCardButton(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ButtonStyleType = EALKButtonStyle::Primary;
}

void UALKCardButton::NativeBuildDefaultLayout(UWidgetTree& Tree)
{
	UALKFrame* Frame = FALKWidgetBuilder::Make<UALKFrame>(Tree, TEXT("Background"));
	Frame->SetHorizontalAlignment(HAlign_Fill);
	Frame->SetVerticalAlignment(VAlign_Fill);
	Background = Frame;
	Tree.RootWidget = Frame;

	UOverlay* Overlay = FALKWidgetBuilder::Make<UOverlay>(Tree, TEXT("CardOverlay"));
	Frame->SetContent(Overlay);

	UVerticalBox* Column = FALKWidgetBuilder::Make<UVerticalBox>(Tree, TEXT("CardColumn"));
	FALKWidgetBuilder::AddToOverlay(Overlay, Column);

	// The picture across the top (collapsed until Set Picture), or the icon in a tinted box.
	Picture = FALKWidgetBuilder::Make<UALKThumbnail>(Tree, TEXT("Picture"));
	Picture->SetVisibility(ESlateVisibility::Collapsed);
	FALKWidgetBuilder::AddToVerticalBox(Column, Picture, FMargin(0.f, 0.f, 0.f, 14.f), HAlign_Fill, VAlign_Fill, 1.f);

	IconBox = FALKWidgetBuilder::Make<UALKFrame>(Tree, TEXT("IconBox"));
	IconBox->SetHorizontalAlignment(HAlign_Center);
	IconBox->SetVerticalAlignment(VAlign_Center);
	CardIcon = FALKWidgetBuilder::MakeIcon(Tree, TEXT("CardIcon"), EALKIcon::None, 28.f);
	IconBox->SetContent(CardIcon);
	USizeBox* IconSize = FALKWidgetBuilder::WrapInSizeBox(Tree, IconBox, 52.f, 52.f);
	IconSize->SetVisibility(ESlateVisibility::Collapsed);
	FALKWidgetBuilder::AddToVerticalBox(Column, IconSize, FMargin(), HAlign_Left, VAlign_Top, 1.f);

	// Title with an optional tag at its right.
	UHorizontalBox* TitleRow = FALKWidgetBuilder::Make<UHorizontalBox>(Tree, TEXT("TitleRow"));
	FALKWidgetBuilder::AddToVerticalBox(Column, TitleRow);
	Label = FALKWidgetBuilder::Make<UALKTitleText>(Tree, TEXT("Label"));
	Label->SetText(ButtonText);
	// A title too long for the card (a longer language) goes on to a second line instead of being cut off; the picture or
	// the icon over it gives up the height.
	Label->SetAutoWrapText(true);
	Label->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
	FALKWidgetBuilder::AddToHorizontalBox(TitleRow, Label, FMargin(), HAlign_Fill, VAlign_Center, 1.f);
	CornerChip = FALKWidgetBuilder::MakeChip(Tree, TEXT("CornerChip"), FText::GetEmpty(), EALKIcon::None);
	CornerChip->SetVisibility(ESlateVisibility::Collapsed);
	FALKWidgetBuilder::AddToHorizontalBox(TitleRow, CornerChip, FMargin(10.f, 0.f, 0.f, 0.f), HAlign_Right, VAlign_Center);

	SubtitleText = FALKWidgetBuilder::MakeText(Tree, TEXT("SubtitleText"));
	// A long line wraps inside the card instead of running out of it.
	SubtitleText->SetAutoWrapText(true);
	SubtitleText->SetVisibility(ESlateVisibility::Collapsed);
	FALKWidgetBuilder::AddToVerticalBox(Column, SubtitleText, FMargin(0.f, 4.f, 0.f, 0.f));

	UHorizontalBox* FooterRow = FALKWidgetBuilder::Make<UHorizontalBox>(Tree, TEXT("FooterRow"));
	FooterRow->SetVisibility(ESlateVisibility::Collapsed);
	FALKWidgetBuilder::AddToVerticalBox(Column, FooterRow, FMargin(0.f, 10.f, 0.f, 0.f));
	FooterDot = FALKWidgetBuilder::MakeIcon(Tree, TEXT("FooterDot"), EALKIcon::Dot, 14.f);
	FALKWidgetBuilder::AddToHorizontalBox(FooterRow, FooterDot, FMargin(0.f, 0.f, 6.f, 0.f), HAlign_Left, VAlign_Center);
	FooterText = FALKWidgetBuilder::MakeText(Tree, TEXT("FooterText"));
	FALKWidgetBuilder::AddToHorizontalBox(FooterRow, FooterText, FMargin(), HAlign_Left, VAlign_Center);

	// Over the picture: a badge at its top left. Over the card: the action pill at its lower right.
	BadgeChip = FALKWidgetBuilder::MakeChip(Tree, TEXT("BadgeChip"), FText::GetEmpty(), EALKIcon::None);
	BadgeChip->SetVisibility(ESlateVisibility::Collapsed);
	FALKWidgetBuilder::AddToOverlay(Overlay, BadgeChip, HAlign_Left, VAlign_Top, FMargin(12.f));
	ActionPill = FALKWidgetBuilder::MakeChip(Tree, TEXT("ActionPill"), FText::GetEmpty(), EALKIcon::None);
	ActionPill->SetPadding(FMargin(20.f, 11.f, 22.f, 12.f));
	ActionPill->SetVisibility(ESlateVisibility::Collapsed);
	FALKWidgetBuilder::AddToOverlay(Overlay, ActionPill, HAlign_Right, VAlign_Bottom);
}

void UALKCardButton::NativeApplyTheme(const UALKTheme& Theme)
{
	Super::NativeApplyTheme(Theme);

	// A card is as large as its slot, not as a menu entry.
	SetMinDimensions(0, 0);
	if (UALKFrame* Frame = Cast<UALKFrame>(Background))
	{
		Frame->SetPadding(bLarge ? FMargin(14.f, 14.f, 18.f, 18.f) : FMargin(18.f));
	}
	if (Label)
	{
		FSlateFontInfo TitleFont = Theme.SubheadingFont;
		TitleFont.Size = bLarge ? FMath::RoundToFloat(Theme.SubheadingFont.Size * 1.5f) : Theme.SubheadingFont.Size;
		FALKWidgetBuilder::SetTextFont(Label, TitleFont);
		Label->SetTextTransformPolicy(Theme.bUppercaseButtons ? ETextTransformPolicy::ToUpper : ETextTransformPolicy::None);
	}
	FALKWidgetBuilder::StyleText(SubtitleText, Theme, EALKTextRole::Caption, Theme.TextMuted);
	FALKWidgetBuilder::StyleText(FooterText, Theme, EALKTextRole::Caption, Theme.TextMuted);
	if (Picture)
	{
		Picture->SetCornerRadius(FMath::Max(0.f, Theme.CornerRadius - 4.f));
	}
	if (IconBox)
	{
		FALKFrameStyle BoxStyle = Theme.MakeChipFrame(Theme.Accent.CopyWithNewOpacity(0.16f), FLinearColor::Transparent);
		BoxStyle.Radius = Theme.CornerRadius;
		IconBox->SetFrameStyle(BoxStyle);
	}
	if (CardIcon)
	{
		CardIcon->SetIcon(CardIconType, Theme.GetAccentTextColor());
	}
	FALKWidgetBuilder::StyleChip(BadgeChip, Theme, Theme.BackgroundTop.CopyWithNewOpacity(0.8f), FLinearColor::Transparent, Theme.TextPrimary);
	FALKWidgetBuilder::StyleChip(CornerChip, Theme, Theme.PanelRaised, Theme.PanelOutline, Theme.TextPrimary);
	if (ActionPill)
	{
		FALKWidgetBuilder::StyleChip(ActionPill, Theme, Theme.Accent, FLinearColor::Transparent, Theme.AccentContrast);
		ActionPill->SetFrameStyle(Theme.MakeSolidButtonFrame(0.f));
	}
	if (FooterDot)
	{
		FooterDot->SetIcon(EALKIcon::Dot, FooterDotColor);
	}
	RefreshVisibility();
}

void UALKCardButton::NativeApplyVisualState(const UALKTheme& Theme, float Highlight, bool bEnabled)
{
	Super::NativeApplyVisualState(Theme, Highlight, bEnabled);
	if (UALKFrame* Frame = Cast<UALKFrame>(Background))
	{
		// A card on the background: a panel at rest, raised with the focus ring, a glow and a deeper shadow when it has
		// focus.
		FALKFrameStyle CardStyle = Theme.MakePanelFrame();
		const FALKFrameStyle Focused = Theme.MakeCardFrame(Highlight);
		CardStyle.Fill = FMath::Lerp(CardStyle.Fill, Focused.Fill, Highlight);
		CardStyle.Ring = Focused.Ring;
		CardStyle.RingWidth = Focused.RingWidth;
		CardStyle.Glow = Focused.Glow;
		CardStyle.GlowSize = Focused.GlowSize;
		if (CardStyle.ShadowBlur > 0.f)
		{
			CardStyle.Shadow.A = FMath::Min(1.f, CardStyle.Shadow.A * (1.f + 0.4f * Highlight));
			CardStyle.ShadowOffset *= 1.0 + 0.5 * Highlight;
		}
		Frame->SetFrameStyle(CardStyle);
	}

	// A focused card lifts off the screen: it grows a touch and its picture, a little dimmed at rest, comes up to full
	// brightness. Reduce Effects keeps the card still.
	const float LiftScale = FMath::Clamp(Theme.CardFocusScale, 1.f, 1.1f) - 1.f;
	const float Scale = 1.f + (FALKEffects::AreReduced() ? 0.f : LiftScale * Highlight);
	SetRenderScale(FVector2D(Scale, Scale));
	if (Picture)
	{
		const float Rest = 1.f - FMath::Min(LiftScale * 5.f, 0.25f);
		const float Brightness = FMath::Lerp(Rest, 1.f, Highlight);
		Picture->SetTint(FLinearColor(Brightness, Brightness, Brightness, 1.f));
		// The picture melts into the card under it, where the title sits.
		const FLinearColor Under = FMath::Lerp(Theme.Panel, Theme.PanelRaised, Highlight);
		Picture->SetVeil(Under.CopyWithNewOpacity(Theme.PictureVeil));
	}
	if (Label)
	{
		Label->SetColorAndOpacity(FSlateColor(bEnabled ? Theme.TextPrimary : Theme.TextMuted));
	}
	SetRenderOpacity(bEnabled ? 1.f : 0.55f);
}

void UALKCardButton::NativeOnAddedToFocusPath(const FFocusEvent& InFocusEvent)
{
	Super::NativeOnAddedToFocusPath(InFocusEvent);
	OnCardFocused.Broadcast(this);
}

void UALKCardButton::SetCard(const FText& Title, const FText& Subtitle, EALKIcon InIcon)
{
	SetButtonText(Title);
	CardIconType = InIcon;
	if (CardIcon)
	{
		CardIcon->SetIcon(CardIconType, CardIcon->Color);
	}
	SetSubtitle(Subtitle);
}

void UALKCardButton::SetSubtitle(const FText& Subtitle)
{
	if (SubtitleText)
	{
		SubtitleText->SetText(Subtitle);
	}
	RefreshVisibility();
}

void UALKCardButton::SetPicture(UTexture2D* InPicture, FName PlaceholderId)
{
	bHasPicture = true;
	if (Picture)
	{
		Picture->SetPicture(InPicture, PlaceholderId);
	}
	RefreshVisibility();
}

void UALKCardButton::ClearPicture()
{
	bHasPicture = false;
	RefreshVisibility();
}

void UALKCardButton::SetBadge(const FText& Text)
{
	FALKWidgetBuilder::SetChipText(BadgeChip, Text);
	if (BadgeChip)
	{
		BadgeChip->SetVisibility(Text.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
}

void UALKCardButton::SetCornerChip(const FText& Text)
{
	FALKWidgetBuilder::SetChipText(CornerChip, Text);
	if (CornerChip)
	{
		CornerChip->SetVisibility(Text.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
}

void UALKCardButton::SetFooter(const FText& Text, FLinearColor DotColor)
{
	FooterDotColor = DotColor;
	if (FooterText)
	{
		FooterText->SetText(Text);
		if (UWidget* Row = FooterText->GetParent())
		{
			Row->SetVisibility(Text.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
		}
	}
	if (FooterDot)
	{
		FooterDot->SetIcon(EALKIcon::Dot, FooterDotColor);
		FooterDot->SetVisibility(FooterDotColor.A > 0.f ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void UALKCardButton::SetActionLabel(const FText& Text, EALKIcon InIcon)
{
	if (!ActionPill)
	{
		return;
	}
	FALKWidgetBuilder::SetChipText(ActionPill, Text);
	if (const UHorizontalBox* Row = Cast<UHorizontalBox>(ActionPill->GetContent()))
	{
		if (UALKIconWidget* PillIcon = Row->GetChildrenCount() > 0 ? Cast<UALKIconWidget>(Row->GetChildAt(0)) : nullptr)
		{
			PillIcon->SetIcon(InIcon, PillIcon->Color);
			PillIcon->SetVisibility(InIcon == EALKIcon::None ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
		}
	}
	ActionPill->SetVisibility(Text.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
}

void UALKCardButton::RefreshVisibility()
{
	if (Picture)
	{
		Picture->SetVisibility(bHasPicture ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (UWidget* IconHolder = IconBox ? IconBox->GetParent() : nullptr)
	{
		IconHolder->SetVisibility(!bHasPicture && CardIconType != EALKIcon::None ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (SubtitleText)
	{
		SubtitleText->SetVisibility(SubtitleText->GetText().IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
}
