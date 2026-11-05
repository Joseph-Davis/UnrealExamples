// Copyright 2026 CodeLikeMe. All Rights Reserved.

#include "Screens/ALKPauseMenuScreen.h"
#include "Online/ALKOnlineSubsystem.h"
#include "Engine/World.h"
#include "ALKMenuConfig.h"
#include "ALKMenuSubsystem.h"
#include "ALKTheme.h"
#include "Widgets/ALKRootLayout.h"
#include "Widgets/CommonActivatableWidgetContainer.h"
#include "CommonActivatableWidget.h"
#include "HAL/PlatformInput.h"
#include "Kismet/GameplayStatics.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ALKPauseMenuScreen)

#define LOCTEXT_NAMESPACE "AssetproLobbyKit"

void UALKPauseMenuScreen::NativeBuildDefaultLayout(UWidgetTree& Tree)
{
	BuildListLayout(Tree, true, {
		FALKInputHint(LOCTEXT("KeyEnter", "Enter"), FPlatformInput::GetGamepadAcceptKey(), LOCTEXT("HintSelect", "Select")),
		FALKInputHint(LOCTEXT("KeyEsc", "Esc"), FPlatformInput::GetGamepadBackKey(), LOCTEXT("HintResume", "Resume"))
			.WithAction(FSimpleDelegate::CreateUObject(this, &ThisClass::Resume))
	});
}

void UALKPauseMenuScreen::NativeOnLayoutReady()
{
	Super::NativeOnLayoutReady();
	SystemCode = 1;
	SetListTexts(GetMenuConfig()->PauseTitle, FText::GetEmpty(), FText::GetEmpty());
}

bool UALKPauseMenuScreen::IsOnlineGame() const
{
	const UWorld* World = GetWorld();
	return World && World->GetNetMode() != NM_Standalone;
}

TArray<FALKMenuEntry> UALKPauseMenuScreen::NativeGetConfigEntries() const
{
	if (!IsOnlineGame())
	{
		return GetMenuConfig()->PauseMenuEntries;
	}
	// Online: the entries for a running match; the Host Panel only for the host.
	const UALKOnlineSubsystem* Online = UALKOnlineSubsystem::Get(this);
	TArray<FALKMenuEntry> Entries = GetMenuConfig()->OnlinePauseMenuEntries;
	Entries.RemoveAll([Online](const FALKMenuEntry& Entry) { return Entry.Action == EALKMenuAction::OpenAdminPanel && (!Online || !Online->IsHost()); });
	return Entries;
}

void UALKPauseMenuScreen::NativeApplyTheme(const UALKTheme& Theme)
{
	Super::NativeApplyTheme(Theme);
	StyleListLayout(Theme);
}

void UALKPauseMenuScreen::NativeOnActivated()
{
	// The title's note tells an online player that nothing is paused.
	SetListTexts(IsOnlineGame() && !GetMenuConfig()->OnlinePauseTitle.IsEmpty() ? GetMenuConfig()->OnlinePauseTitle : GetMenuConfig()->PauseTitle,
		IsOnlineGame() ? GetMenuConfig()->OnlinePauseNote : FText::GetEmpty(), FText::GetEmpty());
	ContentOpacity = 1.f;
	SetListOpacity(ContentOpacity);
	Super::NativeOnActivated();

	if (GetMenuConfig()->bPauseGameWhileOpen && !IsOnlineGame() && !UGameplayStatics::IsGamePaused(this))
	{
		bPausedGame = UGameplayStatics::SetGamePaused(this, true);
	}
}

void UALKPauseMenuScreen::NativeOnDeactivated()
{
	if (bPausedGame)
	{
		UGameplayStatics::SetGamePaused(this, false);
		bPausedGame = false;
	}
	Super::NativeOnDeactivated();
}

void UALKPauseMenuScreen::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// A screen opened from this menu (settings, the saves, the host's panel) lies on the layer above, with this menu still
	// behind it. Its panel and hints step out meanwhile, so only the blurred game shows round the screen on top instead of
	// a second, half hidden menu.
	UALKMenuSubsystem* MenuSubsystem = GetMenuSubsystem();
	const UALKRootLayout* Layout = MenuSubsystem ? MenuSubsystem->GetRootLayout() : nullptr;
	const UCommonActivatableWidgetContainerBase* MenuLayer = Layout ? Layout->GetLayer(EALKLayer::Menu) : nullptr;
	const float Wanted = MenuLayer && MenuLayer->GetActiveWidget() ? 0.f : 1.f;
	if (!FMath::IsNearlyEqual(ContentOpacity, Wanted))
	{
		ContentOpacity = FMath::FInterpConstantTo(ContentOpacity, Wanted, FMath::Min(InDeltaTime, 0.05f), 8.f);
		SetListOpacity(ContentOpacity);
	}
}

bool UALKPauseMenuScreen::NativeHandleMenuKey(const FKey& Key)
{
	// Start / Options toggles the pause menu closed again.
	if (Key == EKeys::Gamepad_Special_Right)
	{
		Resume();
		return true;
	}
	return false;
}

bool UALKPauseMenuScreen::OnBackRequested_Implementation()
{
	PlayThemeSound(EALKUISound::Back);
	Resume();
	return true;
}

void UALKPauseMenuScreen::Resume()
{
	if (UALKMenuSubsystem* MenuSubsystem = GetMenuSubsystem())
	{
		MenuSubsystem->HidePauseMenu();
	}
	else
	{
		CloseScreen();
	}
}

#undef LOCTEXT_NAMESPACE
