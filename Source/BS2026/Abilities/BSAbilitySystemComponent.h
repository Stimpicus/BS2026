// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "BSAbilitySystemComponent.generated.h"

/**
 *  Ability System Component for vehicle pawns.
 *
 *  Owns the Gameplay Ability System state for a single vehicle —
 *  attributes (health), active effects, and granted abilities.
 *  Lives on the pawn; in a production title this would live on
 *  APlayerState so it persists across respawns.
 */
UCLASS(ClassGroup=(Vehicle), meta=(BlueprintSpawnableComponent))
class UBSAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()

public:

	UBSAbilitySystemComponent();

	/**
	 *  Grants every ability in AbilitiesToGrant at level 1.
	 *  Must be called on the server (authority) only.
	 */
	void GrantStartupAbilities(const TArray<TSubclassOf<UGameplayAbility>>& AbilitiesToGrant);
};
