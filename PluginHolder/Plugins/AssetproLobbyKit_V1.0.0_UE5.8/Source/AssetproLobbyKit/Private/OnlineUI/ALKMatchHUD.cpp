// Copyright 2026 CodeLikeMe. All Rights Reserved.

#include "OnlineUI/ALKMatchHUD.h"
#include "OnlineUI/ALKOnlineFlowSubsystem.h"
#include "OnlineUI/ALKScoreboard.h"
#include "ALKDeveloperSettings.h"
#include "ALKMenuConfig.h"
#include "ALKTheme.h"
#include "Lobby/ALKLobbyGameState.h"
#include "Lobby/ALKLobbyPlayerState.h"
#include "Lobby/ALKPlayerNetComponent.h"
#include "Online/ALKOnlineConfig.h"
#include "Online/ALKOnlineSubsystem.h"
#include "Settings/ALKSettingsSubsystem.h"
#include "Widgets/ALKDecor.h"
#include "Widgets/ALKFrame.h"
#include "Widgets/ALKInputLabels.h"
#include "Widgets/ALKPaint.h"
#include "Widgets/ALKTextField.h"
#include "Widgets/ALKTitleText.h"
#include "Widgets/ALKWidgetBuilder.h"
#include "Blueprint/WidgetTree.h"
#include "CommonInputSubsystem.h"
#include "CommonInputTypeEnum.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ALKMatchHUD)

#define LOCTEXT_NAMESPACE "AssetproLobbyKit"

// Keeps the scoreboard up without holding its key: for screenshots, and for laying the scoreboard out.
static TAutoConsoleVariable<int32> CVarALKHudScoreboard(TEXT("ALK.HUD.Scoreboard"), 0,
	TEXT("1 keeps the match overlay's scoreboard up without holding the scoreboard key."));

// Shows the network stats whatever the player's setting says.
static TAutoConsoleVariable<int32> CVarALKHudNetStats(TEXT("ALK.HUD.NetStats"), 0,
	TEXT("1 shows the match overlay's network stats even when the player's setting is off."));

namespace ALKMatchHUDPrivate
{
	static const FName KillEvent(TEXT("Kill"));
	static const FName CaptureEvent(TEXT("Capture"));
	static const FName JoinEvent(TEXT("Join"));
	static const FName LeaveEvent(TEXT("Leave"));

	/** Width of the chat, and of the scoreboard when the screen has room for it. */
	static constexpr float ChatWidth = 624.f;
	static constexpr float ScoreboardWidth = 1368.f;
	/** How many chat lines are remembered for the moment the player opens the chat line. */
	static constexpr int32 ChatMemory = 24;

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

	/** Fades a timed item in over FadeIn seconds and out over its last FadeOut seconds; with Reduce Effects it just shows. */
	static float TimedOpacity(float Age, float Duration, float FadeIn, float FadeOut)
	{
		if (FALKEffects::AreReduced())
		{
			return Age < Duration ? 1.f : 0.f;
		}
		return FMath::Clamp(Age / FadeIn, 0.f, 1.f) * FMath::Clamp((Duration - Age) / FadeOut, 0.f, 1.f);
	}

	static FLinearColor Mix(const FLinearColor& From, const FLinearColor& To, float Alpha)
	{
		return FMath::Lerp(From, To, Alpha).CopyWithNewOpacity(1.f);
	}

	static FSlateFontInfo Scaled(const FSlateFontInfo& Font, float Scale)
	{
		FSlateFontInfo Result = Font;
		Result.Size = FMath::Max(8.f, FMath::RoundToFloat(Font.Size * Scale));
		return Result;
	}

	/**
	 * A thin edge so text that stands straight on the game reads over any scene: dark behind the light text of the dark
	 * skins, light under the dark text of the Light skin.
	 */
	static void AddShadow(UTextBlock* Text, const UALKTheme& Theme)
	{
		if (Text)
		{
			const bool bLightText = Theme.TextPrimary.GetLuminance() > 0.5f;
			Text->SetShadowColorAndOpacity(bLightText ? FLinearColor(0.f, 0.f, 0.f, 0.85f) : FLinearColor(1.f, 1.f, 1.f, 0.75f));
			Text->SetShadowOffset(FVector2D(bLightText ? 1.0 : 0.0, 1.0));
		}
	}

	static void SetOverlayPadding(UWidget* Widget, const FMargin& Margin)
	{
		if (UOverlaySlot* OverlaySlot = Widget ? Cast<UOverlaySlot>(Widget->Slot) : nullptr)
		{
			OverlaySlot->SetPadding(Margin);
		}
	}

	/** A HUD panel: the theme's panel, see-through, without the large shadow of a menu panel. */
	static FALKFrameStyle MakeHudFrame(const UALKTheme& Theme, float Alpha)
	{
		FALKFrameStyle Frame = Theme.MakePanelFrame(Theme.PanelStyle == EALKPanelStyle::Chunky);
		Frame.Fill = Theme.Panel.CopyWithNewOpacity(Theme.Panel.A * Alpha);
		return Frame;
	}
}

UALKMatchHUD::UALKMatchHUD(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

// ---------------------------------------------------------------- Layout

void UALKMatchHUD::NativeBuildDefaultLayout(UWidgetTree& Tree)
{
	using namespace ALKMatchHUDPrivate;

	UOverlay* Root = FALKWidgetBuilder::Make<UOverlay>(Tree, TEXT("HUDRoot"));
	Tree.RootWidget = Root;

	// ---- Top left: network stats, then who is talking
	UVerticalBox* LeftColumn = FALKWidgetBuilder::Make<UVerticalBox>(Tree, TEXT("LeftColumn"));
	FALKWidgetBuilder::AddToOverlay(Root, LeftColumn, HAlign_Left, VAlign_Top);

	UALKFrame* NetFrame = FALKWidgetBuilder::Make<UALKFrame>(Tree, TEXT("NetStatsPanel"));
	NetFrame->SetPadding(FMargin(16.f, 11.f, 16.f, 12.f));
	NetStatsPanel = NetFrame;
	FALKWidgetBuilder::AddToVerticalBox(LeftColumn, NetFrame, FMargin(0.f, 0.f, 0.f, 16.f), HAlign_Left);
	UVerticalBox* NetRows = FALKWidgetBuilder::Make<UVerticalBox>(Tree, TEXT("NetStatsRows"));
	NetFrame->SetContent(FALKWidgetBuilder::WrapInSizeBox(Tree, NetRows, 228.f, 0.f, true));
	auto AddNetRow = [&Tree, NetRows](FName LabelName, FName ValueName, const FText& LabelText) -> UTextBlock*
	{
		UHorizontalBox* NetRow = FALKWidgetBuilder::Make<UHorizontalBox>(Tree);
		FALKWidgetBuilder::AddToHorizontalBox(NetRow, FALKWidgetBuilder::MakeText(Tree, LabelName, LabelText), FMargin(0.f, 0.f, 24.f, 0.f), HAlign_Fill, VAlign_Center, 1.f);
		UTextBlock* Value = FALKWidgetBuilder::MakeText(Tree, ValueName);
		Value->SetJustification(ETextJustify::Right);
		FALKWidgetBuilder::AddToHorizontalBox(NetRow, Value, FMargin(), HAlign_Right, VAlign_Center);
		FALKWidgetBuilder::AddToVerticalBox(NetRows, NetRow, FMargin(0.f, 3.f));
		return Value;
	};
	NetPingText = AddNetRow(TEXT("NetPingLabel"), TEXT("NetPingText"), LOCTEXT("HudNetPing", "Ping"));
	NetLossText = AddNetRow(TEXT("NetLossLabel"), TEXT("NetLossText"), LOCTEXT("HudNetLoss", "Packet loss in / out"));
	NetTrafficText = AddNetRow(TEXT("NetTrafficLabel"), TEXT("NetTrafficText"), LOCTEXT("HudNetTraffic", "In / Out"));
	NetServerText = AddNetRow(TEXT("NetServerLabel"), TEXT("NetServerText"), LOCTEXT("HudNetServer", "Server"));

	VoiceBox = FALKWidgetBuilder::Make<UVerticalBox>(Tree, TEXT("VoiceBox"));
	FALKWidgetBuilder::AddToVerticalBox(LeftColumn, VoiceBox, FMargin(), HAlign_Left);

	// ---- Top middle: [team score] clock over mode [team score]
	UALKFrame* Strip = FALKWidgetBuilder::Make<UALKFrame>(Tree, TEXT("ScoreStrip"));
	Strip->SetPadding(FMargin(7.f));
	ScoreStrip = Strip;
	FALKWidgetBuilder::AddToOverlay(Root, Strip, HAlign_Center, VAlign_Top);
	UHorizontalBox* StripRow = FALKWidgetBuilder::Make<UHorizontalBox>(Tree, TEXT("ScoreStripRow"));
	Strip->SetContent(StripRow);
	TeamBlocksLeft = FALKWidgetBuilder::Make<UHorizontalBox>(Tree, TEXT("TeamBlocksLeft"));
	FALKWidgetBuilder::AddToHorizontalBox(StripRow, TeamBlocksLeft, FMargin(), HAlign_Left, VAlign_Fill);
	UVerticalBox* ClockColumn = FALKWidgetBuilder::Make<UVerticalBox>(Tree, TEXT("ClockColumn"));
	ClockText = FALKWidgetBuilder::MakeText(Tree, TEXT("ClockText"));
	ClockText->SetJustification(ETextJustify::Center);
	FALKWidgetBuilder::AddToVerticalBox(ClockColumn, ClockText, FMargin(), HAlign_Center);
	ModeText = FALKWidgetBuilder::MakeText(Tree, TEXT("ModeText"));
	ModeText->SetJustification(ETextJustify::Center);
	FALKWidgetBuilder::AddToVerticalBox(ClockColumn, ModeText, FMargin(0.f, -2.f, 0.f, 0.f), HAlign_Center);
	FALKWidgetBuilder::AddToHorizontalBox(StripRow, FALKWidgetBuilder::WrapInSizeBox(Tree, ClockColumn, 196.f, 0.f, true), FMargin(12.f, 0.f), HAlign_Center, VAlign_Center);
	TeamBlocksRight = FALKWidgetBuilder::Make<UHorizontalBox>(Tree, TEXT("TeamBlocksRight"));
	FALKWidgetBuilder::AddToHorizontalBox(StripRow, TeamBlocksRight, FMargin(), HAlign_Right, VAlign_Fill);

	// ---- Top right: the event feed
	FeedBox = FALKWidgetBuilder::Make<UVerticalBox>(Tree, TEXT("FeedBox"));
	FALKWidgetBuilder::AddToOverlay(Root, FeedBox, HAlign_Right, VAlign_Top);

	// ---- Upper middle: announcements
	AnnouncementBox = FALKWidgetBuilder::Make<UVerticalBox>(Tree, TEXT("AnnouncementBox"));
	FALKWidgetBuilder::AddToOverlay(Root, AnnouncementBox, HAlign_Center, VAlign_Top);
	UALKTitleText* Title = FALKWidgetBuilder::Make<UALKTitleText>(Tree, TEXT("AnnouncementTitle"));
	Title->SetJustification(ETextJustify::Center);
	AnnouncementTitle = Title;
	FALKWidgetBuilder::AddToVerticalBox(AnnouncementBox, AnnouncementTitle, FMargin(), HAlign_Center);
	AnnouncementSubtitle = FALKWidgetBuilder::MakeText(Tree, TEXT("AnnouncementSubtitle"));
	AnnouncementSubtitle->SetJustification(ETextJustify::Center);
	FALKWidgetBuilder::AddToVerticalBox(AnnouncementBox, AnnouncementSubtitle, FMargin(0.f, 4.f, 0.f, 0.f), HAlign_Center);

	// ---- Bottom left: chat lines over the line to type in
	UALKFrame* ChatFrame = FALKWidgetBuilder::Make<UALKFrame>(Tree, TEXT("ChatPanel"));
	ChatFrame->SetPadding(FMargin(16.f, 14.f, 16.f, 14.f));
	ChatPanel = ChatFrame;
	UVerticalBox* ChatColumn = FALKWidgetBuilder::Make<UVerticalBox>(Tree, TEXT("ChatColumn"));
	ChatFrame->SetContent(ChatColumn);
	ChatLines = FALKWidgetBuilder::Make<UVerticalBox>(Tree, TEXT("ChatLines"));
	FALKWidgetBuilder::AddToVerticalBox(ChatColumn, ChatLines);
	ChatEntryRow = FALKWidgetBuilder::Make<UHorizontalBox>(Tree, TEXT("ChatEntryRow"));
	FALKWidgetBuilder::AddToVerticalBox(ChatColumn, ChatEntryRow, FMargin(0.f, 10.f, 0.f, 0.f));
	ChatChannelChip = FALKWidgetBuilder::MakeChip(Tree, TEXT("ChatChannelChip"), FText::GetEmpty(), EALKIcon::None);
	FALKWidgetBuilder::AddToHorizontalBox(ChatEntryRow, ChatChannelChip, FMargin(0.f, 0.f, 10.f, 0.f), HAlign_Left, VAlign_Center);
	ChatField = Tree.ConstructWidget<UALKTextField>(UALKTextField::StaticClass(), TEXT("ChatField"));
	FALKWidgetBuilder::AddToHorizontalBox(ChatEntryRow, ChatField, FMargin(), HAlign_Fill, VAlign_Center, 1.f);
	USizeBox* ChatSize = FALKWidgetBuilder::Make<USizeBox>(Tree, TEXT("ChatSize"));
	ChatSize->SetWidthOverride(ChatWidth);
	ChatSize->SetContent(ChatFrame);
	FALKWidgetBuilder::AddToOverlay(Root, ChatSize, HAlign_Left, VAlign_Bottom);

	// ---- Bottom middle: the spectator bar
	UALKFrame* SpectatorFrame = FALKWidgetBuilder::Make<UALKFrame>(Tree, TEXT("SpectatorBar"));
	SpectatorFrame->SetPadding(FMargin(20.f, 9.f, 20.f, 10.f));
	SpectatorBar = SpectatorFrame;
	FALKWidgetBuilder::AddToOverlay(Root, SpectatorFrame, HAlign_Center, VAlign_Bottom);
	UHorizontalBox* SpectatorRow = FALKWidgetBuilder::Make<UHorizontalBox>(Tree, TEXT("SpectatorRow"));
	SpectatorFrame->SetContent(SpectatorRow);
	FALKWidgetBuilder::AddToHorizontalBox(SpectatorRow, FALKWidgetBuilder::MakeIcon(Tree, TEXT("SpectatorIcon"), EALKIcon::Eye, 22.f), FMargin(0.f, 0.f, 12.f, 0.f), HAlign_Left, VAlign_Center);
	SpectatorText = FALKWidgetBuilder::MakeText(Tree, TEXT("SpectatorText"));
	FALKWidgetBuilder::AddToHorizontalBox(SpectatorRow, SpectatorText, FMargin(), HAlign_Left, VAlign_Center);
	auto AddSpectateKey = [&Tree, SpectatorRow](FName CapName, FName KeyName, FName LabelName, const FText& LabelText) -> UTextBlock*
	{
		UALKFrame* Cap = FALKWidgetBuilder::Make<UALKFrame>(Tree, CapName);
		Cap->SetPadding(FMargin(9.f, 2.f));
		Cap->SetHorizontalAlignment(HAlign_Center);
		Cap->SetVerticalAlignment(VAlign_Center);
		UTextBlock* KeyLabel = FALKWidgetBuilder::MakeText(Tree, KeyName);
		KeyLabel->SetJustification(ETextJustify::Center);
		Cap->SetContent(KeyLabel);
		FALKWidgetBuilder::AddToHorizontalBox(SpectatorRow, FALKWidgetBuilder::WrapInSizeBox(Tree, Cap, 30.f, 30.f, true), FMargin(26.f, 0.f, 8.f, 0.f), HAlign_Left, VAlign_Center);
		FALKWidgetBuilder::AddToHorizontalBox(SpectatorRow, FALKWidgetBuilder::MakeText(Tree, LabelName, LabelText), FMargin(), HAlign_Left, VAlign_Center);
		return KeyLabel;
	};
	SpectatePrevKeyText = AddSpectateKey(TEXT("SpectatePrevCap"), TEXT("SpectatePrevKeyText"), TEXT("SpectatePrevLabel"), LOCTEXT("HudSpectatePrevious", "Previous"));
	SpectateNextKeyText = AddSpectateKey(TEXT("SpectateNextCap"), TEXT("SpectateNextKeyText"), TEXT("SpectateNextLabel"), LOCTEXT("HudSpectateNext", "Next"));

	// ---- The scoreboard over everything, while its key is held
	ScoreboardDim = FALKWidgetBuilder::Make<UImage>(Tree, TEXT("ScoreboardDim"));
	FALKWidgetBuilder::AddToOverlay(Root, ScoreboardDim);
	Scoreboard = Tree.ConstructWidget<UALKScoreboard>(UALKScoreboard::StaticClass(), TEXT("Scoreboard"));
	USizeBox* ScoreboardSize = FALKWidgetBuilder::Make<USizeBox>(Tree, TEXT("ScoreboardSize"));
	ScoreboardSize->SetWidthOverride(ScoreboardWidth);
	ScoreboardSize->SetContent(Scoreboard);
	FALKWidgetBuilder::AddToOverlay(Root, ScoreboardSize, HAlign_Center, VAlign_Center, FMargin(40.f, 64.f));
}

void UALKMatchHUD::NativeOnLayoutReady()
{
	Super::NativeOnLayoutReady();
	using namespace ALKMatchHUDPrivate;

	// The overlay lets every click through to the game; only the chat line can be clicked, and only while it is open.
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	if (WidgetTree)
	{
		for (const TCHAR* Name : { TEXT("LeftColumn"), TEXT("ScoreboardSize") })
		{
			if (UWidget* Part = WidgetTree->FindWidget(Name))
			{
				Part->SetVisibility(ESlateVisibility::HitTestInvisible);
			}
		}
		if (UWidget* ChatSize = WidgetTree->FindWidget(TEXT("ChatSize")))
		{
			ChatSize->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		}
	}
	SetShown(FeedBox, true);
	SetShown(ScoreStrip, false);
	SetShown(NetStatsPanel, false);
	SetShown(SpectatorBar, false);
	SetShown(AnnouncementBox, false);
	SetShown(ScoreboardDim, false);
	SetShown(Scoreboard, false);
	SetShown(ChatPanel, true, ESlateVisibility::SelfHitTestInvisible);
	SetShown(ChatLines, true);
	SetShown(ChatChannelChip, true);
	SetShown(ChatEntryRow, false);
	if (ChatField)
	{
		ChatField->OnTextCommittedNative.AddUObject(this, &ThisClass::HandleChatCommitted);
	}
}

void UALKMatchHUD::NativeApplyTheme(const UALKTheme& Theme)
{
	Super::NativeApplyTheme(Theme);
	using namespace ALKMatchHUDPrivate;

	// Distances from the screen edges: the overlay's parts sit a little closer to the edge than the kit's notifications.
	const float Side = Theme.HudMargin.Left * 0.7f;
	const float Top = Theme.HudMargin.Top * 0.55f;
	const float Bottom = Theme.HudMargin.Bottom;
	if (WidgetTree)
	{
		SetOverlayPadding(WidgetTree->FindWidget(TEXT("LeftColumn")), FMargin(Side, Top, 0.f, 0.f));
		SetOverlayPadding(WidgetTree->FindWidget(TEXT("ChatSize")), FMargin(Side, 0.f, 0.f, Bottom + 60.f));
	}
	SetOverlayPadding(ScoreStrip, FMargin(0.f, Top, 0.f, 0.f));
	SetOverlayPadding(FeedBox, FMargin(0.f, Top, Theme.HudMargin.Right * 0.7f, 0.f));
	SetOverlayPadding(AnnouncementBox, FMargin(0.f, Top + 150.f, 0.f, 0.f));
	SetOverlayPadding(SpectatorBar, FMargin(0.f, 0.f, 0.f, Bottom + 70.f));

	// ---- Score strip
	if (UALKFrame* Strip = Cast<UALKFrame>(ScoreStrip))
	{
		Strip->SetFrameStyle(MakeHudFrame(Theme, 0.86f));
	}
	if (ClockText)
	{
		FALKWidgetBuilder::StyleText(ClockText, Theme, EALKTextRole::Subheading, Theme.TextPrimary);
		FALKWidgetBuilder::SetTextFont(ClockText, Scaled(Theme.HeadingFont, 0.66f));
	}
	if (ModeText)
	{
		FALKWidgetBuilder::StyleText(ModeText, Theme, EALKTextRole::Data, Theme.TextMuted);
		FALKWidgetBuilder::SetTextFont(ModeText, Scaled(Theme.DataFont, 0.8f));
		ModeText->SetTextTransformPolicy(ETextTransformPolicy::ToUpper);
	}
	ShownClockSeconds = MIN_int32;
	StyleTeamBlocks(Theme);

	// ---- Network stats
	if (UALKFrame* NetFrame = Cast<UALKFrame>(NetStatsPanel))
	{
		NetFrame->SetFrameStyle(MakeHudFrame(Theme, 0.8f));
	}
	if (WidgetTree)
	{
		for (const TCHAR* Name : { TEXT("NetPingLabel"), TEXT("NetLossLabel"), TEXT("NetTrafficLabel"), TEXT("NetServerLabel") })
		{
			FALKWidgetBuilder::StyleText(Cast<UTextBlock>(WidgetTree->FindWidget(Name)), Theme, EALKTextRole::Caption, Theme.TextMuted);
		}
		for (const TCHAR* Name : { TEXT("SpectatePrevLabel"), TEXT("SpectateNextLabel") })
		{
			FALKWidgetBuilder::StyleText(Cast<UTextBlock>(WidgetTree->FindWidget(Name)), Theme, EALKTextRole::Caption, Theme.TextMuted);
		}
		for (const TCHAR* Name : { TEXT("SpectatePrevCap"), TEXT("SpectateNextCap") })
		{
			if (UALKFrame* Cap = Cast<UALKFrame>(WidgetTree->FindWidget(Name)))
			{
				Cap->SetFrameStyle(Theme.MakeChipFrame(Theme.KeyFill, Theme.KeyOutline));
			}
		}
		if (UALKIconWidget* Eye = Cast<UALKIconWidget>(WidgetTree->FindWidget(TEXT("SpectatorIcon"))))
		{
			Eye->SetIcon(EALKIcon::Eye, Theme.GetAccentTextColor());
		}
	}
	for (UTextBlock* Value : { NetPingText.Get(), NetLossText.Get(), NetTrafficText.Get(), NetServerText.Get() })
	{
		FALKWidgetBuilder::StyleText(Value, Theme, EALKTextRole::Data, Theme.TextPrimary);
	}
	NetTimer = 1.f;

	// ---- Spectator bar
	if (UALKFrame* SpectatorFrame = Cast<UALKFrame>(SpectatorBar))
	{
		SpectatorFrame->SetFrameStyle(MakeHudFrame(Theme, 0.86f));
	}
	if (SpectatorText)
	{
		FALKWidgetBuilder::StyleText(SpectatorText, Theme, EALKTextRole::Subheading, Theme.TextPrimary);
		FALKWidgetBuilder::SetTextFont(SpectatorText, Scaled(Theme.SubheadingFont, 0.86f));
	}
	FALKWidgetBuilder::StyleText(SpectatePrevKeyText, Theme, EALKTextRole::Data, Theme.KeyText);
	FALKWidgetBuilder::StyleText(SpectateNextKeyText, Theme, EALKTextRole::Data, Theme.KeyText);

	// ---- Announcement
	if (AnnouncementTitle)
	{
		FALKWidgetBuilder::StyleText(AnnouncementTitle, Theme, EALKTextRole::Heading, Theme.TextPrimary);
		FALKWidgetBuilder::SetTextFont(AnnouncementTitle, Scaled(Theme.HeadingFont, 1.15f));
		AddShadow(AnnouncementTitle, Theme);
	}
	if (AnnouncementSubtitle)
	{
		FALKWidgetBuilder::StyleText(AnnouncementSubtitle, Theme, EALKTextRole::Body, Theme.TextSecondary);
		FALKWidgetBuilder::SetTextFont(AnnouncementSubtitle, Scaled(Theme.SubheadingFont, 0.9f));
		AnnouncementSubtitle->SetTextTransformPolicy(ETextTransformPolicy::ToUpper);
		AddShadow(AnnouncementSubtitle, Theme);
	}

	// ---- Scoreboard
	if (ScoreboardDim)
	{
		ScoreboardDim->SetBrush(UALKTheme::MakeSolidBrush(Theme.ScreenDim.CopyWithNewOpacity(Theme.ScreenDim.A * 0.6f)));
	}

	// ---- Lines made at runtime: feed lines are short lived and simply go, the others are made again in the new look.
	if (FeedBox)
	{
		FeedBox->ClearChildren();
	}
	FeedLines.Reset();
	ApplyChatLook();
	RebuildChatLines();
	RebuildVoiceList();
}

// ---------------------------------------------------------------- Life cycle

void UALKMatchHUD::NativeConstruct()
{
	Super::NativeConstruct();

	if (UALKOnlineSubsystem* Online = UALKOnlineSubsystem::Get(this))
	{
		if (!ChatHandle.IsValid())
		{
			ChatHandle = Online->OnChatMessageNative.AddUObject(this, &ThisClass::HandleChatMessage);
		}
		if (!TalkingHandle.IsValid())
		{
			TalkingHandle = Online->OnPlayerTalkingNative.AddUObject(this, &ThisClass::SetPlayerTalking);
		}
		// What was said before the overlay came up is there when the player opens the chat line.
		if (ChatEntries.Num() == 0)
		{
			const TArray<FALKChatMessage>& History = Online->GetChatHistory();
			for (int32 Index = FMath::Max(0, History.Num() - MaxChatLines); Index < History.Num(); ++Index)
			{
				FChatEntry Entry;
				Entry.Message = History[Index];
				Entry.Age = ChatLineSeconds;
				ChatEntries.Add(Entry);
			}
			RebuildChatLines();
		}
	}
	bStartChecked = false;
	LastTimeRemaining = -1.f;
	SlowTimer = 1.f;
	NetTimer = 1.f;
}

void UALKMatchHUD::NativeDestruct()
{
	if (bChatOpen)
	{
		CloseChat(false);
	}
	UnbindAll();
	Super::NativeDestruct();
}

void UALKMatchHUD::UnbindAll()
{
	if (UALKOnlineSubsystem* Online = UALKOnlineSubsystem::Get(this))
	{
		Online->OnChatMessageNative.Remove(ChatHandle);
		Online->OnPlayerTalkingNative.Remove(TalkingHandle);
	}
	ChatHandle.Reset();
	TalkingHandle.Reset();
	if (AALKLobbyGameState* GameState = BoundGameState.Get())
	{
		GameState->OnFeedEventNative.Remove(FeedHandle);
	}
	BoundGameState.Reset();
	FeedHandle.Reset();
	if (UALKPlayerNetComponent* NetComponent = BoundNetComponent.Get())
	{
		NetComponent->OnFeedEventNative.Remove(PrivateFeedHandle);
	}
	BoundNetComponent.Reset();
	PrivateFeedHandle.Reset();
}

AALKLobbyGameState* UALKMatchHUD::ResolveGameState()
{
	const UWorld* World = GetWorld();
	// The game state arrives a moment after a level change: look again until it is there.
	AALKLobbyGameState* GameState = World ? World->GetGameState<AALKLobbyGameState>() : nullptr;
	if (GameState != BoundGameState.Get())
	{
		if (AALKLobbyGameState* Old = BoundGameState.Get())
		{
			Old->OnFeedEventNative.Remove(FeedHandle);
		}
		FeedHandle.Reset();
		BoundGameState = GameState;
		if (GameState)
		{
			FeedHandle = GameState->OnFeedEventNative.AddUObject(this, &ThisClass::AddFeedEvent);
		}
	}
	return GameState;
}

void UALKMatchHUD::ResolveNetComponent()
{
	const UALKOnlineSubsystem* Online = UALKOnlineSubsystem::Get(this);
	UALKPlayerNetComponent* NetComponent = Online ? Online->GetLocalNetComponent() : nullptr;
	if (NetComponent != BoundNetComponent.Get())
	{
		if (UALKPlayerNetComponent* Old = BoundNetComponent.Get())
		{
			Old->OnFeedEventNative.Remove(PrivateFeedHandle);
		}
		PrivateFeedHandle.Reset();
		BoundNetComponent = NetComponent;
		if (NetComponent)
		{
			// Feed lines meant for this player only.
			PrivateFeedHandle = NetComponent->OnFeedEventNative.AddUObject(this, &ThisClass::AddFeedEvent);
		}
	}
}

AALKLobbyPlayerState* UALKMatchHUD::GetLocalLobbyPlayer() const
{
	const APlayerController* Controller = GetOwningPlayer();
	return Controller ? Controller->GetPlayerState<AALKLobbyPlayerState>() : nullptr;
}

void UALKMatchHUD::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	AALKLobbyGameState* GameState = ResolveGameState();
	UpdateScoreStrip(GameState);
	UpdateFeed(InDeltaTime);
	UpdateChat(InDeltaTime);
	UpdateAnnouncement(InDeltaTime);
	UpdateScoreboard();

	// The parts that read slow-changing things look four times a second, the network stats once.
	SlowTimer += InDeltaTime;
	if (SlowTimer >= 0.25f)
	{
		SlowTimer = 0.f;
		UpdateSlowParts();
	}
	NetTimer += InDeltaTime;
	if (NetTimer >= 1.f)
	{
		NetTimer = 0.f;
		UpdateNetStats();
	}
}

void UALKMatchHUD::UpdateSlowParts()
{
	using namespace ALKMatchHUDPrivate;
	ResolveNetComponent();

	const UGameInstance* GameInstance = GetGameInstance();
	if (const UALKSettingsSubsystem* Settings = GameInstance ? GameInstance->GetSubsystem<UALKSettingsSubsystem>() : nullptr)
	{
		// The HUD opacity setting covers the match overlay too.
		const float Opacity = FMath::Clamp(Settings->GetHudOpacity(), 0.f, 1.f);
		if (!FMath::IsNearlyEqual(GetRenderOpacity(), Opacity))
		{
			SetRenderOpacity(Opacity);
		}
	}

	SetShown(FeedBox, bShowFeed);
	SetShown(VoiceBox, bShowVoiceList);

	// Text chat turned off (or switched off for this overlay) hides the whole chat.
	const UALKOnlineSubsystem* Online = UALKOnlineSubsystem::Get(this);
	const bool bChatAllowed = bShowChat && Online && Online->IsTextChatEnabled();
	if (!bChatAllowed && bChatOpen)
	{
		CloseChat(false);
	}
	SetShown(ChatPanel, bChatAllowed, ESlateVisibility::SelfHitTestInvisible);

	AALKLobbyGameState* GameState = BoundGameState.Get();
	if (GameState && GameState->TeamCount <= 0)
	{
		UpdateFreeForAllBlocks(GameState);
	}
	UpdateSpectatorBar();
}

// ---------------------------------------------------------------- Score strip

void UALKMatchHUD::RebuildTeamBlocks(int32 TeamCount)
{
	BuiltTeamCount = TeamCount;
	TeamBlocks.Reset();
	if (!TeamBlocksLeft || !TeamBlocksRight || !WidgetTree)
	{
		return;
	}
	TeamBlocksLeft->ClearChildren();
	TeamBlocksRight->ClearChildren();

	const UALKOnlineConfig* Config = UALKDeveloperSettings::GetOnlineConfig();
	// A free for all has two blocks as well: your rank on the left, the leader's score on the right.
	const int32 BlockCount = TeamCount > 0 ? TeamCount : 2;
	for (int32 Index = 0; Index < BlockCount; ++Index)
	{
		FTeamBlock Block;
		UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		Text->SetJustification(ETextJustify::Center);
		UALKFrame* Frame = WidgetTree->ConstructWidget<UALKFrame>(UALKFrame::StaticClass());
		Frame->SetPadding(FMargin(10.f, 0.f));
		Frame->SetHorizontalAlignment(HAlign_Center);
		Frame->SetVerticalAlignment(VAlign_Center);
		Frame->SetContent(Text);
		USizeBox* BlockSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		BlockSize->SetMinDesiredWidth(100.f);
		BlockSize->SetMinDesiredHeight(48.f);
		BlockSize->SetContent(Frame);
		FALKWidgetBuilder::AddToHorizontalBox(Index == 0 ? TeamBlocksLeft.Get() : TeamBlocksRight.Get(), BlockSize, FMargin(Index > 1 ? 6.f : 0.f, 0.f, 0.f, 0.f), HAlign_Fill, VAlign_Fill);
		Block.Frame = Frame;
		Block.Text = Text;
		Block.Color = TeamCount > 0 ? Config->GetTeamColor(Index) : FLinearColor::Transparent;
		TeamBlocks.Add(Block);
	}
	if (const UALKTheme* Theme = GetTheme())
	{
		StyleTeamBlocks(*Theme);
	}
}

void UALKMatchHUD::StyleTeamBlocks(const UALKTheme& Theme)
{
	using namespace ALKMatchHUDPrivate;
	for (int32 Index = 0; Index < TeamBlocks.Num(); ++Index)
	{
		const FTeamBlock& Block = TeamBlocks[Index];
		FLinearColor Fill = Block.Color.CopyWithNewOpacity(0.92f);
		// The number is drawn dark on a light team colour and light on a dark one.
		FLinearColor TextColor = Block.Color.GetLuminance() > 0.3f ? Theme.AccentContrast : FLinearColor::White;
		if (BuiltTeamCount <= 0)
		{
			Fill = Index == 0 ? Theme.PanelRaised : Theme.Accent;
			TextColor = Index == 0 ? Theme.TextPrimary : Theme.AccentContrast;
		}
		if (UALKFrame* Frame = Block.Frame.Get())
		{
			FALKFrameStyle BlockFrame = Theme.MakeChipFrame(Fill, FLinearColor::Transparent);
			BlockFrame.Radius = FMath::Max(2.f, Theme.CornerRadius - 4.f);
			Frame->SetFrameStyle(BlockFrame);
		}
		if (UTextBlock* Text = Block.Text.Get())
		{
			FALKWidgetBuilder::SetTextFont(Text, Scaled(Theme.HeadingFont, 0.72f));
			Text->SetColorAndOpacity(FSlateColor(TextColor));
		}
	}
}

void UALKMatchHUD::UpdateScoreStrip(AALKLobbyGameState* GameState)
{
	using namespace ALKMatchHUDPrivate;
	const bool bInMatch = GameState && GameState->Phase == EALKLobbyPhase::InMatch;
	SetShown(ScoreStrip, bShowScoreStrip && bInMatch);
	if (!bInMatch)
	{
		return;
	}

	if (GameState->TeamCount != BuiltTeamCount)
	{
		RebuildTeamBlocks(GameState->TeamCount);
		SlowTimer = 1.f;
	}
	if (GameState->TeamCount > 0)
	{
		for (int32 Index = 0; Index < TeamBlocks.Num(); ++Index)
		{
			FTeamBlock& Block = TeamBlocks[Index];
			const int32 Score = GameState->GetTeamScore(Index);
			if (Block.ShownValue != Score)
			{
				Block.ShownValue = Score;
				if (UTextBlock* Text = Block.Text.Get())
				{
					Text->SetText(FText::AsNumber(Score));
				}
			}
		}
	}

	// The clock counts down to the time limit, or up when the match has none.
	const float Remaining = GameState->GetMatchTimeRemaining();
	const bool bCountsDown = Remaining >= 0.f;
	const int32 Seconds = bCountsDown ? FMath::CeilToInt(Remaining) : GameState->ElapsedTime;
	if (ClockText && Seconds != ShownClockSeconds)
	{
		ShownClockSeconds = Seconds;
		ClockText->SetText(UALKScoreboard::FormatClock(static_cast<float>(Seconds)));
		if (const UALKTheme* Theme = GetTheme())
		{
			// The last half minute stands out.
			ClockText->SetColorAndOpacity(FSlateColor(bCountsDown && Seconds <= 30 ? Theme->Warning : Theme->TextPrimary));
		}
	}
	const FText ModeName = UALKDeveloperSettings::GetOnlineConfig()->GetModeName(GameState->ModeId);
	if (ModeText && (ShownModeId != GameState->ModeId || ModeText->GetText().IsEmpty()))
	{
		ShownModeId = GameState->ModeId;
		ModeText->SetText(ModeName);
	}

	// The overlay's own announcements.
	if (!bStartChecked)
	{
		bStartChecked = true;
		if (GameState->ElapsedTime <= 10)
		{
			ShowAnnouncement(LOCTEXT("HudMatchStarted", "Match started"), ModeName, 3.f);
		}
	}
	if (bCountsDown)
	{
		if (LastTimeRemaining > 60.f && Remaining <= 60.f && Remaining > 0.f)
		{
			ShowAnnouncement(LOCTEXT("HudFinalMinute", "Final minute"), FText::GetEmpty(), 3.f);
		}
		LastTimeRemaining = Remaining;
	}
}

void UALKMatchHUD::UpdateFreeForAllBlocks(AALKLobbyGameState* GameState)
{
	if (TeamBlocks.Num() < 2 || BuiltTeamCount > 0)
	{
		return;
	}
	TArray<AALKLobbyPlayerState*> Players = GameState->GetPlayersOnTeam(INDEX_NONE);
	UALKScoreboard::SortPlayersByScore(Players);
	const int32 Rank = Players.IndexOfByKey(GetLocalLobbyPlayer()) + 1;
	const int32 LeaderScore = Players.Num() > 0 ? FMath::RoundToInt(Players[0]->GetScore()) : 0;
	if (UTextBlock* RankText = TeamBlocks[0].Text.Get())
	{
		// A spectator has no rank.
		RankText->SetText(Rank > 0 ? FText::Format(LOCTEXT("HudRank", "#{0}"), FText::AsNumber(Rank)) : LOCTEXT("HudNoRank", "-"));
	}
	if (UTextBlock* LeaderText = TeamBlocks[1].Text.Get())
	{
		LeaderText->SetText(FText::AsNumber(LeaderScore));
	}
}

// ---------------------------------------------------------------- Feed

void UALKMatchHUD::AddFeedEvent(const FALKFeedEvent& Event)
{
	const UALKTheme* Theme = GetTheme();
	if (!bShowFeed || !FeedBox || !WidgetTree || !Theme)
	{
		return;
	}
	UWidget* Line = MakeFeedLine(Event, *Theme);
	if (!Line)
	{
		return;
	}
	FALKWidgetBuilder::AddToVerticalBox(FeedBox, Line, FMargin(0.f, 0.f, 0.f, 8.f), HAlign_Right);
	FFeedLine Entry;
	Entry.Widget = Line;
	FeedLines.Add(Entry);

	// Only the newest few stay.
	while (FeedLines.Num() > FMath::Max(1, MaxFeedLines))
	{
		if (UWidget* Oldest = FeedLines[0].Widget.Get())
		{
			Oldest->RemoveFromParent();
		}
		FeedLines.RemoveAt(0);
	}
}

UWidget* UALKMatchHUD::MakeFeedLine(const FALKFeedEvent& Event, const UALKTheme& Theme)
{
	using namespace ALKMatchHUDPrivate;
	const UALKOnlineSubsystem* Online = UALKOnlineSubsystem::Get(this);
	const UALKOnlineConfig* Config = UALKDeveloperSettings::GetOnlineConfig();
	const AALKLobbyPlayerState* Local = GetLocalLobbyPlayer();
	const int32 LocalIndex = Local ? Local->GetStableIndex() : INDEX_NONE;
	const FString LocalName = Local ? Local->GetPlayerName() : FString();

	auto IsLocal = [LocalIndex, &LocalName](const FString& Name, int32 StableIndex)
	{
		return StableIndex >= 0 && StableIndex == LocalIndex && Name == LocalName;
	};
	// Names go through the streamer mode filter; the local player's own name is never hidden.
	auto ShownName = [Online, &IsLocal](const FString& Name, int32 StableIndex) -> FString
	{
		return Name.IsEmpty() || !Online || IsLocal(Name, StableIndex) ? Name : Online->GetDisplayName(Name, StableIndex);
	};
	auto TeamTint = [Config, &Theme](int32 TeamId)
	{
		return TeamId >= 0 ? Mix(Config->GetTeamColor(TeamId), Theme.TextPrimary, 0.35f) : Theme.TextPrimary;
	};

	const FString Actor = ShownName(Event.ActorName, Event.ActorStableIndex);
	const FString Target = ShownName(Event.TargetName, Event.TargetStableIndex);
	// Lines about the local player stand out.
	const bool bAboutLocal = IsLocal(Event.ActorName, Event.ActorStableIndex) || IsLocal(Event.TargetName, Event.TargetStableIndex);

	UALKFrame* Frame = WidgetTree->ConstructWidget<UALKFrame>(UALKFrame::StaticClass());
	Frame->SetPadding(FMargin(14.f, 7.f, 14.f, 8.f));
	Frame->SetVisibility(ESlateVisibility::HitTestInvisible);
	FALKFrameStyle LineFrame = Theme.MakeChipFrame(bAboutLocal ? Theme.PanelRaised.CopyWithNewOpacity(0.96f) : Theme.Panel.CopyWithNewOpacity(0.8f),
		bAboutLocal ? Theme.Accent.CopyWithNewOpacity(0.7f) : Theme.PanelOutline.CopyWithNewOpacity(Theme.PanelOutline.A * 0.8f));
	LineFrame.Radius = Theme.CornerRadius;
	Frame->SetFrameStyle(LineFrame);

	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Frame->SetContent(Row);

	const FSlateFontInfo NameFont = Scaled(Theme.SubheadingFont, 0.77f);
	const FSlateFontInfo PlainFont = Scaled(Theme.BodyFont, 0.94f);
	auto AddText = [this, Row](const FString& String, const FLinearColor& Color, const FSlateFontInfo& Font)
	{
		UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		Text->SetText(FText::FromString(String));
		FALKWidgetBuilder::SetTextFont(Text, Font);
		Text->SetColorAndOpacity(FSlateColor(Color));
		FALKWidgetBuilder::AddToHorizontalBox(Row, Text, FMargin(), HAlign_Left, VAlign_Center);
	};
	auto AddIcon = [this, Row, &Theme](EALKIcon Icon, const FMargin& Margin)
	{
		UALKIconWidget* IconWidget = WidgetTree->ConstructWidget<UALKIconWidget>(UALKIconWidget::StaticClass());
		IconWidget->IconSize = 19.f;
		IconWidget->SetIcon(Icon, Theme.TextPrimary);
		FALKWidgetBuilder::AddToHorizontalBox(Row, IconWidget, Margin, HAlign_Left, VAlign_Center);
	};
	const FMargin Between(10.f, 0.f);
	const FMargin Leading(0.f, 0.f, 10.f, 0.f);

	if (Event.EventId == JoinEvent || Event.EventId == LeaveEvent)
	{
		const FText Line = FText::Format(Event.EventId == JoinEvent ? LOCTEXT("HudFeedJoined", "{0} joined") : LOCTEXT("HudFeedLeft", "{0} left"), FText::FromString(Actor));
		AddIcon(EALKIcon::User, Leading);
		AddText(Line.ToString(), Theme.TextSecondary, PlainFont);
	}
	else if (!Target.IsEmpty())
	{
		// Who did it, what, to whom: "Kestrel (+) tin_can".
		if (!Actor.IsEmpty())
		{
			AddText(Actor, TeamTint(Event.ActorTeam), NameFont);
		}
		AddIcon(Event.EventId == CaptureEvent ? EALKIcon::Flag : EALKIcon::Target, Actor.IsEmpty() ? Leading : Between);
		AddText(Target, TeamTint(Event.TargetTeam), NameFont);
	}
	else
	{
		// A line from the game: the actor in the team colour, an icon, then what happened ("Juno_R [flag] captured the point").
		FString Line = Event.Text.ToString();
		const EALKIcon Icon = Event.EventId == KillEvent ? EALKIcon::Target : EALKIcon::Flag;
		if (!Event.ActorName.IsEmpty())
		{
			// The text may or may not start with the actor's name; either way the name is shown once, streamer-safe.
			if (Line.StartsWith(Event.ActorName))
			{
				Line = Line.Mid(Event.ActorName.Len()).TrimStart();
			}
			if (Actor != Event.ActorName)
			{
				Line.ReplaceInline(*Event.ActorName, *Actor);
			}
			AddText(Actor, TeamTint(Event.ActorTeam), NameFont);
			if (!Line.IsEmpty())
			{
				AddIcon(Icon, Between);
				AddText(Line, Theme.TextPrimary, PlainFont);
			}
		}
		else
		{
			if (Line.IsEmpty())
			{
				return nullptr;
			}
			AddIcon(Icon, Leading);
			AddText(Line, Theme.TextPrimary, PlainFont);
		}
	}
	return Frame;
}

void UALKMatchHUD::UpdateFeed(float DeltaTime)
{
	for (int32 Index = FeedLines.Num() - 1; Index >= 0; --Index)
	{
		FFeedLine& Line = FeedLines[Index];
		Line.Age += DeltaTime;
		UWidget* Widget = Line.Widget.Get();
		if (!Widget || Line.Age >= FeedLineSeconds)
		{
			if (Widget)
			{
				Widget->RemoveFromParent();
			}
			FeedLines.RemoveAt(Index);
			continue;
		}
		Widget->SetRenderOpacity(ALKMatchHUDPrivate::TimedOpacity(Line.Age, FeedLineSeconds, 0.15f, 0.6f));
	}
}

// ---------------------------------------------------------------- Voice

void UALKMatchHUD::SetPlayerTalking(const FString& UserId, bool bTalking)
{
	if (UserId.IsEmpty())
	{
		return;
	}
	const bool bWasTalking = Talkers.Contains(UserId);
	if (bTalking == bWasTalking)
	{
		return;
	}
	if (bTalking)
	{
		Talkers.Add(UserId);
	}
	else
	{
		Talkers.Remove(UserId);
	}
	RebuildVoiceList();
}

void UALKMatchHUD::RebuildVoiceList()
{
	using namespace ALKMatchHUDPrivate;
	if (!VoiceBox || !WidgetTree)
	{
		return;
	}
	VoiceBox->ClearChildren();

	const UALKTheme* Theme = GetTheme();
	const UALKOnlineSubsystem* Online = UALKOnlineSubsystem::Get(this);
	if (!Theme || !Online)
	{
		return;
	}
	const UALKOnlineConfig* Config = UALKDeveloperSettings::GetOnlineConfig();
	const AALKLobbyGameState* GameState = BoundGameState.Get();
	const FString LocalId = Online->GetLocalUserId();

	for (const FString& TalkerId : Talkers)
	{
		const bool bLocal = TalkerId == LocalId;
		FString Name;
		FLinearColor TeamColor = FLinearColor::Transparent;
		if (const AALKLobbyPlayerState* Player = GameState ? GameState->FindPlayerByUserId(TalkerId) : nullptr)
		{
			Name = UALKScoreboard::GetShownPlayerName(Player);
			if (Player->TeamId >= 0)
			{
				TeamColor = Config->GetTeamColor(Player->TeamId);
			}
		}
		else if (bLocal)
		{
			Name = Online->GetLocalDisplayName();
		}
		else
		{
			// Not in this game: someone of the party.
			for (const FALKPartyMember& Member : Online->GetPartyMembers())
			{
				if (Member.UserId == TalkerId)
				{
					Name = Online->GetDisplayName(Member.DisplayName, INDEX_NONE);
				}
			}
		}
		if (Name.IsEmpty())
		{
			continue;
		}

		UALKFrame* Frame = WidgetTree->ConstructWidget<UALKFrame>(UALKFrame::StaticClass());
		Frame->SetPadding(FMargin(12.f, 6.f, 16.f, 7.f));
		FALKFrameStyle LineFrame = Theme->MakeChipFrame(Theme->Panel.CopyWithNewOpacity(0.8f), Theme->PanelOutline.CopyWithNewOpacity(Theme->PanelOutline.A * 0.8f));
		LineFrame.Radius = Theme->CornerRadius;
		Frame->SetFrameStyle(LineFrame);
		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		Frame->SetContent(Row);

		// The local player's own line shows the microphone: it is on while they transmit.
		UALKIconWidget* IconWidget = WidgetTree->ConstructWidget<UALKIconWidget>(UALKIconWidget::StaticClass());
		IconWidget->IconSize = 21.f;
		IconWidget->SetIcon(bLocal ? EALKIcon::Microphone : EALKIcon::Speaker, Theme->Success);
		FALKWidgetBuilder::AddToHorizontalBox(Row, IconWidget, FMargin(0.f, 0.f, 10.f, 0.f), HAlign_Left, VAlign_Center);
		if (TeamColor.A > 0.f)
		{
			UImage* TeamBar = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
			TeamBar->SetBrush(UALKTheme::MakeSolidBrush(TeamColor));
			USizeBox* BarSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
			BarSize->SetWidthOverride(4.f);
			BarSize->SetHeightOverride(20.f);
			BarSize->SetContent(TeamBar);
			FALKWidgetBuilder::AddToHorizontalBox(Row, BarSize, FMargin(0.f, 0.f, 10.f, 0.f), HAlign_Left, VAlign_Center);
		}
		UTextBlock* NameText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		NameText->SetText(FText::FromString(Name));
		FALKWidgetBuilder::SetTextFont(NameText, Scaled(Theme->SubheadingFont, 0.77f));
		NameText->SetColorAndOpacity(FSlateColor(Theme->TextPrimary));
		FALKWidgetBuilder::AddToHorizontalBox(Row, NameText, FMargin(), HAlign_Left, VAlign_Center);

		FALKWidgetBuilder::AddToVerticalBox(VoiceBox, Frame, FMargin(0.f, 0.f, 0.f, 8.f), HAlign_Left);
	}
}

// ---------------------------------------------------------------- Chat

void UALKMatchHUD::HandleChatMessage(const FALKChatMessage& Message)
{
	FChatEntry Entry;
	Entry.Message = Message;
	ChatEntries.Add(Entry);
	if (ChatEntries.Num() > ALKMatchHUDPrivate::ChatMemory)
	{
		ChatEntries.RemoveAt(0, ChatEntries.Num() - ALKMatchHUDPrivate::ChatMemory);
	}
	RebuildChatLines();
}

void UALKMatchHUD::RebuildChatLines()
{
	if (!ChatLines || !WidgetTree)
	{
		return;
	}
	ChatLines->ClearChildren();
	const UALKTheme* Theme = GetTheme();
	const int32 First = FMath::Max(0, ChatEntries.Num() - FMath::Max(1, MaxChatLines));
	for (int32 Index = 0; Index < ChatEntries.Num(); ++Index)
	{
		FChatEntry& Entry = ChatEntries[Index];
		Entry.Widget.Reset();
		if (Index < First || !Theme)
		{
			continue;
		}
		if (UWidget* Line = MakeChatLine(Entry.Message, *Theme))
		{
			FALKWidgetBuilder::AddToVerticalBox(ChatLines, Line, FMargin(0.f, 3.f));
			Entry.Widget = Line;
		}
	}
	UpdateChat(0.f);
}

UWidget* UALKMatchHUD::MakeChatLine(const FALKChatMessage& Message, const UALKTheme& Theme)
{
	using namespace ALKMatchHUDPrivate;
	const UALKOnlineSubsystem* Online = UALKOnlineSubsystem::Get(this);
	const UALKOnlineConfig* Config = UALKDeveloperSettings::GetOnlineConfig();

	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Row->SetVisibility(ESlateVisibility::HitTestInvisible);
	const FSlateFontInfo TextFont = Scaled(Theme.BodyFont, 0.94f);

	auto AddText = [this, Row, &Theme](const FText& Text, const FSlateFontInfo& Font, const FLinearColor& Color, const FMargin& Margin, bool bFill) -> UTextBlock*
	{
		UTextBlock* Block = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		Block->SetText(Text);
		FALKWidgetBuilder::SetTextFont(Block, Font);
		Block->SetColorAndOpacity(FSlateColor(Color));
		AddShadow(Block, Theme);
		FALKWidgetBuilder::AddToHorizontalBox(Row, Block, Margin, bFill ? HAlign_Fill : HAlign_Left, VAlign_Top, bFill ? 1.f : 0.f);
		return Block;
	};

	if (Message.Channel == EALKChatChannel::System)
	{
		// Joins, leaves and notices: one quiet line.
		AddText(FText::FromString(Message.Text), TextFont, Theme.TextMuted, FMargin(), true)->SetAutoWrapText(true);
		return Row;
	}

	FLinearColor ChannelColor = Theme.TextMuted;
	FText ChannelName = LOCTEXT("HudChatAll", "All");
	switch (Message.Channel)
	{
	case EALKChatChannel::Team:
		ChannelColor = Message.TeamId >= 0 ? Config->GetTeamColor(Message.TeamId) : Theme.AccentAlt;
		ChannelName = LOCTEXT("HudChatTeam", "Team");
		break;
	case EALKChatChannel::Party:
		ChannelColor = Theme.GetAccentTextColor();
		ChannelName = LOCTEXT("HudChatParty", "Party");
		break;
	case EALKChatChannel::Whisper:
		ChannelColor = Theme.AccentAlt;
		ChannelName = LOCTEXT("HudChatWhisper", "Whisper");
		break;
	default:
		break;
	}

	UTextBlock* ChannelText = AddText(ChannelName, Scaled(Theme.DataFont, 0.86f), ChannelColor, FMargin(0.f, 4.f, 10.f, 0.f), false);
	ChannelText->SetTextTransformPolicy(ETextTransformPolicy::ToUpper);

	// The sender's own name is shown as it is; other names go through the streamer mode filter.
	const bool bOwnLine = Online && Message.SenderId == Online->GetLocalUserId();
	const FString Sender = !Online || bOwnLine ? Message.SenderName : Online->GetDisplayName(Message.SenderName, Message.SenderStableIndex);
	UTextBlock* SenderText = AddText(FText::FromString(Sender), Scaled(Theme.SubheadingFont, 0.77f), Mix(ChannelColor, Theme.TextPrimary, 0.55f), FMargin(0.f, 0.f, 10.f, 0.f), false);
	UTextBlock* MessageText = AddText(FText::FromString(Message.Text), TextFont, Theme.TextPrimary, FMargin(), true);
	MessageText->SetAutoWrapText(true);
	// The channel, the name (the display face) and the message stand on one baseline.
	FALKWidgetBuilder::AlignBaselines({ ChannelText, SenderText, MessageText });
	return Row;
}

void UALKMatchHUD::UpdateChat(float DeltaTime)
{
	float Strongest = 0.f;
	for (FChatEntry& Entry : ChatEntries)
	{
		Entry.Age += DeltaTime;
		UWidget* Line = Entry.Widget.Get();
		if (!Line)
		{
			continue;
		}
		// Every remembered line shows while typing; otherwise a line fades a while after it arrived.
		const float Opacity = bChatOpen ? 1.f : ALKMatchHUDPrivate::TimedOpacity(Entry.Age, ChatLineSeconds, 0.01f, 0.8f);
		Line->SetRenderOpacity(Opacity);
		ALKMatchHUDPrivate::SetShown(Line, Opacity > 0.f);
		Strongest = FMath::Max(Strongest, Opacity);
	}
	// The panel behind the lines fades with the last of them.
	const float Backing = bChatOpen ? 1.f : Strongest;
	if (!FMath::IsNearlyEqual(Backing, ChatBacking, 0.02f) || (Backing <= 0.f) != (ChatBacking <= 0.f))
	{
		ChatBacking = Backing;
		ApplyChatLook();
	}

	// The keyboard goes to the chat line as soon as its widget can take it.
	if (bChatOpen && ChatFocusTries > 0 && ChatField)
	{
		if (ChatField->IsTyping())
		{
			ChatFocusTries = 0;
		}
		else
		{
			--ChatFocusTries;
			ChatField->FocusInput();
		}
	}
}

void UALKMatchHUD::ApplyChatLook()
{
	using namespace ALKMatchHUDPrivate;
	const UALKTheme* Theme = GetTheme();
	if (!Theme)
	{
		return;
	}
	if (UALKFrame* ChatFrame = Cast<UALKFrame>(ChatPanel))
	{
		// A full panel while typing; otherwise a fainter one that stays behind the lines still up and goes with them.
		FALKFrameStyle Look = MakeHudFrame(*Theme, 0.78f);
		if (!bChatOpen)
		{
			Look.Fill = Theme->Panel.CopyWithNewOpacity(Theme->Panel.A * 0.5f * FMath::Clamp(ChatBacking, 0.f, 1.f));
			Look.Outline = FLinearColor::Transparent;
			Look.Shadow = FLinearColor::Transparent;
		}
		ChatFrame->SetFrameStyle(Look);
	}
	SetShown(ChatEntryRow, bChatOpen, ESlateVisibility::SelfHitTestInvisible);
	if (ChatField)
	{
		ChatField->SetVisibility(bChatOpen ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (ChatChannelChip)
	{
		const AALKLobbyPlayerState* Local = GetLocalLobbyPlayer();
		const FLinearColor TeamColor = bChatTeam && Local && Local->TeamId >= 0 ? UALKDeveloperSettings::GetOnlineConfig()->GetTeamColor(Local->TeamId) : Theme->TextMuted;
		FALKWidgetBuilder::SetChipText(ChatChannelChip, bChatTeam ? LOCTEXT("HudChatTeam", "Team") : LOCTEXT("HudChatAll", "All"));
		FALKWidgetBuilder::StyleChip(ChatChannelChip, *Theme, bChatTeam ? TeamColor.CopyWithNewOpacity(0.25f) : Theme->PanelRaised, FLinearColor::Transparent,
			bChatTeam ? Mix(TeamColor, Theme->TextPrimary, 0.4f) : Theme->TextMuted);
	}
}

void UALKMatchHUD::OpenChat_Implementation(bool bTeamChat)
{
	const UALKOnlineSubsystem* Online = UALKOnlineSubsystem::Get(this);
	if (bChatOpen || !bShowChat || !ChatField || !Online || !Online->IsTextChatEnabled())
	{
		return;
	}
	// Team chat needs a team; without one the line goes to everyone.
	const AALKLobbyPlayerState* Local = GetLocalLobbyPlayer();
	bChatTeam = bTeamChat && Local && Local->TeamId >= 0;
	bChatOpen = true;

	ChatField->SetText(FText::GetEmpty());
	ChatField->SetMaxLength(UALKDeveloperSettings::GetOnlineConfig()->ChatMaxLength);
	ChatField->SetHintText(bChatTeam ? LOCTEXT("HudChatHintTeam", "Message your team") : LOCTEXT("HudChatHintAll", "Message everyone"));
	ALKMatchHUDPrivate::SetShown(ChatPanel, true, ESlateVisibility::SelfHitTestInvisible);
	ApplyChatLook();
	UpdateChat(0.f);

	// The field was collapsed a moment ago: keep asking for the keyboard until it has it.
	ChatFocusTries = 8;
	ChatField->FocusInput();
}

bool UALKMatchHUD::IsChatOpen_Implementation() const
{
	return bChatOpen;
}

void UALKMatchHUD::CloseChat(bool bSend)
{
	if (!bChatOpen)
	{
		return;
	}
	const FString Text = ChatField ? ChatField->GetText().ToString().TrimStartAndEnd() : FString();
	const bool bHadKeyboard = ChatField && ChatField->IsTyping();
	// Closed first: giving the keyboard back makes the field report "lost focus", which must not close it again.
	bChatOpen = false;
	ChatFocusTries = 0;

	if (bSend && !Text.IsEmpty())
	{
		const EALKChatChannel Channel = bChatTeam ? EALKChatChannel::Team : EALKChatChannel::All;
		UALKOnlineSubsystem* Online = UALKOnlineSubsystem::Get(this);
		if (UALKPlayerNetComponent* NetComponent = Online ? Online->GetLocalNetComponent() : nullptr)
		{
			NetComponent->ServerSendChat(Channel, Text, FString());
		}
		else if (Online)
		{
			Online->SendChat(Channel, Text, FString());
		}
	}
	if (ChatField)
	{
		ChatField->SetText(FText::GetEmpty());
	}
	ApplyChatLook();
	UpdateChat(0.f);

	// The keyboard goes back to the game.
	if (bHadKeyboard && FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().SetAllUserFocusToGameViewport();
	}
}

void UALKMatchHUD::HandleChatCommitted(const FText& Text, ETextCommit::Type CommitMethod)
{
	if (bChatOpen)
	{
		// Enter sends; clicking away or anything else just closes.
		CloseChat(CommitMethod == ETextCommit::OnEnter);
	}
}

FReply UALKMatchHUD::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	// Escape while typing closes the chat line and stops there, so it does not open the pause menu.
	if (bChatOpen && InKeyEvent.GetKey() == EKeys::Escape)
	{
		CloseChat(false);
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

// ---------------------------------------------------------------- Network stats, spectator bar, announcement, scoreboard

void UALKMatchHUD::UpdateNetStats()
{
	using namespace ALKMatchHUDPrivate;
	const UGameInstance* GameInstance = GetGameInstance();
	const UALKSettingsSubsystem* Settings = GameInstance ? GameInstance->GetSubsystem<UALKSettingsSubsystem>() : nullptr;
	const UALKOnlineSubsystem* Online = UALKOnlineSubsystem::Get(this);
	const bool bForced = CVarALKHudNetStats.GetValueOnGameThread() > 0;
	const FALKNetStats Stats = Online ? Online->GetNetStats() : FALKNetStats();
	const bool bWanted = bShowNetStats && (bForced || (Settings && Settings->IsNetStatsOverlayEnabled() && Stats.bConnected));
	SetShown(NetStatsPanel, bWanted);
	const UALKTheme* Theme = GetTheme();
	if (!bWanted || !Theme)
	{
		return;
	}

	FNumberFormattingOptions OneDecimal;
	OneDecimal.SetMinimumFractionalDigits(1).SetMaximumFractionalDigits(1);
	if (NetPingText)
	{
		NetPingText->SetText(FText::Format(LOCTEXT("HudNetPingValue", "{0} ms"), FText::AsNumber(Stats.PingMs)));
		NetPingText->SetColorAndOpacity(FSlateColor(Theme->GetPingColor(Stats.PingMs)));
	}
	if (NetLossText)
	{
		NetLossText->SetText(FText::Format(LOCTEXT("HudNetLossValue", "{0} / {1} %"), FText::AsNumber(Stats.PacketLossInPercent, &OneDecimal), FText::AsNumber(Stats.PacketLossOutPercent, &OneDecimal)));
		const float Loss = FMath::Max(Stats.PacketLossInPercent, Stats.PacketLossOutPercent);
		NetLossText->SetColorAndOpacity(FSlateColor(Loss < 1.f ? Theme->Success : (Loss < 5.f ? Theme->Warning : Theme->Danger)));
	}
	if (NetTrafficText)
	{
		NetTrafficText->SetText(FText::Format(LOCTEXT("HudNetTrafficValue", "{0} / {1} kb/s"), FText::AsNumber(Stats.InKbps, &OneDecimal), FText::AsNumber(Stats.OutKbps, &OneDecimal)));
	}
	if (NetServerText)
	{
		NetServerText->SetText(Stats.ServerTickRate > 0 ? FText::Format(LOCTEXT("HudNetServerValue", "{0} fps"), FText::AsNumber(Stats.ServerTickRate)) : LOCTEXT("HudNetUnknown", "-"));
	}
}

void UALKMatchHUD::UpdateSpectatorBar()
{
	using namespace ALKMatchHUDPrivate;
	const AALKLobbyPlayerState* Local = GetLocalLobbyPlayer();
	const bool bSpectating = bShowSpectatorBar && Local && Local->bSpectating;
	SetShown(SpectatorBar, bSpectating);
	if (!bSpectating)
	{
		return;
	}

	// Whose view this is: the player state of the pawn the camera follows.
	const APlayerController* Controller = GetOwningPlayer();
	const APawn* Watched = Controller ? Cast<APawn>(Controller->GetViewTarget()) : nullptr;
	const AALKLobbyPlayerState* WatchedPlayer = Watched ? Watched->GetPlayerState<AALKLobbyPlayerState>() : nullptr;
	if (SpectatorText)
	{
		SpectatorText->SetText(WatchedPlayer && WatchedPlayer != Local
			? FText::Format(LOCTEXT("HudSpectating", "Spectating {0}"), FText::FromString(UALKScoreboard::GetShownPlayerName(WatchedPlayer)))
			: LOCTEXT("HudFreeCamera", "Free camera"));
	}

	// The keys the flow reads for stepping, for the input method in use.
	const ULocalPlayer* LocalPlayer = GetOwningLocalPlayer();
	const UCommonInputSubsystem* Input = UCommonInputSubsystem::Get(LocalPlayer);
	const bool bGamepad = Input && Input->GetCurrentInputType() == ECommonInputType::Gamepad;
	const FALKOnlineKeys& Keys = GetMenuConfig()->OnlineKeys;
	if (SpectatePrevKeyText)
	{
		SpectatePrevKeyText->SetText(FALKInputLabels::GetKeyLabel(bGamepad ? Keys.SpectatePreviousGamepad : Keys.SpectatePrevious, LocalPlayer));
	}
	if (SpectateNextKeyText)
	{
		SpectateNextKeyText->SetText(FALKInputLabels::GetKeyLabel(bGamepad ? Keys.SpectateNextGamepad : Keys.SpectateNext, LocalPlayer));
	}
}

void UALKMatchHUD::SpectateStep_Implementation(int32 Direction)
{
	APlayerController* Controller = GetOwningPlayer();
	const AALKLobbyPlayerState* Local = GetLocalLobbyPlayer();
	if (!Controller || !Local || !Local->bSpectating)
	{
		return;
	}
	if (Direction < 0)
	{
		Controller->ServerViewPrevPlayer();
	}
	else
	{
		Controller->ServerViewNextPlayer();
	}
	// The name follows on the next look.
	SlowTimer = 0.1f;
}

void UALKMatchHUD::ShowAnnouncement_Implementation(const FText& InTitle, const FText& InSubtitle, float Seconds)
{
	if (!bShowAnnouncements || !AnnouncementBox)
	{
		return;
	}
	if (AnnouncementTitle)
	{
		AnnouncementTitle->SetText(InTitle);
	}
	if (AnnouncementSubtitle)
	{
		AnnouncementSubtitle->SetText(InSubtitle);
		ALKMatchHUDPrivate::SetShown(AnnouncementSubtitle, !InSubtitle.IsEmpty());
	}
	AnnouncementAge = 0.f;
	AnnouncementDuration = FMath::Max(0.5f, Seconds);
	AnnouncementBox->SetRenderOpacity(FALKEffects::AreReduced() ? 1.f : 0.f);
	ALKMatchHUDPrivate::SetShown(AnnouncementBox, true);
}

void UALKMatchHUD::UpdateAnnouncement(float DeltaTime)
{
	if (AnnouncementDuration <= 0.f || !AnnouncementBox)
	{
		return;
	}
	AnnouncementAge += DeltaTime;
	if (AnnouncementAge >= AnnouncementDuration)
	{
		AnnouncementDuration = 0.f;
		ALKMatchHUDPrivate::SetShown(AnnouncementBox, false);
		return;
	}
	AnnouncementBox->SetRenderOpacity(ALKMatchHUDPrivate::TimedOpacity(AnnouncementAge, AnnouncementDuration, 0.25f, 0.5f));
}

void UALKMatchHUD::SetScoreboardVisible_Implementation(bool bShow)
{
	bScoreboardWanted = bShow;
	UpdateScoreboard();
}

void UALKMatchHUD::UpdateScoreboard()
{
	const bool bWanted = bScoreboardWanted || CVarALKHudScoreboard.GetValueOnGameThread() > 0;
	if (bWanted == bScoreboardShown)
	{
		return;
	}
	bScoreboardShown = bWanted;
	ALKMatchHUDPrivate::SetShown(ScoreboardDim, bWanted);
	ALKMatchHUDPrivate::SetShown(Scoreboard, bWanted);
	if (bWanted && Scoreboard)
	{
		// Up to date in the frame it appears.
		Scoreboard->SetInteractive(false);
		Scoreboard->Refresh();
	}
}

// ---------------------------------------------------------------- Console (development builds)

#if !UE_BUILD_SHIPPING

namespace ALKMatchHUDConsole
{
	static UALKMatchHUD* FindHUD(UWorld* World)
	{
		const UALKOnlineFlowSubsystem* Flow = UALKOnlineFlowSubsystem::Get(World);
		return Flow ? Cast<UALKMatchHUD>(Flow->GetMatchHUD()) : nullptr;
	}

	static void Chat(const TArray<FString>& Args, UWorld* World)
	{
		if (UALKMatchHUD* HUD = FindHUD(World))
		{
			HUD->OpenChat(Args.Num() > 0 && Args[0].Equals(TEXT("team"), ESearchCase::IgnoreCase));
		}
	}

	static void Announce(const TArray<FString>& Args, UWorld* World)
	{
		if (UALKMatchHUD* HUD = FindHUD(World))
		{
			HUD->ShowAnnouncement(FText::FromString(Args.Num() > 0 ? FString::Join(Args, TEXT(" ")) : FString(TEXT("Announcement"))), FText::GetEmpty(), 4.f);
		}
	}

	static void Talk(const TArray<FString>& Args, UWorld* World)
	{
		UALKMatchHUD* HUD = FindHUD(World);
		const AALKLobbyGameState* GameState = World ? World->GetGameState<AALKLobbyGameState>() : nullptr;
		if (!HUD || !GameState)
		{
			return;
		}
		// The first players of the match start talking; 0 makes everyone quiet again.
		const int32 Count = Args.Num() > 0 ? FCString::Atoi(*Args[0]) : 2;
		int32 Index = 0;
		for (const AALKLobbyPlayerState* Player : GameState->GetLobbyPlayers())
		{
			if (Player)
			{
				HUD->SetPlayerTalking(Player->GetUserId(), Index++ < Count);
			}
		}
	}

	static void Feed(const TArray<FString>& Args, UWorld* World)
	{
		UALKMatchHUD* HUD = FindHUD(World);
		const AALKLobbyGameState* GameState = World ? World->GetGameState<AALKLobbyGameState>() : nullptr;
		const TArray<AALKLobbyPlayerState*> Players = GameState ? GameState->GetLobbyPlayers() : TArray<AALKLobbyPlayerState*>();
		if (!HUD || Players.Num() == 0 || !Players[0])
		{
			return;
		}
		// A line as a game would send it: the first player of the match did something.
		FALKFeedEvent Event;
		Event.ActorName = Players[0]->GetPlayerName();
		Event.ActorTeam = Players[0]->TeamId;
		Event.ActorStableIndex = Players[0]->GetStableIndex();
		Event.EventId = ALKMatchHUDPrivate::CaptureEvent;
		Event.Text = FText::FromString(Args.Num() > 0 ? FString::Join(Args, TEXT(" ")) : FString(TEXT("captured the point")));
		HUD->AddFeedEvent(Event);
	}

	static void Spectate(const TArray<FString>& Args, UWorld* World)
	{
		const UALKOnlineSubsystem* Online = UALKOnlineSubsystem::Get(World);
		if (UALKPlayerNetComponent* NetComponent = Online ? Online->GetLocalNetComponent() : nullptr)
		{
			NetComponent->ServerSetSpectating(Args.Num() == 0 || Args[0] != TEXT("0"));
		}
	}
}

static FAutoConsoleCommandWithWorldAndArgs GALKHudChatCommand(TEXT("ALK.HUD.Chat"),
	TEXT("Opens the match overlay's chat line. Add 'team' for team chat."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ALKMatchHUDConsole::Chat));

static FAutoConsoleCommandWithWorldAndArgs GALKHudAnnounceCommand(TEXT("ALK.HUD.Announce"),
	TEXT("Shows an announcement on the match overlay: ALK.HUD.Announce <text>."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ALKMatchHUDConsole::Announce));

static FAutoConsoleCommandWithWorldAndArgs GALKHudTalkCommand(TEXT("ALK.HUD.Talk"),
	TEXT("Marks the first <count> players of the match as talking on the match overlay (0 clears)."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ALKMatchHUDConsole::Talk));

static FAutoConsoleCommandWithWorldAndArgs GALKHudFeedCommand(TEXT("ALK.HUD.Feed"),
	TEXT("Adds a line to the match overlay's event feed for the first player of the match: ALK.HUD.Feed <what happened>."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ALKMatchHUDConsole::Feed));

static FAutoConsoleCommandWithWorldAndArgs GALKHudSpectateCommand(TEXT("ALK.HUD.Spectate"),
	TEXT("Makes the local player a spectator (1, the default) or a player again (0), to see the spectator bar."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ALKMatchHUDConsole::Spectate));

#endif // !UE_BUILD_SHIPPING

#undef LOCTEXT_NAMESPACE
