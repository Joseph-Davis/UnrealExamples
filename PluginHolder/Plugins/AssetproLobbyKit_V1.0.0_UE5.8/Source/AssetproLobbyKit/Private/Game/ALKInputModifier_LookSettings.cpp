// Copyright 2026 CodeLikeMe. All Rights Reserved.

#include "Game/ALKInputModifier_LookSettings.h"
#include "Settings/ALKSettingsSubsystem.h"
#include "EnhancedPlayerInput.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ALKInputModifier_LookSettings)

FInputActionValue UALKInputModifier_LookSettings::ModifyRaw_Implementation(const UEnhancedPlayerInput* PlayerInput, FInputActionValue CurrentValue, float DeltaTime)
{
	const APlayerController* PlayerController = PlayerInput ? PlayerInput->GetOuterAPlayerController() : nullptr;
	const UGameInstance* GameInstance = PlayerController ? PlayerController->GetGameInstance() : nullptr;
	const UALKSettingsSubsystem* Settings = GameInstance ? GameInstance->GetSubsystem<UALKSettingsSubsystem>() : nullptr;
	if (!Settings)
	{
		return CurrentValue;
	}

	const bool bMouse = Device == EALKLookDevice::Mouse;
	const float Sensitivity = bMouse ? Settings->GetMouseSensitivity() : Settings->GetGamepadSensitivity();
	const bool bInvertY = bMouse ? Settings->IsMouseYInverted() : Settings->IsGamepadYInverted();

	FVector Value = CurrentValue.Get<FVector>();
	Value.X *= Sensitivity;
	Value.Y *= Sensitivity * (bInvertY ? -1.f : 1.f);
	return FInputActionValue(CurrentValue.GetValueType(), Value);
}
