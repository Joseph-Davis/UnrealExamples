// Copyright 2026 CodeLikeMe. All Rights Reserved.

#include "Screens/ALKSettingsScreen.h"
#include "Widgets/ALKFrame.h"
#include "Widgets/ALKTitleText.h"
#include "ALKMenuConfig.h"
#include "ALKMenuSubsystem.h"
#include "ALKTheme.h"
#include "Settings/ALKSettingRows.h"
#include "Settings/ALKSettingsSubsystem.h"
#include "Widgets/ALKButton.h"
#include "Widgets/ALKWidgetBuilder.h"
#include "Blueprint/WidgetNavigation.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "GameFramework/GameUserSettings.h"
#include "HAL/PlatformInput.h"
#include "Styling/SlateTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ALKSettingsScreen)

#define LOCTEXT_NAMESPACE "AssetproLobbyKit"

UALKSettingsScreen::UALKSettingsScreen(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bDimScene = true;
}

UALKSettingsSubsystem* UALKSettingsScreen::GetSettingsSubsystem() const
{
	const UGameInstance* GameInstance = GetGameInstance();
	return GameInstance ? GameInstance->GetSubsystem<UALKSettingsSubsystem>() : nullptr;
}

void UALKSettingsScreen::NativeBuildDefaultLayout(UWidgetTree& Tree)
{
	UOverlay* Root = FALKWidgetBuilder::Make<UOverlay>(Tree, TEXT("Root"));
	Tree.RootWidget = Root;
	BackgroundImage = FALKWidgetBuilder::AddBackgroundLayers(Tree, Root);

	UVerticalBox* Main = FALKWidgetBuilder::Make<UVerticalBox>(Tree, TEXT("MainColumn"));
	FALKWidgetBuilder::AddToOverlay(Root, Main);

	SystemLabelText = FALKWidgetBuilder::MakeText(Tree, TEXT("SystemLabelText"));
	FALKWidgetBuilder::AddToVerticalBox(Main, SystemLabelText, FMargin(0.f, 0.f, 0.f, 6.f), HAlign_Left);

	UALKTitleText* Title = FALKWidgetBuilder::Make<UALKTitleText>(Tree, TEXT("TitleText"));
	Title->SetText(LOCTEXT("SettingsTitle", "Settings"));
	TitleText = Title;
	FALKWidgetBuilder::AddToVerticalBox(Main, TitleText, FMargin(), HAlign_Left);

	// Tab row: [Q] Tab Tab Tab [E]
	UHorizontalBox* TabRowBox = FALKWidgetBuilder::Make<UHorizontalBox>(Tree, TEXT("TabRow"));
	TabRow = TabRowBox;
	FALKWidgetBuilder::AddToVerticalBox(Main, TabRowBox, FMargin(0.f, 22.f, 0.f, 0.f), HAlign_Left);

	UHorizontalBox* PrevCap = BuildInputHints(Tree, { FALKInputHint(LOCTEXT("KeyQ", "Q"), EKeys::Gamepad_LeftShoulder, FText::GetEmpty())
		.WithAction(FSimpleDelegate::CreateUObject(this, &ThisClass::ShowPreviousTab)) });
	FALKWidgetBuilder::AddToHorizontalBox(TabRowBox, PrevCap, FMargin(0.f, 0.f, 16.f, 0.f));

	UHorizontalBox* Tabs = FALKWidgetBuilder::Make<UHorizontalBox>(Tree, TEXT("TabBox"));
	TabBox = Tabs;
	FALKWidgetBuilder::AddToHorizontalBox(TabRowBox, Tabs);

	UHorizontalBox* NextCap = BuildInputHints(Tree, { FALKInputHint(LOCTEXT("KeyE", "E"), EKeys::Gamepad_RightShoulder, FText::GetEmpty())
		.WithAction(FSimpleDelegate::CreateUObject(this, &ThisClass::ShowNextTab)) });
	FALKWidgetBuilder::AddToHorizontalBox(TabRowBox, NextCap, FMargin(16.f, 0.f, 0.f, 0.f));

	Separator = FALKWidgetBuilder::Make<UImage>(Tree, TEXT("Separator"));
	FALKWidgetBuilder::AddToVerticalBox(Main, FALKWidgetBuilder::WrapInSizeBox(Tree, Separator, 0.f, 1.f), FMargin(0.f, 14.f, 0.f, 0.f));

	// Body: the list on a panel | the description on a panel
	UHorizontalBox* Body = FALKWidgetBuilder::Make<UHorizontalBox>(Tree, TEXT("Body"));
	FALKWidgetBuilder::AddToVerticalBox(Main, Body, FMargin(0.f, 24.f, 0.f, 0.f), HAlign_Fill, VAlign_Fill, 1.f);

	ListPanel = FALKWidgetBuilder::Make<UALKFrame>(Tree, TEXT("ListPanel"));
	ListPanel->SetPadding(FMargin(16.f, 22.f, 18.f, 22.f));
	FALKWidgetBuilder::AddToHorizontalBox(Body, ListPanel, FMargin(), HAlign_Fill, VAlign_Fill, 0.62f);

	SettingsList = FALKWidgetBuilder::Make<UScrollBox>(Tree, TEXT("SettingsList"));
	SettingsList->SetAnimateWheelScrolling(true);
	ListPanel->SetContent(SettingsList);

	DescriptionPanel = FALKWidgetBuilder::Make<UALKFrame>(Tree, TEXT("DescriptionPanel"));
	DescriptionPanel->SetPadding(FMargin(28.f, 52.f, 28.f, 30.f));
	USizeBox* DescriptionSize = FALKWidgetBuilder::Make<USizeBox>(Tree, TEXT("DescriptionSize"));
	DescriptionSize->SetContent(DescriptionPanel);
	FALKWidgetBuilder::AddToHorizontalBox(Body, DescriptionSize, FMargin(36.f, 0.f, 0.f, 0.f), HAlign_Fill, VAlign_Top, 0.38f);

	UVerticalBox* DescriptionColumn = FALKWidgetBuilder::Make<UVerticalBox>(Tree, TEXT("DescriptionColumn"));
	DescriptionPanel->SetContent(DescriptionColumn);

	DescriptionTitle = FALKWidgetBuilder::MakeText(Tree, TEXT("DescriptionTitle"));
	DescriptionTitle->SetAutoWrapText(true);
	FALKWidgetBuilder::AddToVerticalBox(DescriptionColumn, DescriptionTitle);

	DescriptionRef = FALKWidgetBuilder::MakeText(Tree, TEXT("DescriptionRef"));
	FALKWidgetBuilder::AddToVerticalBox(DescriptionColumn, DescriptionRef, FMargin(0.f, 6.f, 0.f, 0.f));

	DescriptionText = FALKWidgetBuilder::MakeText(Tree, TEXT("DescriptionText"));
	DescriptionText->SetAutoWrapText(true);
	FALKWidgetBuilder::AddToVerticalBox(DescriptionColumn, DescriptionText, FMargin(0.f, 18.f, 0.f, 0.f));

	// Footer: [B] Back  [Y] Reset to Defaults  Apply (mouse)  [< >] Change
	UHorizontalBox* Footer = FALKWidgetBuilder::Make<UHorizontalBox>(Tree, TEXT("Footer"));
	FALKWidgetBuilder::AddToVerticalBox(Main, Footer, FMargin(0.f, 24.f, 0.f, 0.f));

	TArray<UALKButton*> FooterButtons;
	UHorizontalBox* Hints = BuildInputHints(Tree, {
		FALKInputHint(LOCTEXT("KeyEsc", "Esc"), FPlatformInput::GetGamepadBackKey(), LOCTEXT("HintBack", "Back"))
			.WithAction(FSimpleDelegate::CreateUObject(this, &ThisClass::RequestClose)),
		FALKInputHint(FText::GetEmpty(), EKeys::Gamepad_FaceButton_Top, LOCTEXT("ResetDefaults", "Reset to Defaults"))
			.WithAction(FSimpleDelegate::CreateUObject(this, &ThisClass::ResetActiveTabToDefaults)),
		FALKInputHint(FText::GetEmpty(), FText::GetEmpty(), LOCTEXT("Apply", "Apply"))
			.WithAction(FSimpleDelegate::CreateUObject(this, &ThisClass::ApplyChanges)),
		FALKInputHint(LOCTEXT("KeyBrowse", "< >"), LOCTEXT("PadBrowse", "D-Pad"), LOCTEXT("HintChange", "Change"))
	}, &FooterButtons);
	FALKWidgetBuilder::AddToHorizontalBox(Footer, Hints, FMargin(), HAlign_Left, VAlign_Center, 1.f);
	if (FooterButtons.Num() >= 3)
	{
		BackButton = FooterButtons[0];
		ResetButton = FooterButtons[1];
		ApplyButton = FooterButtons[2];
	}
}

void UALKSettingsScreen::NativeOnLayoutReady()
{
	Super::NativeOnLayoutReady();

	// Footer hints run their own action; designer buttons are bound here.
	if (ApplyButton && !HintButtons.Contains(ApplyButton))
	{
		ApplyButton->OnClicked().AddUObject(this, &ThisClass::ApplyChanges);
	}
	if (ResetButton && !HintButtons.Contains(ResetButton))
	{
		ResetButton->OnClicked().AddUObject(this, &ThisClass::ResetActiveTabToDefaults);
	}
	if (BackButton && !HintButtons.Contains(BackButton))
	{
		BackButton->OnClicked().AddUObject(this, &ThisClass::RequestClose);
	}

	// The footer buttons sit under the description panel, where Up would find nothing to move to.
	FCustomWidgetNavigationDelegate NavigateUp;
	NavigateUp.BindDynamic(this, &ThisClass::HandleFooterNavigateUp);
	for (UALKButton* FooterButton : { ResetButton.Get(), BackButton.Get(), ApplyButton.Get() })
	{
		if (FooterButton)
		{
			FooterButton->SetNavigationRuleCustom(EUINavigation::Up, NavigateUp);
		}
	}
}

UWidget* UALKSettingsScreen::HandleFooterNavigateUp(EUINavigation Direction)
{
	UALKSettingRow* Row = LastFocusedRow.Get();
	if (Row && Rows.Contains(Row))
	{
		return Row;
	}
	return Rows.Num() > 0 ? Rows.Last().Get() : nullptr;
}

void UALKSettingsScreen::NativeApplyTheme(const UALKTheme& Theme)
{
	Super::NativeApplyTheme(Theme);

	if (WidgetTree)
	{
		StyleScreenBackground(Theme);
		if (UWidget* Main = WidgetTree->FindWidget(TEXT("MainColumn")))
		{
			if (UOverlaySlot* MainSlot = Cast<UOverlaySlot>(Main->Slot))
			{
				MainSlot->SetPadding(Theme.ScreenPadding);
			}
		}
	}

	if (TitleText)
	{
		FALKWidgetBuilder::StyleText(TitleText, Theme, EALKTextRole::Heading);
		FALKWidgetBuilder::SetTextFont(TitleText, Theme.HeadingFont);
	}
	if (Separator)
	{
		Separator->SetBrush(UALKTheme::MakeSolidBrush(Theme.Separator));
	}
	FALKWidgetBuilder::StyleText(SystemLabelText, Theme, EALKTextRole::Data, Theme.Accent);
	if (SystemLabelText)
	{
		SystemLabelText->SetVisibility(SystemLabelText->GetText().IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
	FALKWidgetBuilder::StyleText(DescriptionRef, Theme, EALKTextRole::Data, Theme.GetAccentTextColor());
	if (DescriptionRef)
	{
		DescriptionRef->SetTextTransformPolicy(ETextTransformPolicy::ToUpper);
	}
	if (ListPanel)
	{
		ListPanel->SetFrameStyle(Theme.MakePanelFrame());
	}
	if (DescriptionPanel)
	{
		if (UALKFrame* DescriptionFrame = Cast<UALKFrame>(DescriptionPanel))
		{
			DescriptionFrame->SetFrameStyle(Theme.MakePanelFrame());
		}
		else
		{
			DescriptionPanel->SetBrush(UALKTheme::MakeSolidBrush(FLinearColor::Transparent));
		}
		if (USizeBox* DescriptionSize = Cast<USizeBox>(DescriptionPanel->GetParent()))
		{
			DescriptionSize->SetMaxDesiredWidth(Theme.SettingsDescriptionWidth);
			DescriptionSize->SetVisibility(Theme.SettingsDescriptionWidth > 0.f ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
		}
	}
	FALKWidgetBuilder::StyleText(DescriptionTitle, Theme, EALKTextRole::Heading, Theme.TextPrimary);
	if (DescriptionTitle)
	{
		FSlateFontInfo Font = Theme.HeadingFont;
		Font.Size = FMath::Max(12.f, FMath::RoundToFloat(Theme.HeadingFont.Size * 0.56f));
		FALKWidgetBuilder::SetTextFont(DescriptionTitle, Font);
	}
	FALKWidgetBuilder::StyleText(DescriptionText, Theme, EALKTextRole::Body);
	if (DescriptionText)
	{
		FSlateFontInfo Font = Theme.BodyFont;
		Font.Size = FMath::Max(10.f, Font.Size - 2.f);
		FALKWidgetBuilder::SetTextFont(DescriptionText, Font);
		DescriptionText->SetLineHeightPercentage(1.25f);
	}

	if (SettingsList)
	{
		// A thin accent thumb and a faint track, beside the list.
		FScrollBarStyle BarStyle = SettingsList->GetWidgetBarStyle();
		FSlateBrush Thumb = UALKTheme::MakeSolidBrush(Theme.Accent.CopyWithNewOpacity(0.85f));
		BarStyle.SetNormalThumbImage(Thumb);
		Thumb.TintColor = FSlateColor(Theme.TextPrimary);
		BarStyle.SetHoveredThumbImage(Thumb);
		Thumb.TintColor = FSlateColor(Theme.Accent);
		BarStyle.SetDraggedThumbImage(Thumb);
		BarStyle.SetVerticalBackgroundImage(UALKTheme::MakeSolidBrush(Theme.TextPrimary.CopyWithNewOpacity(0.06f)));
		BarStyle.SetVerticalTopSlotImage(FSlateNoResource());
		BarStyle.SetVerticalBottomSlotImage(FSlateNoResource());
		SettingsList->SetWidgetBarStyle(BarStyle);
		SettingsList->SetScrollbarThickness(FVector2D(3.0, 3.0));
		SettingsList->SetScrollbarPadding(FMargin(12.f, 0.f, 0.f, 0.f));
	}

	for (UTextBlock* Header : SectionHeaders)
	{
		StyleSectionHeader(Header, Theme);
	}
}

void UALKSettingsScreen::StyleSectionHeader(UTextBlock* Header, const UALKTheme& Theme) const
{
	// Small capitals in the muted text colour.
	FALKWidgetBuilder::StyleText(Header, Theme, EALKTextRole::Data, Theme.TextMuted);
	if (Header)
	{
		Header->SetTextTransformPolicy(ETextTransformPolicy::ToUpper);
	}
}

void UALKSettingsScreen::NativeOnActivated()
{
	if (UALKSettingsSubsystem* Settings = GetSettingsSubsystem())
	{
		AllSettings.Reset();
		for (UALKSetting* Setting : Settings->CreateSettings(this, GetOwningLocalPlayer()))
		{
			AllSettings.Add(Setting);
		}

		// Listen to the subsystem rather than the settings themselves, so its unsaved-changes flag is already updated.
		if (!SettingChangedHandle.IsValid())
		{
			SettingChangedHandle = Settings->OnSettingChangedNative.AddUObject(this, &ThisClass::HandleAnySettingChanged);
		}
	}

	RebuildTabs();
	SetActiveTab(FMath::Clamp(ActiveTab, 0, FMath::Max(0, TabCategories.Num() - 1)));
	UpdateFooter();

	Super::NativeOnActivated();
}

void UALKSettingsScreen::NativeOnDeactivated()
{
	if (UALKSettingsSubsystem* Settings = GetSettingsSubsystem())
	{
		Settings->OnSettingChangedNative.Remove(SettingChangedHandle);
	}
	SettingChangedHandle.Reset();
	bFocusFirstRowPending = false;
	Super::NativeOnDeactivated();
}

void UALKSettingsScreen::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	CapContentWidth(MyGeometry);

	// Focus the first row of a freshly built tab only once it has been laid out: CommonUI places the gamepad cursor at
	// the focused widget's centre, and a row without geometry yet would send the cursor (and gamepad A) elsewhere.
	if (bFocusFirstRowPending && Rows.Num() > 0 && Rows[0])
	{
		const FVector2D RowSize = Rows[0]->GetCachedGeometry().GetLocalSize();
		if (RowSize.X > 1.0 && RowSize.Y > 1.0)
		{
			bFocusFirstRowPending = false;
			Rows[0]->SetFocus();
		}
	}
}

TSubclassOf<UALKSettingRow> UALKSettingsScreen::GetRowClass(EALKSettingType Type) const
{
	const UALKMenuConfig* Config = GetMenuConfig();
	TSubclassOf<UALKSettingRow> RowClass;
	switch (Type)
	{
	case EALKSettingType::Options: RowClass = Config->OptionRowClass; break;
	case EALKSettingType::Slider: RowClass = Config->SliderRowClass; break;
	case EALKSettingType::KeyBinding: RowClass = Config->KeyBindingRowClass; break;
	case EALKSettingType::Action: RowClass = Config->ActionRowClass; break;
	}
	if (RowClass)
	{
		return RowClass;
	}
	switch (Type)
	{
	case EALKSettingType::Slider: return UALKSettingRow_Slider::StaticClass();
	case EALKSettingType::KeyBinding: return UALKSettingRow_KeyBinding::StaticClass();
	case EALKSettingType::Action: return UALKSettingRow_Action::StaticClass();
	case EALKSettingType::Options:
	default: return UALKSettingRow_Options::StaticClass();
	}
}

void UALKSettingsScreen::SetPage(const FText& Title, const TArray<EALKSettingsCategory>& Categories)
{
	bPageMode = true;
	PageTitle = Title;
	PageCategories = Categories;
	ActiveTab = 0;
	if (TitleText)
	{
		TitleText->SetText(PageTitle);
	}
	if (IsActivated())
	{
		RebuildTabs();
		SetActiveTab(0);
	}
}

void UALKSettingsScreen::SetTabbed()
{
	bPageMode = false;
	PageTitle = FText::GetEmpty();
	PageCategories.Reset();
	ActiveTab = 0;
	if (IsActivated())
	{
		RebuildTabs();
		SetActiveTab(0);
	}
}

void UALKSettingsScreen::RebuildTabs()
{
	const UALKMenuConfig* Config = GetMenuConfig();

	auto HasSettings = [this](EALKSettingsCategory Category)
	{
		return AllSettings.ContainsByPredicate([Category](const UALKSetting* Setting) { return Setting && Setting->Category == Category; });
	};

	TabCategories.Reset();
	if (bPageMode && PageCategories.Num() > 0)
	{
		for (EALKSettingsCategory Category : PageCategories)
		{
			if (!TabCategories.Contains(Category) && HasSettings(Category))
			{
				TabCategories.Add(Category);
			}
		}
	}
	else
	{
		for (const FALKSettingsTab& Tab : Config->SettingsTabs)
		{
			if (Tab.bVisible && !TabCategories.Contains(Tab.Category) && HasSettings(Tab.Category))
			{
				TabCategories.Add(Tab.Category);
			}
		}
	}

	// A page is one list: no tabs to switch.
	if (TabRow)
	{
		TabRow->SetVisibility(bPageMode ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	}
	if (Separator)
	{
		if (UWidget* SeparatorHolder = Separator->GetParent())
		{
			SeparatorHolder->SetVisibility(bPageMode ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
		}
	}
	if (TitleText)
	{
		TitleText->SetText(bPageMode ? PageTitle : LOCTEXT("SettingsTitle", "Settings"));
	}

	if (!TabBox)
	{
		return;
	}

	TabBox->ClearChildren();
	TabButtons.Reset();
	if (bPageMode)
	{
		return;
	}
	for (int32 Index = 0; Index < TabCategories.Num(); ++Index)
	{
		UALKButton* TabButton = CreateMenuButton(Config->GetTabDisplayName(TabCategories[Index]), EALKButtonStyle::Tab);
		TabButton->SetIsFocusable(false);
		TabButton->bFocusOnHover = false;
		TabButton->bPlayClickSound = false;
		TabButton->OnClicked().AddUObject(this, &ThisClass::SetActiveTab, Index);
		TabBox->AddChild(TabButton);
		if (UHorizontalBoxSlot* TabSlot = Cast<UHorizontalBoxSlot>(TabButton->Slot))
		{
			TabSlot->SetPadding(FMargin(Index == 0 ? 0.f : 6.f, 0.f, 0.f, 0.f));
			TabSlot->SetVerticalAlignment(VAlign_Center);
		}
		TabButtons.Add(TabButton);
	}
}

void UALKSettingsScreen::ShowPreviousTab()
{
	if (!bPageMode)
	{
		SetActiveTab(ActiveTab - 1);
	}
}

void UALKSettingsScreen::ShowNextTab()
{
	if (!bPageMode)
	{
		SetActiveTab(ActiveTab + 1);
	}
}

void UALKSettingsScreen::SetActiveTab(int32 TabIndex)
{
	if (TabCategories.Num() == 0)
	{
		return;
	}

	const bool bChanged = TabIndex != ActiveTab;
	ActiveTab = (TabIndex + TabCategories.Num()) % TabCategories.Num();
	for (int32 Index = 0; Index < TabButtons.Num(); ++Index)
	{
		TabButtons[Index]->SetActiveVisual(Index == ActiveTab);
	}

	RebuildList();

	if (bChanged && IsActivated())
	{
		PlayThemeSound(EALKUISound::Tab);
	}
	if (IsActivated() && Rows.Num() > 0)
	{
		bFocusFirstRowPending = true;
	}
}

void UALKSettingsScreen::SetActiveCategory(EALKSettingsCategory Category)
{
	const int32 Index = TabCategories.IndexOfByKey(Category);
	if (Index != INDEX_NONE)
	{
		SetActiveTab(Index);
	}
}

void UALKSettingsScreen::RebuildList()
{
	if (!SettingsList || !TabCategories.IsValidIndex(ActiveTab))
	{
		return;
	}

	const UALKTheme& Theme = *GetTheme();

	// A page lists every one of its categories in order; the tabbed screen only the active tab.
	TArray<EALKSettingsCategory> ShownCategories;
	if (bPageMode)
	{
		ShownCategories = TabCategories;
	}
	else
	{
		ShownCategories.Add(TabCategories[ActiveTab]);
	}

	SettingsList->ClearChildren();
	SettingsList->SetScrollOffset(0.f);
	Rows.Reset();
	SectionHeaders.Reset();

	FString LastSection;
	for (EALKSettingsCategory Category : ShownCategories)
	{
		for (UALKSetting* Setting : AllSettings)
		{
			if (!Setting || Setting->Category != Category)
			{
				continue;
			}

			if (!Setting->Section.IsEmpty() && Setting->Section.ToString() != LastSection)
			{
				LastSection = Setting->Section.ToString();
				UTextBlock* Header = FALKWidgetBuilder::MakeText(*WidgetTree, NAME_None, Setting->Section);
				StyleSectionHeader(Header, Theme);
				SettingsList->AddChild(Header);
				if (UScrollBoxSlot* HeaderSlot = Cast<UScrollBoxSlot>(Header->Slot))
				{
					HeaderSlot->SetPadding(FMargin(20.f, SectionHeaders.Num() == 0 ? 0.f : 24.f, 0.f, 8.f));
				}
				SectionHeaders.Add(Header);
			}

			UALKSettingRow* Row = WidgetTree->ConstructWidget<UALKSettingRow>(GetRowClass(Setting->GetSettingType()));
			if (!Row)
			{
				continue;
			}
			Row->SetSetting(Setting);
			Row->OnRowFocused.AddUObject(this, &ThisClass::HandleRowFocused);
			SettingsList->AddChild(Row);
			if (UScrollBoxSlot* RowSlot = Cast<UScrollBoxSlot>(Row->Slot))
			{
				RowSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 2.f));
			}
			Rows.Add(Row);
		}
	}

	DefaultFocusWidget = Rows.Num() > 0 ? static_cast<UWidget*>(Rows[0].Get()) : nullptr;
	UpdateDescription(Rows.Num() > 0 ? Rows[0]->GetSetting() : nullptr);
	UpdateSystemLabel();
}

void UALKSettingsScreen::UpdateSystemLabel()
{
	if (!SystemLabelText)
	{
		return;
	}
	// A page is named by its title; the tabbed screen by the tab shown. Codes follow the category (Video 03, ...).
	int32 Code = 3;
	FText Name = TitleText ? TitleText->GetText() : FText::GetEmpty();
	if (TabCategories.IsValidIndex(ActiveTab))
	{
		Code = 3 + static_cast<int32>(TabCategories[ActiveTab]);
		if (!bPageMode)
		{
			Name = GetMenuConfig()->GetTabDisplayName(TabCategories[ActiveTab]);
		}
	}
	SystemLabelText->SetText(MakeSystemLabel(Code, Name));
}

void UALKSettingsScreen::RefreshRows()
{
	for (UALKSettingRow* Row : Rows)
	{
		if (Row)
		{
			Row->Refresh();
		}
	}
}

void UALKSettingsScreen::UpdateFooter()
{
	const UALKSettingsSubsystem* Settings = GetSettingsSubsystem();
	if (ApplyButton)
	{
		ApplyButton->SetIsEnabled(Settings && Settings->HasUnsavedChanges());
	}
}

void UALKSettingsScreen::UpdateDescription(const UALKSetting* Setting)
{
	if (DescriptionTitle)
	{
		DescriptionTitle->SetText(Setting ? Setting->DisplayName : FText::GetEmpty());
	}
	if (DescriptionText)
	{
		DescriptionText->SetText(Setting ? Setting->Description : FText::GetEmpty());
	}
	if (DescriptionRef)
	{
		// The small line under the title names the setting's section ("Display", "Quality").
		DescriptionRef->SetText(Setting ? Setting->Section : FText::GetEmpty());
		DescriptionRef->SetVisibility(Setting && !Setting->Section.IsEmpty() ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void UALKSettingsScreen::HandleAnySettingChanged(UALKSetting* Setting)
{
	RefreshRows();
	UpdateFooter();
}

void UALKSettingsScreen::HandleRowFocused(UALKSettingRow* Row, bool bFromNavigation)
{
	if (!Row)
	{
		return;
	}
	LastFocusedRow = Row;
	UpdateDescription(Row->GetSetting());
	if (bFromNavigation && SettingsList)
	{
		SettingsList->ScrollWidgetIntoView(Row, true, EDescendantScrollDestination::IntoView, 40.f);
	}
}

bool UALKSettingsScreen::NativeHandleMenuKey(const FKey& Key)
{
	if (!bPageMode && (Key == EKeys::Q || Key == EKeys::Gamepad_LeftShoulder))
	{
		SetActiveTab(ActiveTab - 1);
		return true;
	}
	if (!bPageMode && (Key == EKeys::E || Key == EKeys::Gamepad_RightShoulder))
	{
		SetActiveTab(ActiveTab + 1);
		return true;
	}
	if (Key == EKeys::Gamepad_FaceButton_Top)
	{
		ResetActiveTabToDefaults();
		return true;
	}
	return false;
}

void UALKSettingsScreen::ApplyChanges()
{
	// Resolution / window mode changes can leave the player with an unusable display, so they must be confirmed.
	UALKSettingsSubsystem* Settings = GetSettingsSubsystem();
	const bool bDisplayModeChanging = Settings && Settings->IsDisplayChangePending();

	if (Settings)
	{
		Settings->ApplyAndSave();
	}
	RefreshRows();
	UpdateFooter();

	if (bDisplayModeChanging)
	{
		AskToKeepDisplayMode();
	}
}

void UALKSettingsScreen::AskToKeepDisplayMode()
{
	UALKMenuSubsystem* MenuSubsystem = GetMenuSubsystem();
	if (!MenuSubsystem)
	{
		return;
	}

	FALKDialogDescriptor Descriptor;
	Descriptor.Title = LOCTEXT("KeepDisplayTitle", "Keep Display Settings?");
	Descriptor.Message = LOCTEXT("KeepDisplayMessage", "Your display will return to its previous settings unless you keep the new ones.");
	Descriptor.ConfirmText = LOCTEXT("KeepDisplay", "Keep");
	Descriptor.CancelText = LOCTEXT("RevertDisplay", "Revert");
	Descriptor.TimeoutSeconds = 15.f;

	TWeakObjectPtr<UALKSettingsScreen> WeakThis(this);
	TWeakObjectPtr<UALKSettingsSubsystem> WeakSettings(GetSettingsSubsystem());
	MenuSubsystem->ShowConfirmDialog(Descriptor, [WeakThis, WeakSettings](EALKDialogResult Result)
	{
		if (UALKSettingsSubsystem* Settings = WeakSettings.Get())
		{
			if (Result == EALKDialogResult::Confirmed)
			{
				Settings->ConfirmDisplayMode();
			}
			else
			{
				Settings->RevertDisplayMode();
			}
		}

		if (WeakThis.IsValid())
		{
			WeakThis->RefreshRows();
			WeakThis->UpdateFooter();
		}
	});
}

void UALKSettingsScreen::ResetActiveTabToDefaults()
{
	UALKMenuSubsystem* MenuSubsystem = GetMenuSubsystem();
	if (!MenuSubsystem || !TabCategories.IsValidIndex(ActiveTab))
	{
		return;
	}

	TArray<EALKSettingsCategory> Categories;
	FALKDialogDescriptor Descriptor;
	Descriptor.Title = LOCTEXT("ResetTitle", "Reset to Defaults");
	if (bPageMode)
	{
		Categories = TabCategories;
		Descriptor.Message = FText::Format(LOCTEXT("ResetPageMessage", "Reset every {0} setting to its default value?"), PageTitle);
	}
	else
	{
		Categories.Add(TabCategories[ActiveTab]);
		Descriptor.Message = FText::Format(LOCTEXT("ResetMessage", "Reset every {0} setting to its default value?"), GetMenuConfig()->GetTabDisplayName(TabCategories[ActiveTab]));
	}
	Descriptor.ConfirmText = LOCTEXT("ResetConfirm", "Reset");
	Descriptor.CancelText = LOCTEXT("Cancel", "Cancel");
	Descriptor.bDestructive = true;

	TWeakObjectPtr<UALKSettingsScreen> WeakThis(this);
	MenuSubsystem->ShowConfirmDialog(Descriptor, [WeakThis, Categories](EALKDialogResult Result)
	{
		if (Result != EALKDialogResult::Confirmed || !WeakThis.IsValid())
		{
			return;
		}
		for (UALKSetting* Setting : WeakThis->AllSettings)
		{
			if (Setting && Categories.Contains(Setting->Category) && Setting->GetSettingType() != EALKSettingType::Action)
			{
				Setting->ResetToDefault();
			}
		}
		WeakThis->RefreshRows();
		WeakThis->UpdateFooter();
	});
}

void UALKSettingsScreen::RequestClose()
{
	UALKSettingsSubsystem* Settings = GetSettingsSubsystem();
	UALKMenuSubsystem* MenuSubsystem = GetMenuSubsystem();
	if (!Settings || !Settings->HasUnsavedChanges() || !MenuSubsystem)
	{
		PlayThemeSound(EALKUISound::Back);
		CloseScreen();
		return;
	}

	FALKDialogDescriptor Descriptor;
	Descriptor.Title = LOCTEXT("UnsavedTitle", "Apply Changes");
	Descriptor.Message = LOCTEXT("UnsavedMessage", "There have been changes made to the settings. Do you want to apply them?");
	Descriptor.ConfirmText = LOCTEXT("Apply", "Apply");
	Descriptor.AlternateText = LOCTEXT("Discard", "Discard");
	Descriptor.CancelText = LOCTEXT("Cancel", "Cancel");

	TWeakObjectPtr<UALKSettingsScreen> WeakThis(this);
	MenuSubsystem->ShowConfirmDialog(Descriptor, [WeakThis](EALKDialogResult Result)
	{
		if (!WeakThis.IsValid())
		{
			return;
		}
		UALKSettingsSubsystem* SettingsSubsystem = WeakThis->GetSettingsSubsystem();
		if (Result == EALKDialogResult::Confirmed)
		{
			// May open the "keep display settings?" dialog, which lives on the modal layer and outlives this screen.
			WeakThis->ApplyChanges();
			WeakThis->CloseScreen();
		}
		else if (Result == EALKDialogResult::Alternate)
		{
			if (SettingsSubsystem)
			{
				SettingsSubsystem->RevertUnsavedChanges();
			}
			WeakThis->CloseScreen();
		}
	});
}

bool UALKSettingsScreen::OnBackRequested_Implementation()
{
	RequestClose();
	return true;
}

#undef LOCTEXT_NAMESPACE
