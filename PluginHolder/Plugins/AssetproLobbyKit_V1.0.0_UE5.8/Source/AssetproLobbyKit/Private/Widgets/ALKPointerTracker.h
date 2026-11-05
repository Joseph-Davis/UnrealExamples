// Copyright 2026 CodeLikeMe. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Application/IInputProcessor.h"

/**
 * Tells real mouse movement apart from the mouse moves Slate synthesizes when content moves under a resting cursor
 * (a list scrolling with the keyboard, CommonUI placing the cursor for a gamepad). Input preprocessors only see real
 * moves, so widgets that follow the mouse ask this tracker before taking focus.
 */
class FALKPointerTracker : public IInputProcessor
{
public:
	static void Register();
	static void Unregister();

	/** True while the mouse event being handled comes from the mouse actually moving (at least one pixel). */
	static bool IsMouseMovingThisFrame();

	/** Starts the grace time again from zero (see UALKScreen::IsInActivationGracePeriod). */
	static void RestartGraceTime();

	/**
	 * Seconds since Restart Grace Time, counting at most a tenth of a second per frame: a hitch (a level load, shader
	 * compiles) counts as one short frame, so mouse moves queued during it still arrive inside the grace time.
	 */
	static float GetGraceTime();

	virtual void Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor) override;
	virtual bool HandleMouseMoveEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override;
	virtual const TCHAR* GetDebugName() const override { return TEXT("ALKPointerTracker"); }

private:
	static TSharedPtr<FALKPointerTracker> Instance;
	static uint64 LastRealMoveFrame;
	static float GraceTime;
	static double GraceStartSeconds;
};
