// Copyright 2026 CodeLikeMe. All Rights Reserved.

#include "OnlineUI/ALKLeaderboardScreen.h"
#include "OnlineUI/ALKOnlineFlowSubsystem.h"
#include "OnlineUI/ALKScoreRow.h"
#include "ALKDeveloperSettings.h"
#include "ALKMenuSubsystem.h"
#include "ALKTheme.h"
#include "Online/ALKOnlineSubsystem.h"
#include "Widgets/ALKButton.h"
#include "Widgets/ALKDecor.h"
#include "Widgets/ALKFrame.h"
#include "Widgets/ALKRootLayout.h"
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
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "HAL/PlatformInput.h"
#include "Styling/SlateTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ALKLeaderboardScreen)

#define LOCTEXT_NAMESPACE "AssetproLobbyKit"

namespace ALKLeaderboardScreenPrivate
{
	/** The content never gets wider than this (ultrawide screens). */
	static constexpr float MaxContentWidth = 1500.f;
	/** Width of the value column. */
	static constexpr float ValueWidth = 240.f;
	/** The local player's row is repeated under the list when it is further down than this many rows. */
	static constexpr int32 PinnedAfter = 10;
	/** Seconds to wait for rows before the screen says there are none. */
	static constexpr float LoadTimeout = 20.f;

	static void SetShown(UWidget* Widget, bool bShown, ESlateVisibility ShownAs = ESlateVisibility::HitTestInvisible)
	{
		if (Widget)
		{
			const ESlateVisibility Wanted = bShown ? ShownAs : ESlateVisibility::Collapsed;
			if (Widget->GetVisibility() != Wanted)
			{
				Widget->SetVisibility(Wanted);
			}
		}
	}
}

UALKLeaderboardScreen::UALKLeaderboardScreen(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bDimScene = true;
}

void UALKLeaderboardScreen::NativeBuildDefaultLayout(UWidgetTree& Tree)
{
	using namespace ALKLeaderboardScreenPrivate;

	UOverlay* Root = FALKWidgetBuilder::Make<UOverlay>(Tree, TEXT("Root"));
	Tree.RootWidget = Root;
	BackgroundImage = FALKWidgetBuilder::AddBackgroundLayers(Tree, Root);

	USizeBox* MainSize = FALKWidgetBuilder::Make<USizeBox>(Tree, TEXT("MainSize"));
	MainSize->SetWidthOverride(MaxContentWidth);
	FALKWidgetBuilder::AddToOverlay(Root, MainSize, HAlign_Center, VAlign_Fill);
	UVerticalBox* Main = FALKWidgetBuilder::Make<UVerticalBox>(Tree, TEXT("MainColumn"));
	MainSize->SetContent(Main);

	UALKTitleText* Title = FALKWidgetBuilder::Make<UALKTitleText>(Tree, TEXT("TitleText"));
	Title->SetText(LOCTEXT("LeaderboardsTitle", "Leaderboards"));
	TitleText = Title;
	FALKWidgetBuilder::AddToVerticalBox(Main, TitleText, FMargin(), HAlign_Left);

	// Tab row: [Q] Board Board Board [E]
	UHorizontalBox* TabRow = FALKWidgetBuilder::Make<UHorizontalBox>(Tree, TEXT("TabRow"));
	FALKWidgetBuilder::AddToVerticalBox(Main, TabRow, FMargin(0.f, 22.f, 0.f, 0.f), HAlign_Left);
	UHorizontalBox* PrevCap = BuildInputHints(Tree, { FALKInputHint(LOCTEXT("KeyQ", "Q"), EKeys::Gamepad_LeftShoulder, FText::GetEmpty())
		.WithAction(FSimpleDelegate::CreateUObject(this, &ThisClass::ShowPreviousBoard)) });
	FALKWidgetBuilder::AddToHorizontalBox(TabRow, PrevCap, FMargin(0.f, 0.f, 16.f, 0.f));
	TabBox = FALKWidgetBuilder::Make<UHorizontalBox>(Tree, TEXT("TabBox"));
	FALKWidgetBuilder::AddToHorizontalBox(TabRow, TabBox);
	UHorizontalBox* NextCap = BuildInputHints(Tree, { FALKInputHint(LOCTEXT("KeyE", "E"), EKeys::Gamepad_RightShoulder, FText::GetEmpty())
		.WithAction(FSimpleDelegate::CreateUObject(this, &ThisClass::ShowNextBoard)) });
	FALKWidgetBuilder::AddToHorizontalBox(TabRow, NextCap, FMargin(16.f, 0.f, 0.f, 0.f));

	// The list on a panel: column titles, the rows, and the local player's row again when it is far down.
	UALKFrame* List = FALKWidgetBuilder::Make<UALKFrame>(Tree, TEXT("ListPanel"));
	List->SetPadding(FMargin(20.f, 16.f, 20.f, 18.f));
	ListPanel = List;
	FALKWidgetBuilder::AddToVerticalBox(Main, List, FMargin(0.f, 22.f, 0.f, 0.f), HAlign_Fill, VAlign_Fill, 1.f);
	UVerticalBox* ListColumn = FALKWidgetBuilder::Make<UVerticalBox>(Tree, TEXT("ListColumn"));
	List->SetContent(ListColumn);

	UHorizontalBox* HeaderLine = FALKWidgetBuilder::Make<UHorizontalBox>(Tree, TEXT("HeaderLine"));
	FALKWidgetBuilder::AddToVerticalBox(ListColumn, HeaderLine, FMargin(UALKScoreRow::SidePadding, 0.f, UALKScoreRow::SidePadding, 10.f));
	RankHeader = FALKWidgetBuilder::MakeText(Tree, TEXT("RankHeader"), LOCTEXT("LeaderboardRank", "Rank"));
	RankHeader->SetJustification(ETextJustify::Center);
	FALKWidgetBuilder::AddToHorizontalBox(HeaderLine, FALKWidgetBuilder::WrapInSizeBox(Tree, RankHeader, 44.f, 0.f), FMargin(0.f, 0.f, 0.f, 0.f), HAlign_Left, VAlign_Center);
	PlayerHeader = FALKWidgetBuilder::MakeText(Tree, TEXT("PlayerHeader"), LOCTEXT("LeaderboardPlayer", "Player"));
	FALKWidgetBuilder::AddToHorizontalBox(HeaderLine, PlayerHeader, FMargin(44.f, 0.f, 0.f, 0.f), HAlign_Fill, VAlign_Center, 1.f);
	ValueHeader = FALKWidgetBuilder::MakeText(Tree, TEXT("ValueHeader"));
	ValueHeader->SetJustification(ETextJustify::Right);
	FALKWidgetBuilder::AddToHorizontalBox(HeaderLine, FALKWidgetBuilder::WrapInSizeBox(Tree, ValueHeader, ValueWidth, 0.f), FMargin(), HAlign_Right, VAlign_Center);

	UALKRule* HeaderRule = FALKWidgetBuilder::Make<UALKRule>(Tree, TEXT("HeaderRule"));
	FALKWidgetBuilder::AddToVerticalBox(ListColumn, HeaderRule, FMargin(0.f, 0.f, 0.f, 8.f));

	UOverlay* ListOverlay = FALKWidgetBuilder::Make<UOverlay>(Tree, TEXT("ListOverlay"));
	FALKWidgetBuilder::AddToVerticalBox(ListColumn, ListOverlay, FMargin(), HAlign_Fill, VAlign_Fill, 1.f);
	RowList = FALKWidgetBuilder::Make<UScrollBox>(Tree, TEXT("RowList"));
	RowList->SetAnimateWheelScrolling(true);
	FALKWidgetBuilder::AddToOverlay(ListOverlay, RowList);

	UVerticalBox* StatusBox = FALKWidgetBuilder::Make<UVerticalBox>(Tree, TEXT("StatusBox"));
	FALKWidgetBuilder::AddToOverlay(ListOverlay, StatusBox, HAlign_Center, VAlign_Center);
	Spinner = FALKWidgetBuilder::Make<UALKSpinner>(Tree, TEXT("Spinner"));
	Spinner->Diameter = 36.f;
	FALKWidgetBuilder::AddToVerticalBox(StatusBox, Spinner, FMargin(0.f, 0.f, 0.f, 16.f), HAlign_Center);
	// The spinner while loading, an icon over the line that says why there are no rows.
	StatusIcon = FALKWidgetBuilder::MakeIconDisc(Tree, TEXT("StatusIcon"), EALKIcon::List);
	FALKWidgetBuilder::AddToVerticalBox(StatusBox, StatusIcon, FMargin(0.f, 0.f, 0.f, 16.f), HAlign_Center);
	StatusText = FALKWidgetBuilder::MakeText(Tree, TEXT("StatusText"));
	StatusText->SetJustification(ETextJustify::Center);
	FALKWidgetBuilder::AddToVerticalBox(StatusBox, StatusText, FMargin(), HAlign_Center);

	PinnedBox = FALKWidgetBuilder::Make<UVerticalBox>(Tree, TEXT("PinnedBox"));
	FALKWidgetBuilder::AddToVerticalBox(ListColumn, PinnedBox, FMargin(0.f, 8.f, 0.f, 0.f));

	// Footer: [Enter] Profile  [R] Refresh  [Esc] Back
	UHorizontalBox* Hints = BuildInputHints(Tree, {
		FALKInputHint(LOCTEXT("KeyEnter", "Enter"), FPlatformInput::GetGamepadAcceptKey(), LOCTEXT("LeaderboardHintProfile", "Profile")),
		FALKInputHint(LOCTEXT("KeyR", "R"), EKeys::Gamepad_FaceButton_Top, LOCTEXT("LeaderboardHintRefresh", "Refresh"))
			.WithAction(FSimpleDelegate::CreateUObject(this, &ThisClass::RefreshBoard)),
		FALKInputHint(LOCTEXT("KeyEsc", "Esc"), FPlatformInput::GetGamepadBackKey(), LOCTEXT("HintBack", "Back"))
			.WithAction(FSimpleDelegate::CreateWeakLambda(this, [this]() { OnBackRequested(); }))
	});
	FALKWidgetBuilder::AddToVerticalBox(Main, Hints, FMargin(0.f, 24.f, 0.f, 0.f), HAlign_Left);
}

void UALKLeaderboardScreen::NativeApplyTheme(const UALKTheme& Theme)
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
		if (UALKRule* HeaderRule = Cast<UALKRule>(WidgetTree->FindWidget(TEXT("HeaderRule"))))
		{
			HeaderRule->SetRuleColor(Theme.Separator);
		}
	}
	if (TitleText)
	{
		FALKWidgetBuilder::StyleText(TitleText, Theme, EALKTextRole::Heading);
		FALKWidgetBuilder::SetTextFont(TitleText, Theme.HeadingFont);
	}
	if (UALKFrame* List = Cast<UALKFrame>(ListPanel))
	{
		List->SetFrameStyle(Theme.MakePanelFrame());
	}
	for (UTextBlock* Header : { RankHeader.Get(), PlayerHeader.Get(), ValueHeader.Get() })
	{
		FALKWidgetBuilder::StyleText(Header, Theme, EALKTextRole::Data, Theme.TextMuted);
		if (Header)
		{
			Header->SetTextTransformPolicy(ETextTransformPolicy::ToUpper);
		}
	}
	FALKWidgetBuilder::StyleText(StatusText, Theme, EALKTextRole::Body, Theme.TextMuted);
	FALKWidgetBuilder::StyleIconDisc(StatusIcon, Theme);
	if (Spinner)
	{
		Spinner->SetSpinnerColor(Theme.Accent);
	}
	StyleScrollBar(Theme);
	// The rows are made again: the colours of the first three ranks come from the theme.
	if (Rows.Num() > 0 && !bLoading)
	{
		FillRows();
	}
}

void UALKLeaderboardScreen::StyleScrollBar(const UALKTheme& Theme)
{
	if (!RowList)
	{
		return;
	}
	FScrollBarStyle BarStyle = RowList->GetWidgetBarStyle();
	const FSlateBrush Thumb = UALKTheme::MakeSolidBrush(Theme.Accent.CopyWithNewOpacity(0.85f));
	BarStyle.SetNormalThumbImage(Thumb);
	BarStyle.SetHoveredThumbImage(Thumb);
	BarStyle.SetDraggedThumbImage(Thumb);
	BarStyle.SetVerticalBackgroundImage(UALKTheme::MakeSolidBrush(Theme.TextPrimary.CopyWithNewOpacity(0.06f)));
	RowList->SetWidgetBarStyle(BarStyle);
	RowList->SetScrollbarThickness(FVector2D(3.0, 3.0));
	RowList->SetScrollbarPadding(FMargin(12.f, 0.f, 0.f, 0.f));
}

void UALKLeaderboardScreen::NativeOnActivated()
{
	const UALKOnlineConfig* Config = UALKDeveloperSettings::GetOnlineConfig();
	UALKOnlineSubsystem* Online = UALKOnlineSubsystem::Get(this);
	if (Online && !ReceivedHandle.IsValid())
	{
		ReceivedHandle = Online->OnLeaderboardReceivedNative.AddUObject(this, &ThisClass::HandleLeaderboardReceived);
	}

	// Coming back from a profile keeps the board and its rows; a fresh opening starts on the board it was opened for.
	const bool bReturning = Boards.Num() > 0 && Rows.Num() > 0 && GetLastFocusedWidget() != nullptr;
	if (!bReturning)
	{
		Boards = Config->Leaderboards;
		if (Boards.Num() == 0)
		{
			// No boards listed: one board the provider can fill.
			Boards.Add(FALKLeaderboardDefinition(TEXT("Global"), LOCTEXT("LeaderboardGlobal", "Global"), LOCTEXT("LeaderboardScore", "Score")));
		}
		const UALKOnlineFlowSubsystem* Flow = UALKOnlineFlowSubsystem::Get(this);
		const FName WantedBoard = Flow ? Flow->GetContextBoardId() : NAME_None;
		const int32 WantedIndex = Boards.IndexOfByPredicate([WantedBoard](const FALKLeaderboardDefinition& Each) { return Each.Id == WantedBoard; });
		RebuildTabs();
		ActiveBoard = INDEX_NONE;
		ShowBoard(FMath::Max(0, WantedIndex));
	}
	Super::NativeOnActivated();
}

void UALKLeaderboardScreen::NativeOnDeactivated()
{
	if (UALKOnlineSubsystem* Online = UALKOnlineSubsystem::Get(this))
	{
		Online->OnLeaderboardReceivedNative.Remove(ReceivedHandle);
	}
	ReceivedHandle.Reset();
	bLoading = false;
	Super::NativeOnDeactivated();
}

void UALKLeaderboardScreen::RebuildTabs()
{
	if (!TabBox)
	{
		return;
	}
	TabBox->ClearChildren();
	TabButtons.Reset();
	for (int32 Index = 0; Index < Boards.Num(); ++Index)
	{
		const FALKLeaderboardDefinition& Board = Boards[Index];
		UALKButton* TabButton = CreateMenuButton(Board.Name.IsEmpty() ? FText::FromName(Board.Id) : Board.Name, EALKButtonStyle::Tab);
		if (!TabButton)
		{
			continue;
		}
		// Tabs are switched with Q / E or the shoulder buttons, or clicked; they never take focus from the list.
		TabButton->SetIsFocusable(false);
		TabButton->bFocusOnHover = false;
		TabButton->bPlayClickSound = false;
		TabButton->OnClicked().AddUObject(this, &ThisClass::ShowBoard, Index);
		TabBox->AddChild(TabButton);
		if (UHorizontalBoxSlot* TabSlot = Cast<UHorizontalBoxSlot>(TabButton->Slot))
		{
			TabSlot->SetPadding(FMargin(Index == 0 ? 0.f : 6.f, 0.f, 0.f, 0.f));
			TabSlot->SetVerticalAlignment(VAlign_Center);
		}
		TabButtons.Add(TabButton);
	}
	// A single board needs no tab row.
	if (UWidget* TabRow = TabBox->GetParent())
	{
		TabRow->SetVisibility(Boards.Num() > 1 ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
}

FName UALKLeaderboardScreen::GetCurrentBoardId() const
{
	return Boards.IsValidIndex(ActiveBoard) ? Boards[ActiveBoard].Id : NAME_None;
}

void UALKLeaderboardScreen::ShowPreviousBoard()
{
	ShowBoard(ActiveBoard - 1);
}

void UALKLeaderboardScreen::ShowNextBoard()
{
	ShowBoard(ActiveBoard + 1);
}

void UALKLeaderboardScreen::ShowBoard(int32 BoardIndex)
{
	if (Boards.Num() == 0)
	{
		return;
	}
	const int32 NewBoard = (BoardIndex % Boards.Num() + Boards.Num()) % Boards.Num();
	const bool bChanged = NewBoard != ActiveBoard;
	const bool bFirst = ActiveBoard == INDEX_NONE;
	ActiveBoard = NewBoard;
	for (int32 Index = 0; Index < TabButtons.Num(); ++Index)
	{
		if (TabButtons[Index])
		{
			TabButtons[Index]->SetActiveVisual(Index == ActiveBoard);
		}
	}
	if (ValueHeader)
	{
		const FText Header = Boards[ActiveBoard].ValueHeader;
		ValueHeader->SetText(Header.IsEmpty() ? LOCTEXT("LeaderboardScore", "Score") : Header);
	}
	if (bChanged && !bFirst)
	{
		PlayThemeSound(EALKUISound::Tab);
	}
	RefreshBoard();
}

void UALKLeaderboardScreen::RefreshBoard()
{
	using namespace ALKLeaderboardScreenPrivate;
	Rows.Reset();
	RowWidgets.Reset();
	if (RowList)
	{
		RowList->ClearChildren();
	}
	if (PinnedBox)
	{
		PinnedBox->ClearChildren();
	}
	bFocusPending = false;

	UALKOnlineSubsystem* Online = UALKOnlineSubsystem::Get(this);
	if (!Online || !Online->IsOnlineEnabled() || !Boards.IsValidIndex(ActiveBoard))
	{
		bLoading = false;
		ShowStatus(LOCTEXT("LeaderboardUnavailable", "Leaderboards are not available right now."), false);
		return;
	}
	bLoading = true;
	LoadingTime = 0.f;
	ShowStatus(LOCTEXT("LeaderboardLoading", "Loading the leaderboard"), true);
	Online->QueryLeaderboard(Boards[ActiveBoard].Id, FMath::Max(1, MaxRows));
}

void UALKLeaderboardScreen::ShowStatus(const FText& Text, bool bSpinning)
{
	using namespace ALKLeaderboardScreenPrivate;
	if (StatusText)
	{
		StatusText->SetText(Text);
	}
	SetShown(Spinner, bSpinning);
	SetShown(StatusIcon, !bSpinning && !Text.IsEmpty());
	// The status sits where the rows would be.
	SetShown(StatusText ? StatusText->GetParent() : nullptr, !Text.IsEmpty() || bSpinning);
}

void UALKLeaderboardScreen::HandleLeaderboardReceived(FName BoardId, const TArray<FALKLeaderboardRow>& InRows)
{
	// Rows of a board that is no longer shown (the player switched tabs meanwhile) are dropped.
	if (BoardId != GetCurrentBoardId())
	{
		return;
	}
	bLoading = false;
	Rows = InRows;
	FillRows();
}

void UALKLeaderboardScreen::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	using namespace ALKLeaderboardScreenPrivate;

	if (bLoading)
	{
		// A provider without leaderboards never answers: say so instead of spinning for ever.
		LoadingTime += FMath::Min(InDeltaTime, 0.1f);
		if (LoadingTime >= LoadTimeout)
		{
			bLoading = false;
			ShowStatus(LOCTEXT("LeaderboardNoAnswer", "This leaderboard is not available.\nThe online service of this game may not have leaderboards."), false);
		}
	}

	// Focus the first row once the list is laid out, when this screen is the one in front.
	if (bFocusPending)
	{
		UALKScoreRow* First = RowWidgets.Num() > 0 ? RowWidgets[0].Get() : nullptr;
		if (!First)
		{
			bFocusPending = false;
		}
		else if (IsActivated() && First->GetCachedGeometry().GetLocalSize().X > 1.0)
		{
			bFocusPending = false;
			if (IsTopScreen())
			{
				First->SetFocus();
			}
		}
	}
}

bool UALKLeaderboardScreen::IsTopScreen() const
{
	UALKMenuSubsystem* Menu = GetMenuSubsystem();
	const UALKRootLayout* Layout = Menu ? Menu->GetRootLayout() : nullptr;
	return Layout && Layout->GetTopWidget() == this;
}

UALKScoreRow* UALKLeaderboardScreen::MakeRow(const FALKLeaderboardRow& Row, const UALKTheme& Theme)
{
	using namespace ALKLeaderboardScreenPrivate;
	const UALKOnlineSubsystem* Online = UALKOnlineSubsystem::Get(this);

	UALKScoreRow* Line = CreateWidget<UALKScoreRow>(this, UALKScoreRow::StaticClass());
	if (!Line)
	{
		return nullptr;
	}
	// Other players' names go through the streamer mode filter; the local player's own name is shown as it is.
	const FString ShownName = Row.bLocalPlayer || !Online ? Row.DisplayName : Online->GetDisplayName(Row.DisplayName, Row.Rank - 1);
	FNumberFormattingOptions UpToTwoDecimals;
	UpToTwoDecimals.SetMaximumFractionalDigits(2);

	Line->SetInteractive(true);
	Line->SetColumnWidths({ ValueWidth }, false);
	Line->SetIdentity(Row.Rank, ShownName, Row.UserId, !Row.bLocalPlayer && Online && Online->IsStreamerMode());
	Line->SetValues({ Row.ScoreText.IsEmpty() ? FText::AsNumber(Row.Score, &UpToTwoDecimals) : Row.ScoreText });
	Line->SetBoldColumn(0);
	Line->SetMarks(Row.bLocalPlayer, false, false, false, false, false);
	// The first three ranks are coloured; no pictures or medals.
	Line->SetRankColor(Row.Rank == 1 ? Theme.Warning : (Row.Rank == 2 ? Theme.TextSecondary : (Row.Rank == 3 ? Theme.AccentAlt : FLinearColor::Transparent)));

	TWeakObjectPtr<UALKScoreRow> WeakLine(Line);
	Line->OnClicked().AddWeakLambda(this, [this, WeakLine]() { HandleRowClicked(WeakLine.Get()); });
	return Line;
}

void UALKLeaderboardScreen::FillRows()
{
	using namespace ALKLeaderboardScreenPrivate;
	RowWidgets.Reset();
	if (RowList)
	{
		RowList->ClearChildren();
	}
	if (PinnedBox)
	{
		PinnedBox->ClearChildren();
	}
	const UALKTheme* Theme = GetTheme();
	if (!RowList || !Theme)
	{
		return;
	}
	if (Rows.Num() == 0)
	{
		ShowStatus(LOCTEXT("LeaderboardEmpty", "No scores yet. Play a match to get on the board."), false);
		return;
	}
	ShowStatus(FText::GetEmpty(), false);

	for (const FALKLeaderboardRow& Row : Rows)
	{
		UALKScoreRow* Line = MakeRow(Row, *Theme);
		if (!Line)
		{
			continue;
		}
		RowList->AddChild(Line);
		if (UScrollBoxSlot* LineSlot = Cast<UScrollBoxSlot>(Line->Slot))
		{
			LineSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 2.f));
			LineSlot->SetHorizontalAlignment(HAlign_Fill);
		}
		RowWidgets.Add(Line);
	}

	// The local player's own row again under the list when it is not among the first rows.
	const int32 LocalIndex = Rows.IndexOfByPredicate([](const FALKLeaderboardRow& Each) { return Each.bLocalPlayer; });
	if (PinnedBox && LocalIndex >= PinnedAfter)
	{
		UALKRule* PinnedRule = WidgetTree->ConstructWidget<UALKRule>(UALKRule::StaticClass());
		PinnedRule->SetRuleColor(Theme->Separator);
		FALKWidgetBuilder::AddToVerticalBox(PinnedBox, PinnedRule, FMargin(0.f, 0.f, 0.f, 8.f));
		if (UALKScoreRow* Pinned = MakeRow(Rows[LocalIndex], *Theme))
		{
			FALKWidgetBuilder::AddToVerticalBox(PinnedBox, Pinned);
		}
	}
	bFocusPending = true;
}

UWidget* UALKLeaderboardScreen::NativeGetDesiredFocusTarget() const
{
	if (UWidget* Remembered = GetLastFocusedWidget())
	{
		return Remembered;
	}
	if (RowWidgets.Num() > 0 && RowWidgets[0].IsValid())
	{
		return RowWidgets[0].Get();
	}
	return Super::NativeGetDesiredFocusTarget();
}

bool UALKLeaderboardScreen::NativeHandleMenuKey(const FKey& Key)
{
	if (Key == EKeys::Q || Key == EKeys::Gamepad_LeftShoulder)
	{
		ShowPreviousBoard();
		return true;
	}
	if (Key == EKeys::E || Key == EKeys::Gamepad_RightShoulder)
	{
		ShowNextBoard();
		return true;
	}
	if (Key == EKeys::R || Key == EKeys::Gamepad_FaceButton_Top)
	{
		RefreshBoard();
		return true;
	}
	return Super::NativeHandleMenuKey(Key);
}

void UALKLeaderboardScreen::HandleRowClicked(UALKScoreRow* Row)
{
	if (!Row || Row->GetUserId().IsEmpty())
	{
		return;
	}
	if (UALKOnlineFlowSubsystem* Flow = UALKOnlineFlowSubsystem::Get(this))
	{
		Flow->ShowProfile(Row->GetUserId(), Row->GetShownName());
	}
}

#undef LOCTEXT_NAMESPACE
