// Copyright 2026 CodeLikeMe. All Rights Reserved.

#include "OnlineUI/ALKPlayHubScreen.h"
#include "ALKDeveloperSettings.h"
#include "ALKMenuConfig.h"
#include "ALKTheme.h"
#include "Online/ALKOnlineConfig.h"
#include "Online/ALKOnlineSubsystem.h"
#include "OnlineUI/ALKOnlineFlowSubsystem.h"
#include "OnlineUI/ALKPartyPanel.h"
#include "OnlineUI/ALKQueueWidget.h"
#include "Widgets/ALKButton.h"
#include "Widgets/ALKCardButton.h"
#include "Widgets/ALKDecor.h"
#include "Widgets/ALKFrame.h"
#include "Widgets/ALKInputLabels.h"
#include "Widgets/ALKTitleText.h"
#include "Widgets/ALKWidgetBuilder.h"
#include "Blueprint/WidgetNavigation.h"
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
#include "Engine/Texture2D.h"
#include "HAL/PlatformInput.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ALKPlayHubScreen)

#define LOCTEXT_NAMESPACE "AssetproLobbyKit"

namespace ALKPlayHubScreenPrivate
{
	/** The hub is one column of this width at most, centred: ultrawide screens do not stretch the tiles. */
	static constexpr float ColumnWidth = 1760.f;
	/** As tall as the queue bar, so the rows under it stay put when a search starts. */
	static constexpr float HeaderHeight = 70.f;
	/** Room kept round each playlist card inside the scrolling row, for its focus ring and shadow. */
	static constexpr float CardPadX = 11.f;
	static constexpr float CardPadTop = 8.f;
	static constexpr float CardPadBottom = 20.f;
	static constexpr float MinCardWidth = 280.f;
}

UALKPlayHubScreen::UALKPlayHubScreen(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// A screen full of cards: the backdrop steps back behind them (the theme's Page Blur and Page Dim).
	bDimScene = true;
}

bool UALKPlayHubScreen::IsOnlineEnabled() const
{
	const UALKOnlineSubsystem* Online = UALKOnlineSubsystem::Get(this);
	return Online && Online->IsOnlineEnabled();
}

void UALKPlayHubScreen::NativeBuildDefaultLayout(UWidgetTree& Tree)
{
	using namespace ALKPlayHubScreenPrivate;

	UOverlay* Root = FALKWidgetBuilder::Make<UOverlay>(Tree, TEXT("Root"));
	Tree.RootWidget = Root;
	BackgroundImage = FALKWidgetBuilder::AddBackgroundLayers(Tree, Root);

	// One column, as wide as the screen allows up to its cap.
	USizeBox* ColumnSize = FALKWidgetBuilder::Make<USizeBox>(Tree, TEXT("MainColumn"));
	ColumnSize->SetWidthOverride(ColumnWidth);
	FALKWidgetBuilder::AddToOverlay(Root, ColumnSize, HAlign_Center, VAlign_Fill);
	UVerticalBox* Main = FALKWidgetBuilder::Make<UVerticalBox>(Tree, TEXT("MainBox"));
	ColumnSize->AddChild(Main);

	// Header: the title at the left, the quick match queue in the middle.
	UOverlay* Header = FALKWidgetBuilder::Make<UOverlay>(Tree, TEXT("Header"));
	FALKWidgetBuilder::AddToVerticalBox(Main, FALKWidgetBuilder::WrapInSizeBox(Tree, Header, 0.f, HeaderHeight, true));
	UVerticalBox* TitleBox = FALKWidgetBuilder::Make<UVerticalBox>(Tree, TEXT("TitleBox"));
	FALKWidgetBuilder::AddToOverlay(Header, TitleBox, HAlign_Left, VAlign_Center);
	SystemLabelText = FALKWidgetBuilder::MakeText(Tree, TEXT("SystemLabelText"), MakeSystemLabel(20, LOCTEXT("HubTitle", "Play")));
	FALKWidgetBuilder::AddToVerticalBox(TitleBox, SystemLabelText, FMargin(0.f, 0.f, 0.f, 4.f), HAlign_Left);
	UALKTitleText* Title = FALKWidgetBuilder::Make<UALKTitleText>(Tree, TEXT("TitleText"));
	Title->SetText(LOCTEXT("HubTitle", "Play"));
	TitleText = Title;
	FALKWidgetBuilder::AddToVerticalBox(TitleBox, TitleText, FMargin(), HAlign_Left);
	QueueWidget = Tree.ConstructWidget<UALKQueueWidget>(UALKQueueWidget::StaticClass(), TEXT("QueueWidget"));
	FALKWidgetBuilder::AddToOverlay(Header, QueueWidget, HAlign_Center, VAlign_Center);

	// The rejoin banner, only while a match can be rejoined.
	UALKFrame* Banner = FALKWidgetBuilder::Make<UALKFrame>(Tree, TEXT("RejoinBanner"));
	Banner->SetPadding(FMargin(22.f, 8.f, 10.f, 8.f));
	Banner->SetVerticalAlignment(VAlign_Center);
	Banner->SetVisibility(ESlateVisibility::Collapsed);
	RejoinBanner = Banner;
	FALKWidgetBuilder::AddToVerticalBox(Main, Banner, FMargin(0.f, 16.f, 0.f, 0.f));
	UHorizontalBox* BannerRow = FALKWidgetBuilder::Make<UHorizontalBox>(Tree, TEXT("RejoinRow"));
	Banner->SetContent(BannerRow);
	RejoinIcon = FALKWidgetBuilder::MakeIcon(Tree, TEXT("RejoinIcon"), EALKIcon::Refresh, 22.f);
	FALKWidgetBuilder::AddToHorizontalBox(BannerRow, RejoinIcon, FMargin(0.f, 0.f, 14.f, 0.f), HAlign_Left, VAlign_Center);
	RejoinText = FALKWidgetBuilder::MakeText(Tree, TEXT("RejoinText"));
	RejoinText->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
	FALKWidgetBuilder::AddToHorizontalBox(BannerRow, RejoinText, FMargin(0.f, 0.f, 16.f, 0.f), HAlign_Fill, VAlign_Center, 1.f);
	RejoinButton = CreateMenuButton(LOCTEXT("HubRejoin", "Rejoin"), EALKButtonStyle::Solid);
	FALKWidgetBuilder::AddToHorizontalBox(BannerRow, RejoinButton, FMargin(), HAlign_Right, VAlign_Center);
	DismissButton = CreateMenuButton(LOCTEXT("HubDismiss", "Dismiss"), EALKButtonStyle::Ghost);
	FALKWidgetBuilder::AddToHorizontalBox(BannerRow, DismissButton, FMargin(8.f, 0.f, 0.f, 0.f), HAlign_Right, VAlign_Center);

	OfflineText = FALKWidgetBuilder::MakeText(Tree, TEXT("OfflineText"), LOCTEXT("HubOffline", "Online play is turned off: there is nothing to join or host right now."));
	OfflineText->SetVisibility(ESlateVisibility::Collapsed);
	FALKWidgetBuilder::AddToVerticalBox(Main, OfflineText, FMargin(0.f, 12.f, 0.f, 0.f), HAlign_Left);

	// First row: the featured tile | four tiles | the party.
	auto MakeTile = [&Tree](FName Name)
	{
		return Tree.ConstructWidget<UALKCardButton>(UALKCardButton::StaticClass(), Name);
	};
	UHorizontalBox* TopRow = FALKWidgetBuilder::Make<UHorizontalBox>(Tree, TEXT("TopRow"));
	FALKWidgetBuilder::AddToVerticalBox(Main, TopRow, FMargin(0.f, 22.f, 0.f, 0.f), HAlign_Fill, VAlign_Fill, 0.58f);

	FeaturedTile = MakeTile(TEXT("FeaturedTile"));
	FeaturedTile->bLarge = true;
	FALKWidgetBuilder::AddToHorizontalBox(TopRow, FeaturedTile, FMargin(), HAlign_Fill, VAlign_Fill, 0.335f);

	UVerticalBox* TileGrid = FALKWidgetBuilder::Make<UVerticalBox>(Tree, TEXT("TileGrid"));
	FALKWidgetBuilder::AddToHorizontalBox(TopRow, TileGrid, FMargin(24.f, 0.f, 28.f, 0.f), HAlign_Fill, VAlign_Fill, 0.345f);
	UHorizontalBox* GridTop = FALKWidgetBuilder::Make<UHorizontalBox>(Tree, TEXT("TileGridTop"));
	FALKWidgetBuilder::AddToVerticalBox(TileGrid, GridTop, FMargin(0.f, 0.f, 0.f, 12.f), HAlign_Fill, VAlign_Fill, 1.f);
	UHorizontalBox* GridBottom = FALKWidgetBuilder::Make<UHorizontalBox>(Tree, TEXT("TileGridBottom"));
	FALKWidgetBuilder::AddToVerticalBox(TileGrid, GridBottom, FMargin(0.f, 12.f, 0.f, 0.f), HAlign_Fill, VAlign_Fill, 1.f);

	HostTile = MakeTile(TEXT("HostTile"));
	HostTile->SetCard(LOCTEXT("HubHost", "Host Game"), LOCTEXT("HubHostSub", "Open a lobby, set the rules"), EALKIcon::Server);
	FALKWidgetBuilder::AddToHorizontalBox(GridTop, HostTile, FMargin(0.f, 0.f, 12.f, 0.f), HAlign_Fill, VAlign_Fill, 1.f);
	BrowseTile = MakeTile(TEXT("BrowseTile"));
	BrowseTile->SetCard(LOCTEXT("HubBrowse", "Browse Games"), LOCTEXT("HubBrowseSub", "Find a game to join"), EALKIcon::List);
	FALKWidgetBuilder::AddToHorizontalBox(GridTop, BrowseTile, FMargin(12.f, 0.f, 0.f, 0.f), HAlign_Fill, VAlign_Fill, 1.f);
	CodeTile = MakeTile(TEXT("CodeTile"));
	CodeTile->SetCard(LOCTEXT("HubCode", "Join by Code"), LOCTEXT("HubCodeSub", "Lobby code or address"), EALKIcon::Hash);
	FALKWidgetBuilder::AddToHorizontalBox(GridBottom, CodeTile, FMargin(0.f, 0.f, 12.f, 0.f), HAlign_Fill, VAlign_Fill, 1.f);
	PartyTile = MakeTile(TEXT("PartyTile"));
	PartyTile->SetCard(LOCTEXT("HubParty", "Party"), FText::GetEmpty(), EALKIcon::Users);
	FALKWidgetBuilder::AddToHorizontalBox(GridBottom, PartyTile, FMargin(12.f, 0.f, 0.f, 0.f), HAlign_Fill, VAlign_Fill, 1.f);

	PartyPanel = Tree.ConstructWidget<UALKPartyPanel>(UALKPartyPanel::StaticClass(), TEXT("PartyPanel"));
	FALKWidgetBuilder::AddToHorizontalBox(TopRow, PartyPanel, FMargin(), HAlign_Fill, VAlign_Fill, 0.285f);

	// Second row: the playlists. The row reaches past the column by the room each card keeps for its ring and shadow.
	PlaylistLabel = FALKWidgetBuilder::MakeText(Tree, TEXT("PlaylistLabel"), LOCTEXT("HubPlaylists", "Playlists"));
	FALKWidgetBuilder::AddToVerticalBox(Main, PlaylistLabel, FMargin(0.f, 24.f, 0.f, 12.f), HAlign_Left);
	PlaylistList = FALKWidgetBuilder::Make<UScrollBox>(Tree, TEXT("PlaylistList"));
	PlaylistList->SetOrientation(Orient_Horizontal);
	PlaylistList->SetScrollBarVisibility(ESlateVisibility::Collapsed);
	PlaylistList->SetAnimateWheelScrolling(true);
	FALKWidgetBuilder::AddToVerticalBox(Main, PlaylistList, FMargin(-CardPadX, -CardPadTop, -CardPadX, -CardPadBottom), HAlign_Fill, VAlign_Fill, 0.42f);

	// Footer: [Enter] Select  [Y] Party  [X] Cancel search (while searching)  [Esc] Back
	UHorizontalBox* Footer = FALKWidgetBuilder::Make<UHorizontalBox>(Tree, TEXT("Footer"));
	FALKWidgetBuilder::AddToVerticalBox(Main, Footer, FMargin(0.f, 18.f, 0.f, 0.f), HAlign_Left);
	UHorizontalBox* MainHints = BuildInputHints(Tree, {
		FALKInputHint(LOCTEXT("KeyEnter", "Enter"), FPlatformInput::GetGamepadAcceptKey(), LOCTEXT("HintSelect", "Select")),
		FALKInputHint(LOCTEXT("KeyY", "Y"), EKeys::Gamepad_FaceButton_Top, LOCTEXT("HubHintParty", "Party"))
			.WithAction(FSimpleDelegate::CreateUObject(this, &ThisClass::FocusParty))
	});
	FALKWidgetBuilder::AddToHorizontalBox(Footer, MainHints);
	CancelHints = BuildInputHints(Tree, {
		FALKInputHint(LOCTEXT("KeyX", "X"), EKeys::Gamepad_FaceButton_Left, LOCTEXT("HubHintCancel", "Cancel search"))
			.WithAction(FSimpleDelegate::CreateUObject(this, &ThisClass::CancelSearch))
	});
	CancelHints->SetVisibility(ESlateVisibility::Collapsed);
	FALKWidgetBuilder::AddToHorizontalBox(Footer, CancelHints, FMargin(20.f, 0.f, 0.f, 0.f));
	UHorizontalBox* BackHints = BuildInputHints(Tree, {
		FALKInputHint(LOCTEXT("KeyEsc", "Esc"), FPlatformInput::GetGamepadBackKey(), LOCTEXT("HintBack", "Back"))
			.WithAction(FSimpleDelegate::CreateWeakLambda(this, [this]() { OnBackRequested(); }))
	});
	FALKWidgetBuilder::AddToHorizontalBox(Footer, BackHints, FMargin(20.f, 0.f, 0.f, 0.f));
}

void UALKPlayHubScreen::NativeOnLayoutReady()
{
	Super::NativeOnLayoutReady();

	if (FeaturedTile)
	{
		FeaturedTile->OnClicked().AddUObject(this, &ThisClass::ToggleFeaturedSearch);
	}
	if (HostTile)
	{
		HostTile->OnClicked().AddWeakLambda(this, [this]()
		{
			if (UALKOnlineFlowSubsystem* Flow = UALKOnlineFlowSubsystem::Get(this))
			{
				Flow->OpenHostGame(false);
			}
		});
	}
	if (BrowseTile)
	{
		BrowseTile->OnClicked().AddWeakLambda(this, [this]()
		{
			if (UALKOnlineFlowSubsystem* Flow = UALKOnlineFlowSubsystem::Get(this))
			{
				Flow->OpenServerBrowser();
			}
		});
	}
	if (CodeTile)
	{
		CodeTile->OnClicked().AddWeakLambda(this, [this]()
		{
			if (UALKOnlineFlowSubsystem* Flow = UALKOnlineFlowSubsystem::Get(this))
			{
				Flow->OpenJoinByCode();
			}
		});
	}
	if (PartyTile)
	{
		PartyTile->OnClicked().AddWeakLambda(this, [this]()
		{
			if (UALKOnlineFlowSubsystem* Flow = UALKOnlineFlowSubsystem::Get(this))
			{
				Flow->OpenSocial();
			}
		});
	}
	if (RejoinButton)
	{
		RejoinButton->OnClicked().AddUObject(this, &ThisClass::HandleRejoinClicked);
	}
	if (DismissButton)
	{
		DismissButton->OnClicked().AddUObject(this, &ThisClass::HandleDismissClicked);
	}

	// Right from the tall featured tile goes to the upper of the two tiles beside it.
	if (FeaturedTile && HostTile)
	{
		FeaturedTile->SetNavigationRuleExplicit(EUINavigation::Right, HostTile);
	}
	if (UWidget* TopRow = WidgetTree ? WidgetTree->FindWidget(TEXT("TopRow")) : nullptr)
	{
		FCustomWidgetNavigationDelegate ToBanner;
		ToBanner.BindDynamic(this, &ThisClass::HandleNavigateToBanner);
		TopRow->SetNavigationRuleCustomBoundary(EUINavigation::Up, ToBanner);
	}
}

UWidget* UALKPlayHubScreen::HandleNavigateToBanner(EUINavigation Direction)
{
	return bRejoinShown ? RejoinButton.Get() : nullptr;
}

void UALKPlayHubScreen::NativeApplyTheme(const UALKTheme& Theme)
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
	FALKWidgetBuilder::StyleText(SystemLabelText, Theme, EALKTextRole::Data, Theme.GetAccentTextColor());
	if (SystemLabelText)
	{
		SystemLabelText->SetTextTransformPolicy(ETextTransformPolicy::ToUpper);
		SystemLabelText->SetVisibility(SystemLabelText->GetText().IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
	FALKWidgetBuilder::StyleText(PlaylistLabel, Theme, EALKTextRole::Subheading, Theme.TextPrimary);
	if (PlaylistLabel)
	{
		PlaylistLabel->SetTextTransformPolicy(Theme.bUppercaseTitles ? ETextTransformPolicy::ToUpper : ETextTransformPolicy::None);
	}
	FALKWidgetBuilder::StyleText(OfflineText, Theme, EALKTextRole::Body, Theme.TextMuted);
	FALKWidgetBuilder::StyleText(RejoinText, Theme, EALKTextRole::Body, Theme.TextPrimary);
	if (UALKFrame* Banner = Cast<UALKFrame>(RejoinBanner))
	{
		// A raised bar with the accent colour round it, so it is noticed.
		FALKFrameStyle BannerStyle = Theme.MakeCardFrame();
		BannerStyle.Outline = Theme.FocusStyle == EALKFocusStyle::Solid ? Theme.PanelOutline : Theme.Accent.CopyWithNewOpacity(0.6f);
		Banner->SetFrameStyle(BannerStyle);
	}
	if (RejoinIcon)
	{
		RejoinIcon->SetIcon(EALKIcon::Refresh, Theme.GetAccentTextColor());
	}
	RefreshHub();
}

void UALKPlayHubScreen::NativeOnActivated()
{
	if (UALKOnlineSubsystem* Online = UALKOnlineSubsystem::Get(this))
	{
		if (!MatchmakingHandle.IsValid())
		{
			MatchmakingHandle = Online->OnMatchmakingChangedNative.AddWeakLambda(this, [this](EALKMatchmakingState) { RefreshHub(); });
			PartyHandle = Online->OnPartyChangedNative.AddUObject(this, &ThisClass::RefreshHub);
			StateHandle = Online->OnStateChangedNative.AddWeakLambda(this, [this](EALKOnlineState, EALKOnlineState) { RefreshHub(); });
		}
	}
	RebuildPlaylists();
	RefreshHub();

	// A match to go back to comes first; otherwise the hub opens on Quick Match.
	UWidget* Target = nullptr;
	if (bRejoinShown && RejoinButton)
	{
		Target = RejoinButton.Get();
	}
	else
	{
		for (UALKCardButton* Tile : { FeaturedTile.Get(), HostTile.Get() })
		{
			if (!Target && Tile && Tile->GetIsEnabled())
			{
				Target = Tile;
			}
		}
	}
	DefaultFocusWidget = Target;

	Super::NativeOnActivated();
}

void UALKPlayHubScreen::NativeOnDeactivated()
{
	if (UALKOnlineSubsystem* Online = UALKOnlineSubsystem::Get(this))
	{
		Online->OnMatchmakingChangedNative.Remove(MatchmakingHandle);
		Online->OnPartyChangedNative.Remove(PartyHandle);
		Online->OnStateChangedNative.Remove(StateHandle);
	}
	MatchmakingHandle.Reset();
	PartyHandle.Reset();
	StateHandle.Reset();
	Super::NativeOnDeactivated();
}

void UALKPlayHubScreen::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	LayoutPlaylists();

	// The rejoin window and the wait estimates change with time, not with an event.
	if (IsActivated())
	{
		RefreshClock += FMath::Min(InDeltaTime, 0.1f);
		if (RefreshClock >= 1.f)
		{
			RefreshClock = 0.f;
			RefreshHub();
		}
	}
}

FName UALKPlayHubScreen::GetFeaturedPlaylist() const
{
	const TArray<FALKPlaylist>& Playlists = UALKDeveloperSettings::GetOnlineConfig()->Playlists;
	for (const FALKPlaylist& Playlist : Playlists)
	{
		if (Playlist.bFeatured && !Playlist.Id.IsNone())
		{
			return Playlist.Id;
		}
	}
	return Playlists.Num() > 0 ? Playlists[0].Id : NAME_None;
}

void UALKPlayHubScreen::RebuildPlaylists()
{
	using namespace ALKPlayHubScreenPrivate;
	if (!PlaylistList || !WidgetTree)
	{
		return;
	}
	const TArray<FALKPlaylist>& Playlists = UALKDeveloperSettings::GetOnlineConfig()->Playlists;
	TArray<FName> Ids;
	for (const FALKPlaylist& Playlist : Playlists)
	{
		Ids.Add(Playlist.Id);
	}
	if (Ids == PlaylistIds && PlaylistCards.Num() == Ids.Num())
	{
		// Same cards as last time: the one that had the focus keeps it.
		return;
	}

	PlaylistIds = Ids;
	PlaylistList->ClearChildren();
	PlaylistCards.Reset();
	PlaylistSizes.Reset();
	LastListWidth = 0.f;
	for (int32 Index = 0; Index < Playlists.Num(); ++Index)
	{
		const FALKPlaylist& Playlist = Playlists[Index];
		UALKCardButton* Card = WidgetTree->ConstructWidget<UALKCardButton>(UALKCardButton::StaticClass());
		if (!Card)
		{
			continue;
		}
		Card->SetCard(Playlist.Name.IsEmpty() ? FText::FromName(Playlist.Id) : Playlist.Name, Playlist.Description);
		Card->SetPicture(Playlist.Thumbnail.LoadSynchronous(), Playlist.Id);
		Card->SetCornerChip(Playlist.SizeLabel);
		Card->OnClicked().AddUObject(this, &ThisClass::HandlePlaylistClicked, Index);

		USizeBox* CardSize = FALKWidgetBuilder::WrapInSizeBox(*WidgetTree, Card, 400.f, 0.f);
		PlaylistList->AddChild(CardSize);
		if (UScrollBoxSlot* CardSlot = Cast<UScrollBoxSlot>(CardSize->Slot))
		{
			CardSlot->SetPadding(FMargin(CardPadX, CardPadTop, CardPadX, CardPadBottom));
			CardSlot->SetVerticalAlignment(VAlign_Fill);
		}
		PlaylistCards.Add(Card);
		PlaylistSizes.Add(CardSize);
	}
}

void UALKPlayHubScreen::LayoutPlaylists()
{
	using namespace ALKPlayHubScreenPrivate;
	if (!PlaylistList || PlaylistSizes.Num() == 0)
	{
		return;
	}
	// Each card takes its share of the row: Playlist Columns of them fit, the rest scroll.
	const float ListWidth = PlaylistList->GetCachedGeometry().GetLocalSize().X;
	if (ListWidth > 1.f && !FMath::IsNearlyEqual(ListWidth, LastListWidth, 0.5f))
	{
		LastListWidth = ListWidth;
		const float CardWidth = FMath::Max(MinCardWidth, FMath::FloorToFloat(ListWidth / FMath::Max(1, PlaylistColumns)) - CardPadX * 2.f);
		for (USizeBox* CardSize : PlaylistSizes)
		{
			if (CardSize)
			{
				CardSize->SetWidthOverride(CardWidth);
			}
		}
	}
}

void UALKPlayHubScreen::RefreshHub()
{
	const UALKOnlineSubsystem* Online = UALKOnlineSubsystem::Get(this);
	const UALKOnlineConfig* Config = UALKDeveloperSettings::GetOnlineConfig();
	const UALKTheme& Theme = *GetTheme();
	const bool bOnline = Online && Online->IsOnlineEnabled();
	const EALKMatchmakingState State = bOnline ? Online->GetMatchmakingState() : EALKMatchmakingState::Idle;
	const bool bSearching = State == EALKMatchmakingState::Searching;
	const FName QueuePlaylist = bOnline ? Online->GetQueuePlaylist() : NAME_None;

	if (FeaturedTile)
	{
		const FName FeaturedId = GetFeaturedPlaylist();
		const FALKPlaylist* Featured = Config->FindPlaylist(FeaturedId);
		FText Subtitle = LOCTEXT("HubNoPlaylists", "No playlists are set up yet");
		if (Featured)
		{
			const FText Name = Featured->Name.IsEmpty() ? FText::FromName(Featured->Id) : Featured->Name;
			Subtitle = Featured->SizeLabel.IsEmpty() ? Name : FText::Format(LOCTEXT("HubFeaturedSub", "{0}  ·  {1}"), Name, Featured->SizeLabel);
		}
		FeaturedTile->SetCard(LOCTEXT("HubQuickMatch", "Quick Match"), Subtitle);
		FeaturedTile->SetPicture(Featured ? Featured->Thumbnail.LoadSynchronous() : nullptr, FeaturedId.IsNone() ? FName(TEXT("QuickMatch")) : FeaturedId);
		FeaturedTile->SetBadge(Featured ? LOCTEXT("HubFeaturedBadge", "Featured playlist").ToUpper() : FText::GetEmpty());
		// The pill names what picking the tile does right now.
		switch (State)
		{
		case EALKMatchmakingState::Searching:
			FeaturedTile->SetActionLabel(LOCTEXT("HubCancelSearch", "Cancel search"), EALKIcon::Cross);
			break;
		case EALKMatchmakingState::MatchFound:
		case EALKMatchmakingState::Accepted:
			FeaturedTile->SetActionLabel(LOCTEXT("HubMatchFound", "Match found"), EALKIcon::Check);
			break;
		case EALKMatchmakingState::Joining:
			FeaturedTile->SetActionLabel(LOCTEXT("HubJoining", "Joining..."), EALKIcon::None);
			break;
		default:
			FeaturedTile->SetActionLabel(LOCTEXT("HubFindMatch", "Find match"), EALKIcon::Bolt);
			break;
		}
		FeaturedTile->SetIsEnabled(bOnline && Featured != nullptr);
	}

	for (UALKCardButton* Tile : { HostTile.Get(), BrowseTile.Get(), CodeTile.Get(), PartyTile.Get() })
	{
		if (Tile)
		{
			Tile->SetIsEnabled(bOnline);
		}
	}
	if (PartyTile)
	{
		const int32 PartySize = bOnline ? FMath::Max(1, Online->GetPartyMembers().Num()) : 1;
		const int32 PartyMax = bOnline ? FMath::Max(PartySize, Online->GetPartyMaxSize()) : 1;
		PartyTile->SetSubtitle(FText::Format(LOCTEXT("HubPartySub", "{0} / {1}  ·  invite friends"), FText::AsNumber(PartySize), FText::AsNumber(PartyMax)));
	}

	for (int32 Index = 0; Index < PlaylistCards.Num(); ++Index)
	{
		UALKCardButton* Card = PlaylistCards[Index];
		if (!Card || !PlaylistIds.IsValidIndex(Index))
		{
			continue;
		}
		Card->SetIsEnabled(bOnline);
		// The footer says what the queue is doing: searching this playlist, or how long a search usually takes.
		const float Estimate = bOnline ? Online->GetEstimatedWaitSeconds(PlaylistIds[Index]) : -1.f;
		if (bSearching && QueuePlaylist == PlaylistIds[Index])
		{
			Card->SetFooter(LOCTEXT("HubPlaylistSearching", "Searching..."), Theme.Accent);
		}
		else if (Estimate >= 0.f)
		{
			Card->SetFooter(FText::Format(LOCTEXT("HubPlaylistWait", "est. wait {0}"), UALKQueueWidget::FormatMinutesSeconds(Estimate, false)), Theme.Success);
		}
		else
		{
			Card->SetFooter(FText::GetEmpty(), FLinearColor::Transparent);
		}
	}
	if (PlaylistLabel)
	{
		PlaylistLabel->SetVisibility(PlaylistCards.Num() > 0 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}

	// The rejoin banner. When it goes away under the focus, the focus moves on to Quick Match.
	const bool bWasRejoinShown = bRejoinShown;
	bRejoinShown = bOnline && Online->CanRejoinMatch();
	if (RejoinBanner)
	{
		const bool bBannerFocused = RejoinBanner->HasFocusedDescendants();
		RejoinBanner->SetVisibility(bRejoinShown ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
		if (bWasRejoinShown && !bRejoinShown && bBannerFocused && FeaturedTile)
		{
			FeaturedTile->SetFocus();
		}
	}
	if (RejoinText && bRejoinShown)
	{
		RejoinText->SetText(FText::Format(LOCTEXT("HubRejoinText", "Rejoin {0}: the match you dropped out of is still running."), FText::FromString(Online->GetRejoinServerName())));
	}
	if (OfflineText)
	{
		OfflineText->SetVisibility(bOnline ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
	if (CancelHints)
	{
		CancelHints->SetVisibility(bSearching && GetMenuConfig()->bShowInputHints ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void UALKPlayHubScreen::StartQuickMatch(FName PlaylistId)
{
	UALKOnlineSubsystem* Online = UALKOnlineSubsystem::Get(this);
	if (!Online || PlaylistId.IsNone())
	{
		PlayThemeSound(EALKUISound::Locked);
		return;
	}
	const EALKMatchmakingState State = Online->GetMatchmakingState();
	if (State == EALKMatchmakingState::Searching)
	{
		if (Online->GetQueuePlaylist() == PlaylistId)
		{
			return;
		}
		// Another playlist: the search moves over to it.
		Online->CancelQuickMatch();
	}
	else if (State != EALKMatchmakingState::Idle && State != EALKMatchmakingState::Failed)
	{
		// A match was found or is being joined: nothing to start.
		PlayThemeSound(EALKUISound::Locked);
		return;
	}
	if (!Online->StartQuickMatch(PlaylistId))
	{
		PlayThemeSound(EALKUISound::Locked);
	}
}

void UALKPlayHubScreen::ToggleFeaturedSearch()
{
	const UALKOnlineSubsystem* Online = UALKOnlineSubsystem::Get(this);
	if (Online && Online->GetMatchmakingState() == EALKMatchmakingState::Searching)
	{
		CancelSearch();
	}
	else
	{
		StartQuickMatch(GetFeaturedPlaylist());
	}
}

void UALKPlayHubScreen::CancelSearch()
{
	UALKOnlineSubsystem* Online = UALKOnlineSubsystem::Get(this);
	if (Online && Online->GetMatchmakingState() == EALKMatchmakingState::Searching)
	{
		PlayThemeSound(EALKUISound::Back);
		Online->CancelQuickMatch();
	}
}

void UALKPlayHubScreen::HandlePlaylistClicked(int32 Index)
{
	if (PlaylistIds.IsValidIndex(Index))
	{
		StartQuickMatch(PlaylistIds[Index]);
	}
}

void UALKPlayHubScreen::HandleRejoinClicked()
{
	if (UALKOnlineSubsystem* Online = UALKOnlineSubsystem::Get(this))
	{
		Online->RejoinMatch();
	}
}

void UALKPlayHubScreen::HandleDismissClicked()
{
	if (UALKOnlineSubsystem* Online = UALKOnlineSubsystem::Get(this))
	{
		Online->ClearRejoin();
	}
	RefreshHub();
}

void UALKPlayHubScreen::FocusParty()
{
	if (PartyPanel)
	{
		PartyPanel->FocusFirst();
	}
}

bool UALKPlayHubScreen::NativeHandleMenuKey(const FKey& Key)
{
	if (Key == EKeys::Y || Key == EKeys::Gamepad_FaceButton_Top)
	{
		FocusParty();
		return true;
	}
	if (Key == EKeys::X || Key == EKeys::Gamepad_FaceButton_Left)
	{
		const UALKOnlineSubsystem* Online = UALKOnlineSubsystem::Get(this);
		if (Online && Online->GetMatchmakingState() == EALKMatchmakingState::Searching)
		{
			CancelSearch();
			return true;
		}
	}
	return Super::NativeHandleMenuKey(Key);
}

#undef LOCTEXT_NAMESPACE
