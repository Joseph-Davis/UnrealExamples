// Copyright 2026 CodeLikeMe. All Rights Reserved.

#include "OnlineUI/ALKServerRow.h"
#include "ALKDeveloperSettings.h"
#include "ALKTheme.h"
#include "Online/ALKOnlineConfig.h"
#include "Widgets/ALKDecor.h"
#include "Widgets/ALKFrame.h"
#include "Widgets/ALKTitleText.h"
#include "Widgets/ALKWidgetBuilder.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/SizeBox.h"
#include "Components/SizeBoxSlot.h"
#include "Components/TextBlock.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ALKServerRow)

#define LOCTEXT_NAMESPACE "AssetproLobbyKit"

namespace ALKServerRowPrivate
{
	/** A cell of a fixed width with its content at one side of it. */
	static USizeBox* MakeCell(UWidgetTree& Tree, UWidget* Child, float Width, EHorizontalAlignment HAlign)
	{
		USizeBox* Cell = FALKWidgetBuilder::WrapInSizeBox(Tree, Child, Width, 0.f);
		if (USizeBoxSlot* CellSlot = Cast<USizeBoxSlot>(Child->Slot))
		{
			CellSlot->SetHorizontalAlignment(HAlign);
			CellSlot->SetVerticalAlignment(VAlign_Center);
		}
		return Cell;
	}

	/** How many of the four bars a ping lights. */
	static int32 GetPingLevel(int32 PingMs)
	{
		if (PingMs < 0)
		{
			return 0;
		}
		return PingMs < 60 ? 4 : (PingMs < 120 ? 3 : (PingMs < 200 ? 2 : 1));
	}
}

UALKServerRow::UALKServerRow(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// A list line: the focus fill of a menu entry.
	ButtonStyleType = EALKButtonStyle::Primary;
}

void UALKServerRow::NativeBuildDefaultLayout(UWidgetTree& Tree)
{
	using namespace ALKServerColumns;
	using namespace ALKServerRowPrivate;

	UALKFrame* Frame = FALKWidgetBuilder::Make<UALKFrame>(Tree, TEXT("Background"));
	Frame->SetPadding(FMargin(SidePadding, 0.f));
	Frame->SetHorizontalAlignment(HAlign_Fill);
	Frame->SetVerticalAlignment(VAlign_Fill);
	Background = Frame;
	Tree.RootWidget = Frame;

	UHorizontalBox* Columns = FALKWidgetBuilder::Make<UHorizontalBox>(Tree, TEXT("RowColumns"));
	Frame->SetContent(Columns);

	StarIcon = FALKWidgetBuilder::MakeIcon(Tree, TEXT("StarIcon"), EALKIcon::StarOutline, 20.f);
	FALKWidgetBuilder::AddToHorizontalBox(Columns, MakeCell(Tree, StarIcon, Star, HAlign_Left));

	// The name takes the width that is left; the version tag sits at the end of it.
	UHorizontalBox* NameBox = FALKWidgetBuilder::Make<UHorizontalBox>(Tree, TEXT("NameBox"));
	FALKWidgetBuilder::AddToHorizontalBox(Columns, NameBox, FMargin(0.f, 0.f, 16.f, 0.f), HAlign_Fill, VAlign_Center, 1.f);
	Label = FALKWidgetBuilder::Make<UALKTitleText>(Tree, TEXT("Label"));
	Label->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
	FALKWidgetBuilder::AddToHorizontalBox(NameBox, Label, FMargin(), HAlign_Fill, VAlign_Center, 1.f);
	VersionTag = FALKWidgetBuilder::MakeChip(Tree, TEXT("VersionTag"), LOCTEXT("BrowserDifferentVersion", "Different version"), EALKIcon::None);
	VersionTag->SetVisibility(ESlateVisibility::Collapsed);
	FALKWidgetBuilder::AddToHorizontalBox(NameBox, VersionTag, FMargin(12.f, 0.f, 0.f, 0.f), HAlign_Right, VAlign_Center);

	ModeText = FALKWidgetBuilder::MakeText(Tree, TEXT("ModeText"));
	ModeText->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
	FALKWidgetBuilder::AddToHorizontalBox(Columns, MakeCell(Tree, ModeText, Mode, HAlign_Fill));

	MapText = FALKWidgetBuilder::MakeText(Tree, TEXT("MapText"));
	MapText->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
	FALKWidgetBuilder::AddToHorizontalBox(Columns, MakeCell(Tree, MapText, Map, HAlign_Fill));

	PlayersText = FALKWidgetBuilder::MakeText(Tree, TEXT("PlayersText"));
	PlayersText->SetJustification(ETextJustify::Right);
	FALKWidgetBuilder::AddToHorizontalBox(Columns, MakeCell(Tree, PlayersText, Players, HAlign_Fill));

	// Ping: the number right aligned, then the bars.
	UHorizontalBox* PingBox = FALKWidgetBuilder::Make<UHorizontalBox>(Tree, TEXT("PingBox"));
	PingText = FALKWidgetBuilder::MakeText(Tree, TEXT("PingText"));
	PingText->SetJustification(ETextJustify::Right);
	FALKWidgetBuilder::AddToHorizontalBox(PingBox, PingText, FMargin(), HAlign_Fill, VAlign_Center, 1.f);
	PingIcon = FALKWidgetBuilder::MakeIcon(Tree, TEXT("PingIcon"), EALKIcon::PingBars, 20.f);
	FALKWidgetBuilder::AddToHorizontalBox(PingBox, FALKWidgetBuilder::WrapInSizeBox(Tree, PingIcon, 20.f, 20.f), FMargin(PingBars - 20.f, 0.f, 0.f, 0.f), HAlign_Right, VAlign_Center);
	FALKWidgetBuilder::AddToHorizontalBox(Columns, MakeCell(Tree, PingBox, Ping, HAlign_Fill));

	LockIcon = FALKWidgetBuilder::MakeIcon(Tree, TEXT("LockIcon"), EALKIcon::Lock, 18.f);
	FALKWidgetBuilder::AddToHorizontalBox(Columns, MakeCell(Tree, LockIcon, Lock, HAlign_Right));
}

void UALKServerRow::SetSession(const FALKSessionInfo& InSession, bool bInFavourite, bool bInShowPing)
{
	Session = InSession;
	bFavourite = bInFavourite;
	bShowPing = bInShowPing;
	RefreshContent();
}

void UALKServerRow::SetFavourite(bool bInFavourite)
{
	if (bFavourite != bInFavourite)
	{
		bFavourite = bInFavourite;
		RefreshContent();
	}
}

void UALKServerRow::RefreshContent()
{
	const UALKOnlineConfig* Config = UALKDeveloperSettings::GetOnlineConfig();

	SetButtonText(FText::FromString(Session.ServerName));
	if (ModeText)
	{
		ModeText->SetText(Config->GetModeName(Session.ModeId));
	}
	if (MapText)
	{
		MapText->SetText(Config->GetMapName(Session.MapId));
	}
	if (PlayersText)
	{
		PlayersText->SetText(FText::Format(LOCTEXT("BrowserPlayersValue", "{0} / {1}"), FText::AsNumber(Session.CurrentPlayers), FText::AsNumber(Session.MaxPlayers)));
	}
	if (PingText)
	{
		PingText->SetText(Session.PingMs >= 0 ? FText::Format(LOCTEXT("BrowserPingValue", "{0} ms"), FText::AsNumber(Session.PingMs)) : FText::GetEmpty());
		PingText->SetVisibility(bShowPing ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}
	if (PingIcon)
	{
		PingIcon->SetLevel(ALKServerRowPrivate::GetPingLevel(Session.PingMs));
	}
	if (LockIcon)
	{
		LockIcon->SetVisibility(Session.bPasswordProtected ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}
	if (VersionTag)
	{
		VersionTag->SetVisibility(Session.bVersionMatches ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
	if (const UALKTheme* Theme = UALKDeveloperSettings::GetTheme())
	{
		NativeApplyVisualState(*Theme, GetHighlight(), IsUsable());
	}
}

void UALKServerRow::NativeApplyTheme(const UALKTheme& Theme)
{
	Super::NativeApplyTheme(Theme);

	// As wide as the table, one line high.
	SetMinDimensions(0, FMath::RoundToInt(ALKServerColumns::RowHeight));
	if (Label)
	{
		// The name: the weight of a card title at the size of the body text.
		FSlateFontInfo NameFont = Theme.SubheadingFont;
		NameFont.Size = Theme.BodyFont.Size;
		FALKWidgetBuilder::SetTextFont(Label, NameFont);
		Label->SetTextTransformPolicy(ETextTransformPolicy::None);
		Label->SetShadowColorAndOpacity(FLinearColor::Transparent);
	}
	FSlateFontInfo CellFont = Theme.BodyFont;
	CellFont.Size = FMath::Max(8.f, Theme.BodyFont.Size - 1.f);
	for (UTextBlock* Cell : { ModeText.Get(), MapText.Get() })
	{
		if (Cell)
		{
			FALKWidgetBuilder::StyleText(Cell, Theme, EALKTextRole::Body, Theme.TextMuted);
			FALKWidgetBuilder::SetTextFont(Cell, CellFont);
		}
	}
	FSlateFontInfo NumberFont = Theme.DataFont;
	NumberFont.Size = CellFont.Size;
	for (UTextBlock* Number : { PlayersText.Get(), PingText.Get() })
	{
		if (Number)
		{
			FALKWidgetBuilder::StyleText(Number, Theme, EALKTextRole::Data, Theme.TextPrimary);
			FALKWidgetBuilder::SetTextFont(Number, NumberFont);
		}
	}
	RefreshContent();
}

void UALKServerRow::NativeApplyVisualState(const UALKTheme& Theme, float Highlight, bool bEnabled)
{
	Super::NativeApplyVisualState(Theme, Highlight, bEnabled);

	if (UALKFrame* Frame = Cast<UALKFrame>(Background))
	{
		// The focus fill and outline of a list line, without the small bar of a menu entry.
		Frame->SetFrameStyle(Theme.MakeItemFrame(Highlight, false));
	}

	// On a solid focus fill (the Arcade skin) every text takes the colour that reads on the accent colour.
	const bool bSolid = Theme.FocusStyle == EALKFocusStyle::Solid;
	const float OnFill = bSolid ? Highlight : 0.f;
	// A game of another version cannot be joined: it steps back.
	const float Dim = Session.bVersionMatches ? 1.f : 0.45f;
	const FLinearColor FocusText = Theme.GetFocusTextColor();
	auto Tone = [OnFill, Dim, &FocusText](const FLinearColor& Color)
	{
		const FLinearColor Blended = FMath::Lerp(Color, FocusText, OnFill);
		return Blended.CopyWithNewOpacity(Blended.A * Dim);
	};
	const FLinearColor Primary = Tone(Theme.TextPrimary);
	const FLinearColor Muted = Tone(Theme.TextMuted);

	if (Label)
	{
		Label->SetColorAndOpacity(FSlateColor(Primary));
	}
	if (ModeText)
	{
		ModeText->SetColorAndOpacity(FSlateColor(Muted));
	}
	if (MapText)
	{
		MapText->SetColorAndOpacity(FSlateColor(Muted));
	}
	if (PlayersText)
	{
		PlayersText->SetColorAndOpacity(FSlateColor(Session.IsFull() ? Muted : Primary));
	}
	if (PingText)
	{
		PingText->SetColorAndOpacity(FSlateColor(Primary));
	}
	if (PingIcon)
	{
		PingIcon->SetIcon(EALKIcon::PingBars, Tone(Session.PingMs >= 0 ? Theme.GetPingColor(Session.PingMs) : Theme.TextMuted));
	}
	if (StarIcon)
	{
		const FLinearColor Empty = Theme.TextMuted.CopyWithNewOpacity(Theme.TextMuted.A * 0.55f);
		StarIcon->SetIcon(bFavourite ? EALKIcon::Star : EALKIcon::StarOutline, Tone(bFavourite ? Theme.Warning : Empty));
	}
	if (LockIcon)
	{
		LockIcon->SetIcon(EALKIcon::Lock, Muted);
	}
	if (VersionTag)
	{
		const FLinearColor TagColor = FMath::Lerp(Theme.Warning, FocusText, OnFill);
		FALKWidgetBuilder::StyleChip(VersionTag, Theme, FLinearColor::Transparent, TagColor, TagColor);
	}
}

void UALKServerRow::NativeOnAddedToFocusPath(const FFocusEvent& InFocusEvent)
{
	Super::NativeOnAddedToFocusPath(InFocusEvent);
	OnRowFocused.Broadcast(this);
}

#undef LOCTEXT_NAMESPACE
