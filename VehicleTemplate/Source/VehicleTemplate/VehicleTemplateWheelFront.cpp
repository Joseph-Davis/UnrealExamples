// Copyright Epic Games, Inc. All Rights Reserved.

#include "VehicleTemplateWheelFront.h"
#include "UObject/ConstructorHelpers.h"

UVehicleTemplateWheelFront::UVehicleTemplateWheelFront()
{
	AxleType = EAxleType::Front;
	bAffectedBySteering = true;
	MaxSteerAngle = 40.f;
}