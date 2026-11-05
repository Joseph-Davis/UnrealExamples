// Copyright 2026 CodeLikeMe. All Rights Reserved.

#include "OnlineUI/ALKPlayerOptionsDialog.h"
#include "ALKDeveloperSettings.h"
#include "ALKMenuSubsystem.h"
#include "ALKTheme.h"
#include "Lobby/ALKLobbyGameState.h"
#include "Lobby/ALKLobbyPlayerState.h"
#include "Lobby/ALKPlayerNetComponent.h"
#include "Online/ALKOnlineConfig.h"
#include "Online/ALKOnlineSubsystem.h"
#include "OnlineUI/ALKChatPanel.h"
#include "OnlineUI/ALKOnlineFlowSubsystem.h"
#include "Screens/ALKConfirmDialog.h"
#include "Widgets/ALKButton.h"
#include "Widgets/ALKDecor.h"
#include "Widgets/ALKFrame.h"
#include "Widgets/ALKInputLabels.h"
#include "Widgets/ALKTitleText.h"
#include "Widgets/ALKWidgetBuilder.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Engine/World.h"
#include "HAL/PlatformInput.h"
#include "Styling/SlateTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ALKPlayerOptionsDialog)

#define LOCTEXT_NAMESPACE "AssetproLobbyKit"

namespace ALKPlayerOptionsPrivate
{
	/** Narrower than a question dialog: a list of short entries. */
	static constexpr float WidthShare = 0.7f;
	/** The list scrolls past this height (a host looking at a party member sees every entry). */
	static constexpr float ListMaxHeight = 620.f;
}

UALKPlayerOptionsDialog::UALKPlayerOptionsDialog(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bIsModal = true;
}

void UALKPlayerOptionsDialog::NativeBuildDefaultLayout(UWidgetTree& Tree)
{
	UOverlay* Root = FALKWidgetBuilder::Make<UOverlay>(Tree, TEXT("Root"));
	Tree.RootWidget = Root;

	DimImage = FALKWidgetBuilder::Make<UImage>(Tree, TEXT("DimImage"));
	FALKWidgetBuilder::AddToOverlay(Root, DimImage);

	// A panel over the dimmed screen: who it is about, what can be done, Close.
	Panel = FALKWidgetBuilder::Make<UALKFrame>(Tree, TEXT("Panel"));
	Panel->SetPadding(FMargin(30.f, 34.f, 30.f, 28.f));
	FALKWidgetBuilder::AddToOverlay(Root, FALKWidgetBuilder::WrapInSizeBox(Tree, Panel, 480.f, 0.f), HAlign_Center, VAlign_Center);

	UVerticalBox* Column = FALKWidgetBuilder::Make<UVerticalBox>(Tree, TEXT("PanelColumn"));
	Panel->SetContent(Column);

	UHorizontalBox* Header = FALKWidgetBuilder::Make<UHorizontalBox>(Tree, TEXT("HeaderRow"));
	FALKWidgetBuilder::AddToVerticalBox(Column, Header, FMargin(6.f, 0.f, 6.f, 0.f));
	Avatar = FALKWidgetBuilder::Make<UALKAvatar>(Tree, TEXT("Avatar"));
	Avatar->Diameter = 56.f;
	FALKWidgetBuilder::AddToHorizontalBox(Header, Avatar, FMargin(0.f, 0.f, 18.f, 0.f), HAlign_Left, VAlign_Center);
	UVerticalBox* Texts = FALKWidgetBuilder::Make<UVerticalBox>(Tree, TEXT("HeaderTexts"));
	FALKWidgetBuilder::AddToHorizontalBox(Header, Texts, FMargin(), HAlign_Fill, VAlign_Center, 1.f);
	UALKTitleText* Title = FALKWidgetBuilder::Make<UALKTitleText>(Tree, TEXT("TitleText"));
	Title->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
	TitleText = Title;
	FALKWidgetBuilder::AddToVerticalBox(Texts, TitleText, FMargin(), HAlign_Fill);
	SubtitleText = FALKWidgetBuilder::MakeText(Tree, TEXT("SubtitleText"));
	SubtitleText->SetAutoWrapText(true);
	FALKWidgetBuilder::AddToVerticalBox(Texts, SubtitleText, FMargin(0.f, 4.f, 0.f, 0.f), HAlign_Fill);

	ActionList = FALKWidgetBuilder::Make<UScrollBox>(Tree, TEXT("ActionList"));
	ActionList->SetAnimateWheelScrolling(true);
	ActionList->SetScrollWhenFocusChanges(EScrollWhenFocusChanges::AnimatedScroll);
	USizeBox* ListSize = FALKWidgetBuilder::Make<USizeBox>(Tree, TEXT("ActionListSize"));
	ListSize->SetMaxDesiredHeight(ALKPlayerOptionsPrivate::ListMaxHeight);
	ListSize->AddChild(ActionList);
	FALKWidgetBuilder::AddToVerticalBox(Column, ListSize, FMargin(0.f, 22.f, 0.f, 0.f));

	CloseButton = CreateMenuButton(LOCTEXT("Close", "Close"), EALKButtonStyle::Ghost);
	CloseButton->SetInputHint(FALKInputHint(LOCTEXT("KeyEsc", "Esc"), FPlatformInput::GetGamepadBackKey(), FText::GetEmpty()));
	FALKWidgetBuilder::AddToVerticalBox(Column, CloseButton, FMargin(6.f, 14.f, 8.f, 0.f), HAlign_Fill);
}

void UALKPlayerOptionsDialog::NativeOnLayoutReady()
{
	Super::NativeOnLayoutReady();
	if (CloseButton)
	{
		CloseButton->OnClicked().AddWeakLambda(this, [this]() { OnBackRequested(); });
	}
}

void UALKPlayerOptionsDialog::NativeApplyTheme(const UALKTheme& Theme)
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
		PanelSize->SetWidthOverride(FMath::RoundToFloat(Theme.DialogWidth * ALKPlayerOptionsPrivate::WidthShare));
	}
	if (TitleText)
	{
		FALKWidgetBuilder::StyleText(TitleText, Theme, EALKTextRole::Heading, Theme.TextPrimary);
		FSlateFontInfo Font = Theme.HeadingFont;
		Font.Size = FMath::Max(12.f, FMath::RoundToFloat(Font.Size * 0.62f));
		FALKWidgetBuilder::SetTextFont(TitleText, Font);
		// A player's name is shown as they wrote it.
		TitleText->SetTextTransformPolicy(ETextTransformPolicy::None);
	}
	FALKWidgetBuilder::StyleText(SubtitleText, Theme, EALKTextRole::Caption, Theme.TextMuted);
	if (Avatar)
	{
		Avatar->SetBackColor(Theme.Panel);
	}
	if (ActionList)
	{
		FScrollBarStyle BarStyle = ActionList->GetWidgetBarStyle();
		const FSlateBrush Thumb = UALKTheme::MakeSolidBrush(Theme.Accent.CopyWithNewOpacity(0.85f));
		BarStyle.SetNormalThumbImage(Thumb);
		BarStyle.SetHoveredThumbImage(Thumb);
		BarStyle.SetDraggedThumbImage(Thumb);
		BarStyle.SetVerticalBackgroundImage(UALKTheme::MakeSolidBrush(Theme.TextPrimary.CopyWithNewOpacity(0.06f)));
		ActionList->SetWidgetBarStyle(BarStyle);
		ActionList->SetScrollbarThickness(FVector2D(3.0, 3.0));
		ActionList->SetScrollbarPadding(FMargin(4.f, 0.f, 0.f, 0.f));
	}
}

void UALKPlayerOptionsDialog::NativeOnActivated()
{
	// The dialog is pooled: every opening reads the player it was opened for.
	RefreshTarget();
	Super::NativeOnActivated();
}

// ---------------------------------------------------------------- The player

AALKLobbyPlayerState* UALKPlayerOptionsDialog::FindTargetInGame() const
{
	const UWorld* World = GetWorld();
	const AALKLobbyGameState* GameState = World ? World->GetGameState<AALKLobbyGameState>() : nullptr;
	return GameState ? GameState->FindPlayerByUserId(TargetUserId) : nullptr;
}

bool UALKPlayerOptionsDialog::IsTargetLocal() const
{
	const UALKOnlineSubsystem* Online = UALKOnlineSubsystem::Get(this);
	const AALKLobbyPlayerState* InGame = FindTargetInGame();
	return (InGame && InGame->IsLocalPlayerState()) || (Online && !TargetUserId.IsEmpty() && TargetUserId == Online->GetLocalUserId());
}

bool UALKPlayerOptionsDialog::IsTargetInParty() const
{
	const UALKOnlineSubsystem* Online = UALKOnlineSubsystem::Get(this);
	if (!Online || !Online->IsInParty())
	{
		return false;
	}
	return Online->GetPartyMembers().ContainsByPredicate([this](const FALKPartyMember& Member) { return !Member.bLocal && Member.UserId == TargetUserId; });
}

int32 UALKPlayerOptionsDialog::GetOtherTeam() const
{
	const UWorld* World = GetWorld();
	const AALKLobbyGameState* GameState = World ? World->GetGameState<AALKLobbyGameState>() : nullptr;
	const AALKLobbyPlayerState* InGame = FindTargetInGame();
	if (!GameState || !InGame || GameState->TeamCount < 2 || InGame->bSpectating || !GameState->IsInLobby())
	{
		return INDEX_NONE;
	}
	return (FMath::Max(0, InGame->TeamId) + 1) % GameState->TeamCount;
}

void UALKPlayerOptionsDialog::RefreshTarget()
{
	const UALKOnlineFlowSubsystem* Flow = UALKOnlineFlowSubsystem::Get(this);
	const UALKOnlineSubsystem* Online = UALKOnlineSubsystem::Get(this);
	const UALKOnlineConfig* Config = UALKDeveloperSettings::GetOnlineConfig();

	TargetUserId = Flow ? Flow->GetContextUserId() : FString();
	const FString ContextName = Flow ? Flow->GetContextDisplayName() : FString();
	const AALKLobbyPlayerState* InGame = FindTargetInGame();
	const bool bLocal = IsTargetLocal();

	// The name as streamer mode wants it: the player's own is never hidden, others read "Player 3".
	if (InGame)
	{
		TargetName = bLocal || !Online ? InGame->GetPlayerName() : Online->GetDisplayName(InGame->GetPlayerName(), InGame->GetStableIndex());
	}
	else
	{
		TargetName = bLocal || !Online ? ContextName : Online->GetDisplayName(ContextName, INDEX_NONE);
	}
	if (TargetName.IsEmpty())
	{
		TargetName = LOCTEXT("PlayerOptionsNoName", "Player").ToString();
	}

	if (TitleText)
	{
		TitleText->SetText(FText::FromString(TargetName));
	}
	if (Avatar)
	{
		Avatar->SetPlayer(TargetName);
		Avatar->SetAnonymous(Online && Online->IsStreamerMode() && !bLocal);
	}
	if (SubtitleText)
	{
		// A few facts about the player, the ones that are true.
		TArray<FText> Facts;
		if (bLocal)
		{
			Facts.Add(LOCTEXT("PlayerOptionsYou", "You"));
		}
		if (InGame)
		{
			if (InGame->bHost)
			{
				Facts.Add(LOCTEXT("PlayerOptionsHost", "Host"));
			}
			if (InGame->bSpectating)
			{
				Facts.Add(LOCTEXT("PlayerOptionsSpectating", "Watching"));
			}
			else if (InGame->TeamId >= 0)
			{
				Facts.Add(Config->GetTeamName(InGame->TeamId));
			}
			Facts.Add(FText::Format(LOCTEXT("PlayerOptionsLevel", "Level {0}"), FText::AsNumber(InGame->PlayerLevel)));
		}
		if (!bLocal && IsTargetInParty())
		{
			Facts.Add(LOCTEXT("PlayerOptionsInParty", "In your party"));
		}
		if (Online && !bLocal && Online->IsPlayerBlocked(TargetUserId))
		{
			Facts.Add(LOCTEXT("PlayerOptionsBlocked", "Blocked"));
		}
		else if (Online && !bLocal && Online->IsPlayerMuted(TargetUserId))
		{
			Facts.Add(LOCTEXT("PlayerOptionsMuted", "Muted"));
		}
		SubtitleText->SetText(FText::Join(LOCTEXT("PlayerOptionsFactSeparator", "  \u00b7  "), Facts));
		SubtitleText->SetVisibility(Facts.Num() > 0 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	// One button per entry that applies.
	UALKButton* First = nullptr;
	if (ActionList)
	{
		ActionList->ClearChildren();
		for (const EALKPlayerOption Option : GetAvailableOptions())
		{
			const bool bRemoves = Option == EALKPlayerOption::Kick || Option == EALKPlayerOption::Ban;
			UALKButton* Button = CreateMenuButton(GetOptionText(Option), bRemoves ? EALKButtonStyle::Danger : EALKButtonStyle::Secondary);
			if (!Button)
			{
				continue;
			}
			Button->OnClicked().AddUObject(this, &ThisClass::RunOption, Option);
			ActionList->AddChild(Button);
			if (UScrollBoxSlot* ButtonSlot = Cast<UScrollBoxSlot>(Button->Slot))
			{
				// Room for the focus ring and the chunky skin's hard shadow inside the list's clipping.
				ButtonSlot->SetPadding(FMargin(6.f, 5.f, 8.f, 5.f));
				ButtonSlot->SetHorizontalAlignment(HAlign_Fill);
			}
			First = First ? First : Button;
		}
		ActionList->ScrollToStart();
	}
	DefaultFocusWidget = First ? First : CloseButton.Get();
}

// ---------------------------------------------------------------- The entries

TArray<EALKPlayerOption> UALKPlayerOptionsDialog::GetAvailableOptions() const
{
	TArray<EALKPlayerOption> Options;
	const UALKOnlineSubsystem* Online = UALKOnlineSubsystem::Get(this);
	if (!Online || TargetUserId.IsEmpty())
	{
		return Options;
	}

	Options.Add(EALKPlayerOption::ViewProfile);
	if (IsTargetLocal())
	{
		// Nothing else applies to yourself.
		return Options;
	}

	const AALKLobbyPlayerState* InGame = FindTargetInGame();
	const bool bBlocked = Online->IsPlayerBlocked(TargetUserId);
	Options.Add(EALKPlayerOption::ToggleMute);
	Options.Add(EALKPlayerOption::ToggleBlock);
	if (Online->IsTextChatEnabled() && !bBlocked)
	{
		Options.Add(EALKPlayerOption::Whisper);
	}

	// Party: anyone not in a party, or its leader, can invite; only the leader promotes and removes.
	const bool bInParty = IsTargetInParty();
	if (Online->IsPartyLeader())
	{
		if (bInParty)
		{
			Options.Add(EALKPlayerOption::PromoteInParty);
			Options.Add(EALKPlayerOption::RemoveFromParty);
		}
		else if (!bBlocked && Online->GetPartyMembers().Num() < Online->GetPartyMaxSize())
		{
			Options.Add(EALKPlayerOption::InviteToParty);
		}
	}

	// The host's tools, for a player who is in the game.
	if (InGame && Online->IsHost())
	{
		if (GetOtherTeam() != INDEX_NONE)
		{
			Options.Add(EALKPlayerOption::MoveToOtherTeam);
		}
		Options.Add(EALKPlayerOption::Kick);
		Options.Add(EALKPlayerOption::Ban);
	}
	Options.Add(EALKPlayerOption::Report);
	return Options;
}

FText UALKPlayerOptionsDialog::GetOptionText(EALKPlayerOption Option) const
{
	const UALKOnlineSubsystem* Online = UALKOnlineSubsystem::Get(this);
	switch (Option)
	{
	case EALKPlayerOption::ViewProfile:
		return LOCTEXT("PlayerOptionsProfile", "View profile");
	case EALKPlayerOption::ToggleMute:
		return Online && Online->IsPlayerMuted(TargetUserId) ? LOCTEXT("PlayerOptionsUnmute", "Unmute") : LOCTEXT("PlayerOptionsMute", "Mute");
	case EALKPlayerOption::ToggleBlock:
		return Online && Online->IsPlayerBlocked(TargetUserId) ? LOCTEXT("PlayerOptionsUnblock", "Unblock") : LOCTEXT("PlayerOptionsBlock", "Block");
	case EALKPlayerOption::Whisper:
		return LOCTEXT("PlayerOptionsWhisper", "Whisper");
	case EALKPlayerOption::InviteToParty:
		return LOCTEXT("PlayerOptionsInvite", "Invite to party");
	case EALKPlayerOption::PromoteInParty:
		return LOCTEXT("PlayerOptionsPromote", "Promote to party leader");
	case EALKPlayerOption::RemoveFromParty:
		return LOCTEXT("PlayerOptionsRemove", "Remove from party");
	case EALKPlayerOption::MoveToOtherTeam:
	{
		const int32 OtherTeam = GetOtherTeam();
		return OtherTeam == INDEX_NONE ? LOCTEXT("PlayerOptionsMove", "Move to the other team")
			: FText::Format(LOCTEXT("PlayerOptionsMoveTo", "Move to {0}"), UALKDeveloperSettings::GetOnlineConfig()->GetTeamName(OtherTeam));
	}
	case EALKPlayerOption::Kick:
		return LOCTEXT("PlayerOptionsKick", "Kick");
	case EALKPlayerOption::Ban:
		return LOCTEXT("PlayerOptionsBan", "Ban");
	case EALKPlayerOption::Report:
		return LOCTEXT("PlayerOptionsReport", "Report");
	default:
		return FText::GetEmpty();
	}
}

void UALKPlayerOptionsDialog::RunOption(EALKPlayerOption Option)
{
	UALKOnlineFlowSubsystem* Flow = UALKOnlineFlowSubsystem::Get(this);
	UALKOnlineSubsystem* Online = UALKOnlineSubsystem::Get(this);
	if (!Flow || !Online || TargetUserId.IsEmpty())
	{
		CloseScreen();
		return;
	}

	// Everything the entry needs, read before the dialog closes.
	const FString UserId = TargetUserId;
	const FString Name = TargetName;
	const FText NameText = FText::FromString(Name);
	const AALKLobbyPlayerState* InGame = FindTargetInGame();
	const int32 PlayerId = InGame ? InGame->GetPlayerId() : INDEX_NONE;
	const int32 OtherTeam = GetOtherTeam();

	// The dialog goes first: what the entry opens (a profile, a question) stands on its own.
	CloseScreen();

	switch (Option)
	{
	case EALKPlayerOption::ViewProfile:
		Flow->ShowProfile(UserId, Name);
		break;

	case EALKPlayerOption::ToggleMute:
		Online->SetPlayerMuted(UserId, !Online->IsPlayerMuted(UserId));
		break;

	case EALKPlayerOption::ToggleBlock:
	{
		const bool bBlock = !Online->IsPlayerBlocked(UserId);
		Online->SetPlayerBlocked(UserId, bBlock);
		Flow->ShowNotice(bBlock ? FText::Format(LOCTEXT("PlayerOptionsBlockedNotice", "{0} is blocked"), NameText)
			: FText::Format(LOCTEXT("PlayerOptionsUnblockedNotice", "{0} is no longer blocked"), NameText),
			bBlock ? LOCTEXT("PlayerOptionsBlockedDetail", "You will not see their messages or invites") : FText::GetEmpty());
		break;
	}

	case EALKPlayerOption::Whisper:
		UALKChatPanel::OnWhisperRequested().Broadcast(UserId, Name);
		break;

	case EALKPlayerOption::InviteToParty:
		Online->InviteToParty(UserId);
		Flow->ShowNotice(FText::Format(LOCTEXT("PlayerOptionsInvitedNotice", "Party invite sent to {0}"), NameText), FText::GetEmpty());
		break;

	case EALKPlayerOption::PromoteInParty:
		Online->PromotePartyMember(UserId);
		break;

	case EALKPlayerOption::RemoveFromParty:
		Online->KickFromParty(UserId);
		break;

	case EALKPlayerOption::MoveToOtherTeam:
		if (UALKPlayerNetComponent* NetComponent = Online->GetLocalNetComponent())
		{
			if (PlayerId != INDEX_NONE && OtherTeam != INDEX_NONE)
			{
				NetComponent->ServerMovePlayerToTeam(PlayerId, OtherTeam);
			}
		}
		break;

	case EALKPlayerOption::Kick:
		AskKickOrBan(false);
		break;

	case EALKPlayerOption::Ban:
		AskKickOrBan(true);
		break;

	case EALKPlayerOption::Report:
		Flow->ShowReport(UserId, Name);
		break;

	default:
		break;
	}
}

void UALKPlayerOptionsDialog::AskKickOrBan(bool bBan)
{
	UALKMenuSubsystem* Menu = GetMenuSubsystem();
	const UALKOnlineSubsystem* Online = UALKOnlineSubsystem::Get(this);
	const AALKLobbyPlayerState* InGame = FindTargetInGame();
	if (!Menu || !Online || !InGame)
	{
		return;
	}
	const FText NameText = FText::FromString(TargetName);

	FALKDialogDescriptor Descriptor;
	Descriptor.Title = bBan ? FText::Format(LOCTEXT("PlayerOptionsBanTitle", "Ban {0}?"), NameText) : FText::Format(LOCTEXT("PlayerOptionsKickTitle", "Kick {0}?"), NameText);
	Descriptor.Message = bBan ? LOCTEXT("PlayerOptionsBanMessage", "They are removed from the game and cannot join your games again until you take them off the ban list.")
		: LOCTEXT("PlayerOptionsKickMessage", "They are removed from the game. They can join again.");
	Descriptor.ConfirmText = bBan ? LOCTEXT("PlayerOptionsBan", "Ban") : LOCTEXT("PlayerOptionsKick", "Kick");
	Descriptor.CancelText = LOCTEXT("Cancel", "Cancel");
	Descriptor.bDestructive = bBan;

	const TWeakObjectPtr<UALKPlayerNetComponent> WeakNetComponent(Online->GetLocalNetComponent());
	const int32 PlayerId = InGame->GetPlayerId();
	Menu->ShowConfirmDialog(Descriptor, [WeakNetComponent, PlayerId, bBan](EALKDialogResult Result)
	{
		UALKPlayerNetComponent* NetComponent = WeakNetComponent.Get();
		if (Result != EALKDialogResult::Confirmed || !NetComponent)
		{
			return;
		}
		if (bBan)
		{
			NetComponent->ServerBanPlayer(PlayerId);
		}
		else
		{
			NetComponent->ServerKickPlayer(PlayerId);
		}
	});
}

#undef LOCTEXT_NAMESPACE
