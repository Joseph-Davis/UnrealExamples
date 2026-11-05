// Copyright 2026 CodeLikeMe. All Rights Reserved.

#include "OnlineUI/ALKScoreRow.h"
#include "ALKDeveloperSettings.h"
#include "ALKTheme.h"
#include "Widgets/ALKDecor.h"
#include "Widgets/ALKFrame.h"
#include "Widgets/ALKTitleText.h"
#include "Widgets/ALKWidgetBuilder.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ALKScoreRow)

#define LOCTEXT_NAMESPACE "AssetproLobbyKit"

namespace ALKScoreRowPrivate
{
	static void SetShown(UWidget* Widget, bool bShown)
	{
		if (Widget)
		{
			const ESlateVisibility Wanted = bShown ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;
			if (Widget->GetVisibility() != Wanted)
			{
				Widget->SetVisibility(Wanted);
			}
		}
	}

	/** How many of the four connection bars a ping lights. */
	static int32 PingLevel(int32 PingMs)
	{
		return PingMs < 50 ? 4 : (PingMs < 100 ? 3 : (PingMs < 150 ? 2 : 1));
	}
}

UALKScoreRow::UALKScoreRow(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// A list line: the focus frame of a menu entry.
	ButtonStyleType = EALKButtonStyle::Primary;
}

void UALKScoreRow::NativeBuildDefaultLayout(UWidgetTree& Tree)
{
	UALKFrame* Frame = FALKWidgetBuilder::Make<UALKFrame>(Tree, TEXT("Background"));
	Frame->SetPadding(FMargin(SidePadding, 0.f, SidePadding, 0.f));
	Frame->SetHorizontalAlignment(HAlign_Fill);
	Frame->SetVerticalAlignment(VAlign_Fill);
	Background = Frame;
	Tree.RootWidget = Frame;

	UHorizontalBox* RowBox = FALKWidgetBuilder::Make<UHorizontalBox>(Tree, TEXT("RowBox"));
	Frame->SetContent(RowBox);

	RankText = FALKWidgetBuilder::MakeText(Tree, TEXT("RankText"));
	RankText->SetJustification(ETextJustify::Center);
	FALKWidgetBuilder::AddToHorizontalBox(RowBox, FALKWidgetBuilder::WrapInSizeBox(Tree, RankText, 34.f, 0.f), FMargin(0.f, 0.f, 10.f, 0.f), HAlign_Left, VAlign_Center);

	Avatar = FALKWidgetBuilder::Make<UALKAvatar>(Tree, TEXT("Avatar"));
	Avatar->Diameter = 32.f;
	FALKWidgetBuilder::AddToHorizontalBox(RowBox, Avatar, FMargin(0.f, 0.f, 12.f, 0.f), HAlign_Left, VAlign_Center);

	// The name and its marks share the space left of the numbers; a very long name is cut off rather than pushing them.
	UHorizontalBox* NameBox = FALKWidgetBuilder::Make<UHorizontalBox>(Tree, TEXT("NameBox"));
	NameBox->SetClipping(EWidgetClipping::ClipToBounds);
	FALKWidgetBuilder::AddToHorizontalBox(RowBox, NameBox, FMargin(), HAlign_Fill, VAlign_Center, 1.f);

	Label = FALKWidgetBuilder::Make<UALKTitleText>(Tree, TEXT("Label"));
	FALKWidgetBuilder::AddToHorizontalBox(NameBox, Label, FMargin(), HAlign_Left, VAlign_Center);

	YouChip = FALKWidgetBuilder::MakeChip(Tree, TEXT("YouChip"), LOCTEXT("ScoreRowYou", "You"), EALKIcon::None);
	FALKWidgetBuilder::AddToHorizontalBox(NameBox, YouChip, FMargin(12.f, 0.f, 0.f, 0.f), HAlign_Left, VAlign_Center);

	HostIcon = FALKWidgetBuilder::MakeIcon(Tree, TEXT("HostIcon"), EALKIcon::Crown, 18.f);
	FALKWidgetBuilder::AddToHorizontalBox(NameBox, HostIcon, FMargin(10.f, 0.f, 0.f, 0.f), HAlign_Left, VAlign_Center);

	MvpIcon = FALKWidgetBuilder::MakeIcon(Tree, TEXT("MvpIcon"), EALKIcon::Star, 18.f);
	FALKWidgetBuilder::AddToHorizontalBox(NameBox, MvpIcon, FMargin(10.f, 0.f, 0.f, 0.f), HAlign_Left, VAlign_Center);

	TalkIcon = FALKWidgetBuilder::MakeIcon(Tree, TEXT("TalkIcon"), EALKIcon::Speaker, 18.f);
	FALKWidgetBuilder::AddToHorizontalBox(NameBox, TalkIcon, FMargin(10.f, 0.f, 0.f, 0.f), HAlign_Left, VAlign_Center);

	MutedIcon = FALKWidgetBuilder::MakeIcon(Tree, TEXT("MutedIcon"), EALKIcon::SpeakerMuted, 18.f);
	FALKWidgetBuilder::AddToHorizontalBox(NameBox, MutedIcon, FMargin(10.f, 0.f, 0.f, 0.f), HAlign_Left, VAlign_Center);

	LeftText = FALKWidgetBuilder::MakeText(Tree, TEXT("LeftText"), LOCTEXT("ScoreRowLeft", "Left"));
	FALKWidgetBuilder::AddToHorizontalBox(NameBox, LeftText, FMargin(12.f, 0.f, 0.f, 0.f), HAlign_Left, VAlign_Center);

	CellBox = FALKWidgetBuilder::Make<UHorizontalBox>(Tree, TEXT("CellBox"));
	FALKWidgetBuilder::AddToHorizontalBox(RowBox, CellBox, FMargin(), HAlign_Right, VAlign_Center);

	// Ping: the number, then the bars.
	UHorizontalBox* PingBox = FALKWidgetBuilder::Make<UHorizontalBox>(Tree, TEXT("PingBox"));
	PingText = FALKWidgetBuilder::MakeText(Tree, TEXT("PingText"));
	PingText->SetJustification(ETextJustify::Right);
	FALKWidgetBuilder::AddToHorizontalBox(PingBox, PingText, FMargin(), HAlign_Fill, VAlign_Center, 1.f);
	PingIcon = FALKWidgetBuilder::MakeIcon(Tree, TEXT("PingIcon"), EALKIcon::PingBars, 18.f);
	FALKWidgetBuilder::AddToHorizontalBox(PingBox, PingIcon, FMargin(8.f, 0.f, 0.f, 0.f), HAlign_Right, VAlign_Center);
	USizeBox* PingSize = FALKWidgetBuilder::WrapInSizeBox(Tree, PingBox, PingColumnWidth, 0.f);
	PingSize->SetVisibility(ESlateVisibility::Collapsed);
	FALKWidgetBuilder::AddToHorizontalBox(RowBox, PingSize, FMargin(), HAlign_Right, VAlign_Center);

	RefreshMarks();
}

void UALKScoreRow::SetColumnWidths(const TArray<float>& Widths, bool bWithPing)
{
	bHasPing = bWithPing;
	// The ping holder is the size box round the ping box.
	if (UWidget* PingBox = PingText ? PingText->GetParent() : nullptr)
	{
		ALKScoreRowPrivate::SetShown(PingBox->GetParent() ? PingBox->GetParent() : PingBox, bHasPing);
	}

	if (!CellBox || !WidgetTree)
	{
		return;
	}
	// The same columns again (a refresh): keep the cells.
	bool bSame = CellTexts.Num() == Widths.Num() && CellBox->GetChildrenCount() == Widths.Num();
	for (int32 Index = 0; bSame && Index < Widths.Num(); ++Index)
	{
		const USizeBox* Cell = Cast<USizeBox>(CellBox->GetChildAt(Index));
		bSame = Cell && FMath::IsNearlyEqual(Cell->GetWidthOverride(), Widths[Index]);
	}
	if (bSame)
	{
		return;
	}

	CellBox->ClearChildren();
	CellTexts.Reset();
	for (const float Width : Widths)
	{
		UTextBlock* Cell = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		Cell->SetJustification(ETextJustify::Right);
		USizeBox* CellSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		CellSize->SetWidthOverride(FMath::Max(20.f, Width));
		CellSize->SetContent(Cell);
		FALKWidgetBuilder::AddToHorizontalBox(CellBox, CellSize, FMargin(), HAlign_Right, VAlign_Center);
		CellTexts.Add(Cell);
	}
	RestyleNow();
}

void UALKScoreRow::SetIdentity(int32 Rank, const FString& ShownName, const FString& InUserId, bool bAnonymousAvatar)
{
	UserId = InUserId;
	ShownPlayerName = ShownName;
	SetButtonText(FText::FromString(ShownName));
	if (RankText)
	{
		RankText->SetText(Rank > 0 ? FText::AsNumber(Rank) : FText::GetEmpty());
	}
	if (Avatar)
	{
		Avatar->SetPlayer(ShownName, nullptr);
		Avatar->SetAnonymous(bAnonymousAvatar);
	}
}

void UALKScoreRow::SetValues(const TArray<FText>& Values)
{
	for (int32 Index = 0; Index < CellTexts.Num(); ++Index)
	{
		if (UTextBlock* Cell = CellTexts[Index])
		{
			Cell->SetText(Values.IsValidIndex(Index) ? Values[Index] : FText::GetEmpty());
		}
	}
}

void UALKScoreRow::SetPing(int32 PingMs, bool bShowNumber)
{
	const bool bChanged = ShownPing != PingMs || bPingNumber != bShowNumber;
	ShownPing = PingMs;
	bPingNumber = bShowNumber;
	if (PingText)
	{
		PingText->SetText(bShowNumber && PingMs >= 0 ? FText::AsNumber(PingMs) : FText::GetEmpty());
	}
	if (PingIcon)
	{
		PingIcon->SetLevel(PingMs >= 0 ? ALKScoreRowPrivate::PingLevel(PingMs) : 0);
	}
	if (bChanged)
	{
		RestyleNow();
	}
}

void UALKScoreRow::SetMarks(bool bInLocal, bool bInHost, bool bInMVP, bool bInMuted, bool bInTalking, bool bInLeft)
{
	const bool bChanged = bLocal != bInLocal || bHost != bInHost || bMVP != bInMVP || bMuted != bInMuted || bTalking != bInTalking || bLeft != bInLeft;
	bLocal = bInLocal;
	bHost = bInHost;
	bMVP = bInMVP;
	bMuted = bInMuted;
	bTalking = bInTalking;
	bLeft = bInLeft;
	if (bChanged)
	{
		RefreshMarks();
		RestyleNow();
	}
}

void UALKScoreRow::SetRankColor(FLinearColor InColor)
{
	if (!RankColor.Equals(InColor))
	{
		RankColor = InColor;
		RestyleNow();
	}
}

void UALKScoreRow::SetBoldColumn(int32 ColumnIndex)
{
	if (BoldColumn != ColumnIndex)
	{
		BoldColumn = ColumnIndex;
		RestyleNow();
	}
}

void UALKScoreRow::SetInteractive(bool bInInteractive)
{
	bInteractive = bInInteractive;
	SetIsFocusable(bInteractive);
	bFocusOnHover = bInteractive;
	// A row that only shows lets the mouse through, so the scoreboard never takes a click from the game.
	SetVisibility(bInteractive ? ESlateVisibility::Visible : ESlateVisibility::HitTestInvisible);
}

void UALKScoreRow::RefreshMarks()
{
	using namespace ALKScoreRowPrivate;
	SetShown(YouChip, bLocal);
	SetShown(HostIcon, bHost);
	SetShown(MvpIcon, bMVP);
	// A muted player is never shown as talking.
	SetShown(TalkIcon, bTalking && !bMuted);
	SetShown(MutedIcon, bMuted);
	SetShown(LeftText, bLeft);
}

void UALKScoreRow::RestyleNow()
{
	const UALKTheme* Theme = UALKDeveloperSettings::GetTheme();
	if (!Theme || !bApplyThemeStyles)
	{
		return;
	}
	for (int32 Index = 0; Index < CellTexts.Num(); ++Index)
	{
		if (UTextBlock* Cell = CellTexts[Index])
		{
			FSlateFontInfo CellFont = Index == BoldColumn ? Theme->SubheadingFont : Theme->BodyFont;
			CellFont.Size = Theme->BodyFont.Size;
			FALKWidgetBuilder::SetTextFont(Cell, CellFont);
		}
	}
	NativeApplyVisualState(*Theme, GetHighlight(), IsUsable());
}

void UALKScoreRow::NativeApplyTheme(const UALKTheme& Theme)
{
	Super::NativeApplyTheme(Theme);

	// A list line, not a menu entry: as wide as its list, and lower.
	SetMinDimensions(0, RowHeight);
	if (Label)
	{
		// Names are shown as the players wrote them, in every skin.
		FSlateFontInfo NameFont = Theme.SubheadingFont;
		NameFont.Size = FMath::Max(10.f, FMath::RoundToFloat(Theme.SubheadingFont.Size * 0.82f));
		FALKWidgetBuilder::SetTextFont(Label, NameFont);
		Label->SetTextTransformPolicy(ETextTransformPolicy::None);
		Label->SetShadowColorAndOpacity(FLinearColor::Transparent);
	}
	FALKWidgetBuilder::StyleText(RankText, Theme, EALKTextRole::Data, Theme.TextMuted);
	FALKWidgetBuilder::StyleText(LeftText, Theme, EALKTextRole::Caption, Theme.TextMuted);
	FALKWidgetBuilder::StyleText(PingText, Theme, EALKTextRole::Data, Theme.TextMuted);
	RefreshMarks();
	RestyleNow();
}

void UALKScoreRow::NativeApplyVisualState(const UALKTheme& Theme, float Highlight, bool bEnabled)
{
	Super::NativeApplyVisualState(Theme, Highlight, bEnabled);

	const float Focus = bInteractive ? FMath::Clamp(Highlight, 0.f, 1.f) : 0.f;
	// On a solid focus fill (the Arcade skin) every colour gives way to the contrast colour; a tinted focus keeps them.
	const float Solid = Theme.FocusStyle == EALKFocusStyle::Solid ? Focus : 0.f;
	const FLinearColor FocusText = Theme.GetFocusTextColor();
	const float Dim = bLeft ? 0.5f : 1.f;
	const FLinearColor NameColor = FMath::Lerp(Theme.TextPrimary, FocusText, Focus).CopyWithNewOpacity(Dim);
	const FLinearColor ValueColor = FMath::Lerp(Theme.TextSecondary, FocusText, Focus).CopyWithNewOpacity(Dim);
	const FLinearColor SoftColor = FMath::Lerp(Theme.TextMuted, FocusText, Solid).CopyWithNewOpacity(Dim);

	if (Label)
	{
		Label->SetColorAndOpacity(FSlateColor(NameColor));
	}
	if (RankText)
	{
		RankText->SetColorAndOpacity(FSlateColor(RankColor.A > 0.f ? FMath::Lerp(RankColor, FocusText, Solid).CopyWithNewOpacity(Dim) : SoftColor));
	}
	for (int32 Index = 0; Index < CellTexts.Num(); ++Index)
	{
		if (UTextBlock* Cell = CellTexts[Index])
		{
			Cell->SetColorAndOpacity(FSlateColor(Index == BoldColumn ? NameColor : ValueColor));
		}
	}
	if (PingText)
	{
		PingText->SetColorAndOpacity(FSlateColor(SoftColor));
	}
	if (PingIcon)
	{
		PingIcon->SetIcon(EALKIcon::PingBars, FMath::Lerp(Theme.GetPingColor(FMath::Max(0, ShownPing)), FocusText, Solid).CopyWithNewOpacity(ShownPing >= 0 ? Dim : 0.25f));
	}
	if (LeftText)
	{
		LeftText->SetColorAndOpacity(FSlateColor(SoftColor));
	}
	if (HostIcon)
	{
		HostIcon->SetIcon(EALKIcon::Crown, FMath::Lerp(Theme.Warning, FocusText, Solid));
	}
	if (MvpIcon)
	{
		MvpIcon->SetIcon(EALKIcon::Star, FMath::Lerp(Theme.Warning, FocusText, Solid));
	}
	if (TalkIcon)
	{
		TalkIcon->SetIcon(EALKIcon::Speaker, FMath::Lerp(Theme.Success, FocusText, Solid));
	}
	if (MutedIcon)
	{
		MutedIcon->SetIcon(EALKIcon::SpeakerMuted, FMath::Lerp(Theme.Danger, FocusText, Solid));
	}
	if (YouChip)
	{
		FALKWidgetBuilder::StyleChip(YouChip, Theme, FMath::Lerp(Theme.Accent.CopyWithNewOpacity(0.2f), FocusText.CopyWithNewOpacity(0.16f), Solid), FLinearColor::Transparent,
			FMath::Lerp(Theme.GetAccentTextColor(), FocusText, Solid));
	}
	if (Avatar)
	{
		Avatar->SetBackColor(Theme.Panel);
		Avatar->SetRingColor(bTalking && !bMuted ? Theme.Success : FLinearColor::Transparent);
		Avatar->SetRenderOpacity(Dim);
	}
	if (UALKFrame* Frame = Cast<UALKFrame>(Background))
	{
		FALKFrameStyle RowFrame = Theme.MakeItemFrame(Focus);
		if (bLocal)
		{
			// The local player's line is outlined in the accent colour, also without focus.
			RowFrame.Fill = FMath::Lerp(Theme.Accent.CopyWithNewOpacity(0.14f), RowFrame.Fill, Focus);
			RowFrame.Outline = FMath::Lerp(Theme.Accent.CopyWithNewOpacity(0.7f), RowFrame.Outline, Focus);
		}
		Frame->SetFrameStyle(RowFrame);
	}
}

void UALKScoreRow::NativeOnAddedToFocusPath(const FFocusEvent& InFocusEvent)
{
	Super::NativeOnAddedToFocusPath(InFocusEvent);
	OnRowFocused.Broadcast(this);
}

#undef LOCTEXT_NAMESPACE
