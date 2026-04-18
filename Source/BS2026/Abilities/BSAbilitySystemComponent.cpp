// Copyright Epic Games, Inc. All Rights Reserved.

#include "BSAbilitySystemComponent.h"

UBSAbilitySystemComponent::UBSAbilitySystemComponent()
{
	SetIsReplicatedByDefault(true);
}

void UBSAbilitySystemComponent::GrantStartupAbilities(
	const TArray<TSubclassOf<UGameplayAbility>>& AbilitiesToGrant)
{
	if (!IsOwnerActorAuthoritative())
	{
		return;
	}

	for (const TSubclassOf<UGameplayAbility>& AbilityClass : AbilitiesToGrant)
	{
		if (AbilityClass)
		{
			FGameplayAbilitySpec Spec(AbilityClass, /*Level=*/1, INDEX_NONE, this);
			GiveAbility(Spec);
		}
	}
}
