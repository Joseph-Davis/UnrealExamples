// Copyright 2026 CodeLikeMe. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Screens/ALKMenuListScreen.h"
#include "ALKPauseMenuScreen.generated.h"

/**
 * Pause menu: the menu config's Pause Menu Entries over the game, which shows through blurred, dimmed and drained of
 * colour (theme: Pause Blur, Game Dim, Pause Desaturation). Pauses the game while open (menu config); in an online
 * game it lists the Online Pause Menu Entries instead and the match keeps running. Back or the gamepad's menu button
 * resumes.
 *
 * Designer widget names: GameBlur, DimImage, TitleText, EntriesBox.
 */
UCLASS(Blueprintable)
class ASSETPROLOBBYKIT_API UALKPauseMenuScreen : public UALKMenuListScreen
{
	GENERATED_BODY()

protected:
	virtual void NativeBuildDefaultLayout(UWidgetTree& Tree) override;
	virtual void NativeOnLayoutReady() override;
	virtual void NativeApplyTheme(const UALKTheme& Theme) override;
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual bool NativeHandleMenuKey(const FKey& Key) override;
	virtual bool OnBackRequested_Implementation() override;
	virtual TArray<FALKMenuEntry> NativeGetConfigEntries() const override;

	/** True in a lobby or a match with other players (the game is not paused and the online entries are shown). */
	bool IsOnlineGame() const;

private:
	void Resume();

	bool bPausedGame = false;

	/** 1 while this is the screen on top; fades to 0 while a screen opened from it (settings, the saves) covers it. */
	float ContentOpacity = 1.f;
};
