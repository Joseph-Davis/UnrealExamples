// Copyright 2026 CodeLikeMe. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraModifier.h"
#include "ALKCameraModifier_Drain.generated.h"

/**
 * Drains the colour from the player's view: the game behind the pause menu turns black and white (theme: Pause
 * Desaturation). Fades in real time, so it also fades while the game is paused; the menu subsystem adds it and keeps
 * the view updated while the pause menu is open.
 */
UCLASS()
class ASSETPROLOBBYKIT_API UALKCameraModifier_Drain : public UCameraModifier
{
	GENERATED_BODY()

public:
	/** Fades towards Amount (0 keeps the colour, 1 is black and white) over Seconds of real time. */
	void FadeTo(float InAmount, float Seconds = 0.4f);

	/** How much colour is drained now (0 to 1). */
	float GetAmount() const;

	/** True while a fade is still running. */
	bool IsFading() const;

	virtual void ModifyPostProcess(float DeltaTime, float& PostProcessBlendWeight, FPostProcessSettings& PostProcessSettings) override;

private:
	float From = 0.f;
	float To = 0.f;
	float Duration = 0.4f;
	double StartTime = 0.0;
};
