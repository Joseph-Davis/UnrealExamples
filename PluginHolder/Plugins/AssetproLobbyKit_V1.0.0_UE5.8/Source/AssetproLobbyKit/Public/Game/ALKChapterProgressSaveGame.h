// Copyright 2026 CodeLikeMe. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "ALKChapterProgressSaveGame.generated.h"

/** Which chapters the player has unlocked. Written by the menu subsystem (Unlock Chapter) to the Chapter Progress Slot. */
UCLASS()
class ASSETPROLOBBYKIT_API UALKChapterProgressSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY(SaveGame)
	TArray<FName> UnlockedChapters;
};
