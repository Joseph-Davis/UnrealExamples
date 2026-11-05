// Copyright 2026 CodeLikeMe. All Rights Reserved.

#include "OnlineUI/ALKResultsScreen.h"
#include "OnlineUI/ALKOnlineFlowSubsystem.h"
#include "OnlineUI/ALKScoreboard.h"
#include "ALKDeveloperSettings.h"
#include "ALKTheme.h"
#include "Lobby/ALKLobbyGameState.h"
#include "Lobby/ALKLobbyPlayerState.h"
#include "Lobby/ALKPlayerNetComponent.h"
#include "Online/ALKOnlineConfig.h"
#include "Online/ALKOnlineSubsystem.h"
#include "Widgets/ALKButton.h"
#include "Widgets/ALKDecor.h"
#include "Widgets/ALKFrame.h"
#include "Widgets/ALKTitleText.h"
#include "Widgets/ALKWidgetBuilder.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformInput.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ALKResultsScreen)

#define LOCTEXT_NAMESPACE "AssetproLobbyKit"

namespace ALKResultsScreenPrivate
{
	/** The content never gets wider than this (ultrawide screens). */
	static constexpr float MaxContentWidth = 1728.f;
	/** Heights of the podium blocks for first, second and third place. */
	static constexpr float PodiumHeights[3] = { 168.f, 118.f, 82.f };

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

	static FSlateFontInfo Scaled(const FSlateFontInfo& Font, float Scale)
	{
		FSlateFontInfo Result = Font;
		Result.Size = FMath::Max(8.f, FMath::RoundToFloat(Font.Size * Scale));
		return Result;
	}
}

UALKResultsScreen::UALKResultsScreen(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// The level the match was played in stays visible, blurred, behind the results.
	bAlwaysOverGame = true;
}

void UALKResultsScreen::NativeBuildDefaultLayout(UWidgetTree& Tree)
{
	using namespace ALKResultsScreenPrivate;

	UOverlay* Root = FALKWidgetBuilder::Make<UOverlay>(Tree, TEXT("Root"));
	Tree.RootWidget = Root;
	BackgroundImage = FALKWidgetBuilder::AddBackgroundLayers(Tree, Root);

	USizeBox* MainSize = FALKWidgetBuilder::Make<USizeBox>(Tree, TEXT("MainSize"));
	MainSize->SetWidthOverride(MaxContentWidth);
	FALKWidgetBuilder::AddToOverlay(Root, MainSize, HAlign_Center, VAlign_Fill);
	UVerticalBox* Main = FALKWidgetBuilder::Make<UVerticalBox>(Tree, TEXT("MainColumn"));
	MainSize->SetContent(Main);

	// Header: the outcome over mode and map | the final score, on a band that takes the winning team's colour
	UALKFrame* Band = FALKWidgetBuilder::Make<UALKFrame>(Tree, TEXT("HeaderBand"));
	HeaderBand = Band;
	FALKWidgetBuilder::AddToVerticalBox(Main, Band);
	UHorizontalBox* HeaderRow = FALKWidgetBuilder::Make<UHorizontalBox>(Tree, TEXT("HeaderRow"));
	Band->SetContent(HeaderRow);
	UVerticalBox* Titles = FALKWidgetBuilder::Make<UVerticalBox>(Tree, TEXT("HeaderTitles"));
	FALKWidgetBuilder::AddToHorizontalBox(HeaderRow, Titles, FMargin(), HAlign_Fill, VAlign_Center, 1.f);
	TitleText = FALKWidgetBuilder::Make<UALKTitleText>(Tree, TEXT("TitleText"));
	FALKWidgetBuilder::AddToVerticalBox(Titles, TitleText, FMargin(), HAlign_Left);
	SubtitleText = FALKWidgetBuilder::MakeText(Tree, TEXT("SubtitleText"));
	FALKWidgetBuilder::AddToVerticalBox(Titles, SubtitleText, FMargin(0.f, 2.f, 0.f, 0.f), HAlign_Left);
	ScoreBox = FALKWidgetBuilder::Make<UHorizontalBox>(Tree, TEXT("ScoreBox"));
	FALKWidgetBuilder::AddToHorizontalBox(HeaderRow, ScoreBox, FMargin(24.f, 0.f, 0.f, 0.f), HAlign_Right, VAlign_Center);

	// Body: the podium | the final scoreboard
	UOverlay* BodyOverlay = FALKWidgetBuilder::Make<UOverlay>(Tree, TEXT("BodyOverlay"));
	FALKWidgetBuilder::AddToVerticalBox(Main, BodyOverlay, FMargin(0.f, 24.f, 0.f, 0.f), HAlign_Fill, VAlign_Fill, 1.f);
	UHorizontalBox* Body = FALKWidgetBuilder::Make<UHorizontalBox>(Tree, TEXT("Body"));
	FALKWidgetBuilder::AddToOverlay(BodyOverlay, Body);

	UALKFrame* Podium = FALKWidgetBuilder::Make<UALKFrame>(Tree, TEXT("PodiumPanel"));
	Podium->SetPadding(FMargin(24.f, 22.f, 24.f, 24.f));
	PodiumPanel = Podium;
	FALKWidgetBuilder::AddToHorizontalBox(Body, Podium, FMargin(), HAlign_Fill, VAlign_Fill, 0.36f);
	UVerticalBox* PodiumColumn = FALKWidgetBuilder::Make<UVerticalBox>(Tree, TEXT("PodiumColumn"));
	Podium->SetContent(PodiumColumn);
	PodiumTitle = FALKWidgetBuilder::MakeText(Tree, TEXT("PodiumTitle"), LOCTEXT("ResultsTopPlayers", "Top players"));
	FALKWidgetBuilder::AddToVerticalBox(PodiumColumn, PodiumTitle, FMargin(), HAlign_Left);
	PodiumBox = FALKWidgetBuilder::Make<UHorizontalBox>(Tree, TEXT("PodiumBox"));
	FALKWidgetBuilder::AddToVerticalBox(PodiumColumn, PodiumBox, FMargin(0.f, 16.f, 0.f, 0.f), HAlign_Fill, VAlign_Fill, 1.f);

	Scoreboard = Tree.ConstructWidget<UALKScoreboard>(UALKScoreboard::StaticClass(), TEXT("Scoreboard"));
	// The screen's own header already says mode and map.
	Scoreboard->bShowHeader = false;
	FALKWidgetBuilder::AddToHorizontalBox(Body, Scoreboard, FMargin(28.f, 0.f, 0.f, 0.f), HAlign_Fill, VAlign_Fill, 0.64f);

	EmptyText = FALKWidgetBuilder::MakeText(Tree, TEXT("EmptyText"), LOCTEXT("ResultsNone", "No match results"));
	EmptyText->SetJustification(ETextJustify::Center);
	FALKWidgetBuilder::AddToOverlay(BodyOverlay, FALKWidgetBuilder::MakeEmptyState(Tree, TEXT("EmptyState"), EALKIcon::Flag, EmptyText), HAlign_Center, VAlign_Center);

	// Bottom: the countdown and the rematch vote | the buttons
	UHorizontalBox* BottomRow = FALKWidgetBuilder::Make<UHorizontalBox>(Tree, TEXT("BottomRow"));
	FALKWidgetBuilder::AddToVerticalBox(Main, BottomRow, FMargin(0.f, 22.f, 0.f, 0.f));
	UVerticalBox* Status = FALKWidgetBuilder::Make<UVerticalBox>(Tree, TEXT("StatusColumn"));
	FALKWidgetBuilder::AddToHorizontalBox(BottomRow, Status, FMargin(), HAlign_Fill, VAlign_Center, 1.f);
	CountdownText = FALKWidgetBuilder::MakeText(Tree, TEXT("CountdownText"));
	FALKWidgetBuilder::AddToVerticalBox(Status, CountdownText, FMargin(), HAlign_Left);
	RematchText = FALKWidgetBuilder::MakeText(Tree, TEXT("RematchText"));
	FALKWidgetBuilder::AddToVerticalBox(Status, RematchText, FMargin(0.f, 4.f, 0.f, 0.f), HAlign_Left);

	RematchButton = CreateMenuButton(LOCTEXT("ResultsPlayAgain", "Play again"), EALKButtonStyle::Secondary);
	FALKWidgetBuilder::AddToHorizontalBox(BottomRow, RematchButton, FMargin(14.f, 0.f, 0.f, 0.f));
	ReturnButton = CreateMenuButton(LOCTEXT("ResultsReturnNow", "Return to lobby now"), EALKButtonStyle::Solid);
	FALKWidgetBuilder::AddToHorizontalBox(BottomRow, ReturnButton, FMargin(14.f, 0.f, 0.f, 0.f));
	LeaderboardsButton = CreateMenuButton(LOCTEXT("ResultsLeaderboards", "Leaderboards"), EALKButtonStyle::Secondary);
	FALKWidgetBuilder::AddToHorizontalBox(BottomRow, LeaderboardsButton, FMargin(14.f, 0.f, 0.f, 0.f));
	LeaveButton = CreateMenuButton(LOCTEXT("ResultsLeave", "Leave"), EALKButtonStyle::Danger);
	FALKWidgetBuilder::AddToHorizontalBox(BottomRow, LeaveButton, FMargin(14.f, 0.f, 0.f, 0.f));

	// Footer: [Enter] Select  [Esc] Leave
	UHorizontalBox* Hints = BuildInputHints(Tree, {
		FALKInputHint(LOCTEXT("KeyEnter", "Enter"), FPlatformInput::GetGamepadAcceptKey(), LOCTEXT("ResultsHintSelect", "Select")),
		FALKInputHint(LOCTEXT("KeyEsc", "Esc"), FPlatformInput::GetGamepadBackKey(), LOCTEXT("ResultsLeave", "Leave"))
			.WithAction(FSimpleDelegate::CreateWeakLambda(this, [this]() { OnBackRequested(); }))
	});
	FALKWidgetBuilder::AddToVerticalBox(Main, Hints, FMargin(0.f, 22.f, 0.f, 0.f), HAlign_Left);
}

void UALKResultsScreen::NativeOnLayoutReady()
{
	Super::NativeOnLayoutReady();
	if (RematchButton)
	{
		RematchButton->OnClicked().AddUObject(this, &ThisClass::ToggleRematchVote);
	}
	if (ReturnButton)
	{
		ReturnButton->OnClicked().AddUObject(this, &ThisClass::ReturnToLobbyNow);
	}
	if (LeaderboardsButton)
	{
		LeaderboardsButton->OnClicked().AddUObject(this, &ThisClass::OpenLeaderboards);
	}
	if (LeaveButton)
	{
		LeaveButton->OnClicked().AddUObject(this, &ThisClass::LeaveGame);
	}
}

void UALKResultsScreen::NativeApplyTheme(const UALKTheme& Theme)
{
	Super::NativeApplyTheme(Theme);
	using namespace ALKResultsScreenPrivate;

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
	if (TitleText)
	{
		FALKWidgetBuilder::StyleText(TitleText, Theme, EALKTextRole::Heading);
		FALKWidgetBuilder::SetTextFont(TitleText, Scaled(Theme.HeadingFont, 1.3f));
	}
	FALKWidgetBuilder::StyleText(SubtitleText, Theme, EALKTextRole::Body, Theme.TextSecondary);
	if (UALKFrame* Podium = Cast<UALKFrame>(PodiumPanel))
	{
		Podium->SetFrameStyle(Theme.MakePanelFrame());
	}
	FALKWidgetBuilder::StyleText(PodiumTitle, Theme, EALKTextRole::Data, Theme.TextMuted);
	if (PodiumTitle)
	{
		PodiumTitle->SetTextTransformPolicy(ETextTransformPolicy::ToUpper);
	}
	FALKWidgetBuilder::StyleText(EmptyText, Theme, EALKTextRole::Body, Theme.TextMuted);
	FALKWidgetBuilder::StyleEmptyState(EmptyText, Theme);
	if (CountdownText)
	{
		FALKWidgetBuilder::StyleText(CountdownText, Theme, EALKTextRole::Subheading, Theme.TextPrimary);
		FALKWidgetBuilder::SetTextFont(CountdownText, Scaled(Theme.SubheadingFont, 0.92f));
	}
	FALKWidgetBuilder::StyleText(RematchText, Theme, EALKTextRole::Caption, Theme.TextMuted);

	// The score blocks and the podium are made again in the new look.
	BuiltSignature.Reset();
	RefreshResults();
}

void UALKResultsScreen::NativeOnActivated()
{
	// Screens are pooled: start from the game as it is now.
	BuiltSignature.Reset();
	ShownCountdown = MIN_int32;
	if (Scoreboard)
	{
		Scoreboard->SetInteractive(true);
	}
	RefreshResults();
	Super::NativeOnActivated();
}

void UALKResultsScreen::NativeOnDeactivated()
{
	UnbindGameState();
	Super::NativeOnDeactivated();
}

AALKLobbyGameState* UALKResultsScreen::ResolveGameState()
{
	const UWorld* World = GetWorld();
	// The game state can be missing for a moment after a level change: look again until it is there.
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

void UALKResultsScreen::UnbindGameState()
{
	if (AALKLobbyGameState* Old = BoundGameState.Get())
	{
		Old->OnLobbyStateChangedNative.RemoveAll(this);
		Old->OnPlayersChangedNative.RemoveAll(this);
	}
	BoundGameState.Reset();
}

void UALKResultsScreen::MarkDirty()
{
	bDirty = true;
}

void UALKResultsScreen::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (!IsActivated())
	{
		return;
	}
	ResolveGameState();
	if (bDirty)
	{
		RefreshResults();
	}
	UpdateCountdown();
}

void UALKResultsScreen::RefreshResults()
{
	using namespace ALKResultsScreenPrivate;

	AALKLobbyGameState* GameState = ResolveGameState();
	bDirty = false;
	const UALKTheme* Theme = GetTheme();
	const UALKOnlineConfig* Config = UALKDeveloperSettings::GetOnlineConfig();
	const UALKOnlineSubsystem* Online = UALKOnlineSubsystem::Get(this);

	// There are results once a match runs or has ended; opened by hand from a menu or a lobby there are none.
	bHasResults = GameState != nullptr && GameState->IsInMatch();
	if (WidgetTree)
	{
		SetShown(WidgetTree->FindWidget(TEXT("Body")), bHasResults, ESlateVisibility::SelfHitTestInvisible);
	}
	FALKWidgetBuilder::SetEmptyStateShown(EmptyText, !bHasResults);
	SetShown(ScoreBox, bHasResults);
	if (LeaveButton)
	{
		LeaveButton->SetVisibility(bHasResults ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (HintButtons.Num() > 1 && HintButtons[1])
	{
		HintButtons[1]->SetButtonText(bHasResults ? LOCTEXT("ResultsLeave", "Leave") : LOCTEXT("HintBack", "Back"));
	}

	if (!bHasResults)
	{
		Outcome = 0;
		if (UALKFrame* Band = Cast<UALKFrame>(HeaderBand))
		{
			Band->SetFrameStyle(FALKFrameStyle());
			Band->SetPadding(FMargin(0.f));
		}
		if (TitleText)
		{
			TitleText->SetText(LOCTEXT("ResultsTitle", "Results"));
			if (Theme)
			{
				TitleText->SetColorAndOpacity(FSlateColor(Theme->TextPrimary));
			}
		}
		if (SubtitleText)
		{
			SubtitleText->SetText(FText::GetEmpty());
		}
		if (RematchText)
		{
			RematchText->SetText(FText::GetEmpty());
		}
		if (RematchButton)
		{
			RematchButton->SetVisibility(ESlateVisibility::Collapsed);
		}
		if (ReturnButton)
		{
			ReturnButton->SetVisibility(ESlateVisibility::Collapsed);
		}
		ShownCountdown = MIN_int32;
		UpdateCountdown();
		return;
	}

	// Everyone who played, best first.
	TArray<AALKLobbyPlayerState*> Ranked;
	for (AALKLobbyPlayerState* Player : GameState->GetLobbyPlayers())
	{
		if (Player && !Player->bSpectating)
		{
			Ranked.Add(Player);
		}
	}
	UALKScoreboard::SortPlayersByScore(Ranked);

	const APlayerController* Controller = GetOwningPlayer();
	AALKLobbyPlayerState* Local = Controller ? Controller->GetPlayerState<AALKLobbyPlayerState>() : nullptr;
	const bool bLocalPlays = Local && !Local->bSpectating;
	const bool bOver = GameState->Phase == EALKLobbyPhase::PostMatch;

	// ---- The outcome, as the local player sees it
	Outcome = 0;
	FText Title;
	if (!bOver)
	{
		Title = LOCTEXT("ResultsInProgress", "Match in progress");
	}
	else if (GameState->TeamCount > 0)
	{
		const int32 Winner = GameState->WinnerTeam;
		if (Winner < 0)
		{
			Title = LOCTEXT("ResultsDraw", "Draw");
		}
		else if (bLocalPlays && Local->TeamId == Winner)
		{
			Title = LOCTEXT("ResultsVictory", "Victory");
			Outcome = 1;
		}
		else if (bLocalPlays && Local->TeamId >= 0)
		{
			Title = LOCTEXT("ResultsDefeat", "Defeat");
			Outcome = -1;
		}
		else
		{
			// A spectator reads who won.
			Title = FText::Format(LOCTEXT("ResultsTeamWins", "{0} wins"), Config->GetTeamName(Winner));
		}
	}
	else
	{
		const int32 Place = bLocalPlays ? Ranked.IndexOfByKey(Local) + 1 : 0;
		if (Place > 0)
		{
			Title = FText::Format(LOCTEXT("ResultsPlaced", "You placed {0}"), FText::AsNumber(Place));
			Outcome = Place == 1 ? 1 : 0;
		}
		else
		{
			Title = LOCTEXT("ResultsMatchOver", "Match over");
		}
	}
	// Once a team has won, the header sits on a band in that team's colour (and the title is plain, to read on it).
	const bool bWinnerBand = bOver && GameState->TeamCount > 0 && GameState->WinnerTeam >= 0;
	if (UALKFrame* Band = Cast<UALKFrame>(HeaderBand); Band && Theme)
	{
		Band->SetFrameStyle(bWinnerBand ? Theme->MakeBandFrame(Config->GetTeamColor(GameState->WinnerTeam)) : FALKFrameStyle());
		Band->SetPadding(bWinnerBand ? FMargin(28.f, 12.f, 16.f, 14.f) : FMargin(0.f));
	}
	if (TitleText)
	{
		TitleText->SetText(Title);
		if (Theme)
		{
			const FLinearColor TitleColor = bWinnerBand ? Theme->TextPrimary
				: (Outcome > 0 ? Theme->GetAccentTextColor() : (Outcome < 0 ? FMath::Lerp(Theme->Danger, Theme->TextPrimary, 0.25f).CopyWithNewOpacity(1.f) : Theme->TextPrimary));
			TitleText->SetColorAndOpacity(FSlateColor(TitleColor));
		}
	}
	if (SubtitleText)
	{
		TArray<FText> Parts;
		Parts.Add(Config->GetModeName(GameState->ModeId));
		if (!GameState->MapId.IsNone())
		{
			Parts.Add(Config->GetMapName(GameState->MapId));
		}
		if (!GameState->ServerName.IsEmpty())
		{
			Parts.Add(FText::FromString(GameState->ServerName));
		}
		SubtitleText->SetText(FText::Join(LOCTEXT("ResultsSeparator", "  \u00b7  "), Parts));
	}

	if (Theme)
	{
		RebuildScoreAndPodium(*GameState, Ranked, *Theme);
	}

	// ---- The rematch vote
	// Real players decide; placeholder players go along (as the match game mode counts it).
	int32 Voters = 0;
	int32 Votes = 0;
	for (const AALKLobbyPlayerState* Player : Ranked)
	{
		if (!Player->bPlaceholder)
		{
			++Voters;
			Votes += Player->bRematchVote ? 1 : 0;
		}
	}
	if (RematchText)
	{
		RematchText->SetText(bOver && Voters > 0
			? FText::Format(LOCTEXT("ResultsRematchVotes", "{0} of {1} want a rematch"), FText::AsNumber(Votes), FText::AsNumber(Voters))
			: FText::GetEmpty());
	}
	const bool bCanAct = Online && Online->GetLocalNetComponent() != nullptr;
	if (RematchButton)
	{
		const bool bVoted = Local && Local->bRematchVote;
		RematchButton->SetVisibility(bOver && bLocalPlays && bCanAct ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		RematchButton->SetLeadingIcon(bVoted ? EALKIcon::Check : EALKIcon::None);
		RematchButton->SetActiveVisual(bVoted);
	}
	if (ReturnButton)
	{
		ReturnButton->SetVisibility(bCanAct && Online->IsHost() ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	ShownCountdown = MIN_int32;
	UpdateCountdown();
}

void UALKResultsScreen::RebuildScoreAndPodium(const AALKLobbyGameState& GameState, const TArray<AALKLobbyPlayerState*>& Ranked, const UALKTheme& Theme)
{
	using namespace ALKResultsScreenPrivate;
	const UALKOnlineConfig* Config = UALKDeveloperSettings::GetOnlineConfig();
	const AALKLobbyPlayerState* MostValuable = GameState.Phase == EALKLobbyPhase::PostMatch ? GameState.MVPPlayer.Get() : nullptr;

	// Rebuilt only when what they show changed.
	FString Signature = FString::Printf(TEXT("%d|%d|"), GameState.TeamCount, GameState.WinnerTeam);
	for (int32 TeamId = 0; TeamId < GameState.TeamCount; ++TeamId)
	{
		Signature += FString::Printf(TEXT("%d,"), GameState.GetTeamScore(TeamId));
	}
	for (int32 Index = 0; Index < 3 && Index < Ranked.Num(); ++Index)
	{
		Signature += FString::Printf(TEXT("|%s:%d:%d"), *UALKScoreboard::GetShownPlayerName(Ranked[Index]), FMath::RoundToInt(Ranked[Index]->GetScore()), Ranked[Index] == MostValuable ? 1 : 0);
	}
	if (Signature == BuiltSignature || !WidgetTree)
	{
		return;
	}
	BuiltSignature = Signature;

	// ---- The final score: a block per team
	if (ScoreBox)
	{
		ScoreBox->ClearChildren();
		for (int32 TeamId = 0; TeamId < GameState.TeamCount; ++TeamId)
		{
			const FLinearColor TeamColor = Config->GetTeamColor(TeamId);
			// The winner's block stands out; in a draw, or while the match runs, none is dimmed.
			const bool bWinner = GameState.Phase != EALKLobbyPhase::PostMatch || GameState.WinnerTeam < 0 || GameState.WinnerTeam == TeamId;

			UVerticalBox* TeamColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
			UTextBlock* ScoreText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
			ScoreText->SetText(FText::AsNumber(GameState.GetTeamScore(TeamId)));
			ScoreText->SetJustification(ETextJustify::Center);
			FALKWidgetBuilder::SetTextFont(ScoreText, Scaled(Theme.HeadingFont, 0.9f));
			// Dark on a light team colour, light on a dark one.
			ScoreText->SetColorAndOpacity(FSlateColor(TeamColor.GetLuminance() > 0.3f ? Theme.AccentContrast : FLinearColor::White));
			UALKFrame* Block = WidgetTree->ConstructWidget<UALKFrame>(UALKFrame::StaticClass());
			Block->SetPadding(FMargin(14.f, 0.f));
			Block->SetHorizontalAlignment(HAlign_Center);
			Block->SetVerticalAlignment(VAlign_Center);
			Block->SetContent(ScoreText);
			FALKFrameStyle BlockFrame = Theme.MakeChipFrame(TeamColor.CopyWithNewOpacity(bWinner ? 1.f : 0.55f), FLinearColor::Transparent);
			BlockFrame.Radius = FMath::Max(2.f, Theme.CornerRadius - 2.f);
			Block->SetFrameStyle(BlockFrame);
			USizeBox* BlockSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
			BlockSize->SetMinDesiredWidth(118.f);
			BlockSize->SetMinDesiredHeight(54.f);
			BlockSize->SetContent(Block);
			FALKWidgetBuilder::AddToVerticalBox(TeamColumn, BlockSize);

			UTextBlock* NameText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
			NameText->SetText(Config->GetTeamName(TeamId));
			NameText->SetJustification(ETextJustify::Center);
			FALKWidgetBuilder::StyleText(NameText, Theme, EALKTextRole::Data, bWinner ? Theme.TextPrimary : Theme.TextMuted);
			NameText->SetTextTransformPolicy(ETextTransformPolicy::ToUpper);
			FALKWidgetBuilder::AddToVerticalBox(TeamColumn, NameText, FMargin(0.f, 6.f, 0.f, 0.f), HAlign_Center);

			FALKWidgetBuilder::AddToHorizontalBox(ScoreBox, TeamColumn, FMargin(TeamId > 0 ? 12.f : 0.f, 0.f, 0.f, 0.f), HAlign_Left, VAlign_Center);
		}
	}

	// ---- The podium: second, first, third
	if (PodiumBox)
	{
		PodiumBox->ClearChildren();
		for (const int32 Place : { 2, 1, 3 })
		{
			const AALKLobbyPlayerState* Player = Ranked.IsValidIndex(Place - 1) ? Ranked[Place - 1] : nullptr;
			AddPodiumPlace(Place, Player, Player && Player == MostValuable, Theme);
		}
	}
}

void UALKResultsScreen::AddPodiumPlace(int32 Place, const AALKLobbyPlayerState* Player, bool bMostValuable, const UALKTheme& Theme)
{
	using namespace ALKResultsScreenPrivate;
	const UALKOnlineSubsystem* Online = UALKOnlineSubsystem::Get(this);
	const bool bFirst = Place == 1;
	const FLinearColor PlaceColor = bFirst ? Theme.Warning : (Place == 2 ? Theme.TextSecondary : Theme.AccentAlt);

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	if (Player)
	{
		const FString ShownName = UALKScoreboard::GetShownPlayerName(Player);
		if (bMostValuable)
		{
			// The most valuable player: a star and an "MVP" chip over the picture.
			UALKFrame* Chip = FALKWidgetBuilder::MakeChip(*WidgetTree, NAME_None, LOCTEXT("ResultsMVP", "MVP"), EALKIcon::Star);
			FALKWidgetBuilder::StyleChip(Chip, Theme, Theme.Warning.CopyWithNewOpacity(0.2f), FLinearColor::Transparent, Theme.Warning);
			FALKWidgetBuilder::AddToVerticalBox(Column, Chip, FMargin(0.f, 0.f, 0.f, 10.f), HAlign_Center);
		}
		UALKAvatar* Picture = WidgetTree->ConstructWidget<UALKAvatar>(UALKAvatar::StaticClass());
		Picture->Diameter = bFirst ? 92.f : 70.f;
		Picture->SetPlayer(ShownName, nullptr);
		Picture->SetAnonymous(Online && Online->IsStreamerMode() && !Player->IsLocalPlayerState());
		Picture->SetBackColor(Theme.Panel);
		Picture->SetRingColor(bFirst ? Theme.Warning : FLinearColor::Transparent);
		FALKWidgetBuilder::AddToVerticalBox(Column, Picture, FMargin(), HAlign_Center);

		UTextBlock* NameText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		NameText->SetText(FText::FromString(ShownName));
		NameText->SetJustification(ETextJustify::Center);
		FALKWidgetBuilder::SetTextFont(NameText, Scaled(Theme.SubheadingFont, bFirst ? 1.f : 0.86f));
		NameText->SetColorAndOpacity(FSlateColor(Theme.TextPrimary));
		NameText->SetClipping(EWidgetClipping::ClipToBounds);
		FALKWidgetBuilder::AddToVerticalBox(Column, NameText, FMargin(0.f, 10.f, 0.f, 0.f), HAlign_Center);

		UTextBlock* ScoreText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		ScoreText->SetText(FText::Format(LOCTEXT("ResultsPoints", "{0} points"), FText::AsNumber(FMath::RoundToInt(Player->GetScore()))));
		ScoreText->SetJustification(ETextJustify::Center);
		FALKWidgetBuilder::StyleText(ScoreText, Theme, EALKTextRole::Data, Theme.TextMuted);
		FALKWidgetBuilder::AddToVerticalBox(Column, ScoreText, FMargin(0.f, 2.f, 0.f, 0.f), HAlign_Center);
	}

	// The block the player stands on, with the place on it.
	UTextBlock* PlaceText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	PlaceText->SetText(FText::AsNumber(Place));
	PlaceText->SetJustification(ETextJustify::Center);
	FALKWidgetBuilder::SetTextFont(PlaceText, Scaled(Theme.HeadingFont, bFirst ? 1.1f : 0.85f));
	PlaceText->SetColorAndOpacity(FSlateColor(PlaceColor.CopyWithNewOpacity(Player ? 1.f : 0.4f)));
	UALKFrame* Block = WidgetTree->ConstructWidget<UALKFrame>(UALKFrame::StaticClass());
	Block->SetHorizontalAlignment(HAlign_Center);
	Block->SetVerticalAlignment(VAlign_Top);
	Block->SetPadding(FMargin(8.f, 12.f, 8.f, 8.f));
	Block->SetContent(PlaceText);
	FALKFrameStyle BlockFrame = Theme.MakeCardFrame(0.f);
	if (bFirst)
	{
		BlockFrame.Fill = FMath::Lerp(Theme.PanelRaised, Theme.Accent, 0.22f).CopyWithNewOpacity(1.f);
		BlockFrame.Outline = Theme.Accent.CopyWithNewOpacity(0.6f);
	}
	Block->SetFrameStyle(BlockFrame);
	USizeBox* BlockSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	BlockSize->SetHeightOverride(PodiumHeights[FMath::Clamp(Place - 1, 0, 2)]);
	BlockSize->SetContent(Block);
	FALKWidgetBuilder::AddToVerticalBox(Column, BlockSize, FMargin(0.f, 12.f, 0.f, 0.f), HAlign_Fill);

	FALKWidgetBuilder::AddToHorizontalBox(PodiumBox, Column, FMargin(6.f, 0.f), HAlign_Fill, VAlign_Bottom, 1.f);
}

void UALKResultsScreen::UpdateCountdown()
{
	if (!CountdownText)
	{
		return;
	}
	const AALKLobbyGameState* GameState = BoundGameState.Get();
	if (!bHasResults || !GameState || GameState->Phase != EALKLobbyPhase::PostMatch)
	{
		if (ShownCountdown != MIN_int32 + 1)
		{
			ShownCountdown = MIN_int32 + 1;
			CountdownText->SetText(FText::GetEmpty());
		}
		return;
	}
	const float Remaining = GameState->GetPostMatchTimeRemaining();
	const int32 Seconds = Remaining >= 0.f ? FMath::CeilToInt(Remaining) : -1;
	if (Seconds == ShownCountdown)
	{
		return;
	}
	ShownCountdown = Seconds;
	if (Seconds >= 0)
	{
		CountdownText->SetText(FText::Format(LOCTEXT("ResultsReturning", "Returning to lobby in {0}"), FText::AsNumber(Seconds)));
	}
	else
	{
		// No countdown: the host decides when everyone goes back.
		const UALKOnlineSubsystem* Online = UALKOnlineSubsystem::Get(this);
		CountdownText->SetText(Online && Online->IsHost() ? LOCTEXT("ResultsHostDecides", "Return to the lobby when everyone is ready")
			: LOCTEXT("ResultsWaitingHost", "Waiting for the host to return to the lobby"));
	}
}

UWidget* UALKResultsScreen::NativeGetDesiredFocusTarget() const
{
	if (UWidget* Remembered = GetLastFocusedWidget())
	{
		return Remembered;
	}
	for (UALKButton* Button : { RematchButton.Get(), ReturnButton.Get(), LeaderboardsButton.Get() })
	{
		if (Button && Button->GetIsEnabled() && Button->GetVisibility() != ESlateVisibility::Collapsed && Button->GetVisibility() != ESlateVisibility::Hidden)
		{
			return Button;
		}
	}
	return Super::NativeGetDesiredFocusTarget();
}

bool UALKResultsScreen::OnBackRequested_Implementation()
{
	// In a game Back means leaving it (after a question); opened by hand it just closes.
	if (bHasResults)
	{
		LeaveGame();
		return true;
	}
	return Super::OnBackRequested_Implementation();
}

void UALKResultsScreen::ToggleRematchVote()
{
	const UALKOnlineSubsystem* Online = UALKOnlineSubsystem::Get(this);
	UALKPlayerNetComponent* NetComponent = Online ? Online->GetLocalNetComponent() : nullptr;
	const AALKLobbyPlayerState* Local = NetComponent ? NetComponent->GetLobbyPlayerState() : nullptr;
	if (Local)
	{
		// The tick on the button follows when the player state replicates.
		NetComponent->ServerVoteRematch(!Local->bRematchVote);
	}
}

void UALKResultsScreen::ReturnToLobbyNow()
{
	const UALKOnlineSubsystem* Online = UALKOnlineSubsystem::Get(this);
	if (UALKPlayerNetComponent* NetComponent = Online && Online->IsHost() ? Online->GetLocalNetComponent() : nullptr)
	{
		NetComponent->ServerReturnToLobby();
	}
}

void UALKResultsScreen::OpenLeaderboards()
{
	if (UALKOnlineFlowSubsystem* Flow = UALKOnlineFlowSubsystem::Get(this))
	{
		Flow->OpenLeaderboards(NAME_None);
	}
}

void UALKResultsScreen::LeaveGame()
{
	if (UALKOnlineFlowSubsystem* Flow = UALKOnlineFlowSubsystem::Get(this))
	{
		Flow->LeaveGameWithConfirm();
	}
}

#undef LOCTEXT_NAMESPACE
