// Copyright 2026 CodeLikeMe. All Rights Reserved.

#include "Widgets/ALKPointerTracker.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/PlatformTime.h"

TSharedPtr<FALKPointerTracker> FALKPointerTracker::Instance;
uint64 FALKPointerTracker::LastRealMoveFrame = MAX_uint64;
float FALKPointerTracker::GraceTime = 1000.f;
double FALKPointerTracker::GraceStartSeconds = -1000.0;

void FALKPointerTracker::Register()
{
	if (!Instance.IsValid() && FSlateApplication::IsInitialized())
	{
		Instance = MakeShared<FALKPointerTracker>();
		FSlateApplication::Get().RegisterInputPreProcessor(Instance);
	}
}

void FALKPointerTracker::Unregister()
{
	if (Instance.IsValid() && FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().UnregisterInputPreProcessor(Instance);
	}
	Instance.Reset();
}

bool FALKPointerTracker::IsMouseMovingThisFrame()
{
	// Without the tracker (no Slate application yet) treat every move as real, as before.
	return !Instance.IsValid() || LastRealMoveFrame == GFrameCounter;
}

void FALKPointerTracker::RestartGraceTime()
{
	GraceTime = 0.f;
	GraceStartSeconds = FPlatformTime::Seconds();
}

float FALKPointerTracker::GetGraceTime()
{
	// Without the tracker (no Slate application yet) nothing ticks the grace time, so use the clock.
	return Instance.IsValid() ? GraceTime : static_cast<float>(FPlatformTime::Seconds() - GraceStartSeconds);
}

void FALKPointerTracker::Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor)
{
	// Slate ticks input preprocessors once per frame, after that frame's input.
	if (GraceTime < 10.f)
	{
		GraceTime += FMath::Min(DeltaTime, 0.1f);
	}
}

bool FALKPointerTracker::HandleMouseMoveEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent)
{
	if (FVector2D(MouseEvent.GetCursorDelta()).SizeSquared() >= 1.0)
	{
		LastRealMoveFrame = GFrameCounter;
	}
	// Observe only; never consume.
	return false;
}
