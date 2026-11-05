// Copyright 2026 CodeLikeMe. All Rights Reserved.

#include "Game/ALKCameraModifier_Drain.h"
#include "HAL/PlatformTime.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ALKCameraModifier_Drain)

void UALKCameraModifier_Drain::FadeTo(float InAmount, float Seconds)
{
	From = GetAmount();
	To = FMath::Clamp(InAmount, 0.f, 1.f);
	Duration = FMath::Max(0.f, Seconds);
	StartTime = FPlatformTime::Seconds();
}

float UALKCameraModifier_Drain::GetAmount() const
{
	if (Duration <= 0.f)
	{
		return To;
	}
	const float Progress = FMath::Clamp(static_cast<float>(FPlatformTime::Seconds() - StartTime) / Duration, 0.f, 1.f);
	return FMath::Lerp(From, To, FMath::SmoothStep(0.f, 1.f, Progress));
}

bool UALKCameraModifier_Drain::IsFading() const
{
	return Duration > 0.f && FPlatformTime::Seconds() - StartTime < Duration;
}

void UALKCameraModifier_Drain::ModifyPostProcess(float DeltaTime, float& PostProcessBlendWeight, FPostProcessSettings& PostProcessSettings)
{
	const float Amount = GetAmount();
	if (Amount <= KINDA_SMALL_NUMBER)
	{
		PostProcessBlendWeight = 0.f;
		return;
	}
	const float Saturation = 1.f - Amount;
	PostProcessSettings.bOverride_ColorSaturation = true;
	PostProcessSettings.ColorSaturation = FVector4(Saturation, Saturation, Saturation, 1.f);
	PostProcessBlendWeight = 1.f;
}
