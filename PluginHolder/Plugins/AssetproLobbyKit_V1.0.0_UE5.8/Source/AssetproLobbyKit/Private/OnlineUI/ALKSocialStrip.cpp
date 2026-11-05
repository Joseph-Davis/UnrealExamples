// Copyright 2026 CodeLikeMe. All Rights Reserved.

#include "OnlineUI/ALKSocialStrip.h"
#include "Online/ALKOnlineSubsystem.h"
#include "OnlineUI/ALKFriendsPanel.h"
#include "OnlineUI/ALKPartyPanel.h"
#include "Widgets/ALKWidgetBuilder.h"
#include "Blueprint/WidgetNavigation.h"
#include "Blueprint/WidgetTree.h"
#include "Components/SizeBox.h"
#include "Components/VerticalBox.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ALKSocialStrip)

void UALKSocialStrip::NativeBuildDefaultLayout(UWidgetTree& Tree)
{
	StripSize = FALKWidgetBuilder::Make<USizeBox>(Tree, TEXT("StripSize"));
	StripSize->SetWidthOverride(StripWidth);
	Tree.RootWidget = StripSize;

	UVerticalBox* Column = FALKWidgetBuilder::Make<UVerticalBox>(Tree, TEXT("StripColumn"));
	StripSize->AddChild(Column);

	// The party takes the height it needs; the friends get the rest and scroll when that is not enough.
	PartyPanel = Tree.ConstructWidget<UALKPartyPanel>(UALKPartyPanel::StaticClass(), TEXT("PartyPanel"));
	PartyPanel->SetCompact(true);
	FALKWidgetBuilder::AddToVerticalBox(Column, PartyPanel);

	FriendsPanel = Tree.ConstructWidget<UALKFriendsPanel>(UALKFriendsPanel::StaticClass(), TEXT("FriendsPanel"));
	FriendsPanel->SetCompact(true);
	FALKWidgetBuilder::AddToVerticalBox(Column, FriendsPanel, FMargin(0.f, 16.f, 0.f, 0.f), HAlign_Fill, VAlign_Top, 1.f);
}

void UALKSocialStrip::NativeOnLayoutReady()
{
	Super::NativeOnLayoutReady();

	// Left moves inside the column first (from a line's button back to the line); only at its edge does it leave.
	FCustomWidgetNavigationDelegate NavigateLeft;
	NavigateLeft.BindDynamic(this, &ThisClass::HandleNavigateLeft);
	SetNavigationRuleCustomBoundary(EUINavigation::Left, NavigateLeft);
}

void UALKSocialStrip::NativeConstruct()
{
	Super::NativeConstruct();
	if (StripSize)
	{
		StripSize->SetWidthOverride(StripWidth);
	}
	// Nothing to show without online play. The root is collapsed, so the menu that placed the column keeps the say
	// over the widget itself.
	if (UWidget* Root = GetRootWidget())
	{
		Root->SetVisibility(IsOnlineEnabled() ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
}

bool UALKSocialStrip::IsOnlineEnabled() const
{
	const UALKOnlineSubsystem* Online = UALKOnlineSubsystem::Get(this);
	return Online && Online->IsOnlineEnabled();
}

void UALKSocialStrip::SetStripWidth(float InWidth)
{
	StripWidth = FMath::Max(240.f, InWidth);
	if (StripSize)
	{
		StripSize->SetWidthOverride(StripWidth);
	}
}

bool UALKSocialStrip::FocusFirst()
{
	if (!IsOnlineEnabled())
	{
		return false;
	}
	return (PartyPanel && PartyPanel->FocusFirst()) || (FriendsPanel && FriendsPanel->FocusFirst());
}

bool UALKSocialStrip::HasFocusInside() const
{
	return HasAnyUserFocus() || HasFocusedDescendants();
}

UWidget* UALKSocialStrip::HandleNavigateLeft(EUINavigation Direction)
{
	OnNavigateOut.Broadcast();
	OnNavigateOutEvent.Broadcast();
	// The menu that listens moves the focus; with no listener it stays where it is.
	return nullptr;
}
