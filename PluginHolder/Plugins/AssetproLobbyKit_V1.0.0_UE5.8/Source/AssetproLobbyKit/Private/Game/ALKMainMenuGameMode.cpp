// Copyright 2026 CodeLikeMe. All Rights Reserved.

#include "Game/ALKMainMenuGameMode.h"
#include "Game/ALKPlayerController.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ALKMainMenuGameMode)

AALKMainMenuGameMode::AALKMainMenuGameMode()
{
	PlayerControllerClass = AALKMainMenuPlayerController::StaticClass();
	DefaultPawnClass = nullptr;
}

void AALKMainMenuGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	// Menus need no pawn, so skip RestartPlayer entirely.
}
