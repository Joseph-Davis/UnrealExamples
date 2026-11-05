// Copyright 2026 CodeLikeMe. All Rights Reserved.

#include "Settings/ALKPlayerMappableKeyProfile.h"
#include "EnhancedActionKeyMapping.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ALKPlayerMappableKeyProfile)

int32 UALKPlayerMappableKeyProfile::GetPlayerMappedKeysForRebuildControlMappings(const FEnhancedActionKeyMapping& DefaultMapping, TArray<FKey>& OutKeys) const
{
	const int32 NumKeys = Super::GetPlayerMappedKeysForRebuildControlMappings(DefaultMapping, OutKeys);
	if (NumKeys > 0 || !DefaultMapping.IsPlayerMappable())
	{
		return NumKeys;
	}

	// No player key of this device was found. If that is because the player cleared the device's default mapping, keep
	// the action unbound: returning nothing would make Enhanced Input fall back to the default key.
	if (const FKeyMappingRow* Row = FindKeyMappingRow(DefaultMapping.GetMappingName()))
	{
		const bool bGamepad = DefaultMapping.Key.IsGamepadKey();
		for (const FPlayerKeyMapping& Mapping : Row->Mappings)
		{
			const FKey& DefaultKey = Mapping.GetDefaultKey();
			if (DefaultKey.IsValid() && DefaultKey.IsGamepadKey() == bGamepad && !Mapping.GetCurrentKey().IsValid())
			{
				OutKeys.Add(EKeys::Invalid);
				return OutKeys.Num();
			}
		}
	}
	return 0;
}
