// Copyright Epic Games, Inc. All Rights Reserved.

#include "GA_RespawnVehicle.h"
#include "BS2026Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerStart.h"
#include "TimerManager.h"
#include "BS2026.h"

UGA_RespawnVehicle::UGA_RespawnVehicle()
{
	// Respawn must only execute on the server
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	InstancingPolicy   = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	// Activate automatically when State.Dead is added to the owning ASC
	FAbilityTriggerData TriggerData;
	TriggerData.TriggerTag    = FGameplayTag::RequestGameplayTag(FName("State.Dead"));
	TriggerData.TriggerSource = EGameplayAbilityTriggerSource::OwnedTagAdded;
	AbilityTriggers.Add(TriggerData);
}

void UGA_RespawnVehicle::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
                                         const FGameplayAbilityActorInfo* ActorInfo,
                                         const FGameplayAbilityActivationInfo ActivationInfo,
                                         const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (!ActorInfo || !ActorInfo->IsNetAuthority())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// Copy info by value so the lambda can safely capture it
	FGameplayAbilityActorInfo ActorInfoCopy = *ActorInfo;

	if (RespawnDelay > 0.0f)
	{
		FTimerHandle TimerHandle;
		GetWorld()->GetTimerManager().SetTimer(TimerHandle,
			FTimerDelegate::CreateUObject(this, &UGA_RespawnVehicle::DoRespawn,
				Handle, ActorInfoCopy, ActivationInfo),
			RespawnDelay, false);
	}
	else
	{
		DoRespawn(Handle, ActorInfoCopy, ActivationInfo);
	}
}

void UGA_RespawnVehicle::DoRespawn(FGameplayAbilitySpecHandle Handle,
                                   FGameplayAbilityActorInfo ActorInfo,
                                   FGameplayAbilityActivationInfo ActivationInfo)
{
	if (!VehiclePawnClass)
	{
		UE_LOG(LogBS2026, Warning,
			TEXT("GA_RespawnVehicle: VehiclePawnClass is not set — respawn aborted."));
		EndAbility(Handle, &ActorInfo, ActivationInfo, true, true);
		return;
	}

	APlayerController* PC = Cast<APlayerController>(ActorInfo.PlayerController.Get());
	if (!PC)
	{
		EndAbility(Handle, &ActorInfo, ActivationInfo, true, true);
		return;
	}

	// Find the first PlayerStart in the world
	TArray<AActor*> StartActors;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), APlayerStart::StaticClass(), StartActors);

	if (StartActors.Num() == 0)
	{
		UE_LOG(LogBS2026, Warning,
			TEXT("GA_RespawnVehicle: No APlayerStart found in the world."));
		EndAbility(Handle, &ActorInfo, ActivationInfo, true, true);
		return;
	}

	const FTransform SpawnTransform = StartActors[0]->GetActorTransform();

	// Spawn the new vehicle
	ABS2026Pawn* NewVehicle =
		GetWorld()->SpawnActor<ABS2026Pawn>(VehiclePawnClass, SpawnTransform);

	if (NewVehicle)
	{
		// Destroy the dead vehicle before possessing the new one
		if (AActor* OldAvatar = ActorInfo.AvatarActor.Get())
		{
			OldAvatar->Destroy();
		}

		PC->Possess(NewVehicle);
	}
	else
	{
		UE_LOG(LogBS2026, Error,
			TEXT("GA_RespawnVehicle: Failed to spawn new vehicle."));
	}

	EndAbility(Handle, &ActorInfo, ActivationInfo, true, false);
}
