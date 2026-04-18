// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_FireWeapon.generated.h"

class ABS2026Pawn;

/**
 *  Server-validated weapon-fire ability.
 *
 *  Execution policy: LocalPredicted
 *    - Client: runs immediately for zero-latency cosmetic feedback.
 *    - Server: performs the authoritative lag-compensated hit trace and
 *              applies damage via Gameplay Effects.
 *
 *  Lag compensation:
 *    On the server path, target vehicles are rewound by the estimated
 *    client ping (clamped to MaxRewindWindow) before the trace is performed,
 *    then restored instantly so the simulation is unaffected.
 */
UCLASS()
class UGA_FireWeapon : public UGameplayAbility
{
	GENERATED_BODY()

public:

	UGA_FireWeapon();

	/** Skeletal-mesh socket the weapon trace originates from */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Weapon")
	FName WeaponSocketName = TEXT("WeaponSocket");

	/** Maximum reach of the weapon in centimetres */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Weapon")
	float WeaponRange = 5000.0f;

	/** Sweep sphere radius; set to 0 for a pure line trace */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Weapon")
	float SweepRadius = 30.0f;

	/** Base damage to deal on a hit */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Weapon")
	float DamageAmount = 25.0f;

	/** Gameplay Effect applied to the target on a confirmed hit */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Weapon")
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	/** Gameplay Cue tag played on hit (cosmetic, prediction-aware) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Weapon")
	FGameplayTag HitCueTag;

protected:

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	                             const FGameplayAbilityActorInfo* ActorInfo,
	                             const FGameplayAbilityActivationInfo ActivationInfo,
	                             const FGameplayEventData* TriggerEventData) override;

private:

	/** Compute weapon trace start/end from the avatar's weapon socket (or forward vector fallback) */
	void GetTracePoints(const FGameplayAbilityActorInfo* ActorInfo,
	                    FVector& OutStart, FVector& OutEnd) const;

	/**
	 *  Lag-compensated hit trace executed on the server.
	 *  @param TraceStart    World-space trace origin derived from the avatar's weapon socket.
	 *  @param TraceEnd      World-space trace end point derived from the avatar's forward vector.
	 *  @param ClientPingSec Estimated round-trip ping used to rewind targets.
	 *
	 *  Note: trace points are currently computed independently on both client and server via
	 *  GetTracePoints().  For stricter lag compensation the caller should transmit the
	 *  client-side points and validate them server-side.
	 */
	void ServerPerformHitTrace(const FVector& TraceStart,
	                           const FVector& TraceEnd,
	                           float ClientPingSec);

	/** Apply the damage effect to a vehicle that was hit */
	void ApplyDamageToTarget(ABS2026Pawn* TargetVehicle);

	/** Maximum seconds we will rewind target vehicles for lag compensation */
	static constexpr float MaxRewindWindow = 0.2f;
};
