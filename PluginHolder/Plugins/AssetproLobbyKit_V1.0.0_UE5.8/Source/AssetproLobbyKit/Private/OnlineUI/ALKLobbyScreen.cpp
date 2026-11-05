// Copyright 2026 CodeLikeMe. All Rights Reserved.

#include "OnlineUI/ALKLobbyScreen.h"
#include "ALKDeveloperSettings.h"
#include "ALKTheme.h"
#include "Lobby/ALKLobbyGameState.h"
#include "Lobby/ALKLobbyPlayerState.h"
#include "Lobby/ALKLobbyStatics.h"
#include "Lobby/ALKPlayerNetComponent.h"
#include "Online/ALKOnlineConfig.h"
#include "Online/ALKOnlineSubsystem.h"
#include "OnlineUI/ALKChatPanel.h"
#include "OnlineUI/ALKOnlineFlowSubsystem.h"
#include "OnlineUI/ALKPlayerSlot.h"
#include "Rules/ALKGameRules.h"
#include "Widgets/ALKButton.h"
#include "Widgets/ALKDecor.h"
#include "Widgets/ALKFrame.h"
#include "Widgets/ALKInputLabels.h"
#include "Widgets/ALKThumbnail.h"
#include "Widgets/ALKTitleText.h"
#include "Widgets/ALKWidgetBuilder.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WrapBox.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "HAL/PlatformInput.h"
#include "Styling/SlateTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ALKLobbyScreen)

#define LOCTEXT_NAMESPACE "AssetproLobbyKit"

namespace ALKLobbyScreenPrivate
{
	/** The screen's content never gets wider than this (ultrawide screens): it sits in the middle. */
	static constexpr float ContentMaxWidth = 1920.f;
	/** Between two panels of a column, and between two team panels. */
	static constexpr float PanelGap = 19.f;
	/** Between the left and the right column. */
	static constexpr float ColumnGap = 28.f;
	static constexpr float SeatGap = 10.f;
	/** The chat keeps at least this height; teams that need more than what is left scroll. */
	static constexpr float MinChatHeight = 262.f;
	/**
	 * The teams' scroll box is larger than the panels by this much, so the panels' shadows and the seats' focus rings are
	 * not cut off at its edge.
	 */
	static constexpr float ClipSide = 20.f;
	static constexpr float ClipTop = 12.f;
	static constexpr float ClipBottom = 30.f;
	/** Between the team panels and their scroll bar. */
	static constexpr float ScrollBarGap = 8.f;
	/** How often the screen looks for the game state and reads pings and the player list again. */
	static constexpr float PollInterval = 0.5f;
	/** A dot per player up to here; a larger lobby only shows the count. */
	static constexpr int32 MaxReadyDots = 12;
	/** The label of the Ready button, as a share of the theme's button font. */
	static constexpr float ReadyLabelScale = 0.94f;
	static constexpr float ReadyButtonHeight = 78.f;
	/** Where "Switch team" stands in the footer's hints. */
	static constexpr int32 TeamHintIndex = 2;
}

UALKLobbyScreen::UALKLobbyScreen(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bDimScene = true;
	// Back asks whether to leave the game (On Back Requested).
	bCloseOnBack = false;
	SlotClass = UALKPlayerSlot::StaticClass();
}

// ---------------------------------------------------------------- Layout

void UALKLobbyScreen::NativeBuildDefaultLayout(UWidgetTree& Tree)
{
	using namespace ALKLobbyScreenPrivate;

	UOverlay* Root = FALKWidgetBuilder::Make<UOverlay>(Tree, TEXT("Root"));
	Tree.RootWidget = Root;
	BackgroundImage = FALKWidgetBuilder::AddBackgroundLayers(Tree, Root);

	UVerticalBox* Main = FALKWidgetBuilder::Make<UVerticalBox>(Tree, TEXT("MainColumn"));
	USizeBox* MainSize = FALKWidgetBuilder::Make<USizeBox>(Tree, TEXT("MainSize"));
	MainSize->SetWidthOverride(ContentMaxWidth);
	MainSize->AddChild(Main);
	FALKWidgetBuilder::AddToOverlay(Root, MainSize, HAlign_Center, VAlign_Fill);

	// ---- Header: Lobby | server name | code | public / private, region
	UHorizontalBox* Header = FALKWidgetBuilder::Make<UHorizontalBox>(Tree, TEXT("HeaderRow"));
	FALKWidgetBuilder::AddToVerticalBox(Main, Header, FMargin(), HAlign_Left);
	UALKTitleText* Title = FALKWidgetBuilder::Make<UALKTitleText>(Tree, TEXT("TitleText"));
	Title->SetText(LOCTEXT("LobbyTitle", "Lobby"));
	TitleText = Title;
	FALKWidgetBuilder::AddToHorizontalBox(Header, TitleText, FMargin(), HAlign_Left, VAlign_Bottom);
	ServerNameText = FALKWidgetBuilder::MakeText(Tree, TEXT("ServerNameText"));
	FALKWidgetBuilder::AddToHorizontalBox(Header, ServerNameText, FMargin(22.f, 0.f, 0.f, 9.f), HAlign_Left, VAlign_Bottom);
	CodeChip = CreateMenuButton(FText::GetEmpty(), EALKButtonStyle::Tab);
	CodeChip->SetLeadingIcon(EALKIcon::Copy);
	FALKWidgetBuilder::AddToHorizontalBox(Header, CodeChip, FMargin(22.f, 0.f, 0.f, 2.f), HAlign_Left, VAlign_Bottom);
	InfoChip = FALKWidgetBuilder::MakeChip(Tree, TEXT("InfoChip"), FText::GetEmpty(), EALKIcon::Globe);
	FALKWidgetBuilder::AddToHorizontalBox(Header, InfoChip, FMargin(12.f, 0.f, 0.f, 9.f), HAlign_Left, VAlign_Bottom);

	// ---- Body: teams over the chat | the match over the ready panel
	UHorizontalBox* Body = FALKWidgetBuilder::Make<UHorizontalBox>(Tree, TEXT("Body"));
	FALKWidgetBuilder::AddToVerticalBox(Main, Body, FMargin(0.f, 24.f, 0.f, 0.f), HAlign_Fill, VAlign_Fill, 1.f);

	UVerticalBox* Left = FALKWidgetBuilder::Make<UVerticalBox>(Tree, TEXT("LeftColumn"));
	FALKWidgetBuilder::AddToHorizontalBox(Body, Left, FMargin(), HAlign_Fill, VAlign_Fill, 0.64f);

	// The teams scroll when they need more than the chat leaves them. The scroll box reaches past the panels on every
	// side (and is pulled back by as much), so their shadows are not cut off.
	TeamsScroll = FALKWidgetBuilder::Make<UScrollBox>(Tree, TEXT("TeamsScroll"));
	TeamsScroll->SetAnimateWheelScrolling(true);
	TeamsScroll->SetScrollWhenFocusChanges(EScrollWhenFocusChanges::AnimatedScroll);
	TeamsSize = FALKWidgetBuilder::Make<USizeBox>(Tree, TEXT("TeamsSize"));
	TeamsSize->AddChild(TeamsScroll);
	FALKWidgetBuilder::AddToVerticalBox(Left, TeamsSize, FMargin(-ClipSide, -ClipTop, -ClipSide, -ClipBottom));

	UVerticalBox* TeamsColumn = FALKWidgetBuilder::Make<UVerticalBox>(Tree, TEXT("TeamsColumn"));
	TeamsScroll->AddChild(TeamsColumn);
	if (UScrollBoxSlot* TeamsSlot = Cast<UScrollBoxSlot>(TeamsColumn->Slot))
	{
		TeamsSlot->SetPadding(FMargin(ClipSide, ClipTop, ClipSide, ClipBottom));
		TeamsSlot->SetHorizontalAlignment(HAlign_Fill);
	}
	TeamsBox = FALKWidgetBuilder::Make<UVerticalBox>(Tree, TEXT("TeamsBox"));
	FALKWidgetBuilder::AddToVerticalBox(TeamsColumn, TeamsBox);
	SpectatorsText = FALKWidgetBuilder::MakeText(Tree, TEXT("SpectatorsText"));
	SpectatorsText->SetAutoWrapText(true);
	SpectatorsText->SetVisibility(ESlateVisibility::Collapsed);
	FALKWidgetBuilder::AddToVerticalBox(TeamsColumn, SpectatorsText, FMargin(6.f, 12.f, 6.f, 0.f));

	UALKFrame* Chat = FALKWidgetBuilder::Make<UALKFrame>(Tree, TEXT("ChatFrame"));
	Chat->SetPadding(FMargin(16.f, 14.f, 16.f, 16.f));
	ChatFrame = Chat;
	FALKWidgetBuilder::AddToVerticalBox(Left, Chat, FMargin(0.f, PanelGap, 0.f, 0.f), HAlign_Fill, VAlign_Fill, 1.f);
	ChatPanel = Tree.ConstructWidget<UALKChatPanel>(UALKChatPanel::StaticClass(), TEXT("ChatPanel"));
	Chat->SetContent(ChatPanel);

	UVerticalBox* Right = FALKWidgetBuilder::Make<UVerticalBox>(Tree, TEXT("RightColumn"));
	FALKWidgetBuilder::AddToHorizontalBox(Body, Right, FMargin(ColumnGap, 0.f, 0.f, 0.f), HAlign_Fill, VAlign_Fill, 0.36f);

	// The match: picture, mode, map and size, the first rules, and what can be changed.
	UALKFrame* Match = FALKWidgetBuilder::Make<UALKFrame>(Tree, TEXT("MatchPanel"));
	Match->SetPadding(FMargin(14.f));
	MatchPanel = Match;
	FALKWidgetBuilder::AddToVerticalBox(Right, Match, FMargin(), HAlign_Fill, VAlign_Fill, 1.f);
	UVerticalBox* MatchColumn = FALKWidgetBuilder::Make<UVerticalBox>(Tree, TEXT("MatchColumn"));
	Match->SetContent(MatchColumn);

	MatchPicture = FALKWidgetBuilder::Make<UALKThumbnail>(Tree, TEXT("MatchPicture"));
	// The picture takes the height the rest leaves.
	MatchPicture->ThumbnailSize = FVector2D(320.0, 96.0);
	// The match's name sits right under the picture: the picture fades into the panel there (the theme's Picture Veil).
	MatchPicture->bThemeVeil = true;
	FALKWidgetBuilder::AddToVerticalBox(MatchColumn, MatchPicture, FMargin(), HAlign_Fill, VAlign_Fill, 1.f);

	UALKTitleText* Mode = FALKWidgetBuilder::Make<UALKTitleText>(Tree, TEXT("ModeText"));
	Mode->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
	ModeText = Mode;
	FALKWidgetBuilder::AddToVerticalBox(MatchColumn, ModeText, FMargin(10.f, 18.f, 10.f, 0.f));
	MapText = FALKWidgetBuilder::MakeText(Tree, TEXT("MapText"));
	MapText->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
	FALKWidgetBuilder::AddToVerticalBox(MatchColumn, MapText, FMargin(10.f, 4.f, 10.f, 0.f));

	RuleChips = FALKWidgetBuilder::Make<UWrapBox>(Tree, TEXT("RuleChips"));
	RuleChips->SetInnerSlotPadding(FVector2D(8.0, 8.0));
	FALKWidgetBuilder::AddToVerticalBox(MatchColumn, RuleChips, FMargin(10.f, 14.f, 10.f, 0.f));

	// A wrap box: in a longer language the second button moves to a line of its own instead of running out of the panel.
	UWrapBox* MatchButtons = FALKWidgetBuilder::Make<UWrapBox>(Tree, TEXT("MatchButtons"));
	MatchButtons->SetInnerSlotPadding(FVector2D(12.0, 8.0));
	FALKWidgetBuilder::AddToVerticalBox(MatchColumn, MatchButtons, FMargin(10.f, 16.f, 10.f, 6.f));
	SettingsButton = CreateMenuButton(LOCTEXT("LobbyChangeSettings", "Change settings"), EALKButtonStyle::Secondary);
	MatchButtons->AddChildToWrapBox(SettingsButton);
	VoteButton = CreateMenuButton(LOCTEXT("LobbyVoteMap", "Vote next map"), EALKButtonStyle::Ghost);
	MatchButtons->AddChildToWrapBox(VoteButton);
	VoteText = FALKWidgetBuilder::MakeText(Tree, TEXT("VoteText"));
	VoteText->SetAutoWrapText(true);
	FALKWidgetBuilder::AddToVerticalBox(MatchColumn, VoteText, FMargin(10.f, 4.f, 10.f, 6.f));

	// Ready: how many are, the big button, the host's start, leave, and why it is not starting yet.
	UALKFrame* Ready = FALKWidgetBuilder::Make<UALKFrame>(Tree, TEXT("ReadyPanel"));
	Ready->SetPadding(FMargin(24.f, 22.f, 24.f, 22.f));
	ReadyPanel = Ready;
	FALKWidgetBuilder::AddToVerticalBox(Right, Ready, FMargin(0.f, PanelGap, 0.f, 0.f));
	UVerticalBox* ReadyColumn = FALKWidgetBuilder::Make<UVerticalBox>(Tree, TEXT("ReadyColumn"));
	Ready->SetContent(ReadyColumn);

	UHorizontalBox* CountRow = FALKWidgetBuilder::Make<UHorizontalBox>(Tree, TEXT("ReadyCountRow"));
	FALKWidgetBuilder::AddToVerticalBox(ReadyColumn, CountRow);
	ReadyCountText = FALKWidgetBuilder::MakeText(Tree, TEXT("ReadyCountText"));
	FALKWidgetBuilder::AddToHorizontalBox(CountRow, ReadyCountText, FMargin(), HAlign_Fill, VAlign_Center, 1.f);
	ReadyDots = FALKWidgetBuilder::Make<UHorizontalBox>(Tree, TEXT("ReadyDots"));
	FALKWidgetBuilder::AddToHorizontalBox(CountRow, ReadyDots, FMargin(12.f, 0.f, 0.f, 0.f), HAlign_Right, VAlign_Center);

	ReadyButton = CreateMenuButton(LOCTEXT("LobbyReadyUp", "Ready up"), EALKButtonStyle::Solid);
	ReadyButton->SetLeadingIcon(EALKIcon::Check);
	// It plays the Ready sound itself.
	ReadyButton->bPlayClickSound = false;
	FALKWidgetBuilder::AddToVerticalBox(ReadyColumn, FALKWidgetBuilder::WrapInSizeBox(Tree, ReadyButton, 0.f, ReadyButtonHeight), FMargin(0.f, 18.f, 0.f, 0.f));

	UHorizontalBox* ReadyButtons = FALKWidgetBuilder::Make<UHorizontalBox>(Tree, TEXT("ReadyButtons"));
	FALKWidgetBuilder::AddToVerticalBox(ReadyColumn, ReadyButtons, FMargin(0.f, 18.f, 0.f, 0.f));
	StartButton = CreateMenuButton(LOCTEXT("LobbyStartMatch", "Start match"), EALKButtonStyle::Secondary);
	StartButton->SetLeadingIcon(EALKIcon::Play);
	FALKWidgetBuilder::AddToHorizontalBox(ReadyButtons, StartButton, FMargin(0.f, 0.f, 14.f, 0.f), HAlign_Fill, VAlign_Center, 1.f);
	LeaveButton = CreateMenuButton(LOCTEXT("LobbyLeave", "Leave lobby"), EALKButtonStyle::Danger);
	FALKWidgetBuilder::AddToHorizontalBox(ReadyButtons, LeaveButton, FMargin(), HAlign_Fill, VAlign_Center, 1.f);

	UALKTitleText* Countdown = FALKWidgetBuilder::Make<UALKTitleText>(Tree, TEXT("CountdownText"));
	Countdown->SetVisibility(ESlateVisibility::Collapsed);
	CountdownText = Countdown;
	FALKWidgetBuilder::AddToVerticalBox(ReadyColumn, CountdownText, FMargin(0.f, 16.f, 0.f, 0.f));
	StatusText = FALKWidgetBuilder::MakeText(Tree, TEXT("StatusText"));
	StatusText->SetAutoWrapText(true);
	FALKWidgetBuilder::AddToVerticalBox(ReadyColumn, StatusText, FMargin(0.f, 14.f, 0.f, 0.f));

	// ---- Footer: [Enter] Ready  [T] Chat  [X] Switch team  [Y] Invite  [Tab] Player options  [Esc] Leave
	UHorizontalBox* Hints = BuildInputHints(Tree, {
		FALKInputHint(LOCTEXT("KeyEnter", "Enter"), FText::GetEmpty(), LOCTEXT("LobbyHintReady", "Ready"))
			.WithAction(FSimpleDelegate::CreateUObject(this, &ThisClass::ToggleReady)),
		FALKInputHint(LOCTEXT("KeyT", "T"), FText::GetEmpty(), LOCTEXT("LobbyHintChat", "Chat"))
			.WithAction(FSimpleDelegate::CreateUObject(this, &ThisClass::FocusChat)),
		FALKInputHint(LOCTEXT("KeyX", "X"), EKeys::Gamepad_FaceButton_Left, LOCTEXT("LobbyHintTeam", "Switch team"))
			.WithAction(FSimpleDelegate::CreateUObject(this, &ThisClass::SwitchTeam)),
		FALKInputHint(LOCTEXT("KeyY", "Y"), EKeys::Gamepad_FaceButton_Top, LOCTEXT("LobbyHintInvite", "Invite"))
			.WithAction(FSimpleDelegate::CreateUObject(this, &ThisClass::OpenInvite)),
		FALKInputHint(LOCTEXT("KeyTab", "Tab"), EKeys::Gamepad_Special_Left, LOCTEXT("LobbyHintOptions", "Player options"))
			.WithAction(FSimpleDelegate::CreateUObject(this, &ThisClass::OpenPlayerOptions)),
		FALKInputHint(LOCTEXT("KeyEsc", "Esc"), FPlatformInput::GetGamepadBackKey(), LOCTEXT("LobbyHintLeave", "Leave"))
			.WithAction(FSimpleDelegate::CreateUObject(this, &ThisClass::LeaveLobby))
	});
	FALKWidgetBuilder::AddToVerticalBox(Main, Hints, FMargin(0.f, 22.f, 0.f, 0.f), HAlign_Left);
}

void UALKLobbyScreen::NativeOnLayoutReady()
{
	Super::NativeOnLayoutReady();

	if (CodeChip)
	{
		CodeChip->OnClicked().AddUObject(this, &ThisClass::CopyLobbyCode);
	}
	if (SettingsButton)
	{
		SettingsButton->OnClicked().AddUObject(this, &ThisClass::OpenChangeSettings);
	}
	if (VoteButton)
	{
		VoteButton->OnClicked().AddUObject(this, &ThisClass::VoteNextMap);
	}
	if (ReadyButton)
	{
		ReadyButton->OnClicked().AddUObject(this, &ThisClass::ToggleReady);
	}
	if (StartButton)
	{
		StartButton->OnClicked().AddUObject(this, &ThisClass::StartOrCancelMatch);
	}
	if (LeaveButton)
	{
		LeaveButton->OnClicked().AddUObject(this, &ThisClass::LeaveLobby);
	}
	if (ChatPanel)
	{
		ChatReleasedHandle = ChatPanel->OnInputReleased.AddUObject(this, &ThisClass::HandleChatReleased);
	}
	DefaultFocusWidget = ReadyButton.Get();
}

void UALKLobbyScreen::NativeApplyTheme(const UALKTheme& Theme)
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
	FALKWidgetBuilder::StyleText(ServerNameText, Theme, EALKTextRole::Body, Theme.TextMuted);
	FALKWidgetBuilder::StyleChip(InfoChip, Theme, FLinearColor::Transparent, FLinearColor::Transparent, Theme.TextMuted);
	FALKWidgetBuilder::StyleText(SpectatorsText, Theme, EALKTextRole::Caption, Theme.TextMuted);

	for (UBorder* Each : { ChatFrame.Get(), MatchPanel.Get(), ReadyPanel.Get() })
	{
		if (UALKFrame* Frame = Cast<UALKFrame>(Each))
		{
			Frame->SetFrameStyle(Theme.MakePanelFrame());
		}
	}
	for (const FTeamPanel& Panel : TeamPanels)
	{
		StyleTeamPanel(Panel, Theme);
	}
	if (TeamsScroll)
	{
		FScrollBarStyle BarStyle = TeamsScroll->GetWidgetBarStyle();
		const FSlateBrush Thumb = UALKTheme::MakeSolidBrush(Theme.Accent.CopyWithNewOpacity(0.85f));
		BarStyle.SetNormalThumbImage(Thumb);
		BarStyle.SetHoveredThumbImage(Thumb);
		BarStyle.SetDraggedThumbImage(Thumb);
		BarStyle.SetVerticalBackgroundImage(UALKTheme::MakeSolidBrush(Theme.TextPrimary.CopyWithNewOpacity(0.06f)));
		TeamsScroll->SetWidgetBarStyle(BarStyle);
		TeamsScroll->SetScrollbarThickness(FVector2D(3.0, 3.0));
		TeamsScroll->SetScrollbarPadding(FMargin(ALKLobbyScreenPrivate::ScrollBarGap, 0.f, 0.f, 0.f));
	}

	if (MatchPicture)
	{
		MatchPicture->SetCornerRadius(FMath::Max(0.f, Theme.CornerRadius - 4.f));
	}
	if (ModeText)
	{
		FALKWidgetBuilder::StyleText(ModeText, Theme, EALKTextRole::Heading);
		FSlateFontInfo Font = Theme.HeadingFont;
		Font.Size = FMath::Max(12.f, FMath::RoundToFloat(Theme.HeadingFont.Size * 0.72f));
		FALKWidgetBuilder::SetTextFont(ModeText, Font);
	}
	FALKWidgetBuilder::StyleText(MapText, Theme, EALKTextRole::Body, Theme.TextMuted);
	FALKWidgetBuilder::StyleText(VoteText, Theme, EALKTextRole::Caption, Theme.TextMuted);
	if (RuleChips)
	{
		for (UWidget* Child : RuleChips->GetAllChildren())
		{
			FALKWidgetBuilder::StyleChip(Cast<UALKFrame>(Child), Theme, Theme.PanelRaised, Theme.PanelOutline, Theme.TextPrimary);
		}
	}

	FALKWidgetBuilder::StyleText(ReadyCountText, Theme, EALKTextRole::Subheading, Theme.TextPrimary);
	if (CountdownText)
	{
		FALKWidgetBuilder::StyleText(CountdownText, Theme, EALKTextRole::Heading, Theme.GetAccentTextColor());
		FSlateFontInfo Font = Theme.HeadingFont;
		Font.Size = FMath::Max(12.f, FMath::RoundToFloat(Theme.HeadingFont.Size * 0.72f));
		FALKWidgetBuilder::SetTextFont(CountdownText, Font);
	}
	FALKWidgetBuilder::StyleText(StatusText, Theme, EALKTextRole::Caption, Theme.TextMuted);

	// The Ready button is the screen's main action: its label is larger than a dialog button's.
	ReadyLabel = ReadyButton && ReadyButton->WidgetTree ? Cast<UTextBlock>(ReadyButton->WidgetTree->FindWidget(TEXT("Label"))) : nullptr;

	// The dots, the chips and the seats take their colours from what they show.
	ShownRules.Reset();
	MarkDirty();
}

void UALKLobbyScreen::StyleTeamPanel(const FTeamPanel& Panel, const UALKTheme& Theme) const
{
	const UALKOnlineConfig* Config = UALKDeveloperSettings::GetOnlineConfig();
	if (UALKFrame* Frame = Panel.Frame.Get())
	{
		Frame->SetFrameStyle(Theme.MakePanelFrame());
	}
	if (UALKFrame* Square = Panel.Square.Get())
	{
		// The team's colour; a free for all has none and shows the accent.
		FALKFrameStyle Swatch;
		Swatch.Fill = Panel.TeamId >= 0 ? Config->GetTeamColor(Panel.TeamId) : Theme.Accent;
		Swatch.Radius = FMath::Min(Theme.CornerRadius, 3.f);
		Swatch.OutlineWidth = 0.f;
		Square->SetFrameStyle(Swatch);
	}
	if (UTextBlock* NameText = Panel.NameText.Get())
	{
		FALKWidgetBuilder::StyleText(NameText, Theme, EALKTextRole::Subheading, Theme.TextPrimary);
		NameText->SetTextTransformPolicy(Theme.bUppercaseTitles ? ETextTransformPolicy::ToUpper : ETextTransformPolicy::None);
	}
	if (UTextBlock* CountText = Panel.CountText.Get())
	{
		FALKWidgetBuilder::StyleText(CountText, Theme, EALKTextRole::Data, Theme.TextMuted);
		FSlateFontInfo Font = Theme.DataFont;
		Font.Size = Theme.DataFont.Size + 2.f;
		FALKWidgetBuilder::SetTextFont(CountText, Font);
	}
}

// ---------------------------------------------------------------- Opening and closing

void UALKLobbyScreen::NativeOnActivated()
{
	if (UALKOnlineSubsystem* Online = UALKOnlineSubsystem::Get(this))
	{
		if (!TalkingHandle.IsValid())
		{
			TalkingHandle = Online->OnPlayerTalkingNative.AddUObject(this, &ThisClass::HandlePlayerTalking);
			// Mutes show on the seats.
			ListsHandle = Online->OnListsChangedNative.AddUObject(this, &ThisClass::MarkDirty);
		}
	}
	TalkingUsers.Reset();
	ShownCountdown = INDEX_NONE;
	bChatKeyDown = false;
	PollTime = 0.f;
	BindGameState();
	RefreshLobby();
	if (ChatPanel)
	{
		ChatPanel->RefreshMessages();
	}
	Super::NativeOnActivated();
}

void UALKLobbyScreen::NativeOnDeactivated()
{
	if (UALKOnlineSubsystem* Online = UALKOnlineSubsystem::Get(this))
	{
		Online->OnPlayerTalkingNative.Remove(TalkingHandle);
		Online->OnListsChangedNative.Remove(ListsHandle);
	}
	TalkingHandle.Reset();
	ListsHandle.Reset();
	UnbindGameState();
	Super::NativeOnDeactivated();
}

AALKLobbyGameState* UALKLobbyScreen::GetLobbyState() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetGameState<AALKLobbyGameState>() : nullptr;
}

void UALKLobbyScreen::BindGameState()
{
	AALKLobbyGameState* GameState = GetLobbyState();
	if (GameState == BoundGameState.Get())
	{
		return;
	}
	UnbindGameState();
	BoundGameState = GameState;
	if (GameState)
	{
		StateHandle = GameState->OnLobbyStateChangedNative.AddUObject(this, &ThisClass::MarkDirty);
		PlayersHandle = GameState->OnPlayersChangedNative.AddUObject(this, &ThisClass::MarkDirty);
	}
	MarkDirty();
}

void UALKLobbyScreen::UnbindGameState()
{
	if (AALKLobbyGameState* GameState = BoundGameState.Get())
	{
		GameState->OnLobbyStateChangedNative.Remove(StateHandle);
		GameState->OnPlayersChangedNative.Remove(PlayersHandle);
	}
	StateHandle.Reset();
	PlayersHandle.Reset();
	BoundGameState.Reset();
}

// ---------------------------------------------------------------- Reading the lobby

void UALKLobbyScreen::RefreshLobby()
{
	bDirty = false;
	const AALKLobbyGameState* GameState = GetLobbyState();
	RefreshHeader(GameState);
	RefreshMatchCard(GameState);
	RefreshPlayers(GameState);
	RefreshReadyPanel(GameState);
}

void UALKLobbyScreen::RefreshHeader(const AALKLobbyGameState* GameState)
{
	const UALKOnlineSubsystem* Online = UALKOnlineSubsystem::Get(this);

	if (ServerNameText)
	{
		ServerNameText->SetText(GameState ? FText::FromString(GameState->ServerName) : FText::GetEmpty());
	}
	if (CodeChip)
	{
		// The code as it may be shown (hidden in streamer mode); picking the chip copies the real one.
		const FString Code = Online && GameState ? Online->GetDisplayLobbyCode() : FString();
		CodeChip->SetVisibility(Code.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
		CodeChip->SetButtonText(FText::Format(LOCTEXT("LobbyCodeChip", "Code  {0}"), FText::FromString(Code)).ToUpper());
	}
	if (InfoChip)
	{
		TArray<FText> Parts;
		if (Online && GameState)
		{
			const FALKSessionInfo Session = Online->GetCurrentSession();
			// Only the machine that hosts knows whether the game is listed.
			if (!Session.HostUserId.IsEmpty() && Session.HostUserId == Online->GetLocalUserId())
			{
				Parts.Add(Online->GetCurrentHostSettings().bPrivate ? LOCTEXT("LobbyPrivate", "Private") : LOCTEXT("LobbyPublic", "Public"));
			}
			if (GameState->bPasswordProtected)
			{
				Parts.Add(LOCTEXT("LobbyPassword", "Password"));
			}
			const FString RegionId = Session.Region.IsEmpty() ? Online->GetRegion() : Session.Region;
			if (Session.bLAN)
			{
				Parts.Add(LOCTEXT("LobbyLAN", "LAN"));
			}
			else if (!RegionId.IsEmpty())
			{
				FText RegionName = FText::FromString(RegionId);
				for (const FALKOnlineOption& Region : Online->GetRegions())
				{
					if (Region.Id.Equals(RegionId, ESearchCase::IgnoreCase) && !Region.DisplayName.IsEmpty())
					{
						RegionName = Region.DisplayName;
					}
				}
				Parts.Add(RegionName);
			}
		}
		FALKWidgetBuilder::SetChipText(InfoChip, FText::Join(LOCTEXT("LobbyInfoSeparator", "  \u00b7  "), Parts));
		InfoChip->SetVisibility(Parts.Num() > 0 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void UALKLobbyScreen::RefreshMatchCard(const AALKLobbyGameState* GameState)
{
	const UALKOnlineSubsystem* Online = UALKOnlineSubsystem::Get(this);
	const UALKOnlineConfig* Config = UALKDeveloperSettings::GetOnlineConfig();
	const UALKTheme* Theme = GetTheme();
	const UALKPlayerNetComponent* NetComponent = Online ? Online->GetLocalNetComponent() : nullptr;
	const AALKLobbyPlayerState* LocalPlayer = NetComponent ? NetComponent->GetLobbyPlayerState() : nullptr;
	const bool bWaiting = GameState && GameState->Phase == EALKLobbyPhase::Waiting;

	if (MatchPicture)
	{
		// The map's picture when the config has one; else art drawn from the map's Id.
		const FALKMapDefinition* Map = GameState ? Config->FindMap(GameState->MapId) : nullptr;
		const FName ArtId = GameState ? (GameState->MapId.IsNone() ? GameState->ModeId : GameState->MapId) : NAME_None;
		MatchPicture->SetPicture(Map ? Map->Thumbnail.LoadSynchronous() : nullptr, ArtId.IsNone() ? FName(TEXT("Lobby")) : ArtId);
	}
	if (ModeText)
	{
		const FText ModeName = GameState ? Config->GetModeName(GameState->ModeId) : FText::GetEmpty();
		ModeText->SetText(ModeName.IsEmpty() ? LOCTEXT("LobbyCustomGame", "Custom game") : ModeName);
	}
	if (MapText)
	{
		FText Size;
		if (GameState)
		{
			const int32 TeamCount = FMath::Max(0, GameState->TeamCount);
			const int32 PerTeam = UALKLobbyStatics::GetTeamCapacity(GameState->MaxPlayers, TeamCount);
			if (TeamCount == 2)
			{
				Size = FText::Format(LOCTEXT("LobbySizeVersus", "{0}v{1}"), FText::AsNumber(PerTeam), FText::AsNumber(PerTeam));
			}
			else if (TeamCount > 2)
			{
				Size = FText::Format(LOCTEXT("LobbySizeTeams", "{0} teams of {1}"), FText::AsNumber(TeamCount), FText::AsNumber(PerTeam));
			}
			else
			{
				Size = FText::Format(LOCTEXT("LobbySizePlayers", "{0} {0}|plural(one=player,other=players)"), GameState->MaxPlayers);
			}
		}
		const FText MapName = GameState ? Config->GetMapName(GameState->MapId) : FText::GetEmpty();
		MapText->SetText(MapName.IsEmpty() ? Size : FText::Format(LOCTEXT("LobbyMapAndSize", "{0}  \u00b7  {1}"), MapName, Size));
	}

	// A chip for each of the first rules; made again only when the rules changed.
	if (RuleChips && WidgetTree && Theme)
	{
		const FString RulesNow = GameState ? GameState->ModeId.ToString() + TEXT("|") + GameState->Rules.ToOptionsString() : FString();
		if (RulesNow != ShownRules || (GameState && RuleChips->GetChildrenCount() == 0 && !RulesNow.IsEmpty()))
		{
			ShownRules = RulesNow;
			RuleChips->ClearChildren();
			int32 Count = 0;
			for (const FALKRuleDefinition& Definition : GameState ? Config->GetRulesForMode(GameState->ModeId) : TArray<FALKRuleDefinition>())
			{
				if (Count >= MaxRuleChips || !GameState->Rules.Has(Definition.Id))
				{
					continue;
				}
				const FText Value = Definition.GetValueText(GameState->Rules.GetString(Definition.Id, Definition.DefaultValue));
				const FText Name = Definition.DisplayName.IsEmpty() ? FText::FromName(Definition.Id) : Definition.DisplayName;
				UALKFrame* Chip = FALKWidgetBuilder::MakeChip(*WidgetTree, NAME_None, FText::Format(LOCTEXT("LobbyRuleChip", "{0}: {1}"), Name, Value), EALKIcon::None);
				Chip->SetPadding(FMargin(12.f, 5.f, 12.f, 6.f));
				FALKWidgetBuilder::StyleChip(Chip, *Theme, Theme->PanelRaised, Theme->PanelOutline, Theme->TextPrimary);
				RuleChips->AddChildToWrapBox(Chip);
				++Count;
			}
			RuleChips->SetVisibility(Count > 0 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		}
	}

	// Change settings: the host, while the lobby waits.
	const bool bHost = Online && Online->IsHost();
	if (SettingsButton)
	{
		const bool bShow = bHost && bWaiting;
		const bool bHadFocus = SettingsButton->HasFocusedDescendants() || SettingsButton->HasAnyUserFocus();
		SettingsButton->SetVisibility(bShow ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		if (!bShow && bHadFocus && ReadyButton)
		{
			ReadyButton->SetFocus();
		}
	}

	// The map vote: when the config allows it and the mode has more than one map.
	const TArray<FALKMapDefinition> Maps = GameState ? Config->GetMapsForMode(GameState->ModeId) : TArray<FALKMapDefinition>();
	const bool bCanVote = GameState && Config->bAllowMapVote && Maps.Num() > 1 && GameState->IsInLobby() && GameState->Phase != EALKLobbyPhase::Starting;
	if (VoteButton)
	{
		const bool bHadFocus = VoteButton->HasFocusedDescendants() || VoteButton->HasAnyUserFocus();
		VoteButton->SetVisibility(bCanVote ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		if (!bCanVote && bHadFocus && ReadyButton)
		{
			ReadyButton->SetFocus();
		}
	}
	if (VoteText)
	{
		FText Line;
		if (bCanVote)
		{
			TArray<FText> Tallies;
			for (const FALKMapVote& Vote : GameState->MapVotes)
			{
				Tallies.Add(FText::Format(LOCTEXT("LobbyVoteTally", "{0} ({1})"), Config->GetMapName(Vote.MapId), FText::AsNumber(Vote.Votes)));
			}
			Line = Tallies.Num() > 0 ? FText::Format(LOCTEXT("LobbyVotes", "Map vote: {0}"), FText::Join(LOCTEXT("LobbyListSeparator", ", "), Tallies))
				: LOCTEXT("LobbyNoVotes", "No votes yet: the host's map is played.");
			if (LocalPlayer && !LocalPlayer->VotedMapId.IsNone())
			{
				Line = FText::Format(LOCTEXT("LobbyMyVote", "{0}  \u00b7  Your vote: {1}"), Line, Config->GetMapName(LocalPlayer->VotedMapId));
			}
		}
		VoteText->SetText(Line);
		VoteText->SetVisibility(bCanVote ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (UWidget* ButtonRow = SettingsButton ? SettingsButton->GetParent() : nullptr)
	{
		// No empty row when neither button applies (a player who joined, one map).
		ButtonRow->SetVisibility((bHost && bWaiting) || bCanVote ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
}

// ---------------------------------------------------------------- Teams and seats

UALKPlayerSlot* UALKLobbyScreen::AddSeat(FTeamPanel& Panel)
{
	TSubclassOf<UALKPlayerSlot> Class = SlotClass;
	if (!Class)
	{
		Class = UALKPlayerSlot::StaticClass();
	}
	const int32 ColumnCount = Panel.Columns.Num();
	UALKPlayerSlot* Seat = ColumnCount > 0 ? CreateWidget<UALKPlayerSlot>(this, Class) : nullptr;
	if (!Seat)
	{
		return nullptr;
	}
	// A free for all fills two columns left to right.
	const int32 SeatIndex = Panel.Seats.Num();
	if (UVerticalBox* Column = Panel.Columns[SeatIndex % ColumnCount].Get())
	{
		FALKWidgetBuilder::AddToVerticalBox(Column, Seat, FMargin(0.f, SeatIndex / ColumnCount == 0 ? 0.f : ALKLobbyScreenPrivate::SeatGap, 0.f, 0.f));
	}
	Seat->OnSlotFocused.AddUObject(this, &ThisClass::HandleSeatFocused);
	Panel.Seats.Add(Seat);
	bSeatsChanged = true;
	return Seat;
}

void UALKLobbyScreen::LinkSeats()
{
	// Up and Down go from seat to seat by what they are, not by what is on screen, so the seats of a large lobby that are
	// scrolled out of view are reached too (the scroll box then brings the focused one into view).
	for (int32 PanelIndex = 0; PanelIndex < TeamPanels.Num(); ++PanelIndex)
	{
		const FTeamPanel& Panel = TeamPanels[PanelIndex];
		const int32 ColumnCount = FMath::Max(1, Panel.Columns.Num());
		for (int32 Index = 0; Index < Panel.Seats.Num(); ++Index)
		{
			UALKPlayerSlot* Seat = Panel.Seats[Index].Get();
			if (!Seat)
			{
				continue;
			}
			UALKPlayerSlot* Below = Panel.Seats.IsValidIndex(Index + ColumnCount) ? Panel.Seats[Index + ColumnCount].Get() : nullptr;
			UALKPlayerSlot* Above = Panel.Seats.IsValidIndex(Index - ColumnCount) ? Panel.Seats[Index - ColumnCount].Get() : nullptr;
			// Past the panel's last seat: the panel under it (they stand two side by side); past its first: the one over it.
			if (!Below && TeamPanels.IsValidIndex(PanelIndex + 2) && TeamPanels[PanelIndex + 2].Seats.Num() > 0)
			{
				Below = TeamPanels[PanelIndex + 2].Seats[0].Get();
			}
			if (!Above && TeamPanels.IsValidIndex(PanelIndex - 2) && TeamPanels[PanelIndex - 2].Seats.Num() > 0)
			{
				Above = TeamPanels[PanelIndex - 2].Seats.Last().Get();
			}
			if (Below)
			{
				Seat->SetNavigationRuleExplicit(EUINavigation::Down, Below);
			}
			else
			{
				Seat->SetNavigationRuleBase(EUINavigation::Down, EUINavigationRule::Escape);
			}
			if (Above)
			{
				Seat->SetNavigationRuleExplicit(EUINavigation::Up, Above);
			}
			else
			{
				Seat->SetNavigationRuleBase(EUINavigation::Up, EUINavigationRule::Escape);
			}
		}
	}
}

void UALKLobbyScreen::RebuildTeams(int32 TeamCount, int32 Capacity)
{
	using namespace ALKLobbyScreenPrivate;
	if (!TeamsBox || !WidgetTree)
	{
		return;
	}

	// A seat that has the focus is about to go: the Ready button takes it.
	const UALKPlayerSlot* Focused = FocusedSeat.Get();
	const bool bSeatHadFocus = Focused && (Focused->HasFocusedDescendants() || Focused->HasAnyUserFocus());

	TeamsBox->ClearChildren();
	TeamPanels.Reset();
	FocusedSeat.Reset();
	bSeatsChanged = true;
	BuiltTeamCount = TeamCount;
	BuiltCapacity = Capacity;
	if (TeamCount < 0)
	{
		// No lobby yet.
		return;
	}

	UWidgetTree& Tree = *WidgetTree;
	const UALKOnlineConfig* Config = UALKDeveloperSettings::GetOnlineConfig();
	const UALKTheme* Theme = GetTheme();
	// One panel per team, two side by side; a free for all is one panel with two columns of seats.
	const int32 PanelCount = FMath::Max(1, TeamCount);
	UHorizontalBox* PanelRow = nullptr;
	for (int32 Index = 0; Index < PanelCount; ++Index)
	{
		const bool bSecond = Index % 2 == 1;
		if (!bSecond)
		{
			PanelRow = FALKWidgetBuilder::Make<UHorizontalBox>(Tree);
			FALKWidgetBuilder::AddToVerticalBox(TeamsBox, PanelRow, FMargin(0.f, Index == 0 ? 0.f : PanelGap, 0.f, 0.f));
		}
		FTeamPanel& Panel = TeamPanels.AddDefaulted_GetRef();
		Panel.TeamId = TeamCount > 0 ? Index : INDEX_NONE;

		UALKFrame* Frame = FALKWidgetBuilder::Make<UALKFrame>(Tree);
		Frame->SetPadding(FMargin(14.f, 16.f, 14.f, 14.f));
		Panel.Frame = Frame;
		FALKWidgetBuilder::AddToHorizontalBox(PanelRow, Frame, FMargin(bSecond ? PanelGap : 0.f, 0.f, 0.f, 0.f), HAlign_Fill, VAlign_Top, 1.f);

		UVerticalBox* PanelColumn = FALKWidgetBuilder::Make<UVerticalBox>(Tree);
		Frame->SetContent(PanelColumn);

		// Header: colour square, team name, "3 / 4"
		UHorizontalBox* PanelHeader = FALKWidgetBuilder::Make<UHorizontalBox>(Tree);
		FALKWidgetBuilder::AddToVerticalBox(PanelColumn, PanelHeader, FMargin(6.f, 0.f, 6.f, 14.f));
		UALKFrame* Square = FALKWidgetBuilder::Make<UALKFrame>(Tree);
		Panel.Square = Square;
		FALKWidgetBuilder::AddToHorizontalBox(PanelHeader, FALKWidgetBuilder::WrapInSizeBox(Tree, Square, 14.f, 14.f), FMargin(0.f, 0.f, 12.f, 0.f), HAlign_Left, VAlign_Center);
		UTextBlock* NameText = FALKWidgetBuilder::MakeText(Tree, NAME_None, TeamCount > 0 ? Config->GetTeamName(Index) : LOCTEXT("LobbyPlayers", "Players"));
		Panel.NameText = NameText;
		FALKWidgetBuilder::AddToHorizontalBox(PanelHeader, NameText, FMargin(), HAlign_Fill, VAlign_Center, 1.f);
		UTextBlock* CountText = FALKWidgetBuilder::MakeText(Tree, NAME_None);
		Panel.CountText = CountText;
		FALKWidgetBuilder::AddToHorizontalBox(PanelHeader, CountText, FMargin(12.f, 0.f, 0.f, 0.f), HAlign_Right, VAlign_Center);

		UHorizontalBox* SeatRow = FALKWidgetBuilder::Make<UHorizontalBox>(Tree);
		FALKWidgetBuilder::AddToVerticalBox(PanelColumn, SeatRow);
		const int32 ColumnCount = TeamCount > 0 ? 1 : 2;
		for (int32 Column = 0; Column < ColumnCount; ++Column)
		{
			UVerticalBox* SeatColumn = FALKWidgetBuilder::Make<UVerticalBox>(Tree);
			Panel.Columns.Add(SeatColumn);
			FALKWidgetBuilder::AddToHorizontalBox(SeatRow, SeatColumn, FMargin(Column == 0 ? 0.f : SeatGap, 0.f, 0.f, 0.f), HAlign_Fill, VAlign_Top, 1.f);
		}
		for (int32 Seat = 0; Seat < Capacity; ++Seat)
		{
			AddSeat(Panel);
		}
		if (Theme)
		{
			StyleTeamPanel(Panel, *Theme);
		}
	}
	if (PanelCount > 1 && PanelCount % 2 == 1 && PanelRow)
	{
		// An odd team out keeps the width of the others.
		FALKWidgetBuilder::AddToHorizontalBox(PanelRow, FALKWidgetBuilder::Make<UVerticalBox>(Tree), FMargin(PanelGap, 0.f, 0.f, 0.f), HAlign_Fill, VAlign_Top, 1.f);
	}

	if (bSeatHadFocus && ReadyButton)
	{
		ReadyButton->SetFocus();
	}
}

void UALKLobbyScreen::RefreshPlayers(const AALKLobbyGameState* GameState)
{
	const UALKOnlineSubsystem* Online = UALKOnlineSubsystem::Get(this);
	const int32 TeamCount = GameState ? FMath::Max(0, GameState->TeamCount) : INDEX_NONE;
	const int32 Capacity = GameState ? UALKLobbyStatics::GetTeamCapacity(GameState->MaxPlayers, TeamCount) : 0;
	if (TeamCount != BuiltTeamCount || Capacity != BuiltCapacity)
	{
		RebuildTeams(TeamCount, Capacity);
	}
	if (!GameState)
	{
		if (SpectatorsText)
		{
			SpectatorsText->SetVisibility(ESlateVisibility::Collapsed);
		}
		return;
	}

	for (FTeamPanel& Panel : TeamPanels)
	{
		const TArray<AALKLobbyPlayerState*> Players = GameState->GetPlayersOnTeam(Panel.TeamId);
		// Never fewer seats than players, whatever the limit says.
		const int32 Needed = FMath::Max(Capacity, Players.Num());
		while (Panel.Seats.Num() < Needed && AddSeat(Panel))
		{
		}
		while (Panel.Seats.Num() > Needed)
		{
			if (UALKPlayerSlot* Extra = Panel.Seats.Pop().Get())
			{
				Extra->RemoveFromParent();
			}
			bSeatsChanged = true;
		}
		for (int32 Index = 0; Index < Panel.Seats.Num(); ++Index)
		{
			if (UALKPlayerSlot* Seat = Panel.Seats[Index].Get())
			{
				AALKLobbyPlayerState* Player = Players.IsValidIndex(Index) ? Players[Index] : nullptr;
				Seat->SetPlayer(Player);
				Seat->SetTalking(Player && TalkingUsers.Contains(Player->GetUserId()));
			}
		}
		if (UTextBlock* CountText = Panel.CountText.Get())
		{
			CountText->SetText(FText::Format(LOCTEXT("LobbyTeamCount", "{0} / {1}"), FText::AsNumber(Players.Num()), FText::AsNumber(Capacity)));
		}
	}
	if (bSeatsChanged)
	{
		bSeatsChanged = false;
		LinkSeats();
	}

	if (SpectatorsText)
	{
		TArray<FText> Names;
		for (const AALKLobbyPlayerState* Spectator : GameState->GetSpectators())
		{
			const bool bLocal = Spectator->IsLocalPlayerState();
			Names.Add(FText::FromString(bLocal || !Online ? Spectator->GetPlayerName() : Online->GetDisplayName(Spectator->GetPlayerName(), Spectator->GetStableIndex())));
		}
		SpectatorsText->SetText(FText::Format(LOCTEXT("LobbySpectators", "Watching: {0}"), FText::Join(LOCTEXT("LobbyListSeparator", ", "), Names)));
		SpectatorsText->SetVisibility(Names.Num() > 0 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

// ---------------------------------------------------------------- Ready, start, countdown

bool UALKLobbyScreen::GetStartState(const AALKLobbyGameState* GameState, bool& bOutForce) const
{
	bOutForce = false;
	if (!GameState || GameState->Phase != EALKLobbyPhase::Waiting)
	{
		return false;
	}
	const UALKOnlineConfig* Config = UALKDeveloperSettings::GetOnlineConfig();
	const UALKOnlineSubsystem* Online = UALKOnlineSubsystem::Get(this);
	const UALKPlayerNetComponent* NetComponent = Online ? Online->GetLocalNetComponent() : nullptr;
	const AALKLobbyPlayerState* LocalPlayer = NetComponent ? NetComponent->GetLobbyPlayerState() : nullptr;

	// Pressing Start readies the host, so they count as ready here.
	const int32 Ready = GameState->GetReadyCount() + (LocalPlayer && !LocalPlayer->bReady && !LocalPlayer->bSpectating ? 1 : 0);
	const bool bHasMap = !Config->GetMatchTravelURL(GameState->ModeId, GameState->MapId).IsEmpty();
	const EALKStartBlocker Blocker = UALKLobbyStatics::GetStartBlocker(GameState->GetActivePlayerCount(), Ready, Config->MinPlayersToStart, bHasMap, false);
	if (Blocker == EALKStartBlocker::None)
	{
		return true;
	}
	if (Blocker == EALKStartBlocker::NotAllReady && Config->bAllowForceStart)
	{
		bOutForce = true;
		return true;
	}
	return false;
}

void UALKLobbyScreen::RefreshReadyPanel(const AALKLobbyGameState* GameState)
{
	const UALKOnlineSubsystem* Online = UALKOnlineSubsystem::Get(this);
	const UALKOnlineConfig* Config = UALKDeveloperSettings::GetOnlineConfig();
	const UALKTheme* Theme = GetTheme();
	const UALKPlayerNetComponent* NetComponent = Online ? Online->GetLocalNetComponent() : nullptr;
	const AALKLobbyPlayerState* LocalPlayer = NetComponent ? NetComponent->GetLobbyPlayerState() : nullptr;
	const bool bHost = Online && Online->IsHost();
	const int32 Active = GameState ? GameState->GetActivePlayerCount() : 0;
	const int32 Ready = GameState ? GameState->GetReadyCount() : 0;
	const bool bWaiting = GameState && GameState->Phase == EALKLobbyPhase::Waiting;
	const bool bCountdown = GameState && GameState->Phase == EALKLobbyPhase::Countdown;

	if (ReadyCountText)
	{
		ReadyCountText->SetText(GameState ? FText::Format(LOCTEXT("LobbyReadyCount", "{0} of {1} {1}|plural(one=player,other=players) ready"), Ready, Active)
			: LOCTEXT("LobbyNoLobby", "Waiting for the lobby..."));
	}

	// A dot per player, lit for the ready ones.
	if (ReadyDots && WidgetTree && Theme)
	{
		const int32 DotCount = Active <= ALKLobbyScreenPrivate::MaxReadyDots ? Active : 0;
		while (ReadyDots->GetChildrenCount() > DotCount)
		{
			ReadyDots->RemoveChildAt(ReadyDots->GetChildrenCount() - 1);
		}
		while (ReadyDots->GetChildrenCount() < DotCount)
		{
			UALKIconWidget* Dot = FALKWidgetBuilder::MakeIcon(*WidgetTree, NAME_None, EALKIcon::Dot, 28.f);
			FALKWidgetBuilder::AddToHorizontalBox(ReadyDots, Dot, FMargin(ReadyDots->GetChildrenCount() == 0 ? 0.f : 1.f, 0.f, 0.f, 0.f));
		}
		for (int32 Index = 0; Index < DotCount; ++Index)
		{
			if (UALKIconWidget* Dot = Cast<UALKIconWidget>(ReadyDots->GetChildAt(Index)))
			{
				Dot->SetIcon(EALKIcon::Dot, Index < Ready ? Theme->Accent : Theme->TextMuted.CopyWithNewOpacity(Theme->TextMuted.A * 0.35f));
			}
		}
	}

	if (ReadyButton)
	{
		const bool bReady = LocalPlayer && LocalPlayer->bReady;
		ReadyButton->SetButtonText(bReady ? LOCTEXT("LobbyNotReady", "Not ready") : LOCTEXT("LobbyReadyUp", "Ready up"));
		ReadyButton->SetLeadingIcon(bReady ? EALKIcon::Cross : EALKIcon::Check);
		ReadyButton->SetIsEnabled(LocalPlayer && !LocalPlayer->bSpectating && (bWaiting || bCountdown));
	}

	if (StartButton)
	{
		bool bForce = false;
		const bool bCanStart = GetStartState(GameState, bForce);
		const bool bShow = bHost && GameState != nullptr;
		const bool bEnable = bCountdown || bCanStart;
		const bool bHadFocus = StartButton->HasFocusedDescendants() || StartButton->HasAnyUserFocus();
		StartButton->SetButtonText(bCountdown ? LOCTEXT("LobbyCancelStart", "Cancel") : (bForce ? LOCTEXT("LobbyForceStart", "Force start") : LOCTEXT("LobbyStartMatch", "Start match")));
		StartButton->SetLeadingIcon(bCountdown ? EALKIcon::Cross : EALKIcon::Play);
		StartButton->SetVisibility(bShow ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		StartButton->SetIsEnabled(bEnable);
		if (bHadFocus && (!bShow || !bEnable) && ReadyButton)
		{
			// The button under the focus went away: the Ready button takes it.
			ReadyButton->SetFocus();
		}
		// Down from the wide Ready button goes to the host's Start when it can be picked, else to Leave.
		if (ReadyButton)
		{
			UWidget* Below = bShow && bEnable ? static_cast<UWidget*>(StartButton.Get()) : static_cast<UWidget*>(LeaveButton.Get());
			if (Below)
			{
				ReadyButton->SetNavigationRuleExplicit(EUINavigation::Down, Below);
			}
		}
	}

	if (StatusText)
	{
		FText Status;
		if (!GameState)
		{
			Status = LOCTEXT("LobbyNoLobbyDetail", "The lobby shows as soon as the game is ready.");
		}
		else if (bCountdown)
		{
			Status = GameState->bForcedStart ? LOCTEXT("LobbyStatusForced", "The host started the match.") : LOCTEXT("LobbyStatusAllReady", "Everyone is ready.");
		}
		else if (bWaiting)
		{
			// Why the match is not starting, as everyone sees it.
			const EALKStartBlocker Blocker = GameState->GetStartBlocker(false);
			const FText Reason = UALKLobbyStatics::GetStartBlockerText(Blocker);
			bool bHostForce = false;
			if (bHost && Blocker == EALKStartBlocker::NotAllReady && GetStartState(GameState, bHostForce) && !bHostForce)
			{
				// Only the host is missing, and Start readies them.
				Status = LOCTEXT("LobbyStatusHostOnly", "Everyone else is ready. Ready up or start the match.");
			}
			else if (Blocker == EALKStartBlocker::NotAllReady && Config->bAllowForceStart)
			{
				Status = bHost ? FText::Format(LOCTEXT("LobbyStatusForceHost", "{0}. You can force the start."), Reason)
					: FText::Format(LOCTEXT("LobbyStatusForce", "{0}. The host can force the start."), Reason);
			}
			else
			{
				Status = Blocker == EALKStartBlocker::None ? LOCTEXT("LobbyStatusAllReady", "Everyone is ready.") : Reason;
			}
		}
		StatusText->SetText(Status);
		StatusText->SetVisibility(Status.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}

	// "Switch team" is only a hint while there are teams to switch between.
	if (HintButtons.IsValidIndex(ALKLobbyScreenPrivate::TeamHintIndex) && HintButtons[ALKLobbyScreenPrivate::TeamHintIndex])
	{
		UALKButton* TeamHint = HintButtons[ALKLobbyScreenPrivate::TeamHintIndex];
		const bool bHasTeams = GameState && GameState->TeamCount > 1;
		if (!bHasTeams)
		{
			TeamHint->SetVisibility(ESlateVisibility::Collapsed);
		}
		else if (TeamHint->GetVisibility() == ESlateVisibility::Collapsed && (!IsUsingGamepad() || TeamHint->HasInputHint()))
		{
			TeamHint->SetVisibility(ESlateVisibility::Visible);
		}
	}
	UpdateCountdown(GameState, false);
}

void UALKLobbyScreen::UpdateCountdown(const AALKLobbyGameState* GameState, bool bPlaySound)
{
	// The second that shows: -1 none, 0 "Starting...", else the seconds left.
	int32 Seconds = INDEX_NONE;
	if (GameState && GameState->Phase == EALKLobbyPhase::Countdown)
	{
		Seconds = FMath::Max(1, FMath::CeilToInt(GameState->GetCountdownRemaining()));
	}
	else if (GameState && GameState->Phase == EALKLobbyPhase::Starting)
	{
		Seconds = 0;
	}
	if (Seconds == ShownCountdown)
	{
		return;
	}
	ShownCountdown = Seconds;
	if (CountdownText)
	{
		CountdownText->SetText(Seconds > 0 ? FText::Format(LOCTEXT("LobbyStartingIn", "Starting in {0}"), FText::AsNumber(Seconds))
			: LOCTEXT("LobbyStarting", "Starting..."));
		CountdownText->SetVisibility(Seconds == INDEX_NONE ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
	if (bPlaySound && Seconds > 0)
	{
		PlayThemeSound(EALKUISound::Countdown);
	}
}

void UALKLobbyScreen::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	using namespace ALKLobbyScreenPrivate;

	if (!IsActivated())
	{
		return;
	}

	// The game state arrives a moment after a level change; pings and the player list are read again now and then.
	PollTime += InDeltaTime;
	if (PollTime >= PollInterval)
	{
		PollTime = 0.f;
		BindGameState();
		MarkDirty();
	}
	if (bDirty)
	{
		RefreshLobby();
	}
	UpdateCountdown(GetLobbyState(), true);

	// The teams take the height they need, but leave the chat its room: past that they scroll.
	const UWidget* Holder = TeamsBox ? TeamsBox->GetParent() : nullptr;
	if (TeamsSize && TeamsScroll && Holder)
	{
		const UWidget* Column = TeamsSize->GetParent();
		const float ColumnHeight = Column ? Column->GetCachedGeometry().GetLocalSize().Y : 0.f;
		const float MaxTeams = FMath::Max(UALKPlayerSlot::SlotHeight * 2.f + 80.f, ColumnHeight - MinChatHeight - PanelGap);
		const bool bScrolls = ColumnHeight > 1.f && Holder->GetDesiredSize().Y > MaxTeams + 0.5f;

		// While the scroll bar shows, the panels give up what it takes of the room at their right, and keep their width.
		// Measured, not assumed: the bar's width is the engine's business.
		float BarSpace = TeamsBarSpace;
		const float ScrollWidth = TeamsScroll->GetCachedGeometry().GetLocalSize().X;
		const float HolderWidth = Holder->GetCachedGeometry().GetLocalSize().X;
		if (ScrollWidth > 1.f && HolderWidth > 1.f)
		{
			BarSpace = FMath::Clamp(ScrollWidth - HolderWidth - ClipSide - (ClipSide - TeamsBarSpace), 0.f, ClipSide);
		}

		if (ColumnHeight > 1.f && (bScrolls != bTeamsScroll || !FMath::IsNearlyEqual(MaxTeams, LastMaxTeams, 0.5f) || !FMath::IsNearlyEqual(BarSpace, TeamsBarSpace, 0.5f)))
		{
			bTeamsScroll = bScrolls;
			LastMaxTeams = MaxTeams;
			TeamsBarSpace = BarSpace;
			// Teams that fit: the scroll box reaches past the panels above and below, for their shadows. Teams that scroll:
			// it ends where the panels may end, so nothing shows in the gap to the header and to the chat.
			const float Above = bScrolls ? 0.f : ClipTop;
			const float Below = bScrolls ? 0.f : ClipBottom;
			TeamsSize->SetMaxDesiredHeight(FMath::RoundToFloat(MaxTeams + Above + Below));
			if (UVerticalBoxSlot* SizeSlot = Cast<UVerticalBoxSlot>(TeamsSize->Slot))
			{
				SizeSlot->SetPadding(FMargin(-ClipSide, -Above, -ClipSide, -Below));
			}
			if (UScrollBoxSlot* TeamsSlot = Cast<UScrollBoxSlot>(Holder->Slot))
			{
				TeamsSlot->SetPadding(FMargin(ClipSide, Above, ClipSide - BarSpace, ClipBottom));
			}
		}
	}

	// The Ready button's larger label (the button sets its own font again when the theme changes).
	if (UTextBlock* LabelText = ReadyLabel.Get())
	{
		if (const UALKTheme* Theme = GetTheme())
		{
			const float Wanted = FMath::RoundToFloat(Theme->ButtonFont.Size * ReadyLabelScale);
			if (!FMath::IsNearlyEqual(static_cast<float>(LabelText->GetFont().Size), Wanted))
			{
				FSlateFontInfo Font = Theme->ButtonFont;
				Font.Size = Wanted;
				FALKWidgetBuilder::SetTextFont(LabelText, Font);
			}
		}
	}
}

// ---------------------------------------------------------------- What the player does

void UALKLobbyScreen::ToggleReady()
{
	const UALKOnlineSubsystem* Online = UALKOnlineSubsystem::Get(this);
	UALKPlayerNetComponent* NetComponent = Online ? Online->GetLocalNetComponent() : nullptr;
	const AALKLobbyPlayerState* LocalPlayer = NetComponent ? NetComponent->GetLobbyPlayerState() : nullptr;
	if (!LocalPlayer || LocalPlayer->bSpectating)
	{
		PlayThemeSound(EALKUISound::Locked);
		return;
	}
	const bool bNowReady = !LocalPlayer->bReady;
	PlayThemeSound(bNowReady ? EALKUISound::Ready : EALKUISound::Click);
	NetComponent->ServerSetReady(bNowReady);
}

void UALKLobbyScreen::StartOrCancelMatch()
{
	const UALKOnlineSubsystem* Online = UALKOnlineSubsystem::Get(this);
	UALKPlayerNetComponent* NetComponent = Online ? Online->GetLocalNetComponent() : nullptr;
	const AALKLobbyGameState* GameState = GetLobbyState();
	if (!NetComponent || !GameState || !Online->IsHost())
	{
		PlayThemeSound(EALKUISound::Locked);
		return;
	}
	if (GameState->Phase == EALKLobbyPhase::Countdown)
	{
		NetComponent->ServerCancelCountdown();
		return;
	}
	bool bForce = false;
	if (!GetStartState(GameState, bForce))
	{
		PlayThemeSound(EALKUISound::Locked);
		return;
	}
	NetComponent->ServerStartMatch(bForce);
}

void UALKLobbyScreen::SwitchTeam()
{
	const UALKOnlineSubsystem* Online = UALKOnlineSubsystem::Get(this);
	UALKPlayerNetComponent* NetComponent = Online ? Online->GetLocalNetComponent() : nullptr;
	const AALKLobbyPlayerState* LocalPlayer = NetComponent ? NetComponent->GetLobbyPlayerState() : nullptr;
	const AALKLobbyGameState* GameState = GetLobbyState();
	if (!LocalPlayer || !GameState || GameState->TeamCount < 2 || LocalPlayer->bSpectating)
	{
		PlayThemeSound(EALKUISound::Locked);
		return;
	}
	// The server says so when the other team is full or teams are the host's to choose.
	PlayThemeSound(EALKUISound::Change);
	NetComponent->ServerRequestTeam((FMath::Max(0, LocalPlayer->TeamId) + 1) % GameState->TeamCount);
}

void UALKLobbyScreen::VoteNextMap()
{
	const UALKOnlineSubsystem* Online = UALKOnlineSubsystem::Get(this);
	UALKPlayerNetComponent* NetComponent = Online ? Online->GetLocalNetComponent() : nullptr;
	const AALKLobbyPlayerState* LocalPlayer = NetComponent ? NetComponent->GetLobbyPlayerState() : nullptr;
	const AALKLobbyGameState* GameState = GetLobbyState();
	const UALKOnlineConfig* Config = UALKDeveloperSettings::GetOnlineConfig();
	const TArray<FALKMapDefinition> Maps = GameState ? Config->GetMapsForMode(GameState->ModeId) : TArray<FALKMapDefinition>();
	if (!LocalPlayer || !Config->bAllowMapVote || Maps.Num() < 2)
	{
		PlayThemeSound(EALKUISound::Locked);
		return;
	}
	// The next map of the mode; after the last one the vote is taken back.
	const FName Current = LocalPlayer->VotedMapId;
	const int32 Index = Maps.IndexOfByPredicate([Current](const FALKMapDefinition& Each) { return Each.Id == Current; });
	PlayThemeSound(EALKUISound::Change);
	NetComponent->ServerVoteMap(Maps.IsValidIndex(Index + 1) ? Maps[Index + 1].Id : FName());
}

void UALKLobbyScreen::CopyLobbyCode()
{
	UALKOnlineFlowSubsystem* Flow = UALKOnlineFlowSubsystem::Get(this);
	const UALKOnlineSubsystem* Online = UALKOnlineSubsystem::Get(this);
	const FString Code = Online ? Online->GetLobbyCode() : FString();
	if (!Flow || Code.IsEmpty())
	{
		PlayThemeSound(EALKUISound::Locked);
		return;
	}
	Flow->CopyToClipboard(Code, LOCTEXT("LobbyCodeCopied", "Lobby code copied"));
}

void UALKLobbyScreen::OpenInvite()
{
	if (UALKOnlineFlowSubsystem* Flow = UALKOnlineFlowSubsystem::Get(this))
	{
		Flow->OpenSocial();
	}
}

void UALKLobbyScreen::OpenChangeSettings()
{
	const UALKOnlineSubsystem* Online = UALKOnlineSubsystem::Get(this);
	UALKOnlineFlowSubsystem* Flow = UALKOnlineFlowSubsystem::Get(this);
	if (!Flow || !Online || !Online->IsHost())
	{
		PlayThemeSound(EALKUISound::Locked);
		return;
	}
	Flow->OpenHostGame(true);
}

void UALKLobbyScreen::OpenPlayerOptions()
{
	// The focused seat's player; with the focus somewhere else, go to the players first.
	UALKPlayerSlot* Seat = FocusedSeat.Get();
	if (Seat && (Seat->HasFocusedDescendants() || Seat->HasAnyUserFocus()))
	{
		Seat->SimulateClick();
		return;
	}
	UALKPlayerSlot* Target = nullptr;
	for (const FTeamPanel& Panel : TeamPanels)
	{
		for (const TWeakObjectPtr<UALKPlayerSlot>& Each : Panel.Seats)
		{
			UALKPlayerSlot* Candidate = Each.Get();
			if (!Candidate || Candidate->IsOpenSlot())
			{
				continue;
			}
			// Another player rather than yourself, when there is one.
			if (!Target || (Target->IsLocalPlayerSlot() && !Candidate->IsLocalPlayerSlot()))
			{
				Target = Candidate;
			}
		}
	}
	if (Target)
	{
		Target->SetFocus();
	}
	else
	{
		PlayThemeSound(EALKUISound::Locked);
	}
}

void UALKLobbyScreen::LeaveLobby()
{
	if (UALKOnlineFlowSubsystem* Flow = UALKOnlineFlowSubsystem::Get(this))
	{
		Flow->LeaveGameWithConfirm();
	}
}

void UALKLobbyScreen::FocusChat()
{
	if (ChatPanel)
	{
		ChatPanel->FocusInput();
	}
}

bool UALKLobbyScreen::IsTypingChat() const
{
	return ChatPanel && ChatPanel->IsTyping();
}

// ---------------------------------------------------------------- Events and keys

void UALKLobbyScreen::HandleSeatFocused(UALKPlayerSlot* Seat)
{
	FocusedSeat = Seat;
	// In a lobby whose teams scroll, the focused seat comes into view with room for its focus ring.
	if (Seat && bTeamsScroll && TeamsScroll)
	{
		TeamsScroll->ScrollWidgetIntoView(Seat, true, EDescendantScrollDestination::IntoView, 14.f);
	}
}

void UALKLobbyScreen::HandlePlayerTalking(const FString& UserId, bool bTalking)
{
	if (bTalking)
	{
		TalkingUsers.Add(UserId);
	}
	else
	{
		TalkingUsers.Remove(UserId);
	}
	for (const FTeamPanel& Panel : TeamPanels)
	{
		for (const TWeakObjectPtr<UALKPlayerSlot>& Each : Panel.Seats)
		{
			UALKPlayerSlot* Seat = Each.Get();
			const AALKLobbyPlayerState* Player = Seat ? Seat->GetPlayer() : nullptr;
			if (Player && Player->GetUserId() == UserId)
			{
				Seat->SetTalking(bTalking);
			}
		}
	}
}

void UALKLobbyScreen::HandleChatReleased()
{
	// The chat had nothing to give the keyboard back to.
	if (ReadyButton)
	{
		ReadyButton->SetFocus();
	}
}

bool UALKLobbyScreen::NativeHandleMenuKey(const FKey& Key)
{
	// Letters belong to the chat while its message box has the keyboard.
	const bool bTyping = IsTypingChat();
	if (Key == EKeys::Gamepad_FaceButton_Left || (!bTyping && Key == EKeys::X))
	{
		SwitchTeam();
		return true;
	}
	if (Key == EKeys::Gamepad_FaceButton_Top || (!bTyping && Key == EKeys::Y))
	{
		OpenInvite();
		return true;
	}
	if (Key == EKeys::Gamepad_Special_Left)
	{
		// The keyboard's Tab is taken in Native On Preview Key Down: a focused button would use it to move the focus.
		OpenPlayerOptions();
		return true;
	}
	if (!bTyping && Key == EKeys::T)
	{
		// The message box opens when the key comes up, so its letter is not typed into it.
		bChatKeyDown = true;
		return true;
	}
	return Super::NativeHandleMenuKey(Key);
}

FReply UALKLobbyScreen::NativeOnKeyUp(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (bChatKeyDown && InKeyEvent.GetKey() == EKeys::T)
	{
		bChatKeyDown = false;
		FocusChat();
		return FReply::Handled();
	}
	return Super::NativeOnKeyUp(InGeometry, InKeyEvent);
}

FReply UALKLobbyScreen::NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	// Tab is the player options key. Slate would use it to move the focus to the next widget, so it is taken on its way
	// down to the focused widget. In the chat's message box it stays the chat's (next channel).
	if (InKeyEvent.GetKey() == EKeys::Tab && IsActivated() && !IsTypingChat())
	{
		if (!InKeyEvent.IsRepeat())
		{
			OpenPlayerOptions();
		}
		return FReply::Handled();
	}
	return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
}

bool UALKLobbyScreen::OnBackRequested_Implementation()
{
	// Back never just closes the lobby: it asks whether to leave the game.
	LeaveLobby();
	return true;
}

#undef LOCTEXT_NAMESPACE
