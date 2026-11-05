// Copyright 2026 CodeLikeMe. All Rights Reserved.

#include "Widgets/ALKInputLabels.h"
#include "ALKDeveloperSettings.h"
#include "ALKMenuConfig.h"
#include "CommonInputSubsystem.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/InputDeviceSubsystem.h"
#include "GameFramework/InputSettings.h"

#define LOCTEXT_NAMESPACE "AssetproLobbyKit"

FText FALKInputHint::GetKeyText(bool bGamepad, const ULocalPlayer* LocalPlayer) const
{
	if (!bGamepad)
	{
		return KeyboardKey;
	}
	if (GamepadKey.IsValid())
	{
		return FALKInputLabels::GetKeyLabel(GamepadKey, LocalPlayer);
	}
	return GamepadText;
}

EALKGamepadLabels FALKInputLabels::GetGamepadLabels(const ULocalPlayer* LocalPlayer)
{
	const EALKGamepadLabels Forced = UALKDeveloperSettings::GetMenuConfig()->GamepadLabels;
	if (Forced != EALKGamepadLabels::Auto)
	{
		return Forced;
	}

	FString DeviceName;
	if (LocalPlayer)
	{
		if (const UInputDeviceSubsystem* Devices = UInputDeviceSubsystem::Get())
		{
			const FHardwareDeviceIdentifier Hardware = Devices->GetMostRecentlyUsedHardwareDevice(LocalPlayer->GetPlatformUserId());
			DeviceName = Hardware.HardwareDeviceIdentifier.ToString() + TEXT(" ") + Hardware.InputClassName.ToString();
		}
		if (const UCommonInputSubsystem* Input = UCommonInputSubsystem::Get(LocalPlayer))
		{
			DeviceName += TEXT(" ") + Input->GetCurrentGamepadName().ToString();
		}
	}

	for (const TCHAR* Token : { TEXT("DualSense"), TEXT("DualShock"), TEXT("PlayStation"), TEXT("PS4"), TEXT("PS5") })
	{
		if (DeviceName.Contains(Token))
		{
			return EALKGamepadLabels::PlayStation;
		}
	}
	if (DeviceName.Contains(TEXT("Switch")) || DeviceName.Contains(TEXT("Nintendo")))
	{
		return EALKGamepadLabels::Nintendo;
	}
	return EALKGamepadLabels::Xbox;
}

FText FALKInputLabels::GetKeyLabel(const FKey& Key, const ULocalPlayer* LocalPlayer)
{
	if (!Key.IsValid())
	{
		return LOCTEXT("Unbound", "-");
	}
	if (!Key.IsGamepadKey())
	{
		return Key.GetDisplayName(false);
	}

	struct FPadLabel
	{
		FKey Key;
		FText Xbox;
		FText PlayStation;
		FText Nintendo;
	};
	// Buttons are named by physical position, so the same key shows the name printed on each controller.
	static const TArray<FPadLabel> Labels = {
		{ EKeys::Gamepad_FaceButton_Bottom, LOCTEXT("XboxA", "A"), LOCTEXT("PsCross", "Cross"), LOCTEXT("NinB", "B") },
		{ EKeys::Gamepad_FaceButton_Right, LOCTEXT("XboxB", "B"), LOCTEXT("PsCircle", "Circle"), LOCTEXT("NinA", "A") },
		{ EKeys::Gamepad_FaceButton_Left, LOCTEXT("XboxX", "X"), LOCTEXT("PsSquare", "Square"), LOCTEXT("NinY", "Y") },
		{ EKeys::Gamepad_FaceButton_Top, LOCTEXT("XboxY", "Y"), LOCTEXT("PsTriangle", "Triangle"), LOCTEXT("NinX", "X") },
		{ EKeys::Gamepad_LeftShoulder, LOCTEXT("XboxLB", "LB"), LOCTEXT("PsL1", "L1"), LOCTEXT("NinL", "L") },
		{ EKeys::Gamepad_RightShoulder, LOCTEXT("XboxRB", "RB"), LOCTEXT("PsR1", "R1"), LOCTEXT("NinR", "R") },
		{ EKeys::Gamepad_LeftTrigger, LOCTEXT("XboxLT", "LT"), LOCTEXT("PsL2", "L2"), LOCTEXT("NinZL", "ZL") },
		{ EKeys::Gamepad_RightTrigger, LOCTEXT("XboxRT", "RT"), LOCTEXT("PsR2", "R2"), LOCTEXT("NinZR", "ZR") },
		{ EKeys::Gamepad_LeftThumbstick, LOCTEXT("XboxL3", "L3"), LOCTEXT("PsL3", "L3"), LOCTEXT("NinLStick", "L Stick") },
		{ EKeys::Gamepad_RightThumbstick, LOCTEXT("XboxR3", "R3"), LOCTEXT("PsR3", "R3"), LOCTEXT("NinRStick", "R Stick") },
		{ EKeys::Gamepad_Special_Left, LOCTEXT("XboxView", "View"), LOCTEXT("PsCreate", "Create"), LOCTEXT("NinMinus", "-") },
		{ EKeys::Gamepad_Special_Right, LOCTEXT("XboxMenu", "Menu"), LOCTEXT("PsOptions", "Options"), LOCTEXT("NinPlus", "+") },
		{ EKeys::Gamepad_DPad_Up, LOCTEXT("DPadUp", "D-Pad Up"), LOCTEXT("DPadUp", "D-Pad Up"), LOCTEXT("DPadUp", "D-Pad Up") },
		{ EKeys::Gamepad_DPad_Down, LOCTEXT("DPadDown", "D-Pad Down"), LOCTEXT("DPadDown", "D-Pad Down"), LOCTEXT("DPadDown", "D-Pad Down") },
		{ EKeys::Gamepad_DPad_Left, LOCTEXT("DPadLeft", "D-Pad Left"), LOCTEXT("DPadLeft", "D-Pad Left"), LOCTEXT("DPadLeft", "D-Pad Left") },
		{ EKeys::Gamepad_DPad_Right, LOCTEXT("DPadRight", "D-Pad Right"), LOCTEXT("DPadRight", "D-Pad Right"), LOCTEXT("DPadRight", "D-Pad Right") },
	};

	for (const FPadLabel& Label : Labels)
	{
		if (Label.Key == Key)
		{
			switch (GetGamepadLabels(LocalPlayer))
			{
			case EALKGamepadLabels::PlayStation: return Label.PlayStation;
			case EALKGamepadLabels::Nintendo: return Label.Nintendo;
			default: return Label.Xbox;
			}
		}
	}

	return Key.GetDisplayName(false);
}

#undef LOCTEXT_NAMESPACE
