// Copyright 2026 CodeLikeMe. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraModifier.h"
#include "ALKCameraModifier_FieldOfView.generated.h"

/**
 * Applies the player's Field of View setting to the view of the pawn they control, with or without a Camera Component.
 * The view is scaled by Field Of View / 90, so a camera at the engine default of 90 degrees shows exactly the chosen
 * value, and zoom effects that narrow the camera keep working. Other view targets (cutscene cameras, the spectator view
 * of a player without a pawn, such as in a main menu level) are left alone.
 * The settings subsystem adds it to every local player's camera manager while Apply Field Of View To Camera is on.
 */
UCLASS()
class ASSETPROLOBBYKIT_API UALKCameraModifier_FieldOfView : public UCameraModifier
{
	GENERATED_BODY()

public:
	/** The camera field of view that shows exactly the chosen value: the engine's default camera FOV. */
	static constexpr float ReferenceFieldOfView = 90.f;

	/** The player's Field of View setting, in degrees. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Assetpro Menu")
	float FieldOfView = ReferenceFieldOfView;

	virtual void ModifyCamera(float DeltaTime, FVector ViewLocation, FRotator ViewRotation, float FOV, FVector& NewViewLocation, FRotator& NewViewRotation, float& NewFOV) override;
};
