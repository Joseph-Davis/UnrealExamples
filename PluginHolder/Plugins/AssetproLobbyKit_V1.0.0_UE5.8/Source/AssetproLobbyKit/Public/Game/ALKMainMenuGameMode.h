// Copyright 2026 CodeLikeMe. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "ALKMainMenuGameMode.generated.h"

/**
 * Game mode for main menu levels. Set it as the GameMode Override in your menu level's World Settings: the player
 * gets no pawn and the startup menu (press start or main menu) appears automatically.
 */
UCLASS(Blueprintable)
class ASSETPROLOBBYKIT_API AALKMainMenuGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AALKMainMenuGameMode();

protected:
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;
};
