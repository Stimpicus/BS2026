// Copyright Epic Games, Inc. All Rights Reserved.

#include "BS2026UI.h"

void UBS2026UI::UpdateSpeed(float NewSpeed)
{
	// format the speed to KPH or MPH
	constexpr float CmPerSecondToMPH = 0.0223694f;
	constexpr float CmPerSecondToKPH = 0.036f;
	float FormattedSpeed = FMath::Abs(NewSpeed) * (bIsMPH ? CmPerSecondToMPH : CmPerSecondToKPH);

	// call the Blueprint handler
	OnSpeedUpdate(FormattedSpeed);
}

void UBS2026UI::UpdateGear(int32 NewGear)
{
	// call the Blueprint handler
	OnGearUpdate(NewGear);
}

void UBS2026UI::UpdateHealth(float NewHealth)
{
	// call the Blueprint handler — no conversion needed, value is already in 0–MaxHealth range
	OnHealthUpdate(NewHealth);
}
