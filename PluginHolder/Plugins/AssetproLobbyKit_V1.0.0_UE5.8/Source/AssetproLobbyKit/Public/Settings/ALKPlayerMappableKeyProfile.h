// Copyright 2026 CodeLikeMe. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UserSettings/EnhancedInputUserSettings.h"
#include "ALKPlayerMappableKeyProfile.generated.h"

/**
 * Enhanced Input key profile used for key bindings. It keeps cleared bindings unbound in game.
 *
 * Without it, clearing a gamepad binding (or moving a button to another action) only changes the menu: Enhanced Input
 * 5.4 matches player keys to the default mapping by device type, an unbound key has no device type, so the action falls
 * back to its default button. This profile keeps the action unbound instead.
 *
 * The settings subsystem selects it automatically when the project uses the engine's default key profile class. If your
 * project sets its own class in Project Settings > Enhanced Input, derive that class from this one.
 */
UCLASS()
class ASSETPROLOBBYKIT_API UALKPlayerMappableKeyProfile : public UEnhancedPlayerMappableKeyProfile
{
	GENERATED_BODY()

public:
	virtual int32 GetPlayerMappedKeysForRebuildControlMappings(const FEnhancedActionKeyMapping& DefaultMapping, TArray<FKey>& OutKeys) const override;
};
