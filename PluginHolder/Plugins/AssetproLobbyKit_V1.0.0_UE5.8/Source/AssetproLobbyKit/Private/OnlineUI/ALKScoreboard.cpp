// Copyright 2026 CodeLikeMe. All Rights Reserved.

#include "OnlineUI/ALKScoreboard.h"
#include "OnlineUI/ALKOnlineFlowSubsystem.h"
#include "OnlineUI/ALKScoreRow.h"
#include "ALKDeveloperSettings.h"
#include "ALKMenuConfig.h"
#include "ALKTheme.h"
#include "Lobby/ALKLobbyGameState.h"
#include "Lobby/ALKLobbyPlayerState.h"
#include "Lobby/ALKLobbyStatics.h"
#include "Online/ALKOnlineSubsystem.h"
#include "Widgets/ALKDecor.h"
#include "Widgets/ALKFrame.h"
#include "Widgets/ALKInputLabels.h"
#include "Widgets/ALKTitleText.h"
#include "Widgets/ALKWidgetBuilder.h"
#include "Blueprint/WidgetTree.h"
#include "CommonInputSubsystem.h"
#include "CommonInputTypeEnum.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/Overlay.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Engine/World.h"
#include "Styling/SlateTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ALKScoreboard)

#define LOCTEXT_NAMESPACE "AssetproLobbyKit"

namespace ALKScoreboardPrivate
{
	/** Height of a table's header line. */
	static constexpr float HeaderHeight = 46.f;

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
}

UALKScoreboard::UALKScoreboard(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UALKScoreboard::NativeBuildDefaultLayout(UWidgetTree& Tree)
{
	UALKFrame* Frame = FALKWidgetBuilder::Make<UALKFrame>(Tree, TEXT("Panel"));
	Frame->SetPadding(FMargin(30.f, 26.f, 30.f, 20.f));
	Panel = Frame;
	Tree.RootWidget = Frame;

	UVerticalBox* Column = FALKWidgetBuilder::Make<UVerticalBox>(Tree, TEXT("PanelColumn"));
	Frame->SetContent(Column);

	// Header: mode over map and server name on the left, the clock on the right.
	UHorizontalBox* HeaderRow = FALKWidgetBuilder::Make<UHorizontalBox>(Tree, TEXT("HeaderRow"));
	FALKWidgetBuilder::AddToVerticalBox(Column, HeaderRow, FMargin(0.f, 0.f, 0.f, 18.f));
	UVerticalBox* Titles = FALKWidgetBuilder::Make<UVerticalBox>(Tree, TEXT("HeaderTitles"));
	FALKWidgetBuilder::AddToHorizontalBox(HeaderRow, Titles, FMargin(), HAlign_Fill, VAlign_Center, 1.f);
	ModeText = FALKWidgetBuilder::Make<UALKTitleText>(Tree, TEXT("ModeText"));
	FALKWidgetBuilder::AddToVerticalBox(Titles, ModeText, FMargin(), HAlign_Left);
	DetailText = FALKWidgetBuilder::MakeText(Tree, TEXT("DetailText"));
	FALKWidgetBuilder::AddToVerticalBox(Titles, DetailText, FMargin(0.f, 4.f, 0.f, 0.f), HAlign_Left);

	UVerticalBox* ClockBox = FALKWidgetBuilder::Make<UVerticalBox>(Tree, TEXT("ClockBox"));
	FALKWidgetBuilder::AddToHorizontalBox(HeaderRow, ClockBox, FMargin(20.f, 0.f, 0.f, 0.f), HAlign_Right, VAlign_Center);
	ClockText = FALKWidgetBuilder::MakeText(Tree, TEXT("ClockText"));
	ClockText->SetJustification(ETextJustify::Right);
	FALKWidgetBuilder::AddToVerticalBox(ClockBox, ClockText, FMargin(), HAlign_Right);
	ClockLabel = FALKWidgetBuilder::MakeText(Tree, TEXT("ClockLabel"), LOCTEXT("ScoreTimeLeft", "Time left"));
	FALKWidgetBuilder::AddToVerticalBox(ClockBox, ClockLabel, FMargin(0.f, 2.f, 0.f, 0.f), HAlign_Right);

	// The tables scroll when a full server does not fit.
	UOverlay* TableOverlay = FALKWidgetBuilder::Make<UOverlay>(Tree, TEXT("TableOverlay"));
	FALKWidgetBuilder::AddToVerticalBox(Column, TableOverlay, FMargin(), HAlign_Fill, VAlign_Fill, 1.f);
	TableList = FALKWidgetBuilder::Make<UScrollBox>(Tree, TEXT("TableList"));
	TableList->SetAnimateWheelScrolling(true);
	FALKWidgetBuilder::AddToOverlay(TableOverlay, TableList);
	EmptyText = FALKWidgetBuilder::MakeText(Tree, TEXT("EmptyText"), LOCTEXT("ScoreNoMatch", "No match in progress"));
	EmptyText->SetJustification(ETextJustify::Center);
	FALKWidgetBuilder::AddToOverlay(TableOverlay, FALKWidgetBuilder::MakeEmptyState(Tree, TEXT("EmptyState"), EALKIcon::Flag, EmptyText), HAlign_Center, VAlign_Center, FMargin(0.f, 48.f));

	// Footer: who is watching, and what the scoreboard can do.
	UHorizontalBox* FooterRow = FALKWidgetBuilder::Make<UHorizontalBox>(Tree, TEXT("FooterRow"));
	FALKWidgetBuilder::AddToVerticalBox(Column, FooterRow, FMargin(0.f, 12.f, 0.f, 0.f));
	SpectatorText = FALKWidgetBuilder::MakeText(Tree, TEXT("SpectatorText"));
	SpectatorText->SetClipping(EWidgetClipping::ClipToBounds);
	FALKWidgetBuilder::AddToHorizontalBox(FooterRow, SpectatorText, FMargin(), HAlign_Fill, VAlign_Center, 1.f);
	HintText = FALKWidgetBuilder::MakeText(Tree, TEXT("HintText"));
	FALKWidgetBuilder::AddToHorizontalBox(FooterRow, HintText, FMargin(20.f, 0.f, 0.f, 0.f), HAlign_Right, VAlign_Center);
}

void UALKScoreboard::NativeConstruct()
{
	Super::NativeConstruct();
	if (UALKOnlineSubsystem* Online = UALKOnlineSubsystem::Get(this))
	{
		if (!TalkingHandle.IsValid())
		{
			TalkingHandle = Online->OnPlayerTalkingNative.AddUObject(this, &ThisClass::HandlePlayerTalking);
		}
		if (!ListsHandle.IsValid())
		{
			// Mutes change the marks beside the names.
			ListsHandle = Online->OnListsChangedNative.AddUObject(this, &ThisClass::MarkDirty);
		}
	}
	Refresh();
}

void UALKScoreboard::NativeDestruct()
{
	if (UALKOnlineSubsystem* Online = UALKOnlineSubsystem::Get(this))
	{
		Online->OnPlayerTalkingNative.Remove(TalkingHandle);
		Online->OnListsChangedNative.Remove(ListsHandle);
	}
	TalkingHandle.Reset();
	ListsHandle.Reset();
	UnbindGameState();
	Super::NativeDestruct();
}

AALKLobbyGameState* UALKScoreboard::ResolveGameState()
{
	const UWorld* World = GetWorld();
	// The game state arrives a moment after a level change: look again until it is there.
	AALKLobbyGameState* GameState = World ? World->GetGameState<AALKLobbyGameState>() : nullptr;
	if (GameState != BoundGameState.Get())
	{
		UnbindGameState();
		BoundGameState = GameState;
		if (GameState)
		{
			GameState->OnLobbyStateChangedNative.AddUObject(this, &ThisClass::MarkDirty);
			GameState->OnPlayersChangedNative.AddUObject(this, &ThisClass::MarkDirty);
		}
		bDirty = true;
	}
	return GameState;
}

void UALKScoreboard::UnbindGameState()
{
	if (AALKLobbyGameState* Old = BoundGameState.Get())
	{
		Old->OnLobbyStateChangedNative.RemoveAll(this);
		Old->OnPlayersChangedNative.RemoveAll(this);
	}
	BoundGameState.Reset();
}

void UALKScoreboard::MarkDirty()
{
	bDirty = true;
}

void UALKScoreboard::HandlePlayerTalking(const FString& UserId, bool bTalking)
{
	if (bTalking)
	{
		Talkers.Add(UserId);
	}
	else
	{
		Talkers.Remove(UserId);
	}
	bDirty = true;
}

void UALKScoreboard::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	ResolveGameState();
	// Pings change without an event: read them once a second.
	SecondTimer += InDeltaTime;
	if (SecondTimer >= 1.f)
	{
		SecondTimer = 0.f;
		bDirty = true;
	}
	if (bDirty)
	{
		Refresh();
	}
	UpdateClock();
}

void UALKScoreboard::SetInteractive(bool bInInteractive)
{
	if (bInteractive != bInInteractive)
	{
		bInteractive = bInInteractive;
		// The lines are made again: whether they take focus is decided when they are created.
		BuiltSignature.Reset();
		Refresh();
	}
}

FText UALKScoreboard::FormatClock(float Seconds)
{
	const int32 Total = FMath::Max(0, FMath::CeilToInt(Seconds));
	return FText::FromString(FString::Printf(TEXT("%d:%02d"), Total / 60, Total % 60));
}

FString UALKScoreboard::GetShownPlayerName(const AALKLobbyPlayerState* Player)
{
	if (!Player)
	{
		return FString();
	}
	const UALKOnlineSubsystem* Online = UALKOnlineSubsystem::Get(Player);
	// The local player's own name is never hidden.
	if (!Online || Player->IsLocalPlayerState())
	{
		return Player->GetPlayerName();
	}
	return Online->GetDisplayName(Player->GetPlayerName(), Player->GetStableIndex());
}

TArray<FALKScoreColumn> UALKScoreboard::GetScoreColumns()
{
	TArray<FALKScoreColumn> Columns;
	for (const FALKScoreColumn& Column : UALKDeveloperSettings::GetOnlineConfig()->ScoreColumns)
	{
		// The ping has a column of its own (a number and bars).
		if (Column.Source != EALKScoreSource::Ping)
		{
			Columns.Add(Column);
		}
	}
	if (Columns.Num() == 0)
	{
		Columns.Add(FALKScoreColumn(TEXT("Score"), LOCTEXT("ScoreColumnScore", "Score"), EALKScoreSource::Score, 88.f));
		Columns.Add(FALKScoreColumn(TEXT("Kills"), LOCTEXT("ScoreColumnEliminations", "Eliminations"), EALKScoreSource::Kills, 124.f));
		Columns.Add(FALKScoreColumn(TEXT("Deaths"), LOCTEXT("ScoreColumnDeaths", "Deaths"), EALKScoreSource::Deaths, 84.f));
		Columns.Add(FALKScoreColumn(TEXT("Assists"), LOCTEXT("ScoreColumnAssists", "Assists"), EALKScoreSource::Assists, 84.f));
	}
	return Columns;
}

void UALKScoreboard::SortPlayersByScore(TArray<AALKLobbyPlayerState*>& Players)
{
	Players.RemoveAll([](const AALKLobbyPlayerState* Each) { return Each == nullptr; });
	Players.StableSort([](const AALKLobbyPlayerState& A, const AALKLobbyPlayerState& B)
	{
		if (!FMath::IsNearlyEqual(A.GetScore(), B.GetScore()))
		{
			return A.GetScore() > B.GetScore();
		}
		if (A.Kills != B.Kills)
		{
			return A.Kills > B.Kills;
		}
		if (A.Deaths != B.Deaths)
		{
			return A.Deaths < B.Deaths;
		}
		return A.GetStableIndex() < B.GetStableIndex();
	});
}

UALKScoreRow* UALKScoreboard::GetFirstRow() const
{
	for (const FTable& Table : Tables)
	{
		for (const TWeakObjectPtr<UALKScoreRow>& Row : Table.Rows)
		{
			if (Row.IsValid())
			{
				return Row.Get();
			}
		}
	}
	return nullptr;
}

UALKScoreRow* UALKScoreboard::GetLocalRow() const
{
	for (const FTable& Table : Tables)
	{
		for (const TWeakObjectPtr<UALKScoreRow>& Row : Table.Rows)
		{
			if (Row.IsValid() && Row->IsLocalRow())
			{
				return Row.Get();
			}
		}
	}
	return nullptr;
}

void UALKScoreboard::Refresh()
{
	using namespace ALKScoreboardPrivate;
	bDirty = false;

	AALKLobbyGameState* GameState = ResolveGameState();
	bDirty = false;
	const UALKOnlineConfig* Config = UALKDeveloperSettings::GetOnlineConfig();
	const UALKOnlineSubsystem* Online = UALKOnlineSubsystem::Get(this);
	const UALKTheme* Theme = GetTheme();

	if (WidgetTree)
	{
		SetShown(WidgetTree->FindWidget(TEXT("HeaderRow")), bShowHeader && GameState != nullptr);
		SetShown(WidgetTree->FindWidget(TEXT("FooterRow")), bShowFooter && GameState != nullptr);
	}
	FALKWidgetBuilder::SetEmptyStateShown(EmptyText, GameState == nullptr);
	if (TableList)
	{
		// While playing the list lets the mouse through; in a menu it scrolls and its lines can be picked.
		const ESlateVisibility Shown = bInteractive ? ESlateVisibility::Visible : ESlateVisibility::HitTestInvisible;
		TableList->SetVisibility(GameState ? Shown : ESlateVisibility::Collapsed);
	}
	if (!GameState)
	{
		return;
	}

	// ---- Header
	if (ModeText)
	{
		ModeText->SetText(Config->GetModeName(GameState->ModeId));
	}
	if (DetailText)
	{
		TArray<FText> Parts;
		if (!GameState->MapId.IsNone())
		{
			Parts.Add(Config->GetMapName(GameState->MapId));
		}
		if (!GameState->ServerName.IsEmpty())
		{
			Parts.Add(FText::FromString(GameState->ServerName));
		}
		if (GameState->ScoreLimit > 0)
		{
			Parts.Add(FText::Format(LOCTEXT("ScoreFirstTo", "First to {0}"), FText::AsNumber(GameState->ScoreLimit)));
		}
		DetailText->SetText(FText::Join(LOCTEXT("ScoreDetailSeparator", "  \u00b7  "), Parts));
	}

	// ---- Who is on which table
	const int32 TeamCount = GameState->TeamCount;
	TArray<int32> TeamIds;
	TArray<TArray<AALKLobbyPlayerState*>> TeamPlayers;
	for (int32 TeamId = 0; TeamId < TeamCount; ++TeamId)
	{
		TeamIds.Add(TeamId);
		TeamPlayers.Add(GameState->GetPlayersOnTeam(TeamId));
	}
	TArray<AALKLobbyPlayerState*> NoTeam = GameState->GetPlayersOnTeam(INDEX_NONE);
	if (TeamCount <= 0 || NoTeam.Num() > 0)
	{
		// A free for all is one table; in a team mode it holds the players who have no team yet.
		TeamIds.Add(INDEX_NONE);
		TeamPlayers.Add(MoveTemp(NoTeam));
	}
	TArray<int32> RowCounts;
	for (TArray<AALKLobbyPlayerState*>& Players : TeamPlayers)
	{
		SortPlayersByScore(Players);
		RowCounts.Add(Players.Num());
	}

	const TArray<FALKScoreColumn> Columns = GetScoreColumns();
	TArray<float> Widths;
	FString Signature = FString::Printf(TEXT("%d|"), bInteractive ? 1 : 0);
	for (const FALKScoreColumn& Column : Columns)
	{
		Widths.Add(Column.Width);
		Signature += FString::Printf(TEXT("%s:%d,"), *Column.Id.ToString(), FMath::RoundToInt(Column.Width));
	}
	for (int32 Index = 0; Index < TeamIds.Num(); ++Index)
	{
		Signature += FString::Printf(TEXT("|%d:%d"), TeamIds[Index], RowCounts[Index]);
	}
	if (Signature != BuiltSignature)
	{
		RebuildTables(TeamIds, RowCounts, Columns);
		BuiltSignature = Signature;
	}

	// ---- Fill the lines
	const AALKLobbyPlayerState* MostValuable = GameState->Phase == EALKLobbyPhase::PostMatch ? GameState->MVPPlayer.Get() : nullptr;
	const int32 ScoreColumn = Columns.IndexOfByPredicate([](const FALKScoreColumn& Each) { return Each.Source == EALKScoreSource::Score; });
	const bool bPingNumbers = !Online || Online->IsShowPing();
	const bool bStreamer = Online && Online->IsStreamerMode();
	for (int32 TableIndex = 0; TableIndex < Tables.Num() && TableIndex < TeamPlayers.Num(); ++TableIndex)
	{
		FTable& Table = Tables[TableIndex];
		const TArray<AALKLobbyPlayerState*>& Players = TeamPlayers[TableIndex];
		Table.bHasTeam = Table.TeamId != INDEX_NONE;
		Table.Color = Table.bHasTeam ? Config->GetTeamColor(Table.TeamId) : FLinearColor::Transparent;
		if (UTextBlock* NameText = Table.NameText.Get())
		{
			NameText->SetText(Table.bHasTeam ? Config->GetTeamName(Table.TeamId) : (TeamCount > 0 ? LOCTEXT("ScoreNoTeam", "No team") : LOCTEXT("ScorePlayers", "Players")));
		}
		if (UTextBlock* ScoreText = Table.ScoreText.Get())
		{
			ScoreText->SetText(Table.bHasTeam ? FText::AsNumber(GameState->GetTeamScore(Table.TeamId)) : FText::GetEmpty());
		}
		for (int32 RowIndex = 0; RowIndex < Table.Rows.Num() && RowIndex < Players.Num(); ++RowIndex)
		{
			UALKScoreRow* Row = Table.Rows[RowIndex].Get();
			const AALKLobbyPlayerState* Player = Players[RowIndex];
			if (!Row || !Player)
			{
				continue;
			}
			const bool bLocalPlayer = Player->IsLocalPlayerState();
			const FString& PlayerUserId = Player->GetUserId();
			TArray<FText> Values;
			for (const FALKScoreColumn& Column : Columns)
			{
				Values.Add(UALKLobbyStatics::GetScoreColumnText(Player, Column));
			}
			Row->SetColumnWidths(Widths, true);
			Row->SetIdentity(RowIndex + 1, GetShownPlayerName(Player), PlayerUserId, bStreamer && !bLocalPlayer);
			Row->SetValues(Values);
			Row->SetBoldColumn(ScoreColumn);
			Row->SetPing(FMath::RoundToInt(Player->GetPingInMilliseconds()), bPingNumbers);
			Row->SetMarks(bLocalPlayer, Player->bHost, Player == MostValuable, Online && Online->IsPlayerMuted(PlayerUserId), Talkers.Contains(PlayerUserId), Player->IsInactive());
		}
		if (Theme)
		{
			StyleTable(Table, *Theme);
		}
	}

	// ---- Footer
	if (SpectatorText)
	{
		TArray<FText> Names;
		for (const AALKLobbyPlayerState* Spectator : GameState->GetSpectators())
		{
			Names.Add(FText::FromString(GetShownPlayerName(Spectator)));
		}
		SpectatorText->SetText(Names.Num() == 0 ? FText::GetEmpty()
			: FText::Format(LOCTEXT("ScoreSpectators", "Spectators ({0}):  {1}"), FText::AsNumber(Names.Num()), FText::Join(LOCTEXT("ScoreNameSeparator", ", "), Names)));
	}
	if (HintText)
	{
		// The scoreboard key of the input method in use.
		const UCommonInputSubsystem* Input = UCommonInputSubsystem::Get(GetOwningLocalPlayer());
		const bool bGamepad = Input && Input->GetCurrentInputType() == ECommonInputType::Gamepad;
		const FALKOnlineKeys& Keys = GetMenuConfig()->OnlineKeys;
		const FText KeyName = FALKInputLabels::GetKeyLabel(bGamepad ? Keys.ScoreboardGamepad : Keys.Scoreboard, GetOwningLocalPlayer());
		HintText->SetText(bInteractive ? LOCTEXT("ScoreHintPick", "Select a player for options") : FText::Format(LOCTEXT("ScoreHintHold", "Hold {0}"), KeyName));
	}
	ShownClockSeconds = MIN_int32;
	UpdateClock();
}

void UALKScoreboard::RebuildTables(const TArray<int32>& TeamIds, const TArray<int32>& RowCounts, const TArray<FALKScoreColumn>& Columns)
{
	using namespace ALKScoreboardPrivate;
	Tables.Reset();
	if (!TableList || !WidgetTree)
	{
		return;
	}
	TableList->ClearChildren();

	// The ping column's title comes from the config when it lists one.
	FText PingHeader = LOCTEXT("ScoreColumnPing", "Ping");
	for (const FALKScoreColumn& Column : UALKDeveloperSettings::GetOnlineConfig()->ScoreColumns)
	{
		if (Column.Source == EALKScoreSource::Ping && !Column.Header.IsEmpty())
		{
			PingHeader = Column.Header;
		}
	}

	TSubclassOf<UALKScoreRow> LineClass = RowClass;
	if (!LineClass)
	{
		LineClass = UALKScoreRow::StaticClass();
	}

	for (int32 TableIndex = 0; TableIndex < TeamIds.Num(); ++TableIndex)
	{
		FTable Table;
		Table.TeamId = TeamIds[TableIndex];

		UVerticalBox* TableBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());

		// Header: team name and score on the left, the column titles over their numbers on the right.
		UALKFrame* Header = WidgetTree->ConstructWidget<UALKFrame>(UALKFrame::StaticClass());
		Header->SetPadding(FMargin(UALKScoreRow::SidePadding + 8.f, 0.f, UALKScoreRow::SidePadding, 0.f));
		Header->SetVerticalAlignment(VAlign_Center);
		Table.Header = Header;
		UHorizontalBox* HeaderLine = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		Header->SetContent(HeaderLine);

		UTextBlock* NameText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		Table.NameText = NameText;
		FALKWidgetBuilder::AddToHorizontalBox(HeaderLine, NameText, FMargin(), HAlign_Left, VAlign_Center);
		UTextBlock* ScoreText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		Table.ScoreText = ScoreText;
		FALKWidgetBuilder::AddToHorizontalBox(HeaderLine, ScoreText, FMargin(22.f, 0.f, 0.f, 0.f), HAlign_Left, VAlign_Center);
		FALKWidgetBuilder::AddToHorizontalBox(HeaderLine, WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass()), FMargin(), HAlign_Fill, VAlign_Fill, 1.f);

		auto AddColumnTitle = [this, HeaderLine, &Table](const FText& Title, float Width)
		{
			UTextBlock* TitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
			TitleText->SetText(Title);
			TitleText->SetJustification(ETextJustify::Right);
			USizeBox* TitleSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
			TitleSize->SetWidthOverride(FMath::Max(20.f, Width));
			TitleSize->SetContent(TitleText);
			FALKWidgetBuilder::AddToHorizontalBox(HeaderLine, TitleSize, FMargin(), HAlign_Right, VAlign_Center);
			Table.ColumnTexts.Add(TitleText);
		};
		for (const FALKScoreColumn& Column : Columns)
		{
			AddColumnTitle(Column.Header, Column.Width);
		}
		AddColumnTitle(PingHeader, UALKScoreRow::PingColumnWidth);

		USizeBox* HeaderSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		HeaderSize->SetMinDesiredHeight(HeaderHeight);
		HeaderSize->SetContent(Header);
		FALKWidgetBuilder::AddToVerticalBox(TableBox, HeaderSize, FMargin(0.f, 0.f, 0.f, 4.f));

		for (int32 RowIndex = 0; RowIndex < RowCounts[TableIndex]; ++RowIndex)
		{
			UALKScoreRow* Row = CreateWidget<UALKScoreRow>(this, LineClass);
			if (!Row)
			{
				continue;
			}
			Row->SetInteractive(bInteractive);
			TWeakObjectPtr<UALKScoreRow> WeakRow(Row);
			Row->OnClicked().AddWeakLambda(this, [this, WeakRow]() { HandleRowClicked(WeakRow.Get()); });
			FALKWidgetBuilder::AddToVerticalBox(TableBox, Row, FMargin(0.f, 0.f, 0.f, 2.f));
			Table.Rows.Add(Row);
		}

		TableList->AddChild(TableBox);
		if (UScrollBoxSlot* TableSlot = Cast<UScrollBoxSlot>(TableBox->Slot))
		{
			TableSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 14.f));
			TableSlot->SetHorizontalAlignment(HAlign_Fill);
		}
		Tables.Add(MoveTemp(Table));
	}

	// The arrow keys and the d-pad go line by line through every table. Left to itself, the scroll box steps from one of
	// its children (a whole team's table) to the next, and the lines after a team's first could not be reached.
	// The list follows the focused line, as the kit's other lists do.
	TableList->SetScrollWhenFocusChanges(EScrollWhenFocusChanges::AnimatedScroll);
	TArray<UALKScoreRow*> AllRows;
	for (const FTable& Table : Tables)
	{
		for (const TWeakObjectPtr<UALKScoreRow>& Row : Table.Rows)
		{
			if (Row.IsValid())
			{
				AllRows.Add(Row.Get());
			}
		}
	}
	for (int32 Index = 0; Index < AllRows.Num(); ++Index)
	{
		if (Index > 0)
		{
			AllRows[Index]->SetNavigationRuleExplicit(EUINavigation::Up, AllRows[Index - 1]);
		}
		if (Index + 1 < AllRows.Num())
		{
			AllRows[Index]->SetNavigationRuleExplicit(EUINavigation::Down, AllRows[Index + 1]);
		}
	}
}

void UALKScoreboard::StyleTable(FTable& Table, const UALKTheme& Theme)
{
	if (UALKFrame* Header = Table.Header.Get())
	{
		// A band in the team's colour with a bar of it at the left; a plain raised band without teams.
		FALKFrameStyle Band = Theme.MakeChipFrame(Table.bHasTeam ? Table.Color.CopyWithNewOpacity(0.22f) : Theme.PanelRaised,
			Table.bHasTeam ? Table.Color.CopyWithNewOpacity(0.6f) : Theme.PanelOutline);
		if (Table.bHasTeam)
		{
			Band.Tick = Table.Color;
			Band.TickWidth = 6.f;
		}
		Header->SetFrameStyle(Band);
	}
	if (UTextBlock* NameText = Table.NameText.Get())
	{
		FALKWidgetBuilder::StyleText(NameText, Theme, EALKTextRole::Subheading, Theme.TextPrimary);
		FSlateFontInfo NameFont = Theme.SubheadingFont;
		NameFont.Size = FMath::Max(10.f, FMath::RoundToFloat(Theme.SubheadingFont.Size * 0.92f));
		FALKWidgetBuilder::SetTextFont(NameText, NameFont);
		// As the team names in the lobby: in capitals on the skins that write their titles in them.
		NameText->SetTextTransformPolicy(Theme.bUppercaseTitles ? ETextTransformPolicy::ToUpper : ETextTransformPolicy::None);
	}
	if (UTextBlock* ScoreText = Table.ScoreText.Get())
	{
		FALKWidgetBuilder::StyleText(ScoreText, Theme, EALKTextRole::Subheading, FMath::Lerp(Table.Color, Theme.TextPrimary, 0.3f).CopyWithNewOpacity(1.f));
		FSlateFontInfo ScoreFont = Theme.SubheadingFont;
		ScoreFont.Size = FMath::RoundToFloat(Theme.SubheadingFont.Size * 1.2f);
		FALKWidgetBuilder::SetTextFont(ScoreText, ScoreFont);
	}
	for (const TWeakObjectPtr<UTextBlock>& Title : Table.ColumnTexts)
	{
		if (UTextBlock* TitleText = Title.Get())
		{
			FALKWidgetBuilder::StyleText(TitleText, Theme, EALKTextRole::Data, Theme.TextMuted);
			TitleText->SetTextTransformPolicy(ETextTransformPolicy::ToUpper);
		}
	}
}

void UALKScoreboard::StyleScrollBar(const UALKTheme& Theme)
{
	if (!TableList)
	{
		return;
	}
	FScrollBarStyle BarStyle = TableList->GetWidgetBarStyle();
	const FSlateBrush Thumb = UALKTheme::MakeSolidBrush(Theme.Accent.CopyWithNewOpacity(0.85f));
	BarStyle.SetNormalThumbImage(Thumb);
	BarStyle.SetHoveredThumbImage(Thumb);
	BarStyle.SetDraggedThumbImage(Thumb);
	BarStyle.SetVerticalBackgroundImage(UALKTheme::MakeSolidBrush(Theme.TextPrimary.CopyWithNewOpacity(0.06f)));
	TableList->SetWidgetBarStyle(BarStyle);
	TableList->SetScrollbarThickness(FVector2D(3.0, 3.0));
	TableList->SetScrollbarPadding(FMargin(10.f, 0.f, 0.f, 0.f));
}

void UALKScoreboard::NativeApplyTheme(const UALKTheme& Theme)
{
	Super::NativeApplyTheme(Theme);

	if (UALKFrame* Frame = Cast<UALKFrame>(Panel))
	{
		Frame->SetFrameStyle(Theme.MakePanelFrame());
	}
	if (ModeText)
	{
		FALKWidgetBuilder::StyleText(ModeText, Theme, EALKTextRole::Heading, Theme.TextPrimary);
		FSlateFontInfo ModeFont = Theme.HeadingFont;
		ModeFont.Size = FMath::Max(12.f, FMath::RoundToFloat(Theme.HeadingFont.Size * 0.75f));
		FALKWidgetBuilder::SetTextFont(ModeText, ModeFont);
	}
	FALKWidgetBuilder::StyleText(DetailText, Theme, EALKTextRole::Caption, Theme.TextMuted);
	if (ClockText)
	{
		FALKWidgetBuilder::StyleText(ClockText, Theme, EALKTextRole::Heading, Theme.TextPrimary);
		FSlateFontInfo ClockFont = Theme.HeadingFont;
		ClockFont.Size = FMath::Max(12.f, FMath::RoundToFloat(Theme.HeadingFont.Size * 0.85f));
		FALKWidgetBuilder::SetTextFont(ClockText, ClockFont);
	}
	FALKWidgetBuilder::StyleText(ClockLabel, Theme, EALKTextRole::Data, Theme.TextMuted);
	if (ClockLabel)
	{
		ClockLabel->SetTextTransformPolicy(ETextTransformPolicy::ToUpper);
	}
	FALKWidgetBuilder::StyleText(EmptyText, Theme, EALKTextRole::Body, Theme.TextMuted);
	FALKWidgetBuilder::StyleEmptyState(EmptyText, Theme);
	FALKWidgetBuilder::StyleText(SpectatorText, Theme, EALKTextRole::Caption, Theme.TextMuted);
	FALKWidgetBuilder::StyleText(HintText, Theme, EALKTextRole::Caption, Theme.TextMuted);
	StyleScrollBar(Theme);
	for (FTable& Table : Tables)
	{
		StyleTable(Table, Theme);
	}
	// The clock picks its colour again.
	ShownClockSeconds = MIN_int32;
}

void UALKScoreboard::UpdateClock()
{
	const AALKLobbyGameState* GameState = BoundGameState.Get();
	const float Remaining = GameState && GameState->Phase == EALKLobbyPhase::InMatch ? GameState->GetMatchTimeRemaining() : -1.f;
	const bool bShown = Remaining >= 0.f;
	if (UWidget* ClockBox = ClockText ? ClockText->GetParent() : nullptr)
	{
		// Hidden, not collapsed: the titles keep their place.
		const ESlateVisibility Wanted = bShown ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden;
		if (ClockBox->GetVisibility() != Wanted)
		{
			ClockBox->SetVisibility(Wanted);
		}
	}
	const int32 Seconds = bShown ? FMath::CeilToInt(Remaining) : -1;
	if (!ClockText || Seconds == ShownClockSeconds)
	{
		return;
	}
	ShownClockSeconds = Seconds;
	ClockText->SetText(bShown ? FormatClock(Remaining) : FText::GetEmpty());
	if (const UALKTheme* Theme = GetTheme())
	{
		// The last half minute stands out.
		ClockText->SetColorAndOpacity(FSlateColor(bShown && Seconds <= 30 ? Theme->Warning : Theme->TextPrimary));
	}
}

void UALKScoreboard::HandleRowClicked(UALKScoreRow* Row)
{
	if (!Row || !bInteractive || Row->GetUserId().IsEmpty())
	{
		return;
	}
	if (UALKOnlineFlowSubsystem* Flow = UALKOnlineFlowSubsystem::Get(this))
	{
		Flow->ShowPlayerOptions(Row->GetUserId(), Row->GetShownName());
	}
}

#undef LOCTEXT_NAMESPACE
