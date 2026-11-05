// Copyright 2026 CodeLikeMe. All Rights Reserved.

#include "Screens/ALKMainMenuScreen.h"
#include "ALKMenuConfig.h"
#include "ALKTheme.h"
#include "OnlineUI/ALKSocialStrip.h"
#include "Widgets/ALKButton.h"
#include "Widgets/ALKWidgetBuilder.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "HAL/PlatformInput.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ALKMainMenuScreen)

#define LOCTEXT_NAMESPACE "AssetproLobbyKit"

UALKMainMenuScreen::UALKMainMenuScreen(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCloseOnBack = false;
	TitleRole = EALKTextRole::Title;
	SystemCode = 0;
	// The game's key art (Main Menu Background Image) shows here instead of the theme's backdrop.
	bShowKeyArt = true;
}

void UALKMainMenuScreen::NativeBuildDefaultLayout(UWidgetTree& Tree)
{
	// The Party hint only when the right column is there.
	UClass* SideClass = GetMenuConfig()->MainMenuSideWidgetClass.LoadSynchronous();
	TArray<FALKInputHint> Hints = { FALKInputHint(LOCTEXT("KeyEnter", "Enter"), FPlatformInput::GetGamepadAcceptKey(), LOCTEXT("HintSelect", "Select")) };
	if (SideClass && SideClass->IsChildOf(UALKSocialStrip::StaticClass()))
	{
		Hints.Add(FALKInputHint(LOCTEXT("KeyY", "Y"), EKeys::Gamepad_FaceButton_Top, LOCTEXT("HubHintParty", "Party"))
			.WithAction(FSimpleDelegate::CreateWeakLambda(this, [this]() { FocusSideColumn(); })));
	}
	BuildListLayout(Tree, false, Hints);

	if (UOverlay* Root = SideClass ? Cast<UOverlay>(Tree.RootWidget) : nullptr)
	{
		SideWidget = Tree.ConstructWidget<UUserWidget>(SideClass, TEXT("SideWidget"));
		FALKWidgetBuilder::AddToOverlay(Root, SideWidget, HAlign_Right, VAlign_Center);
	}
}

UALKSocialStrip* UALKMainMenuScreen::GetSocialStrip() const
{
	return Cast<UALKSocialStrip>(SideWidget);
}

bool UALKMainMenuScreen::FocusSideColumn()
{
	UALKSocialStrip* Strip = GetSocialStrip();
	if (!Strip || !Strip->IsVisible() || Strip->HasFocusInside())
	{
		return false;
	}
	UALKButton* Focused = nullptr;
	for (UALKButton* Button : EntryButtons)
	{
		if (Button && Button->HasFocusedDescendants())
		{
			Focused = Button;
			break;
		}
	}
	if (!Strip->FocusFirst())
	{
		return false;
	}
	EntryBeforeSide = Focused;
	return true;
}

void UALKMainMenuScreen::HandleSideNavigateOut()
{
	// Back to the entry the player came from, or the first one that can be picked.
	UALKButton* Target = EntryBeforeSide.Get();
	if (!Target || !Target->GetIsEnabled())
	{
		Target = nullptr;
		for (UALKButton* Button : EntryButtons)
		{
			if (Button && Button->GetIsEnabled() && Button->IsVisible())
			{
				Target = Button;
				break;
			}
		}
	}
	if (Target)
	{
		Target->SetFocus();
	}
}

bool UALKMainMenuScreen::NativeHandleMenuKey(const FKey& Key)
{
	if ((Key == EKeys::Y || Key == EKeys::Gamepad_FaceButton_Top) && FocusSideColumn())
	{
		return true;
	}
	return Super::NativeHandleMenuKey(Key);
}

FNavigationReply UALKMainMenuScreen::NativeOnNavigation(const FGeometry& MyGeometry, const FNavigationEvent& InNavigationEvent, const FNavigationReply& InDefaultReply)
{
	// Right from the entries goes into the side column (Left inside it comes back through On Navigate Out).
	const UALKSocialStrip* Strip = GetSocialStrip();
	if (InNavigationEvent.GetNavigationType() == EUINavigation::Right && Strip && !Strip->HasFocusInside() && FocusSideColumn())
	{
		return FNavigationReply::Stop();
	}
	return Super::NativeOnNavigation(MyGeometry, InNavigationEvent, InDefaultReply);
}

void UALKMainMenuScreen::NativeOnLayoutReady()
{
	Super::NativeOnLayoutReady();

	const UALKMenuConfig* Config = GetMenuConfig();
	SetListTexts(Config->GameTitle, Config->GameSubtitle, Config->GetResolvedVersionText(), Config->HasGameLogo() ? &Config->GameLogo : nullptr,
		LOCTEXT("MainMenuLabel", "Main Menu"));

	if (UALKSocialStrip* Strip = GetSocialStrip())
	{
		Strip->OnNavigateOut.RemoveAll(this);
		Strip->OnNavigateOut.AddUObject(this, &ThisClass::HandleSideNavigateOut);
	}
}

void UALKMainMenuScreen::NativeApplyTheme(const UALKTheme& Theme)
{
	Super::NativeApplyTheme(Theme);
	StyleListLayout(Theme, Theme.MainMenuBackgroundOpacity);
	if (UOverlaySlot* SideSlot = SideWidget ? Cast<UOverlaySlot>(SideWidget->Slot) : nullptr)
	{
		// The column ends above the footer's line (the hints, the line and the room under them, as Style List Layout
		// places them), not behind it.
		FMargin SidePadding = Theme.ScreenPadding;
		SidePadding.Bottom = FMath::Max(SidePadding.Bottom, FMath::Max(24.f, Theme.ScreenPadding.Bottom * 0.4f) + 68.f);
		SideSlot->SetPadding(SidePadding);
	}
}

TArray<FALKMenuEntry> UALKMainMenuScreen::NativeGetConfigEntries() const
{
	return GetMenuConfig()->MainMenuEntries;
}

bool UALKMainMenuScreen::OnBackRequested_Implementation()
{
	if (!GetMenuConfig()->bBackOnMainMenuOffersQuit)
	{
		return false;
	}

	for (const FALKMenuEntry& Entry : DisplayedEntries)
	{
		if (Entry.Action == EALKMenuAction::QuitGame)
		{
			PlayThemeSound(EALKUISound::Back);
			OnEntryClicked(Entry);
			return true;
		}
	}
	return false;
}

#undef LOCTEXT_NAMESPACE
