// Copyright Epic Games, Inc. All Rights Reserved.

#include "BS2026WheelFront.h"
#include "UObject/ConstructorHelpers.h"

UBS2026WheelFront::UBS2026WheelFront()
{
	AxleType = EAxleType::Front;
	bAffectedBySteering = true;
	MaxSteerAngle = 40.f;
}