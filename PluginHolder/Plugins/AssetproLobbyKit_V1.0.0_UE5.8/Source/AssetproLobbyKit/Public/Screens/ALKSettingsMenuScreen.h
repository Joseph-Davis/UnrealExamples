// Copyright 2026 CodeLikeMe. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Screens/ALKMenuListScreen.h"
#include "ALKSettingsMenuScreen.generated.h"

/**
 * Settings menu: the menu config's Settings Menu Entries (General, Video, Audio, Controls, Controller Layout...) as a
 * list under a "Settings" heading. Each entry opens its page; Back returns to the menu underneath.
 *
 * Designer widget names: BackgroundImage, TitleText, EntriesBox.
 */
UCLASS(Blueprintable)
class ASSETPROLOBBYKIT_API UALKSettingsMenuScreen : public UALKMenuListScreen
{
	GENERATED_BODY()

protected:
	virtual void NativeBuildDefaultLayout(UWidgetTree& Tree) override;
	virtual void NativeOnLayoutReady() override;
	virtual void NativeApplyTheme(const UALKTheme& Theme) override;
	virtual TArray<FALKMenuEntry> NativeGetConfigEntries() const override;
};
