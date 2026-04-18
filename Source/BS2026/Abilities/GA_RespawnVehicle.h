// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_RespawnVehicle.generated.h"

class ABS2026Pawn;

/**
 *  Server-only respawn ability.
 *
 *  Triggered automatically when the "State.Dead" Gameplay Tag is added to
 *  the vehicle pawn's Ability System Component (set by UBSHealthSet when
 *  Health reaches 0).
 *
 *  Execution:
 *    1. Finds the first APlayerStart in the world.
 *    2. Spawns a new vehicle pawn of VehiclePawnClass at that location.
 *    3. Possesses the new pawn via the owning APlayerController.
 *    4. Destroys the dead vehicle.
 *    5. Ends itself.
 *
 *  Replace OnPawnDestroyed respawn logic in ABS2026PlayerController
 *  with this ability for server-authoritative, game-mode-aware respawning.
 */
UCLASS()
class UGA_RespawnVehicle : public UGameplayAbility
{
	GENERATED_BODY()

public:

	UGA_RespawnVehicle();

	/** Class of vehicle pawn to spawn — set this in the Blueprint subclass */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Respawn")
	TSubclassOf<ABS2026Pawn> VehiclePawnClass;

	/** Delay in seconds between death and respawn */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Respawn")
	float RespawnDelay = 3.0f;

protected:

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	                             const FGameplayAbilityActorInfo* ActorInfo,
	                             const FGameplayAbilityActivationInfo ActivationInfo,
	                             const FGameplayEventData* TriggerEventData) override;

private:

	/** Performs the actual respawn after RespawnDelay */
	void DoRespawn(FGameplayAbilitySpecHandle Handle,
	               FGameplayAbilityActorInfo ActorInfo,
	               FGameplayAbilityActivationInfo ActivationInfo);
};
