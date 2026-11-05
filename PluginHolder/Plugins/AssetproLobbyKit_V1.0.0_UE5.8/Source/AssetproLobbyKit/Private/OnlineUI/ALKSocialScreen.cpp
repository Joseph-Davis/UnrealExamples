// Copyright 2026 CodeLikeMe. All Rights Reserved.

#include "OnlineUI/ALKSocialScreen.h"
#include "ALKTheme.h"
#include "Online/ALKOnlineSubsystem.h"
#include "OnlineUI/ALKFriendsPanel.h"
#include "OnlineUI/ALKPartyPanel.h"
#include "Widgets/ALKButton.h"
#include "Widgets/ALKDecor.h"
#include "Widgets/ALKInputLabels.h"
#include "Widgets/ALKTitleText.h"
#include "Widgets/ALKWidgetBuilder.h"
#include "Blueprint/WidgetNavigation.h"
#include "Blueprint/WidgetTree.h"
#include "Components/HorizontalBox.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "HAL/PlatformInput.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ALKSocialScreen)

#define LOCTEXT_NAMESPACE "AssetproLobbyKit"

namespace ALKSocialScreenPrivate
{
	/** The two panels sit in one column of this width at most, centred. */
	static constexpr float ColumnWidth = 1500.f;
	static constexpr float PartyWidth = 504.f;
}

UALKSocialScreen::UALKSocialScreen(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bDimScene = true;
}

void UALKSocialScreen::NativeBuildDefaultLayout(UWidgetTree& Tree)
{
	using namespace ALKSocialScreenPrivate;

	UOverlay* Root = FALKWidgetBuilder::Make<UOverlay>(Tree, TEXT("Root"));
	Tree.RootWidget = Root;
	BackgroundImage = FALKWidgetBuilder::AddBackgroundLayers(Tree, Root);

	USizeBox* ColumnSize = FALKWidgetBuilder::Make<USizeBox>(Tree, TEXT("MainColumn"));
	ColumnSize->SetWidthOverride(ColumnWidth);
	FALKWidgetBuilder::AddToOverlay(Root, ColumnSize, HAlign_Center, VAlign_Fill);
	UVerticalBox* Main = FALKWidgetBuilder::Make<UVerticalBox>(Tree, TEXT("MainBox"));
	ColumnSize->AddChild(Main);

	SystemLabelText = FALKWidgetBuilder::MakeText(Tree, TEXT("SystemLabelText"), MakeSystemLabel(21, LOCTEXT("SocialTitle", "Friends & Party")));
	FALKWidgetBuilder::AddToVerticalBox(Main, SystemLabelText, FMargin(0.f, 0.f, 0.f, 6.f), HAlign_Left);
	UALKTitleText* Title = FALKWidgetBuilder::Make<UALKTitleText>(Tree, TEXT("TitleText"));
	Title->SetText(LOCTEXT("SocialTitle", "Friends & Party"));
	TitleText = Title;
	FALKWidgetBuilder::AddToVerticalBox(Main, TitleText, FMargin(), HAlign_Left);

	// Body: the friends list | the party with the platform invite under it.
	UHorizontalBox* Body = FALKWidgetBuilder::Make<UHorizontalBox>(Tree, TEXT("Body"));
	FALKWidgetBuilder::AddToVerticalBox(Main, Body, FMargin(0.f, 28.f, 0.f, 0.f), HAlign_Fill, VAlign_Fill, 1.f);

	FriendsPanel = Tree.ConstructWidget<UALKFriendsPanel>(UALKFriendsPanel::StaticClass(), TEXT("FriendsPanel"));
	FALKWidgetBuilder::AddToHorizontalBox(Body, FriendsPanel, FMargin(), HAlign_Fill, VAlign_Fill, 1.f);

	PartyColumn = FALKWidgetBuilder::Make<UVerticalBox>(Tree, TEXT("PartyColumn"));
	FALKWidgetBuilder::AddToHorizontalBox(Body, FALKWidgetBuilder::WrapInSizeBox(Tree, PartyColumn, PartyWidth, 0.f), FMargin(32.f, 0.f, 0.f, 0.f), HAlign_Right, VAlign_Fill);

	PartyPanel = Tree.ConstructWidget<UALKPartyPanel>(UALKPartyPanel::StaticClass(), TEXT("PartyPanel"));
	// The friends are right here: Invite goes to the list instead of opening this screen again.
	PartyPanel->bInviteOpensSocial = false;
	FALKWidgetBuilder::AddToVerticalBox(PartyColumn, PartyPanel);

	PlatformInviteButton = CreateMenuButton(LOCTEXT("SocialPlatformInvite", "Invite through your platform"), EALKButtonStyle::Secondary);
	PlatformInviteButton->SetLeadingIcon(EALKIcon::Platform);
	FALKWidgetBuilder::AddToVerticalBox(PartyColumn, PlatformInviteButton, FMargin(0.f, 16.f, 0.f, 0.f));

	// Footer: [Enter] Select  [Q] Friends  [E] Recent players  [Esc] Back
	UHorizontalBox* Hints = BuildInputHints(Tree, {
		FALKInputHint(LOCTEXT("KeyEnter", "Enter"), FPlatformInput::GetGamepadAcceptKey(), LOCTEXT("HintSelect", "Select")),
		FALKInputHint(LOCTEXT("KeyQ", "Q"), EKeys::Gamepad_LeftShoulder, LOCTEXT("SocialHintFriends", "Friends"))
			.WithAction(FSimpleDelegate::CreateUObject(this, &ThisClass::ShowFriendsTab, false)),
		FALKInputHint(LOCTEXT("KeyE", "E"), EKeys::Gamepad_RightShoulder, LOCTEXT("SocialHintRecent", "Recent players"))
			.WithAction(FSimpleDelegate::CreateUObject(this, &ThisClass::ShowFriendsTab, true)),
		FALKInputHint(LOCTEXT("KeyEsc", "Esc"), FPlatformInput::GetGamepadBackKey(), LOCTEXT("HintBack", "Back"))
			.WithAction(FSimpleDelegate::CreateWeakLambda(this, [this]() { OnBackRequested(); }))
	});
	FALKWidgetBuilder::AddToVerticalBox(Main, Hints, FMargin(0.f, 24.f, 0.f, 0.f), HAlign_Left);
}

void UALKSocialScreen::NativeOnLayoutReady()
{
	Super::NativeOnLayoutReady();

	if (PlatformInviteButton)
	{
		PlatformInviteButton->OnClicked().AddUObject(this, &ThisClass::ShowPlatformInvite);
	}
	if (PartyPanel)
	{
		PartyPanel->OnInviteRequested.AddUObject(this, &ThisClass::FocusFriends);
	}

	// The panels are not always level with each other, so Left and Right are told where to go.
	if (FriendsPanel)
	{
		FCustomWidgetNavigationDelegate ToParty;
		ToParty.BindDynamic(this, &ThisClass::HandleNavigateToParty);
		FriendsPanel->SetNavigationRuleCustomBoundary(EUINavigation::Right, ToParty);
	}
	if (PartyColumn)
	{
		FCustomWidgetNavigationDelegate ToFriends;
		ToFriends.BindDynamic(this, &ThisClass::HandleNavigateToFriends);
		PartyColumn->SetNavigationRuleCustomBoundary(EUINavigation::Left, ToFriends);
	}
}

void UALKSocialScreen::NativeApplyTheme(const UALKTheme& Theme)
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
}

UWidget* UALKSocialScreen::NativeGetDesiredFocusTarget() const
{
	if (UWidget* Remembered = GetLastFocusedWidget())
	{
		return Remembered;
	}
	// The first friend, or the party when the list is empty.
	if (UWidget* FirstFriend = FriendsPanel ? FriendsPanel->GetFirstFocusTarget() : nullptr)
	{
		return FirstFriend;
	}
	if (UWidget* FirstParty = PartyPanel ? PartyPanel->GetFirstFocusTarget() : nullptr)
	{
		return FirstParty;
	}
	return Super::NativeGetDesiredFocusTarget();
}

UWidget* UALKSocialScreen::HandleNavigateToParty(EUINavigation Direction)
{
	UWidget* Target = PartyPanel ? PartyPanel->GetFirstFocusTarget() : nullptr;
	return Target ? Target : PlatformInviteButton.Get();
}

UWidget* UALKSocialScreen::HandleNavigateToFriends(EUINavigation Direction)
{
	return FriendsPanel ? FriendsPanel->GetFirstFocusTarget() : nullptr;
}

void UALKSocialScreen::ShowFriendsTab(bool bRecent)
{
	if (FriendsPanel)
	{
		FriendsPanel->ShowRecentPlayers(bRecent);
	}
}

void UALKSocialScreen::FocusFriends()
{
	if (!FriendsPanel || !FriendsPanel->FocusFirst())
	{
		ShowPlatformInvite();
	}
}

void UALKSocialScreen::ShowPlatformInvite()
{
	if (UALKOnlineSubsystem* Online = UALKOnlineSubsystem::Get(this))
	{
		Online->ShowInviteOverlay();
	}
}

bool UALKSocialScreen::NativeHandleMenuKey(const FKey& Key)
{
	if (Key == EKeys::Q || Key == EKeys::Gamepad_LeftShoulder)
	{
		ShowFriendsTab(false);
		return true;
	}
	if (Key == EKeys::E || Key == EKeys::Gamepad_RightShoulder)
	{
		ShowFriendsTab(true);
		return true;
	}
	return Super::NativeHandleMenuKey(Key);
}

#undef LOCTEXT_NAMESPACE
