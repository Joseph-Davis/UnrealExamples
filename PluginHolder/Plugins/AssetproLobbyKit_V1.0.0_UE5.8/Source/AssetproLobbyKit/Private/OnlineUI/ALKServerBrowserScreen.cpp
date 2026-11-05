// Copyright 2026 CodeLikeMe. All Rights Reserved.

#include "OnlineUI/ALKServerBrowserScreen.h"
#include "ALKDeveloperSettings.h"
#include "ALKTheme.h"
#include "Online/ALKOnlineConfig.h"
#include "Online/ALKOnlineProvider.h"
#include "Online/ALKOnlineSubsystem.h"
#include "OnlineUI/ALKOnlineFlowSubsystem.h"
#include "OnlineUI/ALKServerRow.h"
#include "OnlineUI/ALKTextEntryDialog.h"
#include "Rules/ALKGameRules.h"
#include "Widgets/ALKButton.h"
#include "Widgets/ALKDecor.h"
#include "Widgets/ALKFrame.h"
#include "Widgets/ALKInputLabels.h"
#include "Widgets/ALKTextField.h"
#include "Widgets/ALKThumbnail.h"
#include "Widgets/ALKTitleText.h"
#include "Widgets/ALKWidgetBuilder.h"
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
#include "Components/SizeBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WrapBox.h"
#include "Components/WrapBoxSlot.h"
#include "Engine/Texture2D.h"
#include "HAL/PlatformInput.h"
#include "Styling/SlateTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ALKServerBrowserScreen)

#define LOCTEXT_NAMESPACE "AssetproLobbyKit"

namespace ALKServerBrowserPrivate
{
	/** The screen's content never gets wider than this (ultrawide screens): it sits in the middle. */
	static constexpr float ContentMaxWidth = 1920.f;
	static constexpr float DetailWidth = 494.f;
	static constexpr float PictureHeight = 190.f;
	/** Room at the right of every row for its focus outline and the chunky skin's hard shadow. */
	static constexpr float RowEndInset = 6.f;
	/** The arrow after a column's title, with the gap before it. */
	static constexpr float ArrowSpace = 20.f;

	static const EALKServerBrowserTab TabOrder[] = { EALKServerBrowserTab::All, EALKServerBrowserTab::Favourites, EALKServerBrowserTab::Recent, EALKServerBrowserTab::LAN };
	static constexpr int32 TabCount = UE_ARRAY_COUNT(TabOrder);

	/** A line of the details: the name muted, the value in the primary colour, a little smaller than the body text. */
	static void StyleDetailLine(UTextBlock* NameText, UTextBlock* ValueText, const UALKTheme& Theme)
	{
		FSlateFontInfo Font = Theme.BodyFont;
		Font.Size = FMath::Max(8.f, Theme.BodyFont.Size - 2.f);
		FALKWidgetBuilder::StyleText(NameText, Theme, EALKTextRole::Body, Theme.TextMuted);
		FALKWidgetBuilder::StyleText(ValueText, Theme, EALKTextRole::Body, Theme.TextPrimary);
		if (NameText)
		{
			FALKWidgetBuilder::SetTextFont(NameText, Font);
		}
		if (ValueText)
		{
			FALKWidgetBuilder::SetTextFont(ValueText, Font);
		}
	}

	static void StyleScrollBar(UScrollBox* List, const UALKTheme& Theme, float BarPadding)
	{
		if (!List)
		{
			return;
		}
		FScrollBarStyle BarStyle = List->GetWidgetBarStyle();
		const FSlateBrush Thumb = UALKTheme::MakeSolidBrush(Theme.Accent.CopyWithNewOpacity(0.85f));
		BarStyle.SetNormalThumbImage(Thumb);
		BarStyle.SetHoveredThumbImage(Thumb);
		BarStyle.SetDraggedThumbImage(Thumb);
		BarStyle.SetVerticalBackgroundImage(UALKTheme::MakeSolidBrush(Theme.TextPrimary.CopyWithNewOpacity(0.06f)));
		List->SetWidgetBarStyle(BarStyle);
		List->SetScrollbarThickness(FVector2D(3.0, 3.0));
		List->SetScrollbarPadding(FMargin(BarPadding, 0.f, 0.f, 0.f));
	}
}

UALKServerBrowserScreen::UALKServerBrowserScreen(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// A screen full of text: the backdrop steps back.
	bDimScene = true;
	RowClass = UALKServerRow::StaticClass();
}

// ---------------------------------------------------------------- Layout

void UALKServerBrowserScreen::NativeBuildDefaultLayout(UWidgetTree& Tree)
{
	using namespace ALKServerBrowserPrivate;

	UOverlay* Root = FALKWidgetBuilder::Make<UOverlay>(Tree, TEXT("Root"));
	Tree.RootWidget = Root;
	BackgroundImage = FALKWidgetBuilder::AddBackgroundLayers(Tree, Root);

	UVerticalBox* Main = FALKWidgetBuilder::Make<UVerticalBox>(Tree, TEXT("MainColumn"));
	USizeBox* MainSize = FALKWidgetBuilder::Make<USizeBox>(Tree, TEXT("MainSize"));
	MainSize->SetWidthOverride(ContentMaxWidth);
	MainSize->AddChild(Main);
	FALKWidgetBuilder::AddToOverlay(Root, MainSize, HAlign_Center, VAlign_Fill);

	// ---- Title and how many games
	UHorizontalBox* TitleRow = FALKWidgetBuilder::Make<UHorizontalBox>(Tree, TEXT("TitleRow"));
	FALKWidgetBuilder::AddToVerticalBox(Main, TitleRow, FMargin(), HAlign_Left);
	UALKTitleText* Title = FALKWidgetBuilder::Make<UALKTitleText>(Tree, TEXT("TitleText"));
	Title->SetText(LOCTEXT("BrowserTitle", "Browse Games"));
	TitleText = Title;
	FALKWidgetBuilder::AddToHorizontalBox(TitleRow, TitleText, FMargin(), HAlign_Left, VAlign_Bottom);
	CountText = FALKWidgetBuilder::MakeText(Tree, TEXT("CountText"));
	FALKWidgetBuilder::AddToHorizontalBox(TitleRow, CountText, FMargin(22.f, 0.f, 0.f, 9.f), HAlign_Left, VAlign_Bottom);

	// ---- Body: tabs, filters and the table | the focused game
	UHorizontalBox* Body = FALKWidgetBuilder::Make<UHorizontalBox>(Tree, TEXT("Body"));
	FALKWidgetBuilder::AddToVerticalBox(Main, Body, FMargin(0.f, 24.f, 0.f, 0.f), HAlign_Fill, VAlign_Fill, 1.f);

	UVerticalBox* Left = FALKWidgetBuilder::Make<UVerticalBox>(Tree, TEXT("ListColumn"));
	FALKWidgetBuilder::AddToHorizontalBox(Body, Left, FMargin(), HAlign_Fill, VAlign_Fill, 1.f);

	TabBox = FALKWidgetBuilder::Make<UHorizontalBox>(Tree, TEXT("TabBox"));
	FALKWidgetBuilder::AddToVerticalBox(Left, TabBox, FMargin(), HAlign_Left);

	// Filters: three switches, the region, the search box and Refresh.
	UHorizontalBox* FilterRow = FALKWidgetBuilder::Make<UHorizontalBox>(Tree, TEXT("FilterRow"));
	FALKWidgetBuilder::AddToVerticalBox(Left, FilterRow, FMargin(0.f, 16.f, 0.f, 0.f));
	NotFullChip = CreateMenuButton(LOCTEXT("BrowserNotFull", "Not full"), EALKButtonStyle::Tab);
	NotEmptyChip = CreateMenuButton(LOCTEXT("BrowserNotEmpty", "Not empty"), EALKButtonStyle::Tab);
	NoPasswordChip = CreateMenuButton(LOCTEXT("BrowserNoPassword", "No password"), EALKButtonStyle::Tab);
	RegionChip = CreateMenuButton(FText::GetEmpty(), EALKButtonStyle::Tab);
	RegionChip->SetLeadingIcon(EALKIcon::Globe);
	for (UALKButton* Chip : { NotFullChip.Get(), NotEmptyChip.Get(), NoPasswordChip.Get(), RegionChip.Get() })
	{
		// A chip plays the Change sound when it flips.
		Chip->bPlayClickSound = false;
		FALKWidgetBuilder::AddToHorizontalBox(FilterRow, Chip, FMargin(0.f, 0.f, 8.f, 0.f), HAlign_Left, VAlign_Center);
	}
	SearchField = Tree.ConstructWidget<UALKTextField>(UALKTextField::StaticClass(), TEXT("SearchField"));
	SearchField->SetLeadingIcon(EALKIcon::Search);
	SearchField->SetHintText(LOCTEXT("BrowserSearchHint", "Search name or map"));
	SearchField->SetMaxLength(48);
	FALKWidgetBuilder::AddToHorizontalBox(FilterRow, SearchField, FMargin(0.f, 0.f, 12.f, 0.f), HAlign_Fill, VAlign_Center, 1.f);
	RefreshSpinner = FALKWidgetBuilder::Make<UALKSpinner>(Tree, TEXT("RefreshSpinner"));
	RefreshSpinner->Diameter = 22.f;
	RefreshSpinner->SetVisibility(ESlateVisibility::Collapsed);
	FALKWidgetBuilder::AddToHorizontalBox(FilterRow, RefreshSpinner, FMargin(0.f, 0.f, 12.f, 0.f), HAlign_Right, VAlign_Center);
	RefreshButton = CreateMenuButton(LOCTEXT("BrowserRefresh", "Refresh"), EALKButtonStyle::Secondary);
	RefreshButton->SetLeadingIcon(EALKIcon::Refresh);
	FALKWidgetBuilder::AddToHorizontalBox(FilterRow, RefreshButton, FMargin(), HAlign_Right, VAlign_Center);

	// The table: sortable header, then the rows.
	UALKFrame* Table = FALKWidgetBuilder::Make<UALKFrame>(Tree, TEXT("TablePanel"));
	Table->SetPadding(FMargin(6.f, 8.f, 6.f, 6.f));
	TablePanel = Table;
	FALKWidgetBuilder::AddToVerticalBox(Left, Table, FMargin(0.f, 14.f, 0.f, 0.f), HAlign_Fill, VAlign_Fill, 1.f);
	UVerticalBox* TableColumn = FALKWidgetBuilder::Make<UVerticalBox>(Tree, TEXT("TableColumn"));
	Table->SetContent(TableColumn);

	HeaderRow = FALKWidgetBuilder::Make<UHorizontalBox>(Tree, TEXT("HeaderRow"));
	FALKWidgetBuilder::AddToVerticalBox(TableColumn, HeaderRow, FMargin(ALKServerColumns::SidePadding, 0.f, ALKServerColumns::SidePadding + RowEndInset, 0.f));
	FALKWidgetBuilder::AddToHorizontalBox(HeaderRow, FALKWidgetBuilder::WrapInSizeBox(Tree, nullptr, ALKServerColumns::Star, 0.f));
	AddHeaderCell(Tree, EALKSessionSort::Name, LOCTEXT("BrowserColumnName", "Server name"), 0.f, false, 0.f);
	AddHeaderCell(Tree, EALKSessionSort::Mode, LOCTEXT("BrowserColumnMode", "Mode"), ALKServerColumns::Mode, false, 0.f);
	AddHeaderCell(Tree, EALKSessionSort::Map, LOCTEXT("BrowserColumnMap", "Map"), ALKServerColumns::Map, false, 0.f);
	// Right aligned titles end where the numbers end; their arrow goes past that.
	AddHeaderCell(Tree, EALKSessionSort::Players, LOCTEXT("BrowserColumnPlayers", "Players"), ALKServerColumns::Players, true, -ArrowSpace);
	AddHeaderCell(Tree, EALKSessionSort::Ping, LOCTEXT("BrowserColumnPing", "Ping"), ALKServerColumns::Ping, true, ALKServerColumns::PingBars - ArrowSpace);
	FALKWidgetBuilder::AddToHorizontalBox(HeaderRow, FALKWidgetBuilder::WrapInSizeBox(Tree, nullptr, ALKServerColumns::Lock, 0.f));

	HeaderRule = FALKWidgetBuilder::Make<UALKRule>(Tree, TEXT("HeaderRule"));
	FALKWidgetBuilder::AddToVerticalBox(TableColumn, HeaderRule, FMargin(0.f, 6.f, 0.f, 4.f));

	UOverlay* ListOverlay = FALKWidgetBuilder::Make<UOverlay>(Tree, TEXT("ListOverlay"));
	FALKWidgetBuilder::AddToVerticalBox(TableColumn, ListOverlay, FMargin(), HAlign_Fill, VAlign_Fill, 1.f);
	ServerList = FALKWidgetBuilder::Make<UScrollBox>(Tree, TEXT("ServerList"));
	ServerList->SetAnimateWheelScrolling(true);
	ServerList->SetScrollWhenFocusChanges(EScrollWhenFocusChanges::AnimatedScroll);
	FALKWidgetBuilder::AddToOverlay(ListOverlay, ServerList);

	// One centred line while the table is empty: searching, or why there is nothing.
	StateBox = FALKWidgetBuilder::Make<UVerticalBox>(Tree, TEXT("StateBox"));
	StateBox->SetVisibility(ESlateVisibility::Collapsed);
	FALKWidgetBuilder::AddToOverlay(ListOverlay, StateBox, HAlign_Center, VAlign_Center);
	StateSpinner = FALKWidgetBuilder::Make<UALKSpinner>(Tree, TEXT("StateSpinner"));
	StateSpinner->Diameter = 38.f;
	FALKWidgetBuilder::AddToVerticalBox(StateBox, StateSpinner, FMargin(0.f, 0.f, 0.f, 18.f), HAlign_Center);
	// The spinner while searching, an icon for why there is nothing once the search is over.
	StateIcon = FALKWidgetBuilder::MakeIconDisc(Tree, TEXT("StateIcon"), EALKIcon::Search);
	FALKWidgetBuilder::AddToVerticalBox(StateBox, StateIcon, FMargin(0.f, 0.f, 0.f, 16.f), HAlign_Center);
	StateText = FALKWidgetBuilder::MakeText(Tree, TEXT("StateText"));
	StateText->SetJustification(ETextJustify::Center);
	FALKWidgetBuilder::AddToVerticalBox(StateBox, StateText, FMargin(), HAlign_Center);
	StateDetailText = FALKWidgetBuilder::MakeText(Tree, TEXT("StateDetailText"));
	StateDetailText->SetJustification(ETextJustify::Center);
	FALKWidgetBuilder::AddToVerticalBox(StateBox, StateDetailText, FMargin(0.f, 8.f, 0.f, 0.f), HAlign_Center);

	// ---- The focused game
	UALKFrame* Detail = FALKWidgetBuilder::Make<UALKFrame>(Tree, TEXT("DetailPanel"));
	Detail->SetPadding(FMargin(14.f));
	DetailPanel = Detail;
	FALKWidgetBuilder::AddToHorizontalBox(Body, FALKWidgetBuilder::WrapInSizeBox(Tree, Detail, DetailWidth, 0.f), FMargin(28.f, 0.f, 0.f, 0.f), HAlign_Right, VAlign_Fill);

	UOverlay* DetailOverlay = FALKWidgetBuilder::Make<UOverlay>(Tree, TEXT("DetailOverlay"));
	Detail->SetContent(DetailOverlay);
	DetailEmptyText = FALKWidgetBuilder::MakeText(Tree, TEXT("DetailEmptyText"), LOCTEXT("BrowserNoDetails", "Pick a game to see its details"));
	DetailEmptyText->SetJustification(ETextJustify::Center);
	DetailEmptyText->SetAutoWrapText(true);
	FALKWidgetBuilder::AddToOverlay(DetailOverlay, FALKWidgetBuilder::MakeEmptyState(Tree, TEXT("DetailEmptyState"), EALKIcon::Server, DetailEmptyText), HAlign_Fill, VAlign_Center, FMargin(24.f, 0.f));

	DetailBox = FALKWidgetBuilder::Make<UVerticalBox>(Tree, TEXT("DetailBox"));
	FALKWidgetBuilder::AddToOverlay(DetailOverlay, DetailBox);

	// The map's picture, with a tag over it while the match is already running.
	UOverlay* PictureOverlay = FALKWidgetBuilder::Make<UOverlay>(Tree, TEXT("DetailPictureOverlay"));
	DetailPicture = FALKWidgetBuilder::Make<UALKThumbnail>(Tree, TEXT("DetailPicture"));
	DetailPicture->bThemeVeil = true;
	FALKWidgetBuilder::AddToOverlay(PictureOverlay, DetailPicture);
	DetailStatusChip = FALKWidgetBuilder::MakeChip(Tree, TEXT("DetailStatusChip"), LOCTEXT("BrowserStatusMatch", "Match in progress"), EALKIcon::Play);
	DetailStatusChip->SetVisibility(ESlateVisibility::Collapsed);
	FALKWidgetBuilder::AddToOverlay(PictureOverlay, DetailStatusChip, HAlign_Left, VAlign_Top, FMargin(10.f));
	FALKWidgetBuilder::AddToVerticalBox(DetailBox, FALKWidgetBuilder::WrapInSizeBox(Tree, PictureOverlay, 0.f, PictureHeight));

	UHorizontalBox* NameRow = FALKWidgetBuilder::Make<UHorizontalBox>(Tree, TEXT("DetailNameRow"));
	FALKWidgetBuilder::AddToVerticalBox(DetailBox, NameRow, FMargin(10.f, 16.f, 10.f, 0.f));
	UALKTitleText* DetailName = FALKWidgetBuilder::Make<UALKTitleText>(Tree, TEXT("DetailNameText"));
	DetailName->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
	DetailNameText = DetailName;
	FALKWidgetBuilder::AddToHorizontalBox(NameRow, DetailNameText, FMargin(), HAlign_Fill, VAlign_Center, 1.f);
	DetailLockIcon = FALKWidgetBuilder::MakeIcon(Tree, TEXT("DetailLockIcon"), EALKIcon::Lock, 22.f);
	FALKWidgetBuilder::AddToHorizontalBox(NameRow, DetailLockIcon, FMargin(12.f, 0.f, 0.f, 0.f), HAlign_Right, VAlign_Center);

	DetailSubText = FALKWidgetBuilder::MakeText(Tree, TEXT("DetailSubText"));
	DetailSubText->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
	FALKWidgetBuilder::AddToVerticalBox(DetailBox, DetailSubText, FMargin(10.f, 4.f, 10.f, 0.f));

	DetailRows = FALKWidgetBuilder::Make<UScrollBox>(Tree, TEXT("DetailRows"));
	DetailRows->SetAnimateWheelScrolling(true);
	FALKWidgetBuilder::AddToVerticalBox(DetailBox, DetailRows, FMargin(10.f, 10.f, 2.f, 0.f), HAlign_Fill, VAlign_Fill, 1.f);

	JoinButton = CreateMenuButton(LOCTEXT("BrowserJoin", "Join game"), EALKButtonStyle::Solid);
	JoinButton->SetLeadingIcon(EALKIcon::Play);
	FALKWidgetBuilder::AddToVerticalBox(DetailBox, FALKWidgetBuilder::WrapInSizeBox(Tree, JoinButton, 0.f, 62.f), FMargin(10.f, 12.f, 10.f, 0.f));

	// Side by side while both fit; a long label (another language, capitals) puts the second one on its own line.
	UWrapBox* DetailButtons = FALKWidgetBuilder::Make<UWrapBox>(Tree, TEXT("DetailButtons"));
	DetailButtons->SetInnerSlotPadding(FVector2D(12.0, 10.0));
	FALKWidgetBuilder::AddToVerticalBox(DetailBox, DetailButtons, FMargin(10.f, 12.f, 10.f, 8.f));
	FavouriteButton = CreateMenuButton(LOCTEXT("BrowserAddFavourite", "Add favourite"), EALKButtonStyle::Secondary);
	CopyButton = CreateMenuButton(LOCTEXT("BrowserCopyAddress", "Copy address"), EALKButtonStyle::Ghost);
	for (UALKButton* Button : { FavouriteButton.Get(), CopyButton.Get() })
	{
		if (UWrapBoxSlot* ButtonSlot = DetailButtons->AddChildToWrapBox(Button))
		{
			// The last button of a line takes the width that is left.
			ButtonSlot->SetFillEmptySpace(true);
		}
	}

	// ---- Footer: [Enter] Join  [F] Favourite  [R] Refresh  [Q] [E] Switch tab  [Esc] Back
	UHorizontalBox* Hints = BuildInputHints(Tree, {
		FALKInputHint(LOCTEXT("KeyEnter", "Enter"), FPlatformInput::GetGamepadAcceptKey(), LOCTEXT("BrowserHintJoin", "Join"))
			.WithAction(FSimpleDelegate::CreateUObject(this, &ThisClass::JoinFocusedGame)),
		FALKInputHint(LOCTEXT("KeyF", "F"), EKeys::Gamepad_FaceButton_Left, LOCTEXT("BrowserHintFavourite", "Favourite"))
			.WithAction(FSimpleDelegate::CreateUObject(this, &ThisClass::ToggleFocusedFavourite)),
		FALKInputHint(LOCTEXT("KeyR", "R"), EKeys::Gamepad_FaceButton_Top, LOCTEXT("BrowserRefresh", "Refresh"))
			.WithAction(FSimpleDelegate::CreateUObject(this, &ThisClass::RefreshGames)),
		FALKInputHint(LOCTEXT("KeyQ", "Q"), EKeys::Gamepad_LeftShoulder, FText::GetEmpty())
			.WithAction(FSimpleDelegate::CreateUObject(this, &ThisClass::StepTab, -1)),
		FALKInputHint(LOCTEXT("KeyE", "E"), EKeys::Gamepad_RightShoulder, LOCTEXT("BrowserHintTab", "Switch tab"))
			.WithAction(FSimpleDelegate::CreateUObject(this, &ThisClass::StepTab, 1)),
		FALKInputHint(LOCTEXT("KeyEsc", "Esc"), FPlatformInput::GetGamepadBackKey(), LOCTEXT("HintBack", "Back"))
			.WithAction(FSimpleDelegate::CreateWeakLambda(this, [this]() { UALKScreen::OnBackRequested_Implementation(); }))
	});
	FALKWidgetBuilder::AddToVerticalBox(Main, Hints, FMargin(0.f, 22.f, 0.f, 0.f), HAlign_Left);
}

void UALKServerBrowserScreen::AddHeaderCell(UWidgetTree& Tree, EALKSessionSort Column, const FText& Title, float Width, bool bRightAligned, float EndPadding)
{
	// A small title that can be picked, and the arrow that shows which way the column sorts.
	UALKButton* Button = CreateMenuButton(Title.ToUpper(), EALKButtonStyle::Hint);
	Button->bPlayClickSound = false;
	UALKIconWidget* Arrow = FALKWidgetBuilder::MakeIcon(Tree, NAME_None, EALKIcon::ChevronDown, 14.f);
	Arrow->SetVisibility(ESlateVisibility::Hidden);

	UHorizontalBox* Content = FALKWidgetBuilder::Make<UHorizontalBox>(Tree);
	FALKWidgetBuilder::AddToHorizontalBox(Content, Button, FMargin(), HAlign_Left, VAlign_Center);
	FALKWidgetBuilder::AddToHorizontalBox(Content, Arrow, FMargin(ALKServerBrowserPrivate::ArrowSpace - 14.f, 0.f, 0.f, 0.f), HAlign_Left, VAlign_Center);

	// The header has no box of its own: an outline round it shows the focus.
	UALKFrame* Frame = FALKWidgetBuilder::Make<UALKFrame>(Tree);
	Frame->SetVisibility(ESlateVisibility::HitTestInvisible);
	UOverlay* Cell = FALKWidgetBuilder::Make<UOverlay>(Tree);
	FALKWidgetBuilder::AddToOverlay(Cell, Frame, HAlign_Fill, VAlign_Fill, FMargin(-8.f, 0.f, -4.f, 0.f));
	FALKWidgetBuilder::AddToOverlay(Cell, Content, HAlign_Fill, VAlign_Center, FMargin(0.f, 6.f));

	if (Width > 0.f)
	{
		USizeBox* Sized = FALKWidgetBuilder::WrapInSizeBox(Tree, Cell, Width, 0.f);
		if (USizeBoxSlot* CellSlot = Cast<USizeBoxSlot>(Cell->Slot))
		{
			CellSlot->SetHorizontalAlignment(bRightAligned ? HAlign_Right : HAlign_Left);
			CellSlot->SetVerticalAlignment(VAlign_Center);
			CellSlot->SetPadding(FMargin(0.f, 0.f, bRightAligned ? EndPadding : 0.f, 0.f));
		}
		FALKWidgetBuilder::AddToHorizontalBox(HeaderRow, Sized);
	}
	else
	{
		// The server name takes the width that is left, like in the rows.
		FALKWidgetBuilder::AddToHorizontalBox(HeaderRow, Cell, FMargin(0.f, 0.f, 16.f, 0.f), HAlign_Left, VAlign_Center, 1.f);
	}

	HeaderColumns.Add(Column);
	HeaderButtons.Add(Button);
	HeaderArrows.Add(Arrow);
	HeaderFrames.Add(Frame);
	HeaderFocused.Add(false);
}

void UALKServerBrowserScreen::BuildTabs()
{
	if (!TabBox || TabButtons.Num() > 0)
	{
		return;
	}
	for (int32 Index = 0; Index < ALKServerBrowserPrivate::TabCount; ++Index)
	{
		// Tabs are switched with Q / E or the bumpers and can be clicked; they never take the focus.
		UALKButton* TabButton = CreateMenuButton(FText::GetEmpty(), EALKButtonStyle::Tab);
		TabButton->SetIsFocusable(false);
		TabButton->bFocusOnHover = false;
		TabButton->bPlayClickSound = false;
		TabButton->OnClicked().AddUObject(this, &ThisClass::SetActiveTab, ALKServerBrowserPrivate::TabOrder[Index]);
		TabBox->AddChild(TabButton);
		if (UHorizontalBoxSlot* TabSlot = Cast<UHorizontalBoxSlot>(TabButton->Slot))
		{
			TabSlot->SetPadding(FMargin(Index == 0 ? 0.f : 8.f, 0.f, 0.f, 0.f));
			TabSlot->SetVerticalAlignment(VAlign_Center);
		}
		TabButtons.Add(TabButton);
	}
}

void UALKServerBrowserScreen::NativeOnLayoutReady()
{
	Super::NativeOnLayoutReady();

	BuildTabs();
	if (NotFullChip)
	{
		NotFullChip->OnClicked().AddUObject(this, &ThisClass::ToggleFilter, 0);
	}
	if (NotEmptyChip)
	{
		NotEmptyChip->OnClicked().AddUObject(this, &ThisClass::ToggleFilter, 1);
	}
	if (NoPasswordChip)
	{
		NoPasswordChip->OnClicked().AddUObject(this, &ThisClass::ToggleFilter, 2);
	}
	if (RegionChip)
	{
		RegionChip->OnClicked().AddUObject(this, &ThisClass::CycleRegion);
	}
	if (SearchField)
	{
		SearchField->OnTextChangedNative.AddUObject(this, &ThisClass::HandleSearchChanged);
		SearchField->OnTextCommittedNative.AddUObject(this, &ThisClass::HandleSearchCommitted);
	}
	if (RefreshButton)
	{
		RefreshButton->OnClicked().AddUObject(this, &ThisClass::RefreshGames);
	}
	for (int32 Index = 0; Index < HeaderButtons.Num(); ++Index)
	{
		HeaderButtons[Index]->OnClicked().AddUObject(this, &ThisClass::SortBy, HeaderColumns[Index]);
	}
	if (JoinButton)
	{
		JoinButton->OnClicked().AddUObject(this, &ThisClass::JoinFocusedGame);
	}
	if (FavouriteButton)
	{
		FavouriteButton->OnClicked().AddUObject(this, &ThisClass::ToggleFocusedFavourite);
	}
	if (CopyButton)
	{
		CopyButton->OnClicked().AddUObject(this, &ThisClass::CopyFocusedAddress);
	}
	UpdateTabs();
	UpdateChips();
	UpdateHeader();
}

void UALKServerBrowserScreen::NativeApplyTheme(const UALKTheme& Theme)
{
	Super::NativeApplyTheme(Theme);

	if (WidgetTree)
	{
		StyleScreenBackground(Theme);
		if (UWidget* MainSize = WidgetTree->FindWidget(TEXT("MainSize")))
		{
			if (UOverlaySlot* MainSlot = Cast<UOverlaySlot>(MainSize->Slot))
			{
				MainSlot->SetPadding(Theme.ScreenPadding);
			}
		}
	}

	FALKWidgetBuilder::StyleText(TitleText, Theme, EALKTextRole::Heading);
	FALKWidgetBuilder::StyleText(CountText, Theme, EALKTextRole::Body, Theme.TextMuted);
	if (UALKFrame* Table = Cast<UALKFrame>(TablePanel))
	{
		Table->SetFrameStyle(Theme.MakePanelFrame());
	}
	if (UALKFrame* Detail = Cast<UALKFrame>(DetailPanel))
	{
		Detail->SetFrameStyle(Theme.MakePanelFrame());
	}
	if (HeaderRule)
	{
		HeaderRule->SetRuleColor(Theme.Separator);
	}
	for (UALKSpinner* Each : { RefreshSpinner.Get(), StateSpinner.Get() })
	{
		if (Each)
		{
			Each->SetSpinnerColor(Theme.GetAccentTextColor());
		}
	}
	FALKWidgetBuilder::StyleText(StateText, Theme, EALKTextRole::Subheading, Theme.TextSecondary);
	FALKWidgetBuilder::StyleText(StateDetailText, Theme, EALKTextRole::Body, Theme.TextMuted);
	FALKWidgetBuilder::StyleText(DetailEmptyText, Theme, EALKTextRole::Body, Theme.TextMuted);
	FALKWidgetBuilder::StyleEmptyState(DetailEmptyText, Theme);
	FALKWidgetBuilder::StyleIconDisc(StateIcon, Theme);

	if (DetailNameText)
	{
		FALKWidgetBuilder::StyleText(DetailNameText, Theme, EALKTextRole::Heading);
		FSlateFontInfo Font = Theme.HeadingFont;
		Font.Size = FMath::Max(12.f, FMath::RoundToFloat(Theme.HeadingFont.Size * 0.66f));
		FALKWidgetBuilder::SetTextFont(DetailNameText, Font);
		// A server's name is shown as its host wrote it.
		DetailNameText->SetTextTransformPolicy(ETextTransformPolicy::None);
	}
	FALKWidgetBuilder::StyleText(DetailSubText, Theme, EALKTextRole::Body, Theme.TextMuted);
	if (DetailLockIcon)
	{
		DetailLockIcon->SetIcon(EALKIcon::Lock, Theme.TextMuted);
	}
	if (DetailPicture)
	{
		DetailPicture->SetCornerRadius(FMath::Max(0.f, Theme.CornerRadius - 4.f));
	}
	FALKWidgetBuilder::StyleChip(DetailStatusChip, Theme, Theme.BackgroundTop.CopyWithNewOpacity(0.82f), FLinearColor::Transparent, Theme.TextPrimary);
	for (int32 Row = 0; Row < DetailRowWidgets.Num(); ++Row)
	{
		ALKServerBrowserPrivate::StyleDetailLine(DetailNameTexts[Row], DetailValueTexts[Row], Theme);
	}

	ALKServerBrowserPrivate::StyleScrollBar(ServerList, Theme, 10.f);
	ALKServerBrowserPrivate::StyleScrollBar(DetailRows, Theme, 8.f);
	if (ServerList)
	{
		// The thin lines between the rows.
		for (UWidget* Child : ServerList->GetAllChildren())
		{
			if (UALKRule* Line = Cast<UALKRule>(Child))
			{
				Line->SetRuleColor(Theme.Separator.CopyWithNewOpacity(Theme.Separator.A * 0.5f));
			}
		}
	}

	for (bool& bFocused : HeaderFocused)
	{
		bFocused = false;
	}
	UpdateHeader();
	ShowDetails();
}

// ---------------------------------------------------------------- Opening and closing

void UALKServerBrowserScreen::NativeOnActivated()
{
	if (UALKOnlineSubsystem* Online = UALKOnlineSubsystem::Get(this))
	{
		if (!SessionsHandle.IsValid())
		{
			SessionsHandle = Online->OnSessionsFoundNative.AddUObject(this, &ThisClass::HandleSessionsFound);
			StateHandle = Online->OnStateChangedNative.AddUObject(this, &ThisClass::HandleStateChanged);
			ListsHandle = Online->OnListsChangedNative.AddUObject(this, &ThisClass::HandleListsChanged);
		}
	}
	UpdateChips();
	UpdateHeader();
	// What the last search found shows at once; the new search replaces it.
	RebuildList();
	bFocusPending = Rows.IsValidIndex(FocusedIndex);
	StartSearch();
	Super::NativeOnActivated();
}

void UALKServerBrowserScreen::NativeOnDeactivated()
{
	if (UALKOnlineSubsystem* Online = UALKOnlineSubsystem::Get(this))
	{
		Online->OnSessionsFoundNative.Remove(SessionsHandle);
		Online->OnStateChangedNative.Remove(StateHandle);
		Online->OnListsChangedNative.Remove(ListsHandle);
	}
	SessionsHandle.Reset();
	StateHandle.Reset();
	ListsHandle.Reset();
	bFocusPending = false;
	Super::NativeOnDeactivated();
}

UWidget* UALKServerBrowserScreen::NativeGetDesiredFocusTarget() const
{
	// What had the focus last (a chip, Join game...), else the game the details show.
	if (UWidget* Remembered = GetLastFocusedWidget())
	{
		return Remembered;
	}
	if (Rows.IsValidIndex(FocusedIndex) && Rows[FocusedIndex])
	{
		return Rows[FocusedIndex];
	}
	return Super::NativeGetDesiredFocusTarget();
}

void UALKServerBrowserScreen::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// The row that should have the focus takes it once the table is laid out.
	if (bFocusPending)
	{
		UALKServerRow* Row = Rows.IsValidIndex(FocusedIndex) ? Rows[FocusedIndex].Get() : nullptr;
		if (!Row)
		{
			bFocusPending = false;
		}
		else if (IsActivated() && Row->GetCachedGeometry().GetLocalSize().X > 1.0)
		{
			bFocusPending = false;
			if (!IsTypingSearch())
			{
				Row->SetFocus();
				if (ServerList)
				{
					ServerList->ScrollWidgetIntoView(Row, false, EDescendantScrollDestination::IntoView, 24.f);
				}
			}
		}
	}

	// The header's titles have no box: their outline follows the focus.
	const UALKTheme* Theme = GetTheme();
	for (int32 Index = 0; Theme && Index < HeaderButtons.Num(); ++Index)
	{
		const UALKButton* Button = HeaderButtons[Index];
		const bool bFocused = Button && (Button->HasFocusedDescendants() || Button->HasAnyUserFocus());
		if (bFocused != HeaderFocused[Index])
		{
			HeaderFocused[Index] = bFocused;
			HeaderFrames[Index]->SetFrameStyle(Theme->MakeChipFrame(FLinearColor::Transparent, bFocused ? Theme->Accent : FLinearColor::Transparent));
		}
	}

	// The rows get narrower while the table's scroll bar shows: the header steps in by what the bar takes, so the columns
	// still line up. Measured, not assumed: the bar's width is the engine's business.
	if (ServerList && HeaderRow)
	{
		float BarSpace = Rows.Num() > 0 ? HeaderInset : 0.f;
		const UALKServerRow* FirstRow = Rows.Num() > 0 ? Rows[0].Get() : nullptr;
		const float ListWidth = ServerList->GetCachedGeometry().GetLocalSize().X;
		const float RowWidth = FirstRow ? FirstRow->GetCachedGeometry().GetLocalSize().X : 0.f;
		if (ListWidth > 1.f && RowWidth > 1.f)
		{
			BarSpace = FMath::Max(0.f, ListWidth - RowWidth - ALKServerBrowserPrivate::RowEndInset);
		}
		if (!FMath::IsNearlyEqual(BarSpace, HeaderInset, 0.5f))
		{
			HeaderInset = BarSpace;
			if (UVerticalBoxSlot* HeaderSlot = Cast<UVerticalBoxSlot>(HeaderRow->Slot))
			{
				HeaderSlot->SetPadding(FMargin(ALKServerColumns::SidePadding, 0.f, ALKServerColumns::SidePadding + ALKServerBrowserPrivate::RowEndInset + BarSpace, 0.f));
			}
		}
	}
}

// ---------------------------------------------------------------- Tabs, filters, sorting

void UALKServerBrowserScreen::SetActiveTab(EALKServerBrowserTab Tab)
{
	const bool bChanged = Tab != ActiveTab;
	ActiveTab = Tab;
	if (bChanged)
	{
		PlayThemeSound(EALKUISound::Tab);
		// Another list: start at its top.
		FocusedKey.Reset();
		FocusedIndex = 0;
	}
	UpdateHeader();
	RebuildList();
	if (bChanged)
	{
		StartSearch();
		if (ServerList)
		{
			ServerList->ScrollToStart();
		}
	}
}

void UALKServerBrowserScreen::StepTab(int32 Direction)
{
	const int32 Count = ALKServerBrowserPrivate::TabCount;
	int32 Index = 0;
	for (int32 Each = 0; Each < Count; ++Each)
	{
		if (ALKServerBrowserPrivate::TabOrder[Each] == ActiveTab)
		{
			Index = Each;
		}
	}
	SetActiveTab(ALKServerBrowserPrivate::TabOrder[((Index + Direction) % Count + Count) % Count]);
}

void UALKServerBrowserScreen::UpdateTabs()
{
	const UALKOnlineSubsystem* Online = UALKOnlineSubsystem::Get(this);
	for (int32 Index = 0; Index < TabButtons.Num(); ++Index)
	{
		const EALKServerBrowserTab Tab = ALKServerBrowserPrivate::TabOrder[Index];
		FText Name;
		int32 Count = 0;
		FALKSessionFilter CountFilter = Filter;
		switch (Tab)
		{
		case EALKServerBrowserTab::Favourites:
			Name = LOCTEXT("BrowserTabFavourites", "Favourites");
			Count = Online ? Online->GetFavouriteServers().Num() : 0;
			break;
		case EALKServerBrowserTab::Recent:
			Name = LOCTEXT("BrowserTabRecent", "Recent");
			Count = Online ? Online->GetRecentServers().Num() : 0;
			break;
		case EALKServerBrowserTab::LAN:
			Name = LOCTEXT("BrowserTabLAN", "LAN");
			CountFilter.bLANOnly = true;
			Count = Online ? Online->GetSearchResults(CountFilter, SortColumn, bSortAscending).Num() : 0;
			break;
		default:
			Name = LOCTEXT("BrowserTabAll", "All");
			CountFilter.bLANOnly = false;
			Count = Online ? Online->GetSearchResults(CountFilter, SortColumn, bSortAscending).Num() : 0;
			break;
		}
		TabButtons[Index]->SetButtonText(FText::Format(LOCTEXT("BrowserTabWithCount", "{0}   {1}"), Name, FText::AsNumber(Count)));
		TabButtons[Index]->SetActiveVisual(Tab == ActiveTab);
	}
}

void UALKServerBrowserScreen::UpdateChips()
{
	auto SetChip = [](UALKButton* Chip, bool bOn)
	{
		if (Chip)
		{
			Chip->SetActiveVisual(bOn);
			Chip->SetLeadingIcon(bOn ? EALKIcon::Check : EALKIcon::None);
		}
	};
	SetChip(NotFullChip, Filter.bHideFull);
	SetChip(NotEmptyChip, Filter.bHideEmpty);
	SetChip(NoPasswordChip, Filter.bHidePasswordProtected);

	if (RegionChip)
	{
		// Only when the provider knows regions (the first, empty one is "any").
		const UALKOnlineSubsystem* Online = UALKOnlineSubsystem::Get(this);
		bool bHasRegions = false;
		for (const FALKOnlineOption& Region : Online ? Online->GetRegions() : TArray<FALKOnlineOption>())
		{
			bHasRegions |= !Region.Id.IsEmpty();
		}
		RegionChip->SetVisibility(bHasRegions ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		RegionChip->SetButtonText(FText::Format(LOCTEXT("BrowserRegionChip", "Region: {0}"), Filter.Region.IsEmpty() ? LOCTEXT("BrowserRegionAny", "Any") : GetRegionName(Filter.Region)));
		RegionChip->SetActiveVisual(!Filter.Region.IsEmpty());
	}
}

void UALKServerBrowserScreen::ToggleFilter(int32 ChipIndex)
{
	switch (ChipIndex)
	{
	case 0: Filter.bHideFull = !Filter.bHideFull; break;
	case 1: Filter.bHideEmpty = !Filter.bHideEmpty; break;
	default: Filter.bHidePasswordProtected = !Filter.bHidePasswordProtected; break;
	}
	PlayThemeSound(EALKUISound::Change);
	UpdateChips();
	// The search already has every game: the chips only choose which of them show.
	RebuildList();
}

void UALKServerBrowserScreen::CycleRegion()
{
	const UALKOnlineSubsystem* Online = UALKOnlineSubsystem::Get(this);
	TArray<FString> Ids;
	Ids.Add(FString());
	for (const FALKOnlineOption& Region : Online ? Online->GetRegions() : TArray<FALKOnlineOption>())
	{
		if (!Region.Id.IsEmpty())
		{
			Ids.AddUnique(Region.Id);
		}
	}
	const int32 Index = FMath::Max(0, Ids.IndexOfByKey(Filter.Region));
	Filter.Region = Ids[(Index + 1) % Ids.Num()];
	PlayThemeSound(EALKUISound::Change);
	UpdateChips();
	RebuildList();
	// The backend may keep games of other regions back: ask again.
	StartSearch();
}

FText UALKServerBrowserScreen::GetRegionName(const FString& RegionId) const
{
	const UALKOnlineSubsystem* Online = UALKOnlineSubsystem::Get(this);
	for (const FALKOnlineOption& Region : Online ? Online->GetRegions() : TArray<FALKOnlineOption>())
	{
		if (Region.Id.Equals(RegionId, ESearchCase::IgnoreCase) && !Region.DisplayName.IsEmpty())
		{
			return Region.DisplayName;
		}
	}
	return FText::FromString(RegionId);
}

void UALKServerBrowserScreen::SortBy(EALKSessionSort Column)
{
	if (bSortPicked && SortColumn == Column)
	{
		bSortAscending = !bSortAscending;
	}
	else if (SortColumn == Column && ActiveTab != EALKServerBrowserTab::Recent)
	{
		// The column the table already sorts by.
		bSortAscending = !bSortAscending;
	}
	else
	{
		SortColumn = Column;
		bSortAscending = true;
	}
	bSortPicked = true;
	PlayThemeSound(EALKUISound::Change);
	UpdateHeader();
	RebuildList();
}

void UALKServerBrowserScreen::UpdateHeader()
{
	const UALKTheme* Theme = GetTheme();
	// Recent stays newest first until the player picks a column.
	const bool bSorted = ActiveTab != EALKServerBrowserTab::Recent || bSortPicked;
	for (int32 Index = 0; Index < HeaderArrows.Num(); ++Index)
	{
		const bool bActive = bSorted && HeaderColumns[Index] == SortColumn;
		HeaderArrows[Index]->SetVisibility(bActive ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
		HeaderArrows[Index]->SetIcon(bSortAscending ? EALKIcon::ChevronDown : EALKIcon::ChevronUp, Theme ? Theme->TextMuted : FLinearColor::White);
	}
}

// ---------------------------------------------------------------- The list

void UALKServerBrowserScreen::StartSearch()
{
	UALKOnlineSubsystem* Online = UALKOnlineSubsystem::Get(this);
	if (!Online || IsShowingBookmarks() || Online->GetState() != EALKOnlineState::Idle)
	{
		UpdateState();
		return;
	}
	// The search asks for everything in the region; the chips and the search box choose from what came back.
	FALKSessionFilter SearchFilter;
	SearchFilter.bLANOnly = ActiveTab == EALKServerBrowserTab::LAN;
	SearchFilter.Region = Filter.Region;
	Online->FindGames(SearchFilter);
	UpdateState();
}

void UALKServerBrowserScreen::RefreshGames()
{
	PlayThemeSound(EALKUISound::Click);
	if (IsShowingBookmarks())
	{
		RebuildList();
		return;
	}
	StartSearch();
}

FString UALKServerBrowserScreen::MakeEntryKey(const FALKSessionInfo& Session)
{
	return Session.SessionId + TEXT("|") + Session.GetFavouriteKey();
}

void UALKServerBrowserScreen::CollectEntries(TArray<FALKServerBookmark>& OutEntries) const
{
	const UALKOnlineSubsystem* Online = UALKOnlineSubsystem::Get(this);
	if (!Online)
	{
		return;
	}

	if (!IsShowingBookmarks())
	{
		FALKSessionFilter ListFilter = Filter;
		ListFilter.bLANOnly = ActiveTab == EALKServerBrowserTab::LAN;
		for (const FALKSessionInfo& Session : Online->GetSearchResults(ListFilter, SortColumn, bSortAscending))
		{
			FALKServerBookmark& Entry = OutEntries.AddDefaulted_GetRef();
			Entry.Session = Session;
		}
		return;
	}

	// Remembered games, as they were last seen. The same chips and search box choose from them.
	const TArray<FALKServerBookmark> Bookmarks = ActiveTab == EALKServerBrowserTab::Favourites ? Online->GetFavouriteServers() : Online->GetRecentServers();
	FALKSessionFilter ListFilter = Filter;
	ListFilter.bLANOnly = false;
	TArray<FALKServerBookmark> Kept;
	for (const FALKServerBookmark& Bookmark : Bookmarks)
	{
		if (ListFilter.Matches(Bookmark.Session))
		{
			Kept.Add(Bookmark);
		}
	}
	if (ActiveTab == EALKServerBrowserTab::Recent && !bSortPicked)
	{
		// Newest first, as the subsystem gives them.
		OutEntries = MoveTemp(Kept);
		return;
	}
	TArray<FALKSessionInfo> Sorted;
	for (const FALKServerBookmark& Bookmark : Kept)
	{
		Sorted.Add(Bookmark.Session);
	}
	FALKSessionFilter::Sort(Sorted, SortColumn, bSortAscending);
	for (const FALKSessionInfo& Session : Sorted)
	{
		const FString Key = MakeEntryKey(Session);
		const int32 Found = Kept.IndexOfByPredicate([&Key](const FALKServerBookmark& Each) { return MakeEntryKey(Each.Session) == Key; });
		if (Found != INDEX_NONE)
		{
			OutEntries.Add(Kept[Found]);
			Kept.RemoveAt(Found);
		}
	}
}

bool UALKServerBrowserScreen::IsAnyRowFocused() const
{
	for (const UALKServerRow* Row : Rows)
	{
		if (Row && (Row->HasFocusedDescendants() || Row->HasAnyUserFocus()))
		{
			return true;
		}
	}
	return false;
}

bool UALKServerBrowserScreen::IsTypingSearch() const
{
	return SearchField && SearchField->IsTyping();
}

void UALKServerBrowserScreen::RebuildList()
{
	if (!ServerList)
	{
		return;
	}
	const UALKOnlineSubsystem* Online = UALKOnlineSubsystem::Get(this);
	const UALKTheme* Theme = GetTheme();

	// A row that has the focus is about to go: a row takes the focus again afterwards. The same when nothing on the
	// screen had it (the table was empty). The search box and the buttons keep theirs.
	const bool bRowHadFocus = IsAnyRowFocused();
	const bool bRefocus = IsActivated() && (bRowHadFocus || HasUserFocus(GetOwningPlayer()));

	ServerList->ClearChildren();
	Rows.Reset();
	Entries.Reset();
	CollectEntries(Entries);

	// The same game as before when it is still listed, else the same place in the list.
	int32 NewIndex = FocusedKey.IsEmpty() ? INDEX_NONE : Entries.IndexOfByPredicate([this](const FALKServerBookmark& Each) { return MakeEntryKey(Each.Session) == FocusedKey; });
	if (NewIndex == INDEX_NONE && Entries.Num() > 0)
	{
		NewIndex = FMath::Clamp(FocusedIndex, 0, Entries.Num() - 1);
	}
	FocusedIndex = NewIndex;
	FocusedKey = Entries.IsValidIndex(NewIndex) ? MakeEntryKey(Entries[NewIndex].Session) : FString();

	TSubclassOf<UALKServerRow> Class = RowClass;
	if (!Class)
	{
		Class = UALKServerRow::StaticClass();
	}
	const bool bShowPing = !Online || Online->IsShowPing();
	for (int32 Index = 0; Index < Entries.Num(); ++Index)
	{
		UALKServerRow* Row = CreateWidget<UALKServerRow>(this, Class);
		if (!Row)
		{
			continue;
		}
		if (Index > 0 && WidgetTree)
		{
			// A thin line between two rows.
			UALKRule* Line = FALKWidgetBuilder::Make<UALKRule>(*WidgetTree);
			if (Theme)
			{
				Line->SetRuleColor(Theme->Separator.CopyWithNewOpacity(Theme->Separator.A * 0.5f));
			}
			ServerList->AddChild(Line);
			if (UScrollBoxSlot* LineSlot = Cast<UScrollBoxSlot>(Line->Slot))
			{
				LineSlot->SetPadding(FMargin(ALKServerColumns::SidePadding, 0.f, ALKServerColumns::SidePadding + ALKServerBrowserPrivate::RowEndInset, 0.f));
			}
		}
		Row->EntryIndex = Index;
		Row->SetSession(Entries[Index].Session, Online && Online->IsFavourite(Entries[Index].Session), bShowPing);
		Row->OnRowFocused.AddUObject(this, &ThisClass::HandleRowFocused);
		const TWeakObjectPtr<UALKServerRow> WeakRow(Row);
		Row->OnClicked().AddWeakLambda(this, [this, WeakRow]() { HandleRowClicked(WeakRow.Get()); });
		ServerList->AddChild(Row);
		if (UScrollBoxSlot* RowSlot = Cast<UScrollBoxSlot>(Row->Slot))
		{
			RowSlot->SetPadding(FMargin(0.f, 2.f, ALKServerBrowserPrivate::RowEndInset, 2.f));
			RowSlot->SetHorizontalAlignment(HAlign_Fill);
		}
		// Right always goes to Join game, whatever row it starts from (set once the row is in the list: the rule is
		// written into its Slate widget).
		if (JoinButton)
		{
			Row->SetNavigationRuleExplicit(EUINavigation::Right, JoinButton);
		}
		Rows.Add(Row);
	}

	UpdateTabs();
	UpdateState();
	ShowDetails();

	if (bRefocus)
	{
		if (Rows.IsValidIndex(FocusedIndex))
		{
			bFocusPending = true;
		}
		else if (bRowHadFocus)
		{
			// Nothing left to focus: the screen keeps the keys.
			SetFocus();
		}
	}
}

void UALKServerBrowserScreen::UpdateState()
{
	const UALKOnlineSubsystem* Online = UALKOnlineSubsystem::Get(this);
	const bool bSearching = Online && Online->GetState() == EALKOnlineState::Searching && !IsShowingBookmarks();
	const bool bEmpty = Entries.Num() == 0;

	if (RefreshSpinner)
	{
		RefreshSpinner->SetVisibility(bSearching ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (CountText)
	{
		CountText->SetText(bSearching && bEmpty ? LOCTEXT("BrowserSearching", "Looking for games...")
			: FText::Format(LOCTEXT("BrowserCount", "{0} {0}|plural(one=game,other=games) found"), Entries.Num()));
	}
	if (StateBox)
	{
		StateBox->SetVisibility(bEmpty ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (StateSpinner)
	{
		StateSpinner->SetVisibility(bSearching ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (StateIcon)
	{
		StateIcon->SetVisibility(bSearching ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
	if (!bEmpty)
	{
		return;
	}

	FText Line;
	FText Detail;
	EALKIcon Icon = EALKIcon::Search;
	if (bSearching)
	{
		Line = LOCTEXT("BrowserSearching", "Looking for games...");
	}
	else if (!Online || !Online->IsOnlineEnabled())
	{
		Line = LOCTEXT("BrowserOffline", "Online play is turned off");
		Icon = EALKIcon::Globe;
	}
	else
	{
		switch (ActiveTab)
		{
		case EALKServerBrowserTab::Favourites:
			Icon = EALKIcon::StarOutline;
			Line = LOCTEXT("BrowserNoFavourites", "No favourites yet");
			Detail = LOCTEXT("BrowserNoFavouritesDetail", "Mark a game with the star to find it here again.");
			break;
		case EALKServerBrowserTab::Recent:
			Icon = EALKIcon::Clock;
			Line = LOCTEXT("BrowserNoRecent", "No recent games");
			Detail = LOCTEXT("BrowserNoRecentDetail", "The games you join show up here.");
			break;
		case EALKServerBrowserTab::LAN:
			Line = LOCTEXT("BrowserNoLAN", "No games on your network");
			Detail = LOCTEXT("BrowserNoGamesDetail", "Try other filters, refresh, or host a game of your own.");
			break;
		default:
			Line = LOCTEXT("BrowserNoGames", "No games found");
			Detail = LOCTEXT("BrowserNoGamesDetail", "Try other filters, refresh, or host a game of your own.");
			break;
		}
	}
	FALKWidgetBuilder::SetIconDiscIcon(StateIcon, Icon);
	if (StateText)
	{
		StateText->SetText(Line);
	}
	if (StateDetailText)
	{
		StateDetailText->SetText(Detail);
		StateDetailText->SetVisibility(Detail.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
}

// ---------------------------------------------------------------- The focused game

bool UALKServerBrowserScreen::GetFocusedGame(FALKSessionInfo& OutSession) const
{
	if (!Entries.IsValidIndex(FocusedIndex))
	{
		return false;
	}
	OutSession = Entries[FocusedIndex].Session;
	return true;
}

bool UALKServerBrowserScreen::GetEntryAddress(const FALKServerBookmark& Entry, FString& OutAddress) const
{
	if (!Entry.ConnectString.IsEmpty())
	{
		OutAddress = Entry.ConnectString;
		return true;
	}
	const UALKOnlineSubsystem* Online = UALKOnlineSubsystem::Get(this);
	const UALKOnlineProvider* Provider = Online ? Online->GetProvider() : nullptr;
	return Provider && Provider->GetSessionConnectString(Entry.Session, OutAddress) && !OutAddress.IsEmpty();
}

void UALKServerBrowserScreen::SetDetailRow(int32& InOutRow, const FText& Name, const FText& Value)
{
	if (!DetailRows || !WidgetTree || Value.IsEmpty())
	{
		return;
	}
	const int32 Row = InOutRow++;
	if (!DetailRowWidgets.IsValidIndex(Row))
	{
		// name ......... value, with a thin line under it
		UWidgetTree& Tree = *WidgetTree;
		UVerticalBox* Line = FALKWidgetBuilder::Make<UVerticalBox>(Tree);
		UHorizontalBox* Pair = FALKWidgetBuilder::Make<UHorizontalBox>(Tree);
		UTextBlock* NameText = FALKWidgetBuilder::MakeText(Tree, NAME_None);
		FALKWidgetBuilder::AddToHorizontalBox(Pair, NameText, FMargin(0.f, 0.f, 18.f, 0.f), HAlign_Left, VAlign_Top);
		UTextBlock* ValueText = FALKWidgetBuilder::MakeText(Tree, NAME_None);
		ValueText->SetJustification(ETextJustify::Right);
		ValueText->SetAutoWrapText(true);
		FALKWidgetBuilder::AddToHorizontalBox(Pair, ValueText, FMargin(), HAlign_Fill, VAlign_Top, 1.f);
		FALKWidgetBuilder::AddToVerticalBox(Line, Pair, FMargin(0.f, 5.f, 0.f, 5.f));
		UALKRule* Rule = FALKWidgetBuilder::Make<UALKRule>(Tree);
		FALKWidgetBuilder::AddToVerticalBox(Line, Rule);
		DetailRows->AddChild(Line);
		if (UScrollBoxSlot* LineSlot = Cast<UScrollBoxSlot>(Line->Slot))
		{
			LineSlot->SetPadding(FMargin(0.f, 0.f, 8.f, 0.f));
			LineSlot->SetHorizontalAlignment(HAlign_Fill);
		}
		if (const UALKTheme* Theme = GetTheme())
		{
			ALKServerBrowserPrivate::StyleDetailLine(NameText, ValueText, *Theme);
			Rule->SetRuleColor(Theme->Separator.CopyWithNewOpacity(Theme->Separator.A * 0.6f));
		}
		DetailRowWidgets.Add(Line);
		DetailNameTexts.Add(NameText);
		DetailValueTexts.Add(ValueText);
	}
	DetailNameTexts[Row]->SetText(Name);
	DetailValueTexts[Row]->SetText(Value);
	DetailRowWidgets[Row]->SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UALKServerBrowserScreen::ShowDetails()
{
	const UALKOnlineSubsystem* Online = UALKOnlineSubsystem::Get(this);
	const UALKOnlineConfig* Config = UALKDeveloperSettings::GetOnlineConfig();
	const bool bHasEntry = Entries.IsValidIndex(FocusedIndex);

	if (DetailBox)
	{
		DetailBox->SetVisibility(bHasEntry ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (DetailEmptyText)
	{
		FALKWidgetBuilder::SetEmptyStateShown(DetailEmptyText, !bHasEntry);
	}

	// Left from the buttons goes back to the game they are about.
	UALKServerRow* FocusedRow = Rows.IsValidIndex(FocusedIndex) ? Rows[FocusedIndex].Get() : nullptr;
	for (UALKButton* Button : { JoinButton.Get(), FavouriteButton.Get() })
	{
		if (Button && FocusedRow)
		{
			Button->SetNavigationRuleExplicit(EUINavigation::Left, FocusedRow);
		}
		else if (Button)
		{
			Button->SetNavigationRuleBase(EUINavigation::Left, EUINavigationRule::Escape);
		}
	}
	if (!bHasEntry)
	{
		return;
	}

	const FALKServerBookmark& Entry = Entries[FocusedIndex];
	const FALKSessionInfo& Session = Entry.Session;
	const FText ModeName = Config->GetModeName(Session.ModeId);
	const FText MapName = Config->GetMapName(Session.MapId);

	if (DetailPicture)
	{
		// The map's picture when the config has one; else art drawn from the map's Id.
		const FALKMapDefinition* Map = Config->FindMap(Session.MapId);
		DetailPicture->SetPicture(Map ? Map->Thumbnail.LoadSynchronous() : nullptr, Session.MapId.IsNone() ? Session.ModeId : Session.MapId);
	}
	if (DetailNameText)
	{
		DetailNameText->SetText(FText::FromString(Session.ServerName));
	}
	if (DetailLockIcon)
	{
		DetailLockIcon->SetVisibility(Session.bPasswordProtected ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (DetailStatusChip)
	{
		DetailStatusChip->SetVisibility(Session.bInProgress ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (DetailSubText)
	{
		DetailSubText->SetText(ModeName.IsEmpty() || MapName.IsEmpty() ? (ModeName.IsEmpty() ? MapName : ModeName)
			: FText::Format(LOCTEXT("BrowserModeMap", "{0}  \u00b7  {1}"), ModeName, MapName));
	}

	// ---- The lines
	int32 Row = 0;
	SetDetailRow(Row, LOCTEXT("BrowserDetailPlayers", "Players"), FText::Format(LOCTEXT("BrowserPlayersValue", "{0} / {1}"), FText::AsNumber(Session.CurrentPlayers), FText::AsNumber(Session.MaxPlayers)));
	if (Session.PingMs >= 0 && (!Online || Online->IsShowPing()))
	{
		SetDetailRow(Row, LOCTEXT("BrowserDetailPing", "Ping"), FText::Format(LOCTEXT("BrowserPingValue", "{0} ms"), FText::AsNumber(Session.PingMs)));
	}
	if (Session.bLAN)
	{
		SetDetailRow(Row, LOCTEXT("BrowserDetailRegion", "Region"), LOCTEXT("BrowserRegionLAN", "Local network"));
	}
	else if (!Session.Region.IsEmpty())
	{
		SetDetailRow(Row, LOCTEXT("BrowserDetailRegion", "Region"), GetRegionName(Session.Region));
	}
	// The rules the host advertises: the ones the config knows by their name, in its order, then the others.
	TArray<FName> Shown;
	for (const FALKRuleDefinition& Definition : Config->RuleDefinitions)
	{
		if (const FString* Value = Session.Rules.Find(Definition.Id))
		{
			Shown.Add(Definition.Id);
			SetDetailRow(Row, Definition.DisplayName.IsEmpty() ? FText::FromName(Definition.Id) : Definition.DisplayName, Definition.GetValueText(*Value));
		}
	}
	TArray<FName> Others;
	Session.Rules.GetKeys(Others);
	Others.Sort(FNameLexicalLess());
	for (const FName Id : Others)
	{
		if (!Shown.Contains(Id))
		{
			SetDetailRow(Row, FText::FromName(Id), FText::FromString(Session.Rules[Id]));
		}
	}
	if (!Session.BuildVersion.IsEmpty())
	{
		SetDetailRow(Row, LOCTEXT("BrowserDetailVersion", "Version"), Session.bVersionMatches
			? FText::Format(LOCTEXT("BrowserVersionMatches", "{0}  \u00b7  matches"), FText::FromString(Session.BuildVersion))
			: FText::Format(LOCTEXT("BrowserVersionDiffers", "{0}  \u00b7  different"), FText::FromString(Session.BuildVersion)));
	}
	for (int32 Unused = Row; Unused < DetailRowWidgets.Num(); ++Unused)
	{
		DetailRowWidgets[Unused]->SetVisibility(ESlateVisibility::Collapsed);
	}

	if (FavouriteButton)
	{
		FavouriteButton->SetButtonText(Online && Online->IsFavourite(Session) ? LOCTEXT("BrowserRemoveFavourite", "Remove favourite") : LOCTEXT("BrowserAddFavourite", "Add favourite"));
	}
	if (CopyButton)
	{
		// Only when the address is known without joining. Without it, Add favourite takes the whole line.
		FString Address;
		const bool bKnown = GetEntryAddress(Entry, Address);
		const bool bHadFocus = CopyButton->HasFocusedDescendants() || CopyButton->HasAnyUserFocus();
		CopyButton->SetVisibility(bKnown ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		if (!bKnown && bHadFocus && FavouriteButton)
		{
			FavouriteButton->SetFocus();
		}
	}
}

void UALKServerBrowserScreen::HandleRowFocused(UALKServerRow* Row)
{
	if (!Row || !Entries.IsValidIndex(Row->EntryIndex) || Row->EntryIndex == FocusedIndex)
	{
		return;
	}
	FocusedIndex = Row->EntryIndex;
	FocusedKey = MakeEntryKey(Entries[FocusedIndex].Session);
	ShowDetails();
}

void UALKServerBrowserScreen::HandleRowClicked(UALKServerRow* Row)
{
	if (Row)
	{
		JoinEntry(Row->EntryIndex);
	}
}

void UALKServerBrowserScreen::JoinFocusedGame()
{
	JoinEntry(FocusedIndex);
}

void UALKServerBrowserScreen::JoinEntry(int32 EntryIndex)
{
	UALKOnlineFlowSubsystem* Flow = UALKOnlineFlowSubsystem::Get(this);
	UALKOnlineSubsystem* Online = UALKOnlineSubsystem::Get(this);
	if (!Entries.IsValidIndex(EntryIndex) || !Flow || !Online)
	{
		PlayThemeSound(EALKUISound::Locked);
		return;
	}
	const FALKServerBookmark Entry = Entries[EntryIndex];
	if (!IsShowingBookmarks())
	{
		// A game the search found: the flow asks for the password when there is one.
		Flow->JoinGameWithPrompt(Entry.Session);
		return;
	}
	if (!Entry.Session.bPasswordProtected)
	{
		Online->JoinBookmark(Entry, FString());
		return;
	}

	// A remembered game with a password: ask for it, then join by what is remembered.
	FALKTextEntryDescriptor Descriptor;
	Descriptor.Title = LOCTEXT("PasswordTitle", "Password Needed");
	Descriptor.Message = FText::Format(LOCTEXT("PasswordMessage", "{0} is protected by a password."), FText::FromString(Entry.Session.ServerName));
	Descriptor.HintText = LOCTEXT("PasswordHint", "Password");
	Descriptor.ConfirmText = LOCTEXT("JoinByCodeConfirm", "Join");
	Descriptor.bIsPassword = true;
	Descriptor.MaxLength = 64;
	const TWeakObjectPtr<UALKOnlineSubsystem> WeakOnline(Online);
	Flow->ShowTextEntry(Descriptor, [WeakOnline, Entry](bool bConfirmed, const FString& Password, const FString&)
	{
		if (bConfirmed && WeakOnline.IsValid())
		{
			WeakOnline->JoinBookmark(Entry, Password);
		}
	});
}

void UALKServerBrowserScreen::ToggleFocusedFavourite()
{
	UALKOnlineSubsystem* Online = UALKOnlineSubsystem::Get(this);
	if (!Online || !Entries.IsValidIndex(FocusedIndex))
	{
		PlayThemeSound(EALKUISound::Locked);
		return;
	}
	const FALKSessionInfo Session = Entries[FocusedIndex].Session;
	PlayThemeSound(EALKUISound::Change);
	// The lists event that follows fills the table again (the star, the Favourites tab).
	Online->SetFavourite(Session, !Online->IsFavourite(Session));
}

void UALKServerBrowserScreen::CopyFocusedAddress()
{
	UALKOnlineFlowSubsystem* Flow = UALKOnlineFlowSubsystem::Get(this);
	FString Address;
	if (!Flow || !Entries.IsValidIndex(FocusedIndex) || !GetEntryAddress(Entries[FocusedIndex], Address))
	{
		PlayThemeSound(EALKUISound::Locked);
		return;
	}
	Flow->CopyToClipboard(Address, LOCTEXT("BrowserAddressCopied", "Address copied"));
}

// ---------------------------------------------------------------- Events and keys

void UALKServerBrowserScreen::HandleSessionsFound(const TArray<FALKSessionInfo>& Sessions)
{
	RebuildList();
}

void UALKServerBrowserScreen::HandleStateChanged(EALKOnlineState NewState, EALKOnlineState OldState)
{
	UpdateState();
}

void UALKServerBrowserScreen::HandleListsChanged()
{
	RebuildList();
}

void UALKServerBrowserScreen::HandleSearchChanged(const FText& Text)
{
	Filter.SearchText = Text.ToString();
	RebuildList();
}

void UALKServerBrowserScreen::HandleSearchCommitted(const FText& Text, ETextCommit::Type CommitMethod)
{
	// Enter in the search box goes on to the games it left.
	if (CommitMethod == ETextCommit::OnEnter)
	{
		FocusList();
	}
}

void UALKServerBrowserScreen::FocusList()
{
	if (UALKServerRow* Row = Rows.IsValidIndex(FocusedIndex) ? Rows[FocusedIndex].Get() : nullptr)
	{
		Row->SetFocus();
	}
	else if (RefreshButton)
	{
		RefreshButton->SetFocus();
	}
}

bool UALKServerBrowserScreen::NativeHandleMenuKey(const FKey& Key)
{
	// Letters belong to the search box while it has the keyboard.
	const bool bTyping = IsTypingSearch();
	if (Key == EKeys::Gamepad_LeftShoulder || (!bTyping && Key == EKeys::Q))
	{
		StepTab(-1);
		return true;
	}
	if (Key == EKeys::Gamepad_RightShoulder || (!bTyping && Key == EKeys::E))
	{
		StepTab(1);
		return true;
	}
	if (Key == EKeys::Gamepad_FaceButton_Top || (!bTyping && Key == EKeys::R))
	{
		RefreshGames();
		return true;
	}
	if (Key == EKeys::Gamepad_FaceButton_Left || (!bTyping && Key == EKeys::F))
	{
		ToggleFocusedFavourite();
		return true;
	}
	return Super::NativeHandleMenuKey(Key);
}

bool UALKServerBrowserScreen::OnBackRequested_Implementation()
{
	// Back in the search box leaves the box, not the screen.
	if (IsTypingSearch())
	{
		FocusList();
		return true;
	}
	return Super::OnBackRequested_Implementation();
}

FReply UALKServerBrowserScreen::NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	// Down in the search box goes to the games (the box itself would only move its cursor).
	if (InKeyEvent.GetKey() == EKeys::Down && IsTypingSearch() && Rows.Num() > 0)
	{
		FocusList();
		return FReply::Handled();
	}
	return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
}

#undef LOCTEXT_NAMESPACE
