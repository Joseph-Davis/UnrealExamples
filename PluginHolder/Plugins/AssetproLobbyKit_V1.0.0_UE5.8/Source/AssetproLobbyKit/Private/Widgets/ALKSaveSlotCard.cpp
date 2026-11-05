// Copyright 2026 CodeLikeMe. All Rights Reserved.

#include "Widgets/ALKSaveSlotCard.h"
#include "ALKDeveloperSettings.h"
#include "ALKMenuConfig.h"
#include "ALKTheme.h"
#include "Widgets/ALKFrame.h"
#include "Widgets/ALKTitleText.h"
#include "Widgets/ALKWidgetBuilder.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/ScaleBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Engine/Texture2D.h"
#include "Misc/PackageName.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ALKSaveSlotCard)

#define LOCTEXT_NAMESPACE "AssetproLobbyKit"

namespace ALKSaveSlotCardPrivate
{
	static constexpr float ThumbWidth = 144.f;
	static constexpr float ThumbHeight = 81.f;
	static constexpr int32 CardHeight = 104;
}

UALKSaveSlotCard::UALKSaveSlotCard(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// A menu entry's frame and accent bar when focused.
	ButtonStyleType = EALKButtonStyle::Primary;
}

void UALKSaveSlotCard::NativeBuildDefaultLayout(UWidgetTree& Tree)
{
	using namespace ALKSaveSlotCardPrivate;

	UALKFrame* Frame = FALKWidgetBuilder::Make<UALKFrame>(Tree, TEXT("Background"));
	Frame->SetPadding(FMargin(18.f, 11.f, 22.f, 11.f));
	Frame->SetHorizontalAlignment(HAlign_Fill);
	Frame->SetVerticalAlignment(VAlign_Fill);
	Background = Frame;
	Tree.RootWidget = Frame;

	UHorizontalBox* Row = FALKWidgetBuilder::Make<UHorizontalBox>(Tree, TEXT("CardRow"));
	Frame->SetContent(Row);

	// The screenshot, or an empty frame with "+" / "--".
	UOverlay* Thumb = FALKWidgetBuilder::Make<UOverlay>(Tree, TEXT("Thumb"));
	UImage* ThumbBack = FALKWidgetBuilder::Make<UImage>(Tree, TEXT("ThumbBack"));
	FALKWidgetBuilder::AddToOverlay(Thumb, ThumbBack);
	UScaleBox* ThumbScale = FALKWidgetBuilder::Make<UScaleBox>(Tree, TEXT("ThumbScale"));
	ThumbScale->SetStretch(EStretch::ScaleToFill);
	ThumbImage = FALKWidgetBuilder::Make<UImage>(Tree, TEXT("ThumbImage"));
	ThumbScale->AddChild(ThumbImage);
	FALKWidgetBuilder::AddToOverlay(Thumb, ThumbScale);
	ThumbEmpty = FALKWidgetBuilder::MakeText(Tree, TEXT("ThumbEmpty"), FText::FromString(TEXT("+")));
	ThumbEmpty->SetJustification(ETextJustify::Center);
	FALKWidgetBuilder::AddToOverlay(Thumb, ThumbEmpty, HAlign_Center, VAlign_Center);
	UALKFrame* ThumbFrame = FALKWidgetBuilder::Make<UALKFrame>(Tree, TEXT("ThumbFrame"));
	ThumbFrame->SetVisibility(ESlateVisibility::HitTestInvisible);
	FALKWidgetBuilder::AddToOverlay(Thumb, ThumbFrame);
	FALKWidgetBuilder::AddToHorizontalBox(Row, FALKWidgetBuilder::WrapInSizeBox(Tree, Thumb, ThumbWidth, ThumbHeight), FMargin(0.f, 0.f, 22.f, 0.f), HAlign_Left, VAlign_Center);

	UVerticalBox* Texts = FALKWidgetBuilder::Make<UVerticalBox>(Tree, TEXT("CardText"));
	FALKWidgetBuilder::AddToHorizontalBox(Row, Texts, FMargin(), HAlign_Fill, VAlign_Center, 1.f);

	UALKFrame* Tag = FALKWidgetBuilder::Make<UALKFrame>(Tree, TEXT("TagFrame"));
	Tag->SetPadding(FMargin(8.f, 1.f, 8.f, 2.f));
	TagFrame = Tag;
	TagText = FALKWidgetBuilder::MakeText(Tree, TEXT("TagText"));
	Tag->SetContent(TagText);
	FALKWidgetBuilder::AddToVerticalBox(Texts, Tag, FMargin(), HAlign_Left);

	Label = FALKWidgetBuilder::Make<UALKTitleText>(Tree, TEXT("Label"));
	FALKWidgetBuilder::AddToVerticalBox(Texts, Label, FMargin(0.f, 5.f, 0.f, 0.f), HAlign_Left);

	InfoText = FALKWidgetBuilder::MakeText(Tree, TEXT("InfoText"));
	FALKWidgetBuilder::AddToVerticalBox(Texts, InfoText, FMargin(0.f, 4.f, 0.f, 0.f), HAlign_Left);

	UVerticalBox* Right = FALKWidgetBuilder::Make<UVerticalBox>(Tree, TEXT("CardRight"));
	FALKWidgetBuilder::AddToHorizontalBox(Row, Right, FMargin(18.f, 0.f, 50.f, 0.f), HAlign_Right, VAlign_Center);
	NumberText = FALKWidgetBuilder::MakeText(Tree, TEXT("NumberText"));
	NumberText->SetJustification(ETextJustify::Right);
	FALKWidgetBuilder::AddToVerticalBox(Right, NumberText, FMargin(), HAlign_Right);
	AgoText = FALKWidgetBuilder::MakeText(Tree, TEXT("AgoText"));
	AgoText->SetJustification(ETextJustify::Right);
	FALKWidgetBuilder::AddToVerticalBox(Right, AgoText, FMargin(0.f, 2.f, 0.f, 0.f), HAlign_Right);
}

void UALKSaveSlotCard::SetSave(const FALKSaveSlotInfo& InInfo, UTexture2D* InThumbnail, bool bInReadOnly)
{
	Info = InInfo;
	Thumbnail = InThumbnail;
	bNewSave = false;
	bReadOnly = bInReadOnly;
	SetIsEnabled(!bReadOnly);
	RefreshContent();
}

void UALKSaveSlotCard::SetNewSave(int32 FreeSlots, int32 MaxSlots)
{
	Info = FALKSaveSlotInfo();
	Thumbnail = nullptr;
	bNewSave = true;
	bReadOnly = false;
	FreeSlotCount = FreeSlots;
	MaxSlotCount = MaxSlots;
	SetIsEnabled(true);
	RefreshContent();
}

FText UALKSaveSlotCard::GetSlotLabel(const FALKSaveSlotInfo& InInfo)
{
	switch (InInfo.Type)
	{
	case EALKSaveSlotType::Quick: return LOCTEXT("SlotQuick", "Quicksave");
	case EALKSaveSlotType::Auto: return FText::Format(LOCTEXT("SlotAuto", "Autosave {0}"), FText::AsNumber(InInfo.Number));
	default: return FText::Format(LOCTEXT("SlotManual", "Manual {0}"), FText::FromString(FString::Printf(TEXT("%02d"), InInfo.Number)));
	}
}

FText UALKSaveSlotCard::FormatPlaytime(float Seconds)
{
	const int32 Total = FMath::Max(0, FMath::FloorToInt(Seconds));
	return FText::FromString(FString::Printf(TEXT("%02d:%02d:%02d"), Total / 3600, (Total / 60) % 60, Total % 60));
}

FText UALKSaveSlotCard::FormatDate(const FDateTime& UtcTime)
{
	const FDateTime Local = UtcTime + (FDateTime::Now() - FDateTime::UtcNow());
	return FText::FromString(FString::Printf(TEXT("%04d.%02d.%02d  %02d:%02d"), Local.GetYear(), Local.GetMonth(), Local.GetDay(), Local.GetHour(), Local.GetMinute()));
}

FText UALKSaveSlotCard::FormatAge(const FDateTime& UtcTime)
{
	const FTimespan Age = FDateTime::UtcNow() - UtcTime;
	if (Age.GetTotalMinutes() < 1.0)
	{
		return LOCTEXT("AgeNow", "Just now");
	}
	if (Age.GetTotalHours() < 24.0)
	{
		return FText::Format(LOCTEXT("AgeHours", "{0} ago"), FText::FromString(FString::Printf(TEXT("%02d:%02d"), Age.GetHours(), Age.GetMinutes())));
	}
	const int32 Days = FMath::FloorToInt(Age.GetTotalDays());
	return Days <= 1 ? LOCTEXT("AgeYesterday", "Yesterday") : FText::Format(LOCTEXT("AgeDays", "{0} days ago"), FText::AsNumber(Days));
}

FText UALKSaveSlotCard::GetChapterLine(const FALKSaveSlotInfo& InInfo)
{
	const UALKMenuConfig* Config = UALKDeveloperSettings::GetMenuConfig();
	const int32 ChapterIndex = Config->Chapters.IndexOfByPredicate([&InInfo](const FALKChapter& Chapter) { return Chapter.Id == InInfo.ChapterId; });
	if (Config->Chapters.IsValidIndex(ChapterIndex))
	{
		// The chapter's current name (and translation), numbered like the chapter select.
		const FText Name = Config->Chapters[ChapterIndex].Name;
		return Config->bNumberChapters
			? FText::Format(LOCTEXT("SaveChapterLine", "Chapter {0}  \u00b7  {1}"), FText::AsNumber(ChapterIndex + 1), Name)
			: Name;
	}
	if (!InInfo.ChapterName.IsEmpty())
	{
		return InInfo.ChapterName;
	}
	// No chapter: the level's name.
	return FText::FromString(FPackageName::GetShortName(InInfo.LevelName));
}

void UALKSaveSlotCard::RefreshContent()
{
	const UALKMenuConfig* Config = UALKDeveloperSettings::GetMenuConfig();
	if (bNewSave)
	{
		SetButtonText(LOCTEXT("NewSave", "New Save"));
		if (InfoText)
		{
			InfoText->SetText(FText::Format(LOCTEXT("NewSaveInfo", "Empty slot  \u00b7  {0} of {1} free"), FText::AsNumber(FreeSlotCount), FText::AsNumber(MaxSlotCount)));
		}
		if (TagFrame)
		{
			TagFrame->SetVisibility(ESlateVisibility::Collapsed);
		}
		if (NumberText)
		{
			NumberText->SetText(FText::GetEmpty());
		}
		if (AgoText)
		{
			AgoText->SetText(FText::GetEmpty());
		}
	}
	else
	{
		SetButtonText(GetChapterLine(Info));
		if (InfoText)
		{
			InfoText->SetText(FText::Format(LOCTEXT("SaveCardInfo", "{0} played  \u00b7  {1}"), FormatPlaytime(Info.PlaytimeSeconds), FormatDate(Info.SavedAt)));
		}
		if (TagFrame)
		{
			TagFrame->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		if (TagText)
		{
			TagText->SetText(bReadOnly ? FText::Format(LOCTEXT("SlotReadOnly", "{0}  \u00b7  Read only"), GetSlotLabel(Info)) : GetSlotLabel(Info));
		}
		if (NumberText)
		{
			const int32 ChapterIndex = Config->Chapters.IndexOfByPredicate([this](const FALKChapter& Chapter) { return Chapter.Id == Info.ChapterId; });
			NumberText->SetText(ChapterIndex != INDEX_NONE ? FText::FromString(FString::Printf(TEXT("%02d"), ChapterIndex + 1)) : FText::GetEmpty());
		}
		if (AgoText)
		{
			AgoText->SetText(FormatAge(Info.SavedAt));
		}
	}
	if (ThumbImage)
	{
		if (Thumbnail)
		{
			ThumbImage->SetBrush(FALKWidgetBuilder::MakeTextureBrush(Thumbnail));
		}
		ThumbImage->SetVisibility(Thumbnail ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (ThumbEmpty)
	{
		ThumbEmpty->SetText(FText::FromString(bNewSave ? TEXT("+") : TEXT("--")));
		ThumbEmpty->SetVisibility(Thumbnail ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
	if (const UALKTheme* Theme = UALKDeveloperSettings::GetTheme())
	{
		NativeApplyVisualState(*Theme, GetHighlight(), IsUsable());
	}
}

void UALKSaveSlotCard::NativeApplyTheme(const UALKTheme& Theme)
{
	Super::NativeApplyTheme(Theme);
	using namespace ALKSaveSlotCardPrivate;

	SetMinDimensions(0, CardHeight);
	if (Label)
	{
		// The chapter line: a heading, smaller than a menu entry.
		FSlateFontInfo LineFont = Theme.HeadingFont;
		LineFont.Size = FMath::Max(12.f, FMath::RoundToFloat(Theme.HeadingFont.Size * 0.42f));
		FALKWidgetBuilder::SetTextFont(Label, LineFont);
	}
	FALKWidgetBuilder::StyleText(InfoText, Theme, EALKTextRole::Data, Theme.TextMuted);
	FALKWidgetBuilder::StyleText(AgoText, Theme, EALKTextRole::Data, Theme.Accent);
	if (TagText)
	{
		FALKWidgetBuilder::StyleText(TagText, Theme, EALKTextRole::Data);
		FSlateFontInfo TagFont = Theme.DataFont;
		TagFont.Size = FMath::Max(8.f, TagFont.Size - 2.f);
		FALKWidgetBuilder::SetTextFont(TagText, TagFont);
		TagText->SetTextTransformPolicy(ETextTransformPolicy::ToUpper);
	}
	for (UTextBlock* Upper : { InfoText.Get(), AgoText.Get() })
	{
		if (Upper)
		{
			Upper->SetTextTransformPolicy(ETextTransformPolicy::ToUpper);
		}
	}
	if (NumberText)
	{
		FALKWidgetBuilder::StyleText(NumberText, Theme, EALKTextRole::Heading);
		FSlateFontInfo NumberFont = Theme.HeadingFont;
		NumberFont.Size = FMath::Max(14.f, FMath::RoundToFloat(Theme.HeadingFont.Size * 0.62f));
		FALKWidgetBuilder::SetTextFont(NumberText, NumberFont);
	}
	if (ThumbEmpty)
	{
		FALKWidgetBuilder::StyleText(ThumbEmpty, Theme, EALKTextRole::Heading, Theme.TextMuted);
	}
	if (WidgetTree)
	{
		if (UImage* ThumbBack = Cast<UImage>(WidgetTree->FindWidget(TEXT("ThumbBack"))))
		{
			ThumbBack->SetBrush(UALKTheme::MakeSolidBrush(FMath::Lerp(Theme.BackgroundTop, Theme.BackgroundBottom, 0.6f)));
		}
	}
	RefreshContent();
}

void UALKSaveSlotCard::NativeApplyVisualState(const UALKTheme& Theme, float Highlight, bool bEnabled)
{
	// Where the focused line is filled with the accent colour (Focus Style: Solid), the small print on it turns to the
	// contrast colour with the focus: the accent and the muted colour would vanish into the fill.
	const bool bSolidFocus = Theme.FocusStyle == EALKFocusStyle::Solid;
	auto OnFocus = [&Theme, bSolidFocus, Highlight](const FLinearColor& Rest)
	{
		return bSolidFocus ? FMath::Lerp(Rest, Theme.AccentContrast.CopyWithNewOpacity(Rest.A), Highlight) : Rest;
	};
	// The accent as it reads as text on the focused line.
	const FLinearColor FocusAccent = bSolidFocus ? Theme.AccentContrast : Theme.GetAccentTextColor();

	// Autosaves and the quick save are tagged in the second accent, manual saves in the accent; the quick save's tag is solid.
	const bool bSystemSlot = Info.Type != EALKSaveSlotType::Manual;
	const FLinearColor TagColor = (bSystemSlot ? Theme.AccentAlt : Theme.Accent).CopyWithNewOpacity(bEnabled ? 1.f : 0.45f);
	const bool bSolid = Info.Type == EALKSaveSlotType::Quick && bEnabled;
	const FLinearColor TagLine = bSolid ? TagColor : OnFocus(TagColor);
	if (UALKFrame* Tag = Cast<UALKFrame>(TagFrame))
	{
		Tag->SetFrameStyle(Theme.MakeChipFrame(bSolid ? TagColor : FLinearColor::Transparent, TagLine));
	}
	if (TagText)
	{
		TagText->SetColorAndOpacity(FSlateColor(bSolid ? Theme.AccentContrast : TagLine));
	}
	if (InfoText)
	{
		InfoText->SetColorAndOpacity(FSlateColor(OnFocus(Theme.TextMuted)));
	}
	if (NumberText)
	{
		NumberText->SetColorAndOpacity(FSlateColor(FMath::Lerp(Theme.TextMuted, FocusAccent, Highlight).CopyWithNewOpacity(bEnabled ? 0.9f : 0.4f)));
	}
	if (AgoText)
	{
		AgoText->SetColorAndOpacity(FSlateColor(FocusAccent));
		AgoText->SetVisibility(Highlight > 0.5f && !bNewSave ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}
	if (ThumbImage)
	{
		ThumbImage->SetRenderOpacity(bEnabled ? 1.f : 0.5f);
	}
	if (WidgetTree)
	{
		if (UALKFrame* ThumbFrame = Cast<UALKFrame>(WidgetTree->FindWidget(TEXT("ThumbFrame"))))
		{
			FALKFrameStyle Outline;
			Outline.Outline = Theme.PanelOutline;
			Outline.OutlineWidth = FMath::Min(Theme.OutlineWidth, 2.f);
			Outline.Radius = FMath::Min(Theme.CornerRadius, 6.f);
			// The empty frame of a new save is filled in the raised colour.
			Outline.Fill = bNewSave ? Theme.PanelRaised : FLinearColor::Transparent;
			ThumbFrame->SetFrameStyle(Outline);
		}
	}
}

void UALKSaveSlotCard::NativeOnAddedToFocusPath(const FFocusEvent& InFocusEvent)
{
	Super::NativeOnAddedToFocusPath(InFocusEvent);
	OnCardFocused.Broadcast(this);
}

#undef LOCTEXT_NAMESPACE
