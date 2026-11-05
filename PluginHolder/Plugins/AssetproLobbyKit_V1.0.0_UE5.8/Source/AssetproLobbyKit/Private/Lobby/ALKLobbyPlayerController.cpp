// Copyright 2026 CodeLikeMe. All Rights Reserved.

#include "Lobby/ALKLobbyPlayerController.h"
#include "Lobby/ALKPlayerNetComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ALKLobbyPlayerController)

AALKLobbyPlayerController::AALKLobbyPlayerController()
{
	NetComponent = CreateDefaultSubobject<UALKPlayerNetComponent>(TEXT("NetComponent"));
	bShowMainMenuOnBeginPlay = false;
	bEnableQuickSaveInput = false;
}
