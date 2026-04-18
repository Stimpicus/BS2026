// Copyright Epic Games, Inc. All Rights Reserved.

#include "BSHealthSet.h"
#include "Net/UnrealNetwork.h"
#include "GameplayEffect.h"
#include "GameplayEffectExtension.h"
#include "AbilitySystemComponent.h"

UBSHealthSet::UBSHealthSet()
{
	InitHealth(100.0f);
	InitMaxHealth(100.0f);
}

void UBSHealthSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION_NOTIFY(UBSHealthSet, Health,    COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UBSHealthSet, MaxHealth, COND_None, REPNOTIFY_Always);
}

void UBSHealthSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	if (Attribute == GetHealthAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.0f, GetMaxHealth());
	}
	else if (Attribute == GetMaxHealthAttribute())
	{
		// MaxHealth must always be at least 1
		NewValue = FMath::Max(NewValue, 1.0f);
	}
}

void UBSHealthSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	if (Data.EvaluatedData.Attribute == GetHealthAttribute())
	{
		// Clamp the confirmed value
		const float ClampedHealth = FMath::Clamp(GetHealth(), 0.0f, GetMaxHealth());
		SetHealth(ClampedHealth);

		// When health reaches zero, apply the State.Dead tag so GA_RespawnVehicle fires.
		// Guard against repeated additions: loose tags are count-based, so adding the
		// tag more than once inflates the count and makes removal harder.
		if (ClampedHealth <= 0.0f)
		{
			if (UAbilitySystemComponent* ASC = GetOwningAbilitySystemComponent())
			{
				const FGameplayTag DeadTag = FGameplayTag::RequestGameplayTag(FName("State.Dead"));
				if (!ASC->HasMatchingGameplayTag(DeadTag))
				{
					ASC->AddLooseGameplayTag(DeadTag);
				}
			}
		}
	}
}

void UBSHealthSet::OnRep_Health(const FGameplayAttributeData& OldHealth)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBSHealthSet, Health, OldHealth);
}

void UBSHealthSet::OnRep_MaxHealth(const FGameplayAttributeData& OldMaxHealth)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UBSHealthSet, MaxHealth, OldMaxHealth);
}
