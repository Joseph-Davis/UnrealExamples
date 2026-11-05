// Copyright 2026 CodeLikeMe. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "InputModifiers.h"
#include "ALKInputModifier_LookSettings.generated.h"

UENUM(BlueprintType)
enum class EALKLookDevice : uint8
{
	Mouse,
	Gamepad
};

/**
 * Add this modifier to your Look action mappings (one for the mouse, one for the right stick) to apply the player's
 * Sensitivity and Invert Y settings automatically.
 */
UCLASS(NotBlueprintable, meta = (DisplayName = "Assetpro Look Settings"))
class ASSETPROLOBBYKIT_API UALKInputModifier_LookSettings : public UInputModifier
{
	GENERATED_BODY()

public:
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Settings")
	EALKLookDevice Device = EALKLookDevice::Mouse;

protected:
	virtual FInputActionValue ModifyRaw_Implementation(const UEnhancedPlayerInput* PlayerInput, FInputActionValue CurrentValue, float DeltaTime) override;
};
