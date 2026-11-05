// Copyright 2026 CodeLikeMe. All Rights Reserved.

#include "OnlineUI/ALKMatchFoundDialog.h"
#include "ALKDeveloperSettings.h"
#include "ALKTheme.h"
#include "Online/ALKOnlineConfig.h"
#include "Online/ALKOnlineSubsystem.h"
#include "Widgets/ALKButton.h"
#include "Widgets/ALKDecor.h"
#include "Widgets/ALKFrame.h"
#include "Widgets/ALKInputLabels.h"
#include "Widgets/ALKPaint.h"
#include "Widgets/ALKThumbnail.h"
#include "Widgets/ALKTitleText.h"
#include "Widgets/ALKWidgetBuilder.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/Image.h"
#include "Components/NativeWidgetHost.h"
#include "Components/Overlay.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Engine/Texture2D.h"
#include "HAL/PlatformInput.h"
#include "Rendering/DrawElements.h"
#include "Widgets/SLeafWidget.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ALKMatchFoundDialog)

#define LOCTEXT_NAMESPACE "AssetproLobbyKit"

namespace ALKMatchFoundDialogPrivate
{
	static constexpr float RingDiameter = 148.f;
	static constexpr float RingWidth = 8.f;
	static constexpr float DotSize = 30.f;
	static constexpr float ButtonHeight = 64.f;
}

/** A ring that empties as the time to answer runs out: a full track with an arc in the accent colour over it. */
class SALKCountdownRing : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SALKCountdownRing) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
	}

	/** How much of the ring is filled, 0 to 1. */
	void SetFraction(float InFraction)
	{
		const float Clamped = FMath::Clamp(InFraction, 0.f, 1.f);
		if (!FMath::IsNearlyEqual(Clamped, Fraction, 0.0005f))
		{
			Fraction = Clamped;
			Invalidate(EInvalidateWidgetReason::Paint);
		}
	}

	/** The ring's track, the arc that runs out, and the disc inside the ring (it keeps the seconds readable over a map's picture). */
	void SetColors(const FLinearColor& InTrack, const FLinearColor& InFill, const FLinearColor& InBack)
	{
		Track = InTrack;
		Fill = InFill;
		Back = InBack;
		Invalidate(EInvalidateWidgetReason::Paint);
	}

	virtual FVector2D ComputeDesiredSize(float) const override
	{
		return FVector2D(ALKMatchFoundDialogPrivate::RingDiameter, ALKMatchFoundDialogPrivate::RingDiameter);
	}

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& Out, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override
	{
		using namespace ALKMatchFoundDialogPrivate;
		const FVector2f Size = FVector2f(Geometry.GetLocalSize());
		const FVector2f Center = Size * 0.5f;
		const float Radius = FMath::Min(Size.X, Size.Y) * 0.5f - 1.f;
		const FLinearColor Tint = InWidgetStyle.GetColorAndOpacityTint();
		// The track's outline lies inside its radius; the arc runs along the middle of it, from the top, clockwise.
		FALKPaint::Circle(Out, LayerId, Geometry, Center, Radius, Back * Tint, Track * Tint, RingWidth);
		if (Fraction > 0.001f)
		{
			FALKPaint::Arc(Out, LayerId + 1, Geometry, Center, Radius - RingWidth * 0.5f, -90.f, -90.f + 360.f * Fraction, Fill * Tint, RingWidth);
		}
		return LayerId + 2;
	}

private:
	FLinearColor Track = FLinearColor(1.f, 1.f, 1.f, 0.2f);
	FLinearColor Fill = FLinearColor::White;
	FLinearColor Back = FLinearColor::Transparent;
	float Fraction = 1.f;
};

UALKMatchFoundDialog::UALKMatchFoundDialog(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bIsModal = true;
}

void UALKMatchFoundDialog::NativeBuildDefaultLayout(UWidgetTree& Tree)
{
	using namespace ALKMatchFoundDialogPrivate;

	UOverlay* Root = FALKWidgetBuilder::Make<UOverlay>(Tree, TEXT("Root"));
	Tree.RootWidget = Root;

	DimImage = FALKWidgetBuilder::Make<UImage>(Tree, TEXT("DimImage"));
	FALKWidgetBuilder::AddToOverlay(Root, DimImage);

	UALKFrame* Frame = FALKWidgetBuilder::Make<UALKFrame>(Tree, TEXT("Panel"));
	Frame->SetPadding(FMargin(44.f, 44.f, 44.f, 42.f));
	Panel = Frame;
	FALKWidgetBuilder::AddToOverlay(Root, FALKWidgetBuilder::WrapInSizeBox(Tree, Frame, 680.f, 0.f), HAlign_Center, VAlign_Center);

	UVerticalBox* Column = FALKWidgetBuilder::Make<UVerticalBox>(Tree, TEXT("PanelColumn"));
	// The map's picture lies across the top of the panel, behind the ring: it ends a little under the ring and over the
	// title (skins without the picture veil show it as a strip with a hard lower edge).
	MapBand = FALKWidgetBuilder::AddPanelBand(Tree, Frame, Column, FMargin(44.f, 44.f, 44.f, 42.f), 44.f + RingDiameter + 14.f, TEXT("MapBand"));

	// The ring with the seconds in it.
	UOverlay* RingBox = FALKWidgetBuilder::Make<UOverlay>(Tree, TEXT("RingBox"));
	RingHost = FALKWidgetBuilder::Make<UNativeWidgetHost>(Tree, TEXT("RingHost"));
	FALKWidgetBuilder::AddToOverlay(RingBox, RingHost);
	SecondsText = FALKWidgetBuilder::MakeText(Tree, TEXT("SecondsText"));
	SecondsText->SetJustification(ETextJustify::Center);
	FALKWidgetBuilder::AddToOverlay(RingBox, SecondsText, HAlign_Center, VAlign_Center);
	FALKWidgetBuilder::AddToVerticalBox(Column, FALKWidgetBuilder::WrapInSizeBox(Tree, RingBox, RingDiameter, RingDiameter), FMargin(), HAlign_Center);

	UALKTitleText* Title = FALKWidgetBuilder::Make<UALKTitleText>(Tree, TEXT("TitleText"));
	Title->SetText(LOCTEXT("MatchFoundTitle", "Match found"));
	Title->SetJustification(ETextJustify::Center);
	TitleText = Title;
	FALKWidgetBuilder::AddToVerticalBox(Column, TitleText, FMargin(0.f, 22.f, 0.f, 0.f), HAlign_Center);

	DetailText = FALKWidgetBuilder::MakeText(Tree, TEXT("DetailText"));
	DetailText->SetJustification(ETextJustify::Center);
	DetailText->SetAutoWrapText(true);
	FALKWidgetBuilder::AddToVerticalBox(Column, DetailText, FMargin(0.f, 6.f, 0.f, 0.f), HAlign_Fill);

	DotBox = FALKWidgetBuilder::Make<UHorizontalBox>(Tree, TEXT("DotBox"));
	FALKWidgetBuilder::AddToVerticalBox(Column, DotBox, FMargin(0.f, 28.f, 0.f, 0.f), HAlign_Center);

	CountText = FALKWidgetBuilder::MakeText(Tree, TEXT("CountText"));
	CountText->SetJustification(ETextJustify::Center);
	FALKWidgetBuilder::AddToVerticalBox(Column, CountText, FMargin(0.f, 14.f, 0.f, 0.f), HAlign_Center);

	WaitingText = FALKWidgetBuilder::MakeText(Tree, TEXT("WaitingText"), LOCTEXT("MatchFoundWaiting", "Waiting for the other players..."));
	WaitingText->SetJustification(ETextJustify::Center);
	WaitingText->SetVisibility(ESlateVisibility::Collapsed);
	FALKWidgetBuilder::AddToVerticalBox(Column, WaitingText, FMargin(0.f, 6.f, 0.f, 0.f), HAlign_Center);

	// Accept takes two thirds of the row.
	UHorizontalBox* Buttons = FALKWidgetBuilder::Make<UHorizontalBox>(Tree, TEXT("ButtonRow"));
	FALKWidgetBuilder::AddToVerticalBox(Column, Buttons, FMargin(0.f, 34.f, 0.f, 0.f));

	FALKInputHint AcceptHint(FText::GetEmpty(), FPlatformInput::GetGamepadAcceptKey(), FText::GetEmpty());
	AcceptHint.bOnlyWhenFocused = true;
	AcceptButton = CreateMenuButton(LOCTEXT("MatchFoundAccept", "Accept"), EALKButtonStyle::Solid);
	AcceptButton->SetLeadingIcon(EALKIcon::Check);
	AcceptButton->SetInputHint(AcceptHint);
	FALKWidgetBuilder::AddToHorizontalBox(Buttons, FALKWidgetBuilder::WrapInSizeBox(Tree, AcceptButton, 0.f, ButtonHeight), FMargin(), HAlign_Fill, VAlign_Center, 2.f);

	DeclineButton = CreateMenuButton(LOCTEXT("MatchFoundDecline", "Decline"), EALKButtonStyle::Secondary);
	DeclineButton->SetInputHint(FALKInputHint(LOCTEXT("KeyEsc", "Esc"), FPlatformInput::GetGamepadBackKey(), FText::GetEmpty()));
	FALKWidgetBuilder::AddToHorizontalBox(Buttons, FALKWidgetBuilder::WrapInSizeBox(Tree, DeclineButton, 0.f, ButtonHeight), FMargin(16.f, 0.f, 0.f, 0.f), HAlign_Fill, VAlign_Center, 1.f);
}

void UALKMatchFoundDialog::NativeOnLayoutReady()
{
	Super::NativeOnLayoutReady();
	if (AcceptButton)
	{
		AcceptButton->OnClicked().AddUObject(this, &ThisClass::AcceptMatch);
	}
	if (DeclineButton)
	{
		DeclineButton->OnClicked().AddUObject(this, &ThisClass::DeclineMatch);
	}
	DefaultFocusWidget = AcceptButton.Get();
}

void UALKMatchFoundDialog::NativeConstruct()
{
	// The ring is a Slate widget of its own; the host forgets it whenever the dialog leaves the screen.
	if (!Ring.IsValid())
	{
		Ring = SNew(SALKCountdownRing);
	}
	if (RingHost)
	{
		RingHost->SetContent(Ring.ToSharedRef());
	}
	Super::NativeConstruct();
}

void UALKMatchFoundDialog::NativeApplyTheme(const UALKTheme& Theme)
{
	Super::NativeApplyTheme(Theme);

	if (DimImage)
	{
		DimImage->SetBrush(UALKTheme::MakeSolidBrush(Theme.ScreenDim));
	}
	if (UALKFrame* Frame = Cast<UALKFrame>(Panel))
	{
		Frame->SetFrameStyle(Theme.MakeDialogFrame());
	}
	if (USizeBox* PanelSize = Panel ? Cast<USizeBox>(Panel->GetParent()) : nullptr)
	{
		PanelSize->SetWidthOverride(Theme.DialogWidth);
	}
	FALKWidgetBuilder::StylePanelBand(MapBand, Theme);
	if (Ring.IsValid())
	{
		// The disc in the panel's colour: over the bare panel it does not show, over a map's picture the seconds stay readable.
		Ring->SetColors(Theme.PanelOutline, Theme.Accent, Theme.Panel.CopyWithNewOpacity(Theme.Panel.A * 0.9f));
	}
	FALKWidgetBuilder::StyleText(SecondsText, Theme, EALKTextRole::Heading, Theme.TextPrimary);
	if (SecondsText)
	{
		FSlateFontInfo Font = Theme.HeadingFont;
		Font.Size = FMath::RoundToFloat(Theme.HeadingFont.Size * 1.55f);
		FALKWidgetBuilder::SetTextFont(SecondsText, Font);
	}
	if (TitleText)
	{
		FALKWidgetBuilder::StyleText(TitleText, Theme, EALKTextRole::Heading, Theme.TextPrimary);
		FSlateFontInfo Font = Theme.HeadingFont;
		Font.Size = FMath::Max(12.f, FMath::RoundToFloat(Theme.HeadingFont.Size * 0.92f));
		FALKWidgetBuilder::SetTextFont(TitleText, Font);
	}
	FALKWidgetBuilder::StyleText(DetailText, Theme, EALKTextRole::Body, Theme.TextMuted);
	FALKWidgetBuilder::StyleText(CountText, Theme, EALKTextRole::Caption, Theme.TextMuted);
	FALKWidgetBuilder::StyleText(WaitingText, Theme, EALKTextRole::Caption, Theme.GetAccentTextColor());
	StyleDots(Theme);
}

void UALKMatchFoundDialog::StyleDots(const UALKTheme& Theme)
{
	for (int32 Index = 0; Index < DotFrames.Num(); ++Index)
	{
		// Accepted: the accent colour with a tick. Waiting: an outlined, empty dot.
		const bool bDone = Index < AcceptedCount;
		FALKFrameStyle DotStyle;
		DotStyle.Fill = bDone ? Theme.Accent : Theme.PanelRaised;
		DotStyle.Outline = bDone ? FLinearColor::Transparent : Theme.PanelOutline;
		DotStyle.OutlineWidth = FMath::Min(Theme.OutlineWidth, 2.f);
		DotStyle.Radius = ALKMatchFoundDialogPrivate::DotSize * 0.5f;
		DotFrames[Index]->SetFrameStyle(DotStyle);
		if (DotTicks.IsValidIndex(Index) && DotTicks[Index])
		{
			DotTicks[Index]->SetIcon(EALKIcon::Check, Theme.AccentContrast);
			DotTicks[Index]->SetVisibility(bDone ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
		}
	}
}

bool UALKMatchFoundDialog::IsMatchPending() const
{
	const UALKOnlineSubsystem* Online = UALKOnlineSubsystem::Get(this);
	const EALKMatchmakingState State = Online ? Online->GetMatchmakingState() : EALKMatchmakingState::Idle;
	return State == EALKMatchmakingState::MatchFound || State == EALKMatchmakingState::Accepted;
}

void UALKMatchFoundDialog::NativeOnActivated()
{
	if (UALKOnlineSubsystem* Online = UALKOnlineSubsystem::Get(this))
	{
		if (!MatchmakingHandle.IsValid())
		{
			MatchmakingHandle = Online->OnMatchmakingChangedNative.AddUObject(this, &ThisClass::HandleMatchmakingChanged);
		}
	}
	ShownSeconds = INDEX_NONE;
	RefreshMatch();
	RefreshCountdown(false);

	Super::NativeOnActivated();
	PlayThemeSound(EALKUISound::MatchFound);
}

void UALKMatchFoundDialog::NativeOnDeactivated()
{
	if (UALKOnlineSubsystem* Online = UALKOnlineSubsystem::Get(this))
	{
		Online->OnMatchmakingChangedNative.Remove(MatchmakingHandle);
	}
	MatchmakingHandle.Reset();
	if (!IsMatchPending())
	{
		// The next match starts with a full ring.
		TotalSeconds = 0.f;
	}
	Super::NativeOnDeactivated();
}

void UALKMatchFoundDialog::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (!IsActivated())
	{
		return;
	}
	if (!IsMatchPending())
	{
		// The match started, fell through or timed out (also when the dialog was opened with no match to show).
		CloseScreen();
		return;
	}
	RefreshCountdown(true);
}

void UALKMatchFoundDialog::HandleMatchmakingChanged(EALKMatchmakingState NewState)
{
	if (NewState == EALKMatchmakingState::MatchFound || NewState == EALKMatchmakingState::Accepted)
	{
		RefreshMatch();
	}
	else
	{
		CloseScreen();
	}
}

void UALKMatchFoundDialog::RefreshMatch()
{
	using namespace ALKMatchFoundDialogPrivate;
	const UALKOnlineSubsystem* Online = UALKOnlineSubsystem::Get(this);
	if (!Online)
	{
		return;
	}
	const UALKOnlineConfig* Config = UALKDeveloperSettings::GetOnlineConfig();
	const FALKSessionInfo Match = Online->GetFoundMatch();
	const FALKPlaylist* Playlist = Config->FindPlaylist(Online->GetQueuePlaylist());
	bAccepted = Online->GetMatchmakingState() == EALKMatchmakingState::Accepted;

	// The map's picture, when the match says which map and the map has one.
	const FALKMapDefinition* Map = Config->FindMap(Match.MapId);
	FALKWidgetBuilder::SetPanelBandPicture(MapBand, Map ? Map->Thumbnail.LoadSynchronous() : nullptr);

	if (DetailText)
	{
		// Mode, map, size and ping: whatever the backend told us about the match.
		TArray<FText> Parts;
		const FText ModeName = Config->GetModeName(Match.ModeId.IsNone() && Playlist ? Playlist->ModeId : Match.ModeId);
		if (!ModeName.IsEmpty())
		{
			Parts.Add(ModeName);
		}
		const FText MapName = Config->GetMapName(Match.MapId);
		if (!MapName.IsEmpty())
		{
			Parts.Add(MapName);
		}
		if (Playlist && !Playlist->SizeLabel.IsEmpty())
		{
			Parts.Add(Playlist->SizeLabel);
		}
		else if (Match.MaxPlayers > 0)
		{
			Parts.Add(FText::Format(LOCTEXT("MatchFoundPlayers", "{0} players"), FText::AsNumber(Match.MaxPlayers)));
		}
		if (Match.PingMs >= 0)
		{
			Parts.Add(FText::Format(LOCTEXT("MatchFoundPing", "{0} ms"), FText::AsNumber(Match.PingMs)));
		}
		DetailText->SetText(FText::Join(LOCTEXT("ListSeparator", "  ·  "), Parts));
		DetailText->SetVisibility(Parts.Num() > 0 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	const int32 Needed = FMath::Max(0, Online->GetMatchPlayersNeeded());
	AcceptedCount = FMath::Clamp(Online->GetMatchPlayersAccepted(), 0, Needed);
	if (CountText)
	{
		CountText->SetText(FText::Format(LOCTEXT("MatchFoundCount", "{0} of {1} players accepted"), FText::AsNumber(AcceptedCount), FText::AsNumber(Needed)));
		CountText->SetVisibility(Needed > 0 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	// A dot per player, while they fit in a row.
	const int32 WantedDots = Needed <= MaxDots ? Needed : 0;
	if (DotBox && WidgetTree && DotFrames.Num() != WantedDots)
	{
		DotBox->ClearChildren();
		DotFrames.Reset();
		DotTicks.Reset();
		for (int32 Index = 0; Index < WantedDots; ++Index)
		{
			UOverlay* Dot = FALKWidgetBuilder::Make<UOverlay>(*WidgetTree);
			UALKFrame* DotFrame = FALKWidgetBuilder::Make<UALKFrame>(*WidgetTree);
			FALKWidgetBuilder::AddToOverlay(Dot, DotFrame);
			UALKIconWidget* Tick = FALKWidgetBuilder::MakeIcon(*WidgetTree, NAME_None, EALKIcon::Check, 18.f);
			FALKWidgetBuilder::AddToOverlay(Dot, Tick, HAlign_Center, VAlign_Center);
			FALKWidgetBuilder::AddToHorizontalBox(DotBox, FALKWidgetBuilder::WrapInSizeBox(*WidgetTree, Dot, DotSize, DotSize), FMargin(Index == 0 ? 0.f : 20.f, 0.f, 0.f, 0.f));
			DotFrames.Add(DotFrame);
			DotTicks.Add(Tick);
		}
	}
	if (DotBox)
	{
		DotBox->SetVisibility(WantedDots > 0 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	StyleDots(*GetTheme());

	// Accepted: nothing left to pick, the dialog waits for the others.
	if (WaitingText)
	{
		WaitingText->SetText(Needed > 0 && AcceptedCount >= Needed
			? LOCTEXT("MatchFoundStarting", "Everyone is in. Joining the match...")
			: LOCTEXT("MatchFoundWaiting", "Waiting for the other players..."));
		WaitingText->SetVisibility(bAccepted ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (AcceptButton)
	{
		AcceptButton->SetIsEnabled(!bAccepted);
	}
	if (DeclineButton)
	{
		DeclineButton->SetIsEnabled(!bAccepted);
	}
}

void UALKMatchFoundDialog::RefreshCountdown(bool bPlaySound)
{
	const UALKOnlineSubsystem* Online = UALKOnlineSubsystem::Get(this);
	const float Remaining = Online ? Online->GetMatchAcceptRemaining() : 0.f;
	TotalSeconds = FMath::Max(TotalSeconds, Remaining);
	if (Ring.IsValid())
	{
		Ring->SetFraction(TotalSeconds > 0.f ? Remaining / TotalSeconds : 0.f);
	}

	const int32 Seconds = FMath::CeilToInt(Remaining);
	if (Seconds != ShownSeconds)
	{
		ShownSeconds = Seconds;
		if (SecondsText)
		{
			SecondsText->SetText(FText::AsNumber(Seconds));
		}
		// The last five seconds tick.
		if (bPlaySound && Seconds >= 1 && Seconds <= 5)
		{
			PlayThemeSound(EALKUISound::Countdown);
		}
	}
}

void UALKMatchFoundDialog::AcceptMatch()
{
	UALKOnlineSubsystem* Online = UALKOnlineSubsystem::Get(this);
	if (!Online || Online->GetMatchmakingState() != EALKMatchmakingState::MatchFound)
	{
		return;
	}
	// The dialog keeps the focus while its buttons are off, so Back still reaches it.
	bAccepted = true;
	SetFocus();
	Online->RespondToMatch(true);
	if (IsActivated())
	{
		RefreshMatch();
	}
}

void UALKMatchFoundDialog::DeclineMatch()
{
	if (UALKOnlineSubsystem* Online = UALKOnlineSubsystem::Get(this))
	{
		if (Online->GetMatchmakingState() == EALKMatchmakingState::MatchFound)
		{
			Online->RespondToMatch(false);
		}
		else if (Online->GetMatchmakingState() == EALKMatchmakingState::Accepted)
		{
			// Already accepted: Back leaves the queue instead.
			Online->CancelQuickMatch();
		}
	}
	PlayThemeSound(EALKUISound::Back);
	CloseScreen();
}

bool UALKMatchFoundDialog::OnBackRequested_Implementation()
{
	DeclineMatch();
	return true;
}

UWidget* UALKMatchFoundDialog::NativeGetDesiredFocusTarget() const
{
	if (bAccepted)
	{
		return const_cast<UALKMatchFoundDialog*>(this);
	}
	return Super::NativeGetDesiredFocusTarget();
}

#undef LOCTEXT_NAMESPACE
