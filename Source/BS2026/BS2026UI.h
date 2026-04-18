// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BS2026UI.generated.h"

/**
 *  Simple Vehicle HUD class
 *  Displays the current speed, gear, and health.
 *  Widget setup is handled in a Blueprint subclass.
 */
UCLASS(abstract)
class UBS2026UI : public UUserWidget
{
	GENERATED_BODY()

protected:

	/** Controls the display of speed in Km/h or MPH */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Vehicle")
	bool bIsMPH = false;

public:

	/** Called to update the speed display */
	void UpdateSpeed(float NewSpeed);

	/** Called to update the gear display */
	void UpdateGear(int32 NewGear);

	/**
	 *  Called when the vehicle's Health attribute changes via GAS.
	 *  Delivers the raw attribute value (0–MaxHealth) to the Blueprint widget
	 *  without requiring per-frame polling.
	 */
	void UpdateHealth(float NewHealth);

protected:

	/** Implemented in Blueprint to display the new speed */
	UFUNCTION(BlueprintImplementableEvent, Category="Vehicle")
	void OnSpeedUpdate(float NewSpeed);

	/** Implemented in Blueprint to display the new gear */
	UFUNCTION(BlueprintImplementableEvent, Category="Vehicle")
	void OnGearUpdate(int32 NewGear);

	/** Implemented in Blueprint to display the new health value */
	UFUNCTION(BlueprintImplementableEvent, Category="Vehicle")
	void OnHealthUpdate(float NewHealth);
};
