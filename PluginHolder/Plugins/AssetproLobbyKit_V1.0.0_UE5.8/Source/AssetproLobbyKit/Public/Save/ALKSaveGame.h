// Copyright 2026 CodeLikeMe. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Save/ALKSaveTypes.h"
#include "ALKSaveGame.generated.h"

/**
 * The save the slot screens write and read. It already holds what loading needs with no code: the level, the player's
 * position and view, the chapter and the playtime. Loading puts the player back there.
 *
 * Your game's own data: make a Blueprint (or C++) child of this class with your variables, select it as the menu config's
 * Save Game Class, fill it in On Gather Save Data and read it back in On Save Loaded (Assetpro Save Subsystem).
 */
UCLASS(Blueprintable, BlueprintType)
class ASSETPROLOBBYKIT_API UALKSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	/** The slot's details (also kept in the index; here so a lost index can be rebuilt). */
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Assetpro Save")
	FALKSaveSlotInfo Info;

	/** The player pawn's location, rotation and scale when saved. */
	UPROPERTY(BlueprintReadWrite, SaveGame, Category = "Assetpro Save")
	FTransform PlayerTransform;

	/** Where the player was looking (the controller's rotation). */
	UPROPERTY(BlueprintReadWrite, SaveGame, Category = "Assetpro Save")
	FRotator ControlRotation = FRotator::ZeroRotator;

	/** False when there was no player pawn to save; loading then leaves the player at the level's Player Start. */
	UPROPERTY(BlueprintReadWrite, SaveGame, Category = "Assetpro Save")
	bool bHasPlayerTransform = false;
};

/** The list of saves and their screenshots (one small file, see Save Slot Prefix). */
UCLASS()
class ASSETPROLOBBYKIT_API UALKSaveIndex : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY()
	TArray<FALKSaveSlotInfo> Slots;

	UPROPERTY()
	TArray<FALKSaveThumbnail> Thumbnails;

	/** Which autosave slot the next autosave replaces (1..Autosave Slots). */
	UPROPERTY()
	int32 NextAutosave = 1;
};
