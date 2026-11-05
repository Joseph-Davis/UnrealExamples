// Copyright 2026 CodeLikeMe. All Rights Reserved.

#include "OnlineUI/ALKAdminScreen.h"
#include "OnlineUI/ALKOnlineFlowSubsystem.h"
#include "OnlineUI/ALKScoreboard.h"
#include "ALKDeveloperSettings.h"
#include "ALKMenuSubsystem.h"
#include "ALKTheme.h"
#include "Lobby/ALKLobbyGameState.h"
#include "Lobby/ALKLobbyPlayerState.h"
#include "Lobby/ALKLobbyStatics.h"
#include "Lobby/ALKPlayerNetComponent.h"
#include "Online/ALKOnlineConfig.h"
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
#include "Components/PanelWidget.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Engine/World.h"
#include "HAL/PlatformInput.h"
#include "Styling/SlateTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ALKAdminScreen)

#define LOCTEXT_NAMESPACE "AssetproLobbyKit"

namespace ALKAdminScreenPrivate
{
	/** The content never gets wider than this (ultrawide screens). */
	static constexpr float MaxContentWidth = 1500.f;
	static constexpr int32 TabCount = 3;
	static constexpr int32 PlayersTab = 0;
	static constexpr int32 BansTab = 1;
	static constexpr int32 MatchTab = 2;

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

	static bool IsInside(const UWidget* Child, const UWidget* Ancestor)
	{
		for (const UWidget* Current = Child; Current; Current = Current->GetParent())
		{
			if (Current == Ancestor)
			{
				return true;
			}
		}
		return false;
	}
}

UALKAdminScreen::UALKAdminScreen(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bDimScene = true;
}

void UALKAdminScreen::NativeBuildDefaultLayout(UWidgetTree& Tree)
{
	using namespace ALKAdminScreenPrivate;

	UOverlay* Root = FALKWidgetBuilder::Make<UOverlay>(Tree, TEXT("Root"));
	Tree.RootWidget = Root;
	BackgroundImage = FALKWidgetBuilder::AddBackgroundLayers(Tree, Root);

	USizeBox* MainSize = FALKWidgetBuilder::Make<USizeBox>(Tree, TEXT("MainSize"));
	MainSize->SetWidthOverride(MaxContentWidth);
	FALKWidgetBuilder::AddToOverlay(Root, MainSize, HAlign_Center, VAlign_Fill);
	UVerticalBox* Main = FALKWidgetBuilder::Make<UVerticalBox>(Tree, TEXT("MainColumn"));
	MainSize->SetContent(Main);

	UALKTitleText* Title = FALKWidgetBuilder::Make<UALKTitleText>(Tree, TEXT("TitleText"));
	Title->SetText(LOCTEXT("AdminTitle", "Host Tools"));
	TitleText = Title;
	FALKWidgetBuilder::AddToVerticalBox(Main, TitleText, FMargin(), HAlign_Left);

	// Tab row: [Q] Players Bans Match [E]
	UHorizontalBox* TabRow = FALKWidgetBuilder::Make<UHorizontalBox>(Tree, TEXT("TabRow"));
	FALKWidgetBuilder::AddToVerticalBox(Main, TabRow, FMargin(0.f, 22.f, 0.f, 0.f), HAlign_Left);
	UHorizontalBox* PrevCap = BuildInputHints(Tree, { FALKInputHint(LOCTEXT("KeyQ", "Q"), EKeys::Gamepad_LeftShoulder, FText::GetEmpty())
		.WithAction(FSimpleDelegate::CreateUObject(this, &ThisClass::ShowPreviousTab)) });
	FALKWidgetBuilder::AddToHorizontalBox(TabRow, PrevCap, FMargin(0.f, 0.f, 16.f, 0.f));
	TabBox = FALKWidgetBuilder::Make<UHorizontalBox>(Tree, TEXT("TabBox"));
	FALKWidgetBuilder::AddToHorizontalBox(TabRow, TabBox);
	UHorizontalBox* NextCap = BuildInputHints(Tree, { FALKInputHint(LOCTEXT("KeyE", "E"), EKeys::Gamepad_RightShoulder, FText::GetEmpty())
		.WithAction(FSimpleDelegate::CreateUObject(this, &ThisClass::ShowNextTab)) });
	FALKWidgetBuilder::AddToHorizontalBox(TabRow, NextCap, FMargin(16.f, 0.f, 0.f, 0.f));

	// The body panel holds the three pages; only the shown tab's page is visible.
	UALKFrame* Body = FALKWidgetBuilder::Make<UALKFrame>(Tree, TEXT("BodyPanel"));
	Body->SetPadding(FMargin(22.f, 20.f, 22.f, 22.f));
	BodyPanel = Body;
	FALKWidgetBuilder::AddToVerticalBox(Main, Body, FMargin(0.f, 22.f, 0.f, 0.f), HAlign_Fill, VAlign_Fill, 1.f);
	UOverlay* Pages = FALKWidgetBuilder::Make<UOverlay>(Tree, TEXT("Pages"));
	Body->SetContent(Pages);

	PlayersList = FALKWidgetBuilder::Make<UScrollBox>(Tree, TEXT("PlayersList"));
	PlayersList->SetAnimateWheelScrolling(true);
	FALKWidgetBuilder::AddToOverlay(Pages, PlayersList);
	BansList = FALKWidgetBuilder::Make<UScrollBox>(Tree, TEXT("BansList"));
	BansList->SetAnimateWheelScrolling(true);
	FALKWidgetBuilder::AddToOverlay(Pages, BansList);

	MatchBox = FALKWidgetBuilder::Make<UVerticalBox>(Tree, TEXT("MatchBox"));
	FALKWidgetBuilder::AddToOverlay(Pages, MatchBox, HAlign_Left, VAlign_Top);
	MatchStatusText = FALKWidgetBuilder::MakeText(Tree, TEXT("MatchStatusText"));
	FALKWidgetBuilder::AddToVerticalBox(MatchBox, MatchStatusText, FMargin(4.f, 0.f, 0.f, 14.f), HAlign_Left);
	auto AddMatchButton = [this](const FText& Text) -> UALKButton*
	{
		UALKButton* Button = CreateMenuButton(Text, EALKButtonStyle::Primary);
		FALKWidgetBuilder::AddToVerticalBox(MatchBox, Button, FMargin(0.f, 0.f, 0.f, 6.f), HAlign_Left);
		return Button;
	};
	ChangeSettingsButton = AddMatchButton(LOCTEXT("AdminChangeSettings", "Change settings"));
	MapButton = AddMatchButton(FText::GetEmpty());
	StartButton = AddMatchButton(LOCTEXT("AdminStart", "Start match"));
	ForceStartButton = AddMatchButton(LOCTEXT("AdminForceStart", "Force start"));
	CancelCountdownButton = AddMatchButton(LOCTEXT("AdminCancelCountdown", "Cancel countdown"));
	PauseButton = AddMatchButton(FText::GetEmpty());
	EndMatchButton = AddMatchButton(FText::GetEmpty());
	MatchHintText = FALKWidgetBuilder::MakeText(Tree, TEXT("MatchHintText"));
	FALKWidgetBuilder::AddToVerticalBox(MatchBox, MatchHintText, FMargin(4.f, 10.f, 0.f, 0.f), HAlign_Left);

	StatusText = FALKWidgetBuilder::MakeText(Tree, TEXT("StatusText"));
	StatusText->SetJustification(ETextJustify::Center);
	FALKWidgetBuilder::AddToOverlay(Pages, StatusText, HAlign_Center, VAlign_Center, FMargin(0.f, 60.f));

	// Footer: [Enter] Select  [Esc] Back
	UHorizontalBox* Hints = BuildInputHints(Tree, {
		FALKInputHint(LOCTEXT("KeyEnter", "Enter"), FPlatformInput::GetGamepadAcceptKey(), LOCTEXT("AdminHintSelect", "Select")),
		FALKInputHint(LOCTEXT("KeyEsc", "Esc"), FPlatformInput::GetGamepadBackKey(), LOCTEXT("HintBack", "Back"))
			.WithAction(FSimpleDelegate::CreateWeakLambda(this, [this]() { OnBackRequested(); }))
	});
	FALKWidgetBuilder::AddToVerticalBox(Main, Hints, FMargin(0.f, 24.f, 0.f, 0.f), HAlign_Left);
}

void UALKAdminScreen::NativeOnLayoutReady()
{
	Super::NativeOnLayoutReady();
	RebuildTabs();
	if (ChangeSettingsButton)
	{
		ChangeSettingsButton->OnClicked().AddUObject(this, &ThisClass::ChangeSettings);
	}
	if (MapButton)
	{
		MapButton->OnClicked().AddUObject(this, &ThisClass::CycleMap);
	}
	if (StartButton)
	{
		StartButton->OnClicked().AddUObject(this, &ThisClass::StartMatch, false);
	}
	if (ForceStartButton)
	{
		ForceStartButton->OnClicked().AddUObject(this, &ThisClass::StartMatch, true);
	}
	if (CancelCountdownButton)
	{
		CancelCountdownButton->OnClicked().AddUObject(this, &ThisClass::CancelCountdown);
	}
	if (PauseButton)
	{
		PauseButton->OnClicked().AddUObject(this, &ThisClass::TogglePause);
	}
	if (EndMatchButton)
	{
		EndMatchButton->OnClicked().AddUObject(this, &ThisClass::EndMatch);
	}
}

void UALKAdminScreen::RebuildTabs()
{
	if (!TabBox)
	{
		return;
	}
	TabBox->ClearChildren();
	TabButtons.Reset();
	const FText Names[ALKAdminScreenPrivate::TabCount] = { LOCTEXT("AdminTabPlayers", "Players"), LOCTEXT("AdminTabBans", "Bans"), LOCTEXT("AdminTabMatch", "Match") };
	for (int32 Index = 0; Index < ALKAdminScreenPrivate::TabCount; ++Index)
	{
		UALKButton* TabButton = CreateMenuButton(Names[Index], EALKButtonStyle::Tab);
		if (!TabButton)
		{
			continue;
		}
		// Tabs are switched with Q / E or the shoulder buttons, or clicked; they never take focus from the page.
		TabButton->SetIsFocusable(false);
		TabButton->bFocusOnHover = false;
		TabButton->bPlayClickSound = false;
		TabButton->OnClicked().AddUObject(this, &ThisClass::ShowTab, Index);
		TabBox->AddChild(TabButton);
		if (UHorizontalBoxSlot* TabSlot = Cast<UHorizontalBoxSlot>(TabButton->Slot))
		{
			TabSlot->SetPadding(FMargin(Index == 0 ? 0.f : 6.f, 0.f, 0.f, 0.f));
			TabSlot->SetVerticalAlignment(VAlign_Center);
		}
		TabButton->SetActiveVisual(Index == ActiveTab);
		TabButtons.Add(TabButton);
	}
}

void UALKAdminScreen::NativeApplyTheme(const UALKTheme& Theme)
{
	Super::NativeApplyTheme(Theme);
	using namespace ALKAdminScreenPrivate;

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
		FALKWidgetBuilder::SetTextFont(TitleText, Theme.HeadingFont);
	}
	if (UALKFrame* Body = Cast<UALKFrame>(BodyPanel))
	{
		Body->SetFrameStyle(Theme.MakePanelFrame());
	}
	if (MatchStatusText)
	{
		FALKWidgetBuilder::StyleText(MatchStatusText, Theme, EALKTextRole::Subheading, Theme.TextPrimary);
		FALKWidgetBuilder::SetTextFont(MatchStatusText, Scaled(Theme.SubheadingFont, 0.92f));
	}
	FALKWidgetBuilder::StyleText(MatchHintText, Theme, EALKTextRole::Caption, Theme.TextMuted);
	FALKWidgetBuilder::StyleText(StatusText, Theme, EALKTextRole::Body, Theme.TextMuted);
	StyleScrollBar(PlayersList, Theme);
	StyleScrollBar(BansList, Theme);

	// The lines are made again in the new look.
	PlayersSignature.Reset();
	BansSignature.Reset();
	RefreshAll();
}

void UALKAdminScreen::StyleScrollBar(UScrollBox* List, const UALKTheme& Theme) const
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
	List->SetScrollbarPadding(FMargin(12.f, 0.f, 0.f, 0.f));
}

void UALKAdminScreen::NativeOnActivated()
{
	if (UALKOnlineSubsystem* Online = UALKOnlineSubsystem::Get(this))
	{
		if (!ListsHandle.IsValid())
		{
			// The ban list is one of the online subsystem's lists.
			ListsHandle = Online->OnListsChangedNative.AddUObject(this, &ThisClass::MarkDirty);
		}
	}
	PlayersSignature.Reset();
	BansSignature.Reset();
	RefreshAll();
	// The lines were just made: focus goes to the first button once they are laid out.
	bRestoreFocus = true;
	Super::NativeOnActivated();
}

void UALKAdminScreen::NativeOnDeactivated()
{
	if (UALKOnlineSubsystem* Online = UALKOnlineSubsystem::Get(this))
	{
		Online->OnListsChangedNative.Remove(ListsHandle);
	}
	ListsHandle.Reset();
	UnbindGameState();
	Super::NativeOnDeactivated();
}

AALKLobbyGameState* UALKAdminScreen::ResolveGameState()
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

void UALKAdminScreen::UnbindGameState()
{
	if (AALKLobbyGameState* Old = BoundGameState.Get())
	{
		Old->OnLobbyStateChangedNative.RemoveAll(this);
		Old->OnPlayersChangedNative.RemoveAll(this);
	}
	BoundGameState.Reset();
}

void UALKAdminScreen::MarkDirty()
{
	bDirty = true;
}

UALKPlayerNetComponent* UALKAdminScreen::GetNetComponent() const
{
	const UALKOnlineSubsystem* Online = UALKOnlineSubsystem::Get(this);
	return Online ? Online->GetLocalNetComponent() : nullptr;
}

bool UALKAdminScreen::IsLocalHost() const
{
	const UALKOnlineSubsystem* Online = UALKOnlineSubsystem::Get(this);
	return Online && Online->IsInGame() && Online->IsHost() && GetNetComponent() != nullptr;
}

bool UALKAdminScreen::IsTopScreen() const
{
	UALKMenuSubsystem* Menu = GetMenuSubsystem();
	const UALKRootLayout* Layout = Menu ? Menu->GetRootLayout() : nullptr;
	return Layout && Layout->GetTopWidget() == this;
}

void UALKAdminScreen::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (!IsActivated())
	{
		return;
	}
	ResolveGameState();
	// Pings and the countdown change without an event.
	SecondTimer += InDeltaTime;
	if (SecondTimer >= 1.f)
	{
		SecondTimer = 0.f;
		bDirty = true;
	}
	if (bDirty)
	{
		RefreshAll();
	}

	// Lines that were made again took the focus with them: put it on the page's first button once that is laid out.
	if (bRestoreFocus)
	{
		UWidget* Target = GetDesiredFocusTarget();
		if (!Target || Target == this)
		{
			bRestoreFocus = false;
			if (IsTopScreen())
			{
				SetFocus();
			}
		}
		else if (Target->GetCachedGeometry().GetLocalSize().X > 1.0)
		{
			bRestoreFocus = false;
			if (IsTopScreen())
			{
				Target->SetFocus();
			}
		}
	}
}

// ---------------------------------------------------------------- Tabs

void UALKAdminScreen::ShowPreviousTab()
{
	ShowTab(ActiveTab - 1);
}

void UALKAdminScreen::ShowNextTab()
{
	ShowTab(ActiveTab + 1);
}

void UALKAdminScreen::ShowTab(int32 TabIndex)
{
	using namespace ALKAdminScreenPrivate;
	const int32 NewTab = (TabIndex % TabCount + TabCount) % TabCount;
	if (NewTab != ActiveTab)
	{
		ActiveTab = NewTab;
		PlayThemeSound(EALKUISound::Tab);
		bRestoreFocus = true;
	}
	for (int32 Index = 0; Index < TabButtons.Num(); ++Index)
	{
		if (TabButtons[Index])
		{
			TabButtons[Index]->SetActiveVisual(Index == ActiveTab);
		}
	}
	RefreshAll();
}

bool UALKAdminScreen::NativeHandleMenuKey(const FKey& Key)
{
	if (IsLocalHost())
	{
		if (Key == EKeys::Q || Key == EKeys::Gamepad_LeftShoulder)
		{
			ShowPreviousTab();
			return true;
		}
		if (Key == EKeys::E || Key == EKeys::Gamepad_RightShoulder)
		{
			ShowNextTab();
			return true;
		}
	}
	return Super::NativeHandleMenuKey(Key);
}

UWidget* UALKAdminScreen::FindFirstButton(UWidget* Within) const
{
	if (!Within || Within->GetVisibility() == ESlateVisibility::Collapsed)
	{
		return nullptr;
	}
	if (UALKButton* Button = Cast<UALKButton>(Within))
	{
		// A user widget's own default visibility is "not hit-testable itself", so only hidden buttons are left out.
		return Button->GetIsEnabled() && Button->GetVisibility() != ESlateVisibility::Hidden ? Button : nullptr;
	}
	if (const UPanelWidget* Container = Cast<UPanelWidget>(Within))
	{
		for (UWidget* Child : Container->GetAllChildren())
		{
			if (UWidget* Found = FindFirstButton(Child))
			{
				return Found;
			}
		}
	}
	return nullptr;
}

UWidget* UALKAdminScreen::NativeGetDesiredFocusTarget() const
{
	using namespace ALKAdminScreenPrivate;
	UWidget* Page = ActiveTab == PlayersTab ? static_cast<UWidget*>(PlayersList.Get()) : (ActiveTab == BansTab ? static_cast<UWidget*>(BansList.Get()) : static_cast<UWidget*>(MatchBox.Get()));
	// The button used last, while it is still on the page that is shown.
	UWidget* Remembered = GetLastFocusedWidget();
	if (Remembered && Page && IsInside(Remembered, Page) && Page->GetVisibility() != ESlateVisibility::Collapsed)
	{
		return Remembered;
	}
	if (UWidget* First = FindFirstButton(Page))
	{
		return First;
	}
	// Nothing to pick (not the host, an empty ban list): the screen itself keeps the keys.
	return const_cast<UALKAdminScreen*>(this);
}

// ---------------------------------------------------------------- Content

void UALKAdminScreen::RefreshAll()
{
	using namespace ALKAdminScreenPrivate;
	AALKLobbyGameState* GameState = ResolveGameState();
	bDirty = false;
	const UALKTheme* Theme = GetTheme();
	const bool bHost = IsLocalHost();

	if (UWidget* TabRow = TabBox ? TabBox->GetParent() : nullptr)
	{
		TabRow->SetVisibility(bHost ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
	SetShown(PlayersList, bHost && ActiveTab == PlayersTab, ESlateVisibility::Visible);
	SetShown(BansList, bHost && ActiveTab == BansTab, ESlateVisibility::Visible);
	SetShown(MatchBox, bHost && ActiveTab == MatchTab, ESlateVisibility::SelfHitTestInvisible);

	FText Status;
	if (!bHost)
	{
		Status = LOCTEXT("AdminNotHost", "Only the host of a game can use these tools.");
	}
	else if (Theme)
	{
		RefreshPlayers(GameState, *Theme);
		RefreshBans(*Theme);
		RefreshMatch(GameState);
		if (ActiveTab == BansTab && BansList && BansList->GetChildrenCount() == 0)
		{
			Status = LOCTEXT("AdminNoBans", "Nobody is banned.");
		}
		else if (ActiveTab == PlayersTab && PlayersList && PlayersList->GetChildrenCount() == 0)
		{
			Status = LOCTEXT("AdminNoPlayers", "Nobody is in the game yet.");
		}
	}
	if (StatusText)
	{
		StatusText->SetText(Status);
		SetShown(StatusText, !Status.IsEmpty());
	}
	// A button that was hidden or turned off while it had focus (Force start once the countdown runs) hands the focus on.
	if (IsActivated() && GetLastFocusedWidget() == nullptr)
	{
		bRestoreFocus = true;
	}
}

void UALKAdminScreen::RefreshPlayers(AALKLobbyGameState* GameState, const UALKTheme& Theme)
{
	using namespace ALKAdminScreenPrivate;
	if (!PlayersList || !WidgetTree)
	{
		return;
	}
	const UALKOnlineConfig* Config = UALKDeveloperSettings::GetOnlineConfig();
	const UALKOnlineSubsystem* Online = UALKOnlineSubsystem::Get(this);
	const TArray<AALKLobbyPlayerState*> Players = GameState ? GameState->GetLobbyPlayers() : TArray<AALKLobbyPlayerState*>();
	const int32 TeamCount = GameState ? GameState->TeamCount : 0;
	const bool bStreamer = Online && Online->IsStreamerMode();

	// The lines are made again only when who is there (or on which team) changed, so focus stays put meanwhile.
	FString Signature = FString::Printf(TEXT("%d|%d|"), TeamCount, bStreamer ? 1 : 0);
	for (const AALKLobbyPlayerState* Player : Players)
	{
		if (Player)
		{
			Signature += FString::Printf(TEXT("%d:%s:%d:%d:%d|"), Player->GetPlayerId(), *Player->GetPlayerName(), Player->TeamId, Player->bHost ? 1 : 0, Player->bSpectating ? 1 : 0);
		}
	}
	if (Signature != PlayersSignature)
	{
		PlayersSignature = Signature;
		if (PlayersList->HasFocusedDescendants())
		{
			bRestoreFocus = true;
		}
		PlayersList->ClearChildren();
		PlayerLines.Reset();

		for (AALKLobbyPlayerState* Player : Players)
		{
			if (!Player)
			{
				continue;
			}
			const bool bLocalPlayer = Player->IsLocalPlayerState();
			const FString ShownName = UALKScoreboard::GetShownPlayerName(Player);
			const int32 PlayerId = Player->GetPlayerId();
			const int32 TeamId = Player->TeamId;

			UALKFrame* Row = WidgetTree->ConstructWidget<UALKFrame>(UALKFrame::StaticClass());
			Row->SetPadding(FMargin(14.f, 8.f, 10.f, 8.f));
			Row->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
			FALKFrameStyle RowFrame = Theme.MakeCardFrame(0.f);
			RowFrame.Shadow = FLinearColor::Transparent;
			Row->SetFrameStyle(RowFrame);
			UHorizontalBox* Line = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
			Row->SetContent(Line);

			// The team's colour as a bar, the picture, the name over team and ping.
			UImage* TeamBar = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
			TeamBar->SetBrush(UALKTheme::MakeSolidBrush(TeamId >= 0 ? Config->GetTeamColor(TeamId) : FLinearColor::Transparent));
			USizeBox* BarSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
			BarSize->SetWidthOverride(4.f);
			BarSize->SetHeightOverride(30.f);
			BarSize->SetContent(TeamBar);
			FALKWidgetBuilder::AddToHorizontalBox(Line, BarSize, FMargin(0.f, 0.f, 12.f, 0.f), HAlign_Left, VAlign_Center);

			UALKAvatar* Picture = WidgetTree->ConstructWidget<UALKAvatar>(UALKAvatar::StaticClass());
			Picture->Diameter = 36.f;
			Picture->SetPlayer(ShownName, nullptr);
			Picture->SetAnonymous(bStreamer && !bLocalPlayer);
			Picture->SetBackColor(Theme.PanelRaised);
			FALKWidgetBuilder::AddToHorizontalBox(Line, Picture, FMargin(0.f, 0.f, 14.f, 0.f), HAlign_Left, VAlign_Center);

			UVerticalBox* Texts = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
			Texts->SetClipping(EWidgetClipping::ClipToBounds);
			FALKWidgetBuilder::AddToHorizontalBox(Line, Texts, FMargin(), HAlign_Fill, VAlign_Center, 1.f);
			UHorizontalBox* NameLine = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
			FALKWidgetBuilder::AddToVerticalBox(Texts, NameLine, FMargin(), HAlign_Left);
			UTextBlock* NameText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
			NameText->SetText(FText::FromString(ShownName));
			FALKWidgetBuilder::SetTextFont(NameText, Scaled(Theme.SubheadingFont, 0.86f));
			NameText->SetColorAndOpacity(FSlateColor(Theme.TextPrimary));
			FALKWidgetBuilder::AddToHorizontalBox(NameLine, NameText, FMargin(), HAlign_Left, VAlign_Center);
			if (Player->bHost)
			{
				UALKIconWidget* Crown = WidgetTree->ConstructWidget<UALKIconWidget>(UALKIconWidget::StaticClass());
				Crown->IconSize = 18.f;
				Crown->SetIcon(EALKIcon::Crown, Theme.Warning);
				FALKWidgetBuilder::AddToHorizontalBox(NameLine, Crown, FMargin(10.f, 0.f, 0.f, 0.f), HAlign_Left, VAlign_Center);
			}
			UTextBlock* DetailText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
			FALKWidgetBuilder::StyleText(DetailText, Theme, EALKTextRole::Caption, Theme.TextMuted);
			FALKWidgetBuilder::AddToVerticalBox(Texts, DetailText, FMargin(0.f, 2.f, 0.f, 0.f), HAlign_Left);

			if (bLocalPlayer)
			{
				// The host's own line has nothing to do to it.
				UALKFrame* Chip = FALKWidgetBuilder::MakeChip(*WidgetTree, NAME_None, LOCTEXT("AdminYou", "You"), EALKIcon::None);
				FALKWidgetBuilder::StyleChip(Chip, Theme, Theme.Accent.CopyWithNewOpacity(0.2f), FLinearColor::Transparent, Theme.GetAccentTextColor());
				FALKWidgetBuilder::AddToHorizontalBox(Line, Chip, FMargin(12.f, 0.f, 10.f, 0.f), HAlign_Right, VAlign_Center);
			}
			else
			{
				if (TeamCount > 1 && TeamId >= 0 && !Player->bSpectating)
				{
					const int32 NextTeam = (TeamId + 1) % TeamCount;
					UALKButton* MoveButton = CreateMenuButton(FText::Format(LOCTEXT("AdminMoveTo", "Move to {0}"), Config->GetTeamName(NextTeam)), EALKButtonStyle::Secondary);
					MoveButton->OnClicked().AddUObject(this, &ThisClass::MovePlayer, PlayerId, NextTeam);
					FALKWidgetBuilder::AddToHorizontalBox(Line, MoveButton, FMargin(10.f, 0.f, 0.f, 0.f), HAlign_Right, VAlign_Center);
				}
				UALKButton* KickButton = CreateMenuButton(LOCTEXT("AdminKick", "Kick"), EALKButtonStyle::Secondary);
				KickButton->OnClicked().AddUObject(this, &ThisClass::AskKick, PlayerId, ShownName, false);
				FALKWidgetBuilder::AddToHorizontalBox(Line, KickButton, FMargin(10.f, 0.f, 0.f, 0.f), HAlign_Right, VAlign_Center);
				UALKButton* BanButton = CreateMenuButton(LOCTEXT("AdminBan", "Ban"), EALKButtonStyle::Danger);
				BanButton->OnClicked().AddUObject(this, &ThisClass::AskKick, PlayerId, ShownName, true);
				FALKWidgetBuilder::AddToHorizontalBox(Line, BanButton, FMargin(10.f, 0.f, 0.f, 0.f), HAlign_Right, VAlign_Center);
			}

			PlayersList->AddChild(Row);
			if (UScrollBoxSlot* RowSlot = Cast<UScrollBoxSlot>(Row->Slot))
			{
				RowSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));
				RowSlot->SetHorizontalAlignment(HAlign_Fill);
			}
			FPlayerLine Entry;
			Entry.Player = Player;
			Entry.DetailText = DetailText;
			PlayerLines.Add(Entry);
		}
	}

	// Team and ping, read again every time.
	const bool bPingNumbers = !Online || Online->IsShowPing();
	for (const FPlayerLine& Entry : PlayerLines)
	{
		const AALKLobbyPlayerState* Player = Entry.Player.Get();
		UTextBlock* DetailText = Entry.DetailText.Get();
		if (!Player || !DetailText)
		{
			continue;
		}
		TArray<FText> Parts;
		if (Player->bSpectating)
		{
			Parts.Add(LOCTEXT("AdminSpectating", "Spectating"));
		}
		else if (Player->TeamId >= 0)
		{
			Parts.Add(Config->GetTeamName(Player->TeamId));
		}
		if (bPingNumbers)
		{
			Parts.Add(FText::Format(LOCTEXT("AdminPing", "{0} ms"), FText::AsNumber(FMath::RoundToInt(Player->GetPingInMilliseconds()))));
		}
		DetailText->SetText(FText::Join(LOCTEXT("AdminSeparator", "  \u00b7  "), Parts));
	}
}

void UALKAdminScreen::RefreshBans(const UALKTheme& Theme)
{
	using namespace ALKAdminScreenPrivate;
	const UALKOnlineSubsystem* Online = UALKOnlineSubsystem::Get(this);
	if (!BansList || !WidgetTree || !Online)
	{
		return;
	}
	const TArray<FALKBanEntry> Bans = Online->GetBanList();
	const bool bStreamer = Online->IsStreamerMode();
	FString Signature = bStreamer ? TEXT("s|") : TEXT("n|");
	for (const FALKBanEntry& Ban : Bans)
	{
		Signature += Ban.UserId + TEXT("|");
	}
	if (Signature == BansSignature)
	{
		return;
	}
	BansSignature = Signature;
	if (BansList->HasFocusedDescendants())
	{
		bRestoreFocus = true;
	}
	BansList->ClearChildren();

	for (const FALKBanEntry& Ban : Bans)
	{
		UALKFrame* Row = WidgetTree->ConstructWidget<UALKFrame>(UALKFrame::StaticClass());
		Row->SetPadding(FMargin(18.f, 8.f, 10.f, 8.f));
		Row->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		FALKFrameStyle RowFrame = Theme.MakeCardFrame(0.f);
		RowFrame.Shadow = FLinearColor::Transparent;
		Row->SetFrameStyle(RowFrame);
		UHorizontalBox* Line = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		Row->SetContent(Line);

		UVerticalBox* Texts = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Texts->SetClipping(EWidgetClipping::ClipToBounds);
		FALKWidgetBuilder::AddToHorizontalBox(Line, Texts, FMargin(), HAlign_Fill, VAlign_Center, 1.f);
		// Streamer mode hides who was banned and from where.
		const FString ShownName = Online->GetDisplayName(Ban.DisplayName.IsEmpty() ? Ban.UserId : Ban.DisplayName, INDEX_NONE);
		UTextBlock* NameText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		NameText->SetText(FText::FromString(ShownName));
		FALKWidgetBuilder::SetTextFont(NameText, Scaled(Theme.SubheadingFont, 0.86f));
		NameText->SetColorAndOpacity(FSlateColor(Theme.TextPrimary));
		FALKWidgetBuilder::AddToVerticalBox(Texts, NameText, FMargin(), HAlign_Left);
		TArray<FText> Parts;
		Parts.Add(FText::Format(LOCTEXT("AdminBannedOn", "Banned {0}"), FText::AsDate(Ban.BannedAt, EDateTimeStyle::Medium)));
		if (!bStreamer && !Ban.Address.IsEmpty())
		{
			Parts.Add(FText::FromString(Ban.Address));
		}
		UTextBlock* DetailText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		DetailText->SetText(FText::Join(LOCTEXT("AdminSeparator", "  \u00b7  "), Parts));
		FALKWidgetBuilder::StyleText(DetailText, Theme, EALKTextRole::Caption, Theme.TextMuted);
		FALKWidgetBuilder::AddToVerticalBox(Texts, DetailText, FMargin(0.f, 2.f, 0.f, 0.f), HAlign_Left);

		UALKButton* UnbanButton = CreateMenuButton(LOCTEXT("AdminUnban", "Unban"), EALKButtonStyle::Secondary);
		UnbanButton->OnClicked().AddUObject(this, &ThisClass::Unban, Ban.UserId);
		FALKWidgetBuilder::AddToHorizontalBox(Line, UnbanButton, FMargin(12.f, 0.f, 0.f, 0.f), HAlign_Right, VAlign_Center);

		BansList->AddChild(Row);
		if (UScrollBoxSlot* RowSlot = Cast<UScrollBoxSlot>(Row->Slot))
		{
			RowSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));
			RowSlot->SetHorizontalAlignment(HAlign_Fill);
		}
	}
}

void UALKAdminScreen::RefreshMatch(AALKLobbyGameState* GameState)
{
	const UALKOnlineConfig* Config = UALKDeveloperSettings::GetOnlineConfig();
	const EALKLobbyPhase Phase = GameState ? GameState->Phase : EALKLobbyPhase::Waiting;
	const bool bLobby = GameState && GameState->IsInLobby();
	const bool bCountdown = GameState && Phase == EALKLobbyPhase::Countdown;
	const bool bPlaying = GameState && Phase == EALKLobbyPhase::InMatch;
	const bool bOver = GameState && Phase == EALKLobbyPhase::PostMatch;

	auto SetButton = [](UALKButton* Button, bool bShown, bool bEnabled)
	{
		if (Button)
		{
			Button->SetVisibility(bShown ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
			Button->SetIsEnabled(bEnabled);
		}
	};

	FText Status;
	FText Hint;
	if (!GameState)
	{
		Status = LOCTEXT("AdminNoGame", "Not in a game");
	}
	else if (bCountdown)
	{
		Status = FText::Format(LOCTEXT("AdminCountdown", "Starting in {0}"), FText::AsNumber(FMath::CeilToInt(GameState->GetCountdownRemaining())));
	}
	else if (bLobby)
	{
		Status = FText::Format(LOCTEXT("AdminLobby", "In the lobby  \u00b7  {0} of {1} ready"), FText::AsNumber(GameState->GetReadyCount()), FText::AsNumber(GameState->GetActivePlayerCount()));
	}
	else if (bPlaying)
	{
		Status = GameState->bServerPaused ? LOCTEXT("AdminPaused", "The match is paused") : LOCTEXT("AdminPlaying", "The match is running");
	}
	else
	{
		Status = LOCTEXT("AdminOver", "The match is over");
	}

	// ---- Lobby
	const TArray<FALKMapDefinition> Maps = GameState ? Config->GetMapsForMode(GameState->ModeId) : TArray<FALKMapDefinition>();
	SetButton(ChangeSettingsButton, bLobby, !bCountdown && Phase != EALKLobbyPhase::Starting);
	SetButton(MapButton, bLobby, Maps.Num() > 1 && !bCountdown && Phase != EALKLobbyPhase::Starting);
	if (MapButton && GameState)
	{
		MapButton->SetButtonText(GameState->MapId.IsNone() && Maps.Num() == 0
			? LOCTEXT("AdminNoMaps", "Map: none listed in the Online Config")
			: FText::Format(LOCTEXT("AdminMap", "Map: {0}"), Config->GetMapName(GameState->MapId)));
	}
	const EALKStartBlocker Blocker = GameState ? GameState->GetStartBlocker(false) : EALKStartBlocker::NotEnoughPlayers;
	const EALKStartBlocker ForceBlocker = GameState ? GameState->GetStartBlocker(true) : EALKStartBlocker::NotEnoughPlayers;
	const bool bWaiting = bLobby && Phase == EALKLobbyPhase::Waiting;
	SetButton(StartButton, bWaiting, Blocker == EALKStartBlocker::None);
	SetButton(ForceStartButton, bWaiting && Config->bAllowForceStart, ForceBlocker == EALKStartBlocker::None);
	SetButton(CancelCountdownButton, bCountdown, true);
	if (bWaiting && Blocker != EALKStartBlocker::None)
	{
		Hint = UALKLobbyStatics::GetStartBlockerText(Blocker);
	}

	// ---- Match
	SetButton(PauseButton, bPlaying, true);
	if (PauseButton && GameState)
	{
		PauseButton->SetButtonText(GameState->bServerPaused ? LOCTEXT("AdminResume", "Resume match") : LOCTEXT("AdminPause", "Pause match"));
	}
	SetButton(EndMatchButton, bPlaying || bOver, true);
	if (EndMatchButton)
	{
		EndMatchButton->SetButtonText(bOver ? LOCTEXT("AdminReturn", "Return to lobby now") : LOCTEXT("AdminEndMatch", "End match and return to lobby"));
	}

	if (MatchStatusText)
	{
		MatchStatusText->SetText(Status);
	}
	if (MatchHintText)
	{
		MatchHintText->SetText(Hint);
		MatchHintText->SetVisibility(Hint.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
}

// ---------------------------------------------------------------- Actions

void UALKAdminScreen::AskKick(int32 PlayerId, FString ShownName, bool bBan)
{
	UALKMenuSubsystem* Menu = GetMenuSubsystem();
	if (!Menu || !IsLocalHost())
	{
		return;
	}
	const FText Name = FText::FromString(ShownName);
	FALKDialogDescriptor Descriptor;
	Descriptor.Title = FText::Format(bBan ? LOCTEXT("AdminBanTitle", "Ban {0}?") : LOCTEXT("AdminKickTitle", "Kick {0}?"), Name);
	Descriptor.Message = bBan
		? LOCTEXT("AdminBanMessage", "They are removed from the game and cannot join your games again until you unban them.")
		: LOCTEXT("AdminKickMessage", "They are removed from the game. They can join again.");
	Descriptor.ConfirmText = bBan ? LOCTEXT("AdminBan", "Ban") : LOCTEXT("AdminKick", "Kick");
	Descriptor.CancelText = LOCTEXT("Cancel", "Cancel");
	Descriptor.bDestructive = true;

	TWeakObjectPtr<UALKAdminScreen> WeakThis(this);
	Menu->ShowConfirmDialog(Descriptor, [WeakThis, PlayerId, bBan](EALKDialogResult Result)
	{
		const UALKAdminScreen* Self = WeakThis.Get();
		UALKPlayerNetComponent* NetComponent = Self ? Self->GetNetComponent() : nullptr;
		if (Result == EALKDialogResult::Confirmed && NetComponent)
		{
			if (bBan)
			{
				NetComponent->ServerBanPlayer(PlayerId);
			}
			else
			{
				NetComponent->ServerKickPlayer(PlayerId);
			}
		}
	});
}

void UALKAdminScreen::MovePlayer(int32 PlayerId, int32 TeamId)
{
	if (UALKPlayerNetComponent* NetComponent = IsLocalHost() ? GetNetComponent() : nullptr)
	{
		NetComponent->ServerMovePlayerToTeam(PlayerId, TeamId);
	}
}

void UALKAdminScreen::Unban(FString BannedUserId)
{
	if (UALKPlayerNetComponent* NetComponent = IsLocalHost() ? GetNetComponent() : nullptr)
	{
		NetComponent->ServerUnbanPlayer(BannedUserId);
	}
}

void UALKAdminScreen::ChangeSettings()
{
	if (UALKOnlineFlowSubsystem* Flow = UALKOnlineFlowSubsystem::Get(this))
	{
		Flow->OpenHostGame(true);
	}
}

void UALKAdminScreen::CycleMap()
{
	const AALKLobbyGameState* GameState = BoundGameState.Get();
	UALKPlayerNetComponent* NetComponent = IsLocalHost() ? GetNetComponent() : nullptr;
	if (!GameState || !NetComponent)
	{
		return;
	}
	// The next map of the mode after the one that is set.
	const TArray<FALKMapDefinition> Maps = UALKDeveloperSettings::GetOnlineConfig()->GetMapsForMode(GameState->ModeId);
	if (Maps.Num() == 0)
	{
		return;
	}
	const FName CurrentMap = GameState->MapId;
	const int32 Current = Maps.IndexOfByPredicate([CurrentMap](const FALKMapDefinition& Each) { return Each.Id == CurrentMap; });
	NetComponent->ServerChangeMap(Maps[(Current + 1) % Maps.Num()].Id);
	PlayThemeSound(EALKUISound::Change);
}

void UALKAdminScreen::StartMatch(bool bForce)
{
	if (UALKPlayerNetComponent* NetComponent = IsLocalHost() ? GetNetComponent() : nullptr)
	{
		NetComponent->ServerStartMatch(bForce);
	}
}

void UALKAdminScreen::CancelCountdown()
{
	if (UALKPlayerNetComponent* NetComponent = IsLocalHost() ? GetNetComponent() : nullptr)
	{
		NetComponent->ServerCancelCountdown();
	}
}

void UALKAdminScreen::TogglePause()
{
	const AALKLobbyGameState* GameState = BoundGameState.Get();
	UALKPlayerNetComponent* NetComponent = IsLocalHost() ? GetNetComponent() : nullptr;
	if (GameState && NetComponent)
	{
		NetComponent->ServerSetPaused(!GameState->bServerPaused);
	}
}

void UALKAdminScreen::EndMatch()
{
	UALKMenuSubsystem* Menu = GetMenuSubsystem();
	const AALKLobbyGameState* GameState = BoundGameState.Get();
	if (!Menu || !GameState || !IsLocalHost())
	{
		return;
	}
	// After the match there is nothing to lose: go back at once. A running match asks first.
	if (GameState->Phase == EALKLobbyPhase::PostMatch)
	{
		if (UALKPlayerNetComponent* NetComponent = GetNetComponent())
		{
			NetComponent->ServerReturnToLobby();
		}
		return;
	}
	FALKDialogDescriptor Descriptor;
	Descriptor.Title = LOCTEXT("AdminEndTitle", "End the Match?");
	Descriptor.Message = LOCTEXT("AdminEndMessage", "The match ends for everyone and the game returns to the lobby.");
	Descriptor.ConfirmText = LOCTEXT("AdminEndConfirm", "End match");
	Descriptor.CancelText = LOCTEXT("Cancel", "Cancel");
	Descriptor.bDestructive = true;

	TWeakObjectPtr<UALKAdminScreen> WeakThis(this);
	Menu->ShowConfirmDialog(Descriptor, [WeakThis](EALKDialogResult Result)
	{
		const UALKAdminScreen* Self = WeakThis.Get();
		UALKPlayerNetComponent* NetComponent = Self ? Self->GetNetComponent() : nullptr;
		if (Result == EALKDialogResult::Confirmed && NetComponent)
		{
			NetComponent->ServerReturnToLobby();
		}
	});
}

#undef LOCTEXT_NAMESPACE
