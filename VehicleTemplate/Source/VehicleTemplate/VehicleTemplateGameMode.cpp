// Copyright Epic Games, Inc. All Rights Reserved.

#include "VehicleTemplateGameMode.h"
#include "VehicleTemplatePlayerController.h"

AVehicleTemplateGameMode::AVehicleTemplateGameMode()
{
	PlayerControllerClass = AVehicleTemplatePlayerController::StaticClass();
}
