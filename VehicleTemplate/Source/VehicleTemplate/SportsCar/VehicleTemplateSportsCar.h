// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "VehicleTemplatePawn.h"
#include "VehicleTemplateSportsCar.generated.h"

/**
 *  Sports car wheeled vehicle implementation
 */
UCLASS(abstract)
class AVehicleTemplateSportsCar : public AVehicleTemplatePawn
{
	GENERATED_BODY()
	
public:

	AVehicleTemplateSportsCar();
};
