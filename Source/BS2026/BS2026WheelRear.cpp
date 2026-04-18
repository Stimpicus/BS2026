// Copyright Epic Games, Inc. All Rights Reserved.

#include "BS2026WheelRear.h"
#include "UObject/ConstructorHelpers.h"

UBS2026WheelRear::UBS2026WheelRear()
{
	AxleType = EAxleType::Rear;
	bAffectedByHandbrake = true;
	bAffectedByEngine = true;
}