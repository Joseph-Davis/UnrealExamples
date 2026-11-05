// Copyright 2026 CodeLikeMe. All Rights Reserved.

#include "Game/ALKCameraModifier_FieldOfView.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ALKCameraModifier_FieldOfView)

void UALKCameraModifier_FieldOfView::ModifyCamera(float DeltaTime, FVector ViewLocation, FRotator ViewRotation, float FOV, FVector& NewViewLocation, FRotator& NewViewRotation, float& NewFOV)
{
	Super::ModifyCamera(DeltaTime, ViewLocation, ViewRotation, FOV, NewViewLocation, NewViewRotation, NewFOV);

	// Only the view of the pawn the player controls. Cutscene cameras keep the framing they were authored with, and so does
	// the spectator pawn the engine gives a player without a pawn (a main menu level, for one).
	const APlayerController* PlayerController = CameraOwner ? CameraOwner->GetOwningPlayerController() : nullptr;
	const AActor* ViewTarget = GetViewTarget();
	if (!PlayerController || !ViewTarget || ViewTarget != PlayerController->GetPawn())
	{
		return;
	}

	const float Scale = FMath::Lerp(1.f, FieldOfView / ReferenceFieldOfView, Alpha);
	NewFOV = FMath::Clamp(FOV * Scale, 5.f, 170.f);
}
