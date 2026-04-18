// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "BSHealthSet.generated.h"

// Convenience macro that generates the four standard GAS attribute accessors.
// Using a project-specific name avoids conflicts if the engine ever changes
// whether AttributeSet.h defines ATTRIBUTE_ACCESSORS.
#define BS_ATTRIBUTE_ACCESSORS(ClassName, PropertyName)        \
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName)               \
	GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName)               \
	GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

/**
 *  Attribute set that tracks vehicle health.
 *
 *  Health:    current hit points (clamped 0 → MaxHealth).
 *  MaxHealth: maximum hit points.
 *
 *  When Health reaches 0 the set adds the "State.Dead" Gameplay Tag to the
 *  owning ASC, which in turn triggers the UGA_RespawnVehicle ability.
 */
UCLASS()
class UBSHealthSet : public UAttributeSet
{
	GENERATED_BODY()

public:

	UBSHealthSet();

	// ── Attributes ────────────────────────────────────────────────────

	/** Current health (0 → MaxHealth) */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_Health, Category="Health")
	FGameplayAttributeData Health;
	BS_ATTRIBUTE_ACCESSORS(UBSHealthSet, Health)

	/** Maximum health */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_MaxHealth, Category="Health")
	FGameplayAttributeData MaxHealth;
	BS_ATTRIBUTE_ACCESSORS(UBSHealthSet, MaxHealth)

protected:

	// Clamp attribute values before they are written
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;

	// React to confirmed attribute changes (damage, healing, etc.)
	virtual void PostGameplayEffectExecute(const struct FGameplayEffectModCallbackData& Data) override;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:

	UFUNCTION()
	void OnRep_Health(const FGameplayAttributeData& OldHealth);

	UFUNCTION()
	void OnRep_MaxHealth(const FGameplayAttributeData& OldMaxHealth);
};
