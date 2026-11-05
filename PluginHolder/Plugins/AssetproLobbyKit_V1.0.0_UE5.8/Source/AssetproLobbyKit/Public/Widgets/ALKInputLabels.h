// Copyright 2026 CodeLikeMe. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "ALKTypes.h"

class ULocalPlayer;

/**
 * One "key + label" hint, such as "[Esc] Back" in a screen footer. The key follows the input method in use: the keyboard
 * key text for mouse and keyboard, the button printed on the player's controller for gamepads (A / Cross / B...).
 * A hint with an Action is a button: clicking it runs the action.
 */
struct ASSETPROLOBBYKIT_API FALKInputHint
{
	FText KeyboardKey;
	FText GamepadText;
	FKey GamepadKey;
	FText Label;

	/** Runs when the hint is clicked. Hints without one only show the key. */
	FSimpleDelegate Action;

	/** Show the key only while the button has focus (the accept button on whichever dialog button is focused). */
	bool bOnlyWhenFocused = false;

	FALKInputHint() = default;

	FALKInputHint(const FText& InKeyboardKey, const FKey& InGamepadKey, const FText& InLabel)
		: KeyboardKey(InKeyboardKey), GamepadKey(InGamepadKey), Label(InLabel)
	{
	}

	/** For hints that are not a single button (e.g. "Left / Right"). */
	FALKInputHint(const FText& InKeyboardKey, const FText& InGamepadText, const FText& InLabel)
		: KeyboardKey(InKeyboardKey), GamepadText(InGamepadText), Label(InLabel)
	{
	}

	/** Makes the hint clickable. */
	FALKInputHint& WithAction(FSimpleDelegate InAction)
	{
		Action = MoveTemp(InAction);
		return *this;
	}

	/** The key to show for a gamepad (true) or mouse and keyboard (false). Empty when the hint has none for it. */
	FText GetKeyText(bool bGamepad, const ULocalPlayer* LocalPlayer) const;
};

/** Player facing names for keys and gamepad buttons (Xbox, PlayStation or Nintendo style). */
struct ASSETPROLOBBYKIT_API FALKInputLabels
{
	/** Short display name for a key. Gamepad buttons follow the controller family of the player. */
	static FText GetKeyLabel(const FKey& Key, const ULocalPlayer* LocalPlayer);

	/** The label style to use: the menu config override, or the family of the player's most recent controller. */
	static EALKGamepadLabels GetGamepadLabels(const ULocalPlayer* LocalPlayer);
};
