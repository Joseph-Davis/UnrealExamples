// Copyright 2026 CodeLikeMe. All Rights Reserved.

#include "Screens/ALKMenuListScreen.h"
#include "ALKMenuSubsystem.h"
#include "ALKTheme.h"
#include "Widgets/ALKButton.h"
#include "Widgets/ALKDecor.h"
#include "Widgets/ALKFrame.h"
#include "Widgets/ALKTitleText.h"
#include "Components/SizeBox.h"
#include "Widgets/ALKWidgetBuilder.h"
#include "Blueprint/WidgetTree.h"
#include "Components/BackgroundBlur.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ALKMenuListScreen)

UALKMenuListScreen::UALKMenuListScreen(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UALKMenuListScreen::NativeOnLayoutReady()
{
	Super::NativeOnLayoutReady();
	RebuildEntries();
}

void UALKMenuListScreen::NativeConstruct()
{
	Super::NativeConstruct();

	// Listens while on screen: pooled screens are removed and added again.
	if (UALKMenuSubsystem* MenuSubsystem = GetMenuSubsystem(); MenuSubsystem && !EntriesChangedHandle.IsValid())
	{
		EntriesChangedHandle = MenuSubsystem->OnMenuEntriesChanged.AddUObject(this, &ThisClass::RefreshEntryStates);
	}
}

void UALKMenuListScreen::NativeOnActivated()
{
	// Before the base class hands out focus, so it goes to the first entry that can be used.
	RefreshEntryStates();
	Super::NativeOnActivated();
}

void UALKMenuListScreen::NativeDestruct()
{
	if (UALKMenuSubsystem* MenuSubsystem = GetMenuSubsystem())
	{
		MenuSubsystem->OnMenuEntriesChanged.Remove(EntriesChangedHandle);
	}
	EntriesChangedHandle.Reset();
	Super::NativeDestruct();
}

void UALKMenuListScreen::RefreshEntryStates()
{
	const UALKMenuSubsystem* MenuSubsystem = GetMenuSubsystem();
	UALKButton* FirstEnabled = nullptr;
	bool bLostFocus = false;
	for (int32 Index = 0; Index < EntryButtons.Num(); ++Index)
	{
		UALKButton* Button = EntryButtons[Index];
		if (!Button)
		{
			continue;
		}
		const bool bEnabled = !MenuSubsystem || !DisplayedEntries.IsValidIndex(Index) || MenuSubsystem->IsMenuEntryEnabled(DisplayedEntries[Index]);
		if (Button->GetIsEnabled() != bEnabled)
		{
			bLostFocus |= !bEnabled && Button->HasAnyUserFocus();
			Button->SetIsEnabled(bEnabled);
		}
		if (bEnabled && !FirstEnabled)
		{
			FirstEnabled = Button;
		}
	}

	DefaultFocusWidget = FirstEnabled ? FirstEnabled : (EntryButtons.Num() > 0 ? EntryButtons[0].Get() : nullptr);
	if (bLostFocus && FirstEnabled)
	{
		FirstEnabled->SetFocus();
	}
}

TArray<FALKMenuEntry> UALKMenuListScreen::GetMenuEntries_Implementation() const
{
	return NativeGetConfigEntries();
}

void UALKMenuListScreen::RebuildEntries()
{
	if (!EntriesBox)
	{
		return;
	}

	EntriesBox->ClearChildren();
	EntryButtons.Reset();
	DisplayedEntries.Reset();

	for (const FALKMenuEntry& Entry : GetMenuEntries())
	{
		if (!Entry.bVisible)
		{
			continue;
		}

		UALKButton* Button = CreateMenuButton(Entry.Label, Entry.Style);
		if (!Button)
		{
			continue;
		}
		Button->ButtonId = Entry.Id;
		Button->OnClicked().AddUObject(this, &ThisClass::HandleEntryClicked, DisplayedEntries.Num());
		EntriesBox->AddChild(Button);

		EntryButtons.Add(Button);
		DisplayedEntries.Add(Entry);
	}

	RefreshEntryStates();
	LayoutEntryButtons(*GetTheme());
}

void UALKMenuListScreen::NativeApplyTheme(const UALKTheme& Theme)
{
	Super::NativeApplyTheme(Theme);
	LayoutEntryButtons(Theme);
}

EHorizontalAlignment UALKMenuListScreen::GetEntryAlignment(const UALKTheme& Theme) const
{
	return Theme.MainMenuAlignment == HAlign_Fill ? HAlign_Left : Theme.MainMenuAlignment.GetValue();
}

void UALKMenuListScreen::BuildListLayout(UWidgetTree& Tree, bool bOverGame, const TArray<FALKInputHint>& Hints)
{
	bAlwaysOverGame = bOverGame;
	UOverlay* Root = FALKWidgetBuilder::Make<UOverlay>(Tree, TEXT("Root"));
	Tree.RootWidget = Root;
	// Over the game (the pause menu) the game shows through, blurred and dimmed.
	BackgroundImage = FALKWidgetBuilder::AddBackgroundLayers(Tree, Root, TEXT("BackgroundImage"), !bOverGame);

	// The header: an optional small label (see System Label) at the top of the screen.
	UVerticalBox* HeaderBox = FALKWidgetBuilder::Make<UVerticalBox>(Tree, TEXT("Header"));
	Header = HeaderBox;
	FALKWidgetBuilder::AddToOverlay(Root, HeaderBox, HAlign_Fill, VAlign_Top);
	SystemLabelText = FALKWidgetBuilder::MakeText(Tree, TEXT("SystemLabelText"));
	FALKWidgetBuilder::AddToVerticalBox(HeaderBox, SystemLabelText, FMargin(0.f, 0.f, 0.f, 10.f), HAlign_Left);

	// Over the game (the pause menu) the list sits on a panel.
	UVerticalBox* Column = FALKWidgetBuilder::Make<UVerticalBox>(Tree, TEXT("ContentColumn"));
	if (bOverGame)
	{
		Panel = FALKWidgetBuilder::Make<UALKFrame>(Tree, TEXT("Panel"));
		Panel->SetPadding(FMargin(44.f, 40.f, 44.f, 44.f));
		Panel->SetContent(Column);
		ContentColumn = Panel;
	}
	else
	{
		ContentColumn = Column;
	}
	FALKWidgetBuilder::AddToOverlay(Root, ContentColumn, HAlign_Left, VAlign_Center);

	LogoImage = FALKWidgetBuilder::Make<UImage>(Tree, TEXT("LogoImage"));
	LogoImage->SetVisibility(ESlateVisibility::Collapsed);
	FALKWidgetBuilder::AddToVerticalBox(Column, LogoImage, FMargin(), HAlign_Left);

	UALKTitleText* Title = FALKWidgetBuilder::Make<UALKTitleText>(Tree, TEXT("TitleText"));
	TitleText = Title;
	FALKWidgetBuilder::AddToVerticalBox(Column, TitleText, FMargin(), HAlign_Left);

	TitleRule = FALKWidgetBuilder::Make<UALKRule>(Tree, TEXT("TitleRule"));
	TitleRule->Thickness = 3.f;
	USizeBox* TitleRuleSize = FALKWidgetBuilder::WrapInSizeBox(Tree, TitleRule, 56.f, 0.f);
	FALKWidgetBuilder::AddToVerticalBox(Column, TitleRuleSize, FMargin(2.f, 10.f, 0.f, 0.f), HAlign_Left);

	SubtitleText = FALKWidgetBuilder::MakeText(Tree, TEXT("SubtitleText"));
	SubtitleText->SetVisibility(ESlateVisibility::Collapsed);
	FALKWidgetBuilder::AddToVerticalBox(Column, SubtitleText, FMargin(4.f, 10.f, 0.f, 0.f), HAlign_Left);

	UVerticalBox* Entries = FALKWidgetBuilder::Make<UVerticalBox>(Tree, TEXT("EntriesBox"));
	EntriesBox = Entries;
	FALKWidgetBuilder::AddToVerticalBox(Column, Entries, FMargin(0.f, 36.f, 0.f, 0.f), HAlign_Left);

	UVerticalBox* FooterColumn = FALKWidgetBuilder::Make<UVerticalBox>(Tree, TEXT("FooterColumn"));
	Footer = FooterColumn;
	FALKWidgetBuilder::AddToOverlay(Root, FooterColumn, HAlign_Fill, VAlign_Bottom);
	UALKRule* FooterRule = FALKWidgetBuilder::Make<UALKRule>(Tree, TEXT("FooterRule"));
	Rules.Add(FooterRule);
	FALKWidgetBuilder::AddToVerticalBox(FooterColumn, FooterRule, FMargin(0.f, 0.f, 0.f, 16.f));
	UHorizontalBox* FooterBox = FALKWidgetBuilder::Make<UHorizontalBox>(Tree, TEXT("Footer"));
	FALKWidgetBuilder::AddToVerticalBox(FooterColumn, FooterBox);
	UHorizontalBox* HintRow = BuildInputHints(Tree, Hints);
	FALKWidgetBuilder::AddToHorizontalBox(FooterBox, HintRow, FMargin(), HAlign_Left, VAlign_Center, 1.f);
	FooterNote = FALKWidgetBuilder::MakeText(Tree, TEXT("FooterNote"));
	FooterNote->SetVisibility(ESlateVisibility::Collapsed);
	FALKWidgetBuilder::AddToHorizontalBox(FooterBox, FooterNote, FMargin(), HAlign_Right, VAlign_Center);
}

void UALKMenuListScreen::SetListTexts(const FText& Title, const FText& Subtitle, const FText& Note, const FSlateBrush* Logo, const FText& LabelName)
{
	if (SystemLabelText)
	{
		SystemLabelText->SetText(MakeSystemLabel(SystemCode, LabelName.IsEmpty() ? Title : LabelName));
	}
	const bool bUseLogo = Logo && Logo->GetResourceObject() && LogoImage;
	if (LogoImage)
	{
		if (bUseLogo)
		{
			LogoImage->SetBrush(*Logo);
		}
		LogoImage->SetVisibility(bUseLogo ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (TitleText)
	{
		TitleText->SetText(Title);
		TitleText->SetVisibility(bUseLogo || Title.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
	if (SubtitleText)
	{
		SubtitleText->SetText(Subtitle);
		SubtitleText->SetVisibility(Subtitle.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
	if (FooterNote)
	{
		FooterNote->SetText(Note);
		FooterNote->SetVisibility(Note.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
}

void UALKMenuListScreen::StyleListLayout(const UALKTheme& Theme, float BackgroundOpacity)
{
	StyleScreenBackground(Theme, BackgroundOpacity);

	FALKWidgetBuilder::StyleText(TitleText, Theme, TitleRole);
	FALKWidgetBuilder::StyleText(SubtitleText, Theme, EALKTextRole::Data);
	FALKWidgetBuilder::StyleText(FooterNote, Theme, EALKTextRole::Data);
	FALKWidgetBuilder::StyleText(SystemLabelText, Theme, EALKTextRole::Data, Theme.GetAccentTextColor());
	if (SystemLabelText)
	{
		SystemLabelText->SetVisibility(SystemLabelText->GetText().IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
	for (UALKRule* Rule : Rules)
	{
		if (Rule)
		{
			Rule->SetRuleColor(Theme.Separator);
		}
	}
	if (TitleRule)
	{
		TitleRule->SetRuleColor(Theme.Accent);
	}
	if (Panel)
	{
		Panel->SetFrameStyle(Theme.MakePanelFrame());
	}
	if (UOverlaySlot* HeaderSlot = Header ? Cast<UOverlaySlot>(Header->Slot) : nullptr)
	{
		HeaderSlot->SetPadding(FMargin(Theme.ScreenPadding.Left * 0.5f, FMath::Max(28.f, Theme.ScreenPadding.Top * 0.42f), Theme.ScreenPadding.Right * 0.5f, 0.f));
	}

	const EHorizontalAlignment Alignment = GetEntryAlignment(Theme);
	const ETextJustify::Type Justify = Alignment == HAlign_Center ? ETextJustify::Center : (Alignment == HAlign_Right ? ETextJustify::Right : ETextJustify::Left);
	if (UOverlaySlot* ColumnSlot = ContentColumn ? Cast<UOverlaySlot>(ContentColumn->Slot) : nullptr)
	{
		ColumnSlot->SetHorizontalAlignment(Alignment);
		ColumnSlot->SetPadding(Theme.ScreenPadding);
	}
	const TArray<UWidget*> AlignedChildren = { LogoImage, TitleText, SubtitleText, EntriesBox };
	for (UWidget* Child : AlignedChildren)
	{
		if (UVerticalBoxSlot* ChildSlot = Child ? Cast<UVerticalBoxSlot>(Child->Slot) : nullptr)
		{
			ChildSlot->SetHorizontalAlignment(Alignment);
		}
	}
	for (UTextBlock* Text : { TitleText.Get(), SubtitleText.Get() })
	{
		if (Text)
		{
			Text->SetJustification(Justify);
		}
	}
	if (UOverlaySlot* FooterSlot = Footer ? Cast<UOverlaySlot>(Footer->Slot) : nullptr)
	{
		FooterSlot->SetPadding(FMargin(Theme.ScreenPadding.Left * 0.5f, 0.f, Theme.ScreenPadding.Right * 0.5f, FMath::Max(24.f, Theme.ScreenPadding.Bottom * 0.4f)));
	}
}

void UALKMenuListScreen::SetListOpacity(float Opacity)
{
	for (UWidget* Part : { ContentColumn.Get(), Header.Get(), Footer.Get() })
	{
		if (Part)
		{
			Part->SetRenderOpacity(Opacity);
		}
	}
}

void UALKMenuListScreen::LayoutEntryButtons(const UALKTheme& Theme)
{
	const EHorizontalAlignment Alignment = GetEntryAlignment(Theme);
	for (int32 Index = 0; Index < EntryButtons.Num(); ++Index)
	{
		if (UVerticalBoxSlot* EntrySlot = Cast<UVerticalBoxSlot>(EntryButtons[Index]->Slot))
		{
			EntrySlot->SetPadding(FMargin(0.f, Index == 0 ? 0.f : Theme.MenuButtonSpacing, 0.f, 0.f));
			EntrySlot->SetHorizontalAlignment(Alignment);
		}
	}
}

UALKButton* UALKMenuListScreen::GetEntryButton(FName EntryId) const
{
	for (UALKButton* Button : EntryButtons)
	{
		if (Button && Button->ButtonId == EntryId)
		{
			return Button;
		}
	}
	return nullptr;
}

void UALKMenuListScreen::HandleEntryClicked(int32 EntryIndex)
{
	if (DisplayedEntries.IsValidIndex(EntryIndex))
	{
		OnEntryClicked(DisplayedEntries[EntryIndex]);
	}
}

void UALKMenuListScreen::OnEntryClicked_Implementation(const FALKMenuEntry& Entry)
{
	if (UALKMenuSubsystem* MenuSubsystem = GetMenuSubsystem())
	{
		MenuSubsystem->ExecuteMenuEntry(Entry);
	}
}
