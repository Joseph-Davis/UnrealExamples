// Copyright Epic Games, Inc. All Rights Reserved.

#include "VehicleTemplateWheelRear.h"
#include "UObject/ConstructorHelpers.h"

UVehicleTemplateWheelRear::UVehicleTemplateWheelRear()
{
	AxleType = EAxleType::Rear;
	bAffectedByHandbrake = true;
	bAffectedByEngine = true;
}