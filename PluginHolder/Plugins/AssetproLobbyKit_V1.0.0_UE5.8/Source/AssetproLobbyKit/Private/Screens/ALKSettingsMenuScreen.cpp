// Copyright 2026 CodeLikeMe. All Rights Reserved.

#include "Screens/ALKSettingsMenuScreen.h"
#include "ALKMenuConfig.h"
#include "ALKTheme.h"
#include "HAL/PlatformInput.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ALKSettingsMenuScreen)

#define LOCTEXT_NAMESPACE "AssetproLobbyKit"

void UALKSettingsMenuScreen::NativeBuildDefaultLayout(UWidgetTree& Tree)
{
	BuildListLayout(Tree, false, {
		FALKInputHint(LOCTEXT("KeyEnter", "Enter"), FPlatformInput::GetGamepadAcceptKey(), LOCTEXT("HintSelect", "Select")),
		FALKInputHint(LOCTEXT("KeyEsc", "Esc"), FPlatformInput::GetGamepadBackKey(), LOCTEXT("HintBack", "Back"))
			.WithAction(FSimpleDelegate::CreateWeakLambda(this, [this]() { OnBackRequested(); }))
	});
}

void UALKSettingsMenuScreen::NativeOnLayoutReady()
{
	Super::NativeOnLayoutReady();
	SystemCode = 2;
	SetListTexts(LOCTEXT("SettingsMenuTitle", "Settings"), FText::GetEmpty(), FText::GetEmpty());
}

void UALKSettingsMenuScreen::NativeApplyTheme(const UALKTheme& Theme)
{
	Super::NativeApplyTheme(Theme);
	StyleListLayout(Theme);
}

TArray<FALKMenuEntry> UALKSettingsMenuScreen::NativeGetConfigEntries() const
{
	return GetMenuConfig()->SettingsMenuEntries;
}

#undef LOCTEXT_NAMESPACE
