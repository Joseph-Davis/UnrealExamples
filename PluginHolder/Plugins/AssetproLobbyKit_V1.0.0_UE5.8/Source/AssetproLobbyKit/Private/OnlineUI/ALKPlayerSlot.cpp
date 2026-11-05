// Copyright 2026 CodeLikeMe. All Rights Reserved.

#include "OnlineUI/ALKPlayerSlot.h"
#include "ALKDeveloperSettings.h"
#include "ALKTheme.h"
#include "Lobby/ALKLobbyPlayerState.h"
#include "Online/ALKOnlineConfig.h"
#include "Online/ALKOnlineSubsystem.h"
#include "OnlineUI/ALKOnlineFlowSubsystem.h"
#include "Widgets/ALKDecor.h"
#include "Widgets/ALKFrame.h"
#include "Widgets/ALKTitleText.h"
#include "Widgets/ALKWidgetBuilder.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/Overlay.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ALKPlayerSlot)

#define LOCTEXT_NAMESPACE "AssetproLobbyKit"

namespace ALKPlayerSlotPrivate
{
	static constexpr float AvatarSize = 40.f;
	/** A long name ends in "..." here, so the crown stays next to it and the ping and Ready tag keep their place. */
	static constexpr float NameMaxWidth = 196.f;
	static constexpr float ReadyChipWidth = 100.f;

	/** How many of the four bars a ping lights. */
	static int32 GetPingLevel(int32 Milliseconds)
	{
		return Milliseconds < 60 ? 4 : (Milliseconds < 120 ? 3 : (Milliseconds < 200 ? 2 : 1));
	}
}

UALKPlayerSlot::UALKPlayerSlot(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ButtonStyleType = EALKButtonStyle::Primary;
}

void UALKPlayerSlot::NativeBuildDefaultLayout(UWidgetTree& Tree)
{
	using namespace ALKPlayerSlotPrivate;

	UALKFrame* Frame = FALKWidgetBuilder::Make<UALKFrame>(Tree, TEXT("Background"));
	Frame->SetPadding(FMargin(0.f));
	Frame->SetHorizontalAlignment(HAlign_Fill);
	Frame->SetVerticalAlignment(VAlign_Fill);
	Background = Frame;
	Tree.RootWidget = Frame;

	UOverlay* SeatOverlay = FALKWidgetBuilder::Make<UOverlay>(Tree, TEXT("SeatOverlay"));
	Frame->SetContent(SeatOverlay);

	// ---- A taken seat: bar | picture | name and status | speaker, ping, ready
	PlayerBox = FALKWidgetBuilder::Make<UHorizontalBox>(Tree, TEXT("PlayerBox"));
	FALKWidgetBuilder::AddToOverlay(SeatOverlay, PlayerBox);

	TeamBar = FALKWidgetBuilder::Make<UALKFrame>(Tree, TEXT("TeamBar"));
	FALKWidgetBuilder::AddToHorizontalBox(PlayerBox, FALKWidgetBuilder::WrapInSizeBox(Tree, TeamBar, 4.f, 0.f), FMargin(8.f, 12.f, 0.f, 12.f), HAlign_Left, VAlign_Fill);

	Avatar = FALKWidgetBuilder::Make<UALKAvatar>(Tree, TEXT("Avatar"));
	Avatar->Diameter = AvatarSize;
	FALKWidgetBuilder::AddToHorizontalBox(PlayerBox, Avatar, FMargin(12.f, 0.f, 14.f, 0.f), HAlign_Left, VAlign_Center);

	UVerticalBox* Texts = FALKWidgetBuilder::Make<UVerticalBox>(Tree, TEXT("SeatTexts"));
	FALKWidgetBuilder::AddToHorizontalBox(PlayerBox, Texts, FMargin(), HAlign_Fill, VAlign_Center, 1.f);

	UHorizontalBox* NameRow = FALKWidgetBuilder::Make<UHorizontalBox>(Tree, TEXT("NameRow"));
	FALKWidgetBuilder::AddToVerticalBox(Texts, NameRow, FMargin(), HAlign_Left);
	Label = FALKWidgetBuilder::Make<UALKTitleText>(Tree, TEXT("Label"));
	Label->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
	USizeBox* NameSize = FALKWidgetBuilder::Make<USizeBox>(Tree, TEXT("NameSize"));
	NameSize->SetMaxDesiredWidth(NameMaxWidth);
	NameSize->AddChild(Label);
	FALKWidgetBuilder::AddToHorizontalBox(NameRow, NameSize, FMargin(), HAlign_Left, VAlign_Center);
	CrownIcon = FALKWidgetBuilder::MakeIcon(Tree, TEXT("CrownIcon"), EALKIcon::Crown, 18.f);
	FALKWidgetBuilder::AddToHorizontalBox(NameRow, CrownIcon, FMargin(8.f, 0.f, 0.f, 0.f), HAlign_Left, VAlign_Center);

	UHorizontalBox* StatusRow = FALKWidgetBuilder::Make<UHorizontalBox>(Tree, TEXT("StatusRow"));
	FALKWidgetBuilder::AddToVerticalBox(Texts, StatusRow, FMargin(0.f, 3.f, 0.f, 0.f), HAlign_Left);
	PlatformIcon = FALKWidgetBuilder::MakeIcon(Tree, TEXT("PlatformIcon"), EALKIcon::Platform, 15.f);
	FALKWidgetBuilder::AddToHorizontalBox(StatusRow, PlatformIcon, FMargin(0.f, 0.f, 6.f, 0.f), HAlign_Left, VAlign_Center);
	StatusText = FALKWidgetBuilder::MakeText(Tree, TEXT("StatusText"));
	FALKWidgetBuilder::AddToHorizontalBox(StatusRow, StatusText, FMargin(), HAlign_Left, VAlign_Center);

	VoiceIcon = FALKWidgetBuilder::MakeIcon(Tree, TEXT("VoiceIcon"), EALKIcon::Speaker, 20.f);
	FALKWidgetBuilder::AddToHorizontalBox(PlayerBox, VoiceIcon, FMargin(8.f, 0.f, 14.f, 0.f), HAlign_Right, VAlign_Center);

	PingText = FALKWidgetBuilder::MakeText(Tree, TEXT("PingText"));
	PingText->SetJustification(ETextJustify::Right);
	FALKWidgetBuilder::AddToHorizontalBox(PlayerBox, FALKWidgetBuilder::WrapInSizeBox(Tree, PingText, 34.f, 0.f, true), FMargin(0.f, 0.f, 8.f, 0.f), HAlign_Right, VAlign_Center);
	PingIcon = FALKWidgetBuilder::MakeIcon(Tree, TEXT("PingIcon"), EALKIcon::PingBars, 20.f);
	FALKWidgetBuilder::AddToHorizontalBox(PlayerBox, PingIcon, FMargin(0.f, 0.f, 16.f, 0.f), HAlign_Right, VAlign_Center);

	ReadyChip = FALKWidgetBuilder::MakeChip(Tree, TEXT("ReadyChip"), FText::GetEmpty(), EALKIcon::Check);
	ReadyChip->SetPadding(FMargin(10.f, 7.f, 10.f, 8.f));
	FALKWidgetBuilder::AddToHorizontalBox(PlayerBox, FALKWidgetBuilder::WrapInSizeBox(Tree, ReadyChip, ReadyChipWidth, 0.f, true), FMargin(0.f, 0.f, 12.f, 0.f), HAlign_Right, VAlign_Center);

	// ---- A free seat: a plus and "Open slot"
	OpenBox = FALKWidgetBuilder::Make<UHorizontalBox>(Tree, TEXT("OpenBox"));
	OpenBox->SetVisibility(ESlateVisibility::Collapsed);
	FALKWidgetBuilder::AddToOverlay(SeatOverlay, OpenBox);
	OpenIcon = FALKWidgetBuilder::MakeIcon(Tree, TEXT("OpenIcon"), EALKIcon::Plus, 20.f);
	FALKWidgetBuilder::AddToHorizontalBox(OpenBox, OpenIcon, FMargin(30.f, 0.f, 22.f, 0.f), HAlign_Left, VAlign_Center);
	OpenText = FALKWidgetBuilder::MakeText(Tree, TEXT("OpenText"), LOCTEXT("SlotOpen", "Open slot"));
	FALKWidgetBuilder::AddToHorizontalBox(OpenBox, OpenText, FMargin(), HAlign_Left, VAlign_Center);
}

void UALKPlayerSlot::SetPlayer(AALKLobbyPlayerState* InPlayer)
{
	if (Player.Get() != InPlayer)
	{
		Player = InPlayer;
		// Who is talking is told again by the lobby for the new player.
		bTalking = false;
	}
	RefreshSlot();
}

void UALKPlayerSlot::SetTalking(bool bInTalking)
{
	if (bTalking != bInTalking)
	{
		bTalking = bInTalking;
		RefreshSlot();
	}
}

FString UALKPlayerSlot::GetShownName() const
{
	const AALKLobbyPlayerState* Seated = Player.Get();
	if (!Seated)
	{
		return FString();
	}
	const UALKOnlineSubsystem* Online = UALKOnlineSubsystem::Get(this);
	// The player's own name is never hidden; others read "Player 3" in streamer mode.
	return bLocalPlayer || !Online ? Seated->GetPlayerName() : Online->GetDisplayName(Seated->GetPlayerName(), Seated->GetStableIndex());
}

void UALKPlayerSlot::RefreshSlot()
{
	const AALKLobbyPlayerState* Seated = Player.Get();
	const UALKOnlineSubsystem* Online = UALKOnlineSubsystem::Get(this);

	if (PlayerBox)
	{
		PlayerBox->SetVisibility(Seated ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (OpenBox)
	{
		OpenBox->SetVisibility(Seated ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}

	bLocalPlayer = Seated && Seated->IsLocalPlayerState();
	bReady = Seated && Seated->bReady;
	bMuted = Seated && Online && !bLocalPlayer && Online->IsPlayerMuted(Seated->GetUserId());
	TeamId = Seated ? Seated->TeamId : INDEX_NONE;
	PingMs = Seated ? FMath::Max(0, FMath::RoundToInt(Seated->GetPingInMilliseconds())) : 0;

	if (Seated)
	{
		const FString ShownName = GetShownName();
		SetButtonText(FText::FromString(ShownName));
		if (Avatar)
		{
			Avatar->SetPlayer(ShownName);
			Avatar->SetAnonymous(Online && Online->IsStreamerMode() && !bLocalPlayer);
		}
		if (CrownIcon)
		{
			CrownIcon->SetVisibility(Seated->bHost ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		}
		if (StatusText)
		{
			StatusText->SetText(bLocalPlayer ? LOCTEXT("SlotYou", "You")
				: (bTalking ? LOCTEXT("SlotSpeaking", "Speaking") : FText::Format(LOCTEXT("SlotLevel", "Level {0}"), FText::AsNumber(Seated->PlayerLevel))));
		}
		if (PingText)
		{
			PingText->SetText(FText::AsNumber(PingMs));
			PingText->SetVisibility(!Online || Online->IsShowPing() ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
		}
		if (PingIcon)
		{
			PingIcon->SetLevel(ALKPlayerSlotPrivate::GetPingLevel(PingMs));
		}
		FALKWidgetBuilder::SetChipText(ReadyChip, bReady ? LOCTEXT("SlotReady", "Ready") : LOCTEXT("SlotNotReady", "Not ready"));
	}
	else
	{
		SetButtonText(FText::GetEmpty());
	}

	if (const UALKTheme* Theme = UALKDeveloperSettings::GetTheme())
	{
		ApplyPlayerColors(*Theme);
		NativeApplyVisualState(*Theme, GetHighlight(), IsUsable());
	}
}

void UALKPlayerSlot::NativeApplyTheme(const UALKTheme& Theme)
{
	Super::NativeApplyTheme(Theme);

	// As wide as its column, one seat high.
	SetMinDimensions(0, FMath::RoundToInt(SlotHeight));
	if (Label)
	{
		FSlateFontInfo NameFont = Theme.SubheadingFont;
		NameFont.Size = FMath::Max(10.f, FMath::RoundToFloat(Theme.SubheadingFont.Size * 0.88f));
		FALKWidgetBuilder::SetTextFont(Label, NameFont);
		Label->SetTextTransformPolicy(ETextTransformPolicy::None);
		Label->SetShadowColorAndOpacity(FLinearColor::Transparent);
	}
	FALKWidgetBuilder::StyleText(StatusText, Theme, EALKTextRole::Caption, Theme.TextMuted);
	FALKWidgetBuilder::StyleText(PingText, Theme, EALKTextRole::Data, Theme.TextMuted);
	FALKWidgetBuilder::StyleText(OpenText, Theme, EALKTextRole::Body, Theme.TextMuted);
	if (Avatar)
	{
		Avatar->SetBackColor(Theme.PanelRaised);
	}
	if (CrownIcon)
	{
		CrownIcon->SetIcon(EALKIcon::Crown, Theme.Warning);
	}
	if (PlatformIcon)
	{
		PlatformIcon->SetIcon(EALKIcon::Platform, Theme.TextMuted);
	}
	RefreshSlot();
}

void UALKPlayerSlot::ApplyPlayerColors(const UALKTheme& Theme)
{
	const UALKOnlineConfig* Config = UALKDeveloperSettings::GetOnlineConfig();

	if (TeamBar)
	{
		// Free for all has no team colour: no bar.
		FALKFrameStyle Bar;
		Bar.Fill = TeamId >= 0 ? Config->GetTeamColor(TeamId) : FLinearColor::Transparent;
		Bar.Radius = 2.f;
		Bar.OutlineWidth = 0.f;
		TeamBar->SetFrameStyle(Bar);
	}
	if (Avatar)
	{
		Avatar->SetRingColor(bTalking ? Theme.Success : FLinearColor::Transparent);
	}
	if (StatusText)
	{
		StatusText->SetColorAndOpacity(FSlateColor(bTalking && !bLocalPlayer ? Theme.Success : Theme.TextMuted));
	}
	if (VoiceIcon)
	{
		VoiceIcon->SetIcon(bMuted ? EALKIcon::SpeakerMuted : EALKIcon::Speaker, bMuted ? Theme.Danger : Theme.TextMuted.CopyWithNewOpacity(Theme.TextMuted.A * 0.75f));
	}
	if (PingIcon)
	{
		PingIcon->SetIcon(EALKIcon::PingBars, Theme.GetPingColor(PingMs));
	}
	if (ReadyChip)
	{
		// Ready: filled with a tick. Not ready: an outline.
		const bool bChunky = Theme.PanelStyle == EALKPanelStyle::Chunky;
		const FLinearColor ChipText = bReady ? Theme.AccentContrast : Theme.TextMuted;
		FALKWidgetBuilder::StyleChip(ReadyChip, Theme, bReady ? Theme.Accent : FLinearColor::Transparent,
			bReady ? (bChunky ? Theme.PanelOutline : FLinearColor::Transparent) : Theme.PanelOutline, ChipText);
		if (bReady && bChunky)
		{
			FALKFrameStyle Chip = ReadyChip->GetFrameStyle();
			Chip.Shadow = Theme.Shadow;
			Chip.ShadowOffset = FVector2D(3.0, 3.0);
			Chip.ShadowBlur = 0.f;
			ReadyChip->SetFrameStyle(Chip);
		}
		if (const UHorizontalBox* ChipRow = Cast<UHorizontalBox>(ReadyChip->GetContent()))
		{
			for (UWidget* Child : ChipRow->GetAllChildren())
			{
				if (UALKIconWidget* CheckIcon = Cast<UALKIconWidget>(Child))
				{
					CheckIcon->SetVisibility(bReady ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
				}
				else if (UTextBlock* ChipLabel = Cast<UTextBlock>(Child))
				{
					ChipLabel->SetTextTransformPolicy(bReady && Theme.bUppercaseButtons ? ETextTransformPolicy::ToUpper : ETextTransformPolicy::None);
				}
			}
		}
	}
}

void UALKPlayerSlot::NativeApplyVisualState(const UALKTheme& Theme, float Highlight, bool bEnabled)
{
	Super::NativeApplyVisualState(Theme, Highlight, bEnabled);

	const bool bOpen = IsOpenSlot();
	if (UALKFrame* Frame = Cast<UALKFrame>(Background))
	{
		// A card on the team's panel: the focus ring round it, and a light tint so the focused seat stands out from the
		// local player's (which has the accent outline all the time).
		FALKFrameStyle Seat = Theme.MakeCardFrame(Highlight);
		if (bOpen)
		{
			Seat.Fill = Theme.PanelRaised.CopyWithNewOpacity(Theme.PanelRaised.A * 0.7f * Highlight);
			Seat.Shadow = FLinearColor::Transparent;
		}
		else
		{
			Seat.Fill = FMath::Lerp(Theme.PanelRaised, FMath::Lerp(Theme.PanelRaised, Theme.Accent, 0.05f), Highlight).CopyWithNewOpacity(Theme.PanelRaised.A);
			if (bLocalPlayer)
			{
				Seat.Outline = Theme.Accent;
				Seat.OutlineWidth = FMath::Max(Theme.OutlineWidth, 2.f);
			}
		}
		Frame->SetFrameStyle(Seat);
	}
	if (Label)
	{
		// The fill is never the solid accent colour, so the name keeps the primary colour in every skin.
		Label->SetColorAndOpacity(FSlateColor(Theme.TextPrimary));
	}
	const FLinearColor OpenColor = FMath::Lerp(Theme.TextMuted, Theme.TextPrimary, Highlight);
	if (OpenText)
	{
		OpenText->SetColorAndOpacity(FSlateColor(OpenColor));
	}
	if (OpenIcon)
	{
		OpenIcon->SetIcon(EALKIcon::Plus, OpenColor);
	}
}

void UALKPlayerSlot::NativeOnAddedToFocusPath(const FFocusEvent& InFocusEvent)
{
	Super::NativeOnAddedToFocusPath(InFocusEvent);
	OnSlotFocused.Broadcast(this);
}

void UALKPlayerSlot::NativeOnClicked()
{
	Super::NativeOnClicked();

	UALKOnlineFlowSubsystem* Flow = bOpenOptionsWhenPicked ? UALKOnlineFlowSubsystem::Get(this) : nullptr;
	if (!Flow)
	{
		return;
	}
	if (const AALKLobbyPlayerState* Seated = Player.Get())
	{
		Flow->ShowPlayerOptions(Seated->GetUserId(), GetShownName());
	}
	else
	{
		// An open slot: invite someone.
		Flow->OpenSocial();
	}
}

#undef LOCTEXT_NAMESPACE
