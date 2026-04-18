// Copyright Epic Games, Inc. All Rights Reserved.

#include "GA_FireWeapon.h"
#include "BS2026Pawn.h"
#include "BSNetworkPredictionComponent.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "GameplayEffectTypes.h"
#include "Components/SkeletalMeshComponent.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "BS2026.h"

UGA_FireWeapon::UGA_FireWeapon()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
}

void UGA_FireWeapon::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
                                     const FGameplayAbilityActorInfo* ActorInfo,
                                     const FGameplayAbilityActivationInfo ActivationInfo,
                                     const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	FVector TraceStart, TraceEnd;
	GetTracePoints(ActorInfo, TraceStart, TraceEnd);

	// Server path: perform the authoritative, lag-compensated trace
	if (ActorInfo->IsNetAuthority())
	{
		float ClientPingSec = 0.0f;
		if (APlayerController* PC = Cast<APlayerController>(ActorInfo->PlayerController.Get()))
		{
			if (PC->PlayerState)
			{
				ClientPingSec = PC->PlayerState->GetPingInMilliseconds() / 1000.0f;
			}
		}

		ServerPerformHitTrace(TraceStart, TraceEnd, ClientPingSec);
	}

	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

void UGA_FireWeapon::GetTracePoints(const FGameplayAbilityActorInfo* ActorInfo,
                                    FVector& OutStart, FVector& OutEnd) const
{
	AActor* Avatar = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	if (!Avatar)
	{
		OutStart = OutEnd = FVector::ZeroVector;
		return;
	}

	if (USkeletalMeshComponent* Mesh = Avatar->FindComponentByClass<USkeletalMeshComponent>())
	{
		if (Mesh->DoesSocketExist(WeaponSocketName))
		{
			OutStart = Mesh->GetSocketLocation(WeaponSocketName);
			OutEnd   = OutStart + Mesh->GetSocketRotation(WeaponSocketName).Vector() * WeaponRange;
			return;
		}
	}

	// Fallback: fire straight ahead from the actor's origin
	OutStart = Avatar->GetActorLocation();
	OutEnd   = OutStart + Avatar->GetActorForwardVector() * WeaponRange;
}

void UGA_FireWeapon::ServerPerformHitTrace(const FVector& TraceStart,
                                           const FVector& TraceEnd,
                                           float ClientPingSec)
{
	const float ServerNow    = GetWorld()->GetTimeSeconds();
	const float RewindTarget = ServerNow - FMath::Min(ClientPingSec, MaxRewindWindow);

	// ── Rewind all remote vehicles to the client's fire time ─────────
	TArray<ABS2026Pawn*> RewoundVehicles;
	TArray<FTransform>   OriginalTransforms;

	for (TActorIterator<ABS2026Pawn> It(GetWorld()); It; ++It)
	{
		ABS2026Pawn* OtherVehicle = *It;

		// Skip the shooter
		if (OtherVehicle == GetAvatarActorFromActorInfo())
		{
			continue;
		}

		UBSNetworkPredictionComponent* PredComp =
			OtherVehicle->FindComponentByClass<UBSNetworkPredictionComponent>();
		if (!PredComp)
		{
			continue;
		}

		FBSPhysicsState RewindState;
		if (PredComp->GetStateAtTime(RewindTarget, RewindState))
		{
			OriginalTransforms.Add(OtherVehicle->GetActorTransform());
			OtherVehicle->SetActorLocationAndRotation(
				RewindState.Position, RewindState.Rotation,
				false, nullptr, ETeleportType::TeleportPhysics);
			RewoundVehicles.Add(OtherVehicle);
		}
	}

	// ── Perform the hit trace ─────────────────────────────────────────
	FHitResult HitResult;
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(GetAvatarActorFromActorInfo());

	bool bHit = false;

	if (SweepRadius > 0.0f)
	{
		bHit = GetWorld()->SweepSingleByChannel(
			HitResult, TraceStart, TraceEnd,
			FQuat::Identity,
			ECC_Vehicle,
			FCollisionShape::MakeSphere(SweepRadius),
			QueryParams);
	}
	else
	{
		bHit = GetWorld()->LineTraceSingleByChannel(
			HitResult, TraceStart, TraceEnd, ECC_Vehicle, QueryParams);
	}

	// ── Restore all rewound vehicles ──────────────────────────────────
	for (int32 i = 0; i < RewoundVehicles.Num(); ++i)
	{
		RewoundVehicles[i]->SetActorTransform(
			OriginalTransforms[i], false, nullptr, ETeleportType::TeleportPhysics);
	}

	// ── Apply damage and cue if we hit a vehicle ──────────────────────
	if (bHit)
	{
		if (ABS2026Pawn* HitVehicle = Cast<ABS2026Pawn>(HitResult.GetActor()))
		{
			ApplyDamageToTarget(HitVehicle);

			if (HitCueTag.IsValid())
			{
				FGameplayCueParameters CueParams;
				CueParams.Location = HitResult.ImpactPoint;
				CueParams.Normal = HitResult.ImpactNormal;

				if (UAbilitySystemComponent* HitASC =
					UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(HitVehicle))
				{
					HitASC->ExecuteGameplayCue(HitCueTag, CueParams);
				}
			}
		}
	}
}

void UGA_FireWeapon::ApplyDamageToTarget(ABS2026Pawn* TargetVehicle)
{
	if (!DamageEffectClass || !TargetVehicle)
	{
		return;
	}

	UAbilitySystemComponent* TargetASC =
		UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetVehicle);
	UAbilitySystemComponent* SourceASC = GetAbilitySystemComponentFromActorInfo();

	if (!TargetASC || !SourceASC)
	{
		return;
	}

	FGameplayEffectContextHandle EffectContext = SourceASC->MakeEffectContext();
	EffectContext.AddSourceObject(GetAvatarActorFromActorInfo());

	FGameplayEffectSpecHandle SpecHandle =
		SourceASC->MakeOutgoingSpec(DamageEffectClass, /*Level=*/1.0f, EffectContext);

	if (SpecHandle.IsValid())
	{
		// Pass the actual damage magnitude via a SetByCaller tag
		UAbilitySystemBlueprintLibrary::AssignTagSetByCallerMagnitude(
			SpecHandle,
			FGameplayTag::RequestGameplayTag(FName("Data.Damage")),
			-DamageAmount   // negative = reducing health
		);

		SourceASC->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetASC);
	}
}
