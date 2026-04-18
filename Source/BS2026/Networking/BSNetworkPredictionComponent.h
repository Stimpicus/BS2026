// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BSNetworkPredictionComponent.generated.h"

class UChaosWheeledVehicleMovementComponent;

/**
 *  A snapshot of vehicle physics state.
 *  Used both as an interpolation target on remote clients and as a
 *  history entry for server-side lag compensation.
 */
USTRUCT(BlueprintType)
struct FBSPhysicsState
{
	GENERATED_BODY()

	UPROPERTY()
	FVector Position = FVector::ZeroVector;

	UPROPERTY()
	FRotator Rotation = FRotator::ZeroRotator;

	UPROPERTY()
	FVector LinearVelocity = FVector::ZeroVector;

	UPROPERTY()
	FVector AngularVelocity = FVector::ZeroVector;

	/** Server world time at which this snapshot was taken */
	UPROPERTY()
	float Timestamp = 0.0f;
};

/**
 *  Handles client-prediction and server-reconciliation for Chaos vehicle physics.
 *
 *  Server:
 *    - Captures physics state at ServerUpdateInterval and multicasts it.
 *    - Maintains a rolling history buffer for lag compensation (≤MaxRewindTime seconds).
 *
 *  Non-owning clients:
 *    - Buffers the last InterpolationBufferSize states and smoothly interpolates
 *      the remote vehicle between them, eliminating the "ghost car" jitter that
 *      occurs when using SetReplicateMovement(true) on a Chaos physics body.
 *
 *  Owning client:
 *    - Passes input directly to the Chaos movement component (no interpolation).
 *    - The server state broadcast is ignored for the locally-controlled pawn.
 */
UCLASS(ClassGroup=(Vehicle), meta=(BlueprintSpawnableComponent))
class UBSNetworkPredictionComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UBSNetworkPredictionComponent();

	/** How often (seconds) the server broadcasts physics state to all clients (~30 Hz) */
	UPROPERTY(EditAnywhere, Category="Network")
	float ServerUpdateInterval = 0.033f;

	/** Number of remote-state snapshots kept for client interpolation */
	static constexpr int32 InterpolationBufferSize = 3;

	/** Maximum lag compensation rewind window (seconds) */
	static constexpr float MaxRewindTime = 0.2f;

	/** Hard cap on the lag-compensation history buffer */
	static constexpr int32 MaxHistorySize = 30;

protected:

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
	                           FActorComponentTickFunction* ThisTickFunction) override;

public:

	/** Route steering input through this component (called by the pawn) */
	void SetSteeringInput(float Value);

	/** Route throttle input through this component (called by the pawn) */
	void SetThrottleInput(float Value);

	/** Route brake input through this component (called by the pawn) */
	void SetBrakeInput(float Value);

	/** Route handbrake input through this component (called by the pawn) */
	void SetHandbrakeInput(bool bActive);

	/**
	 *  Retrieves the physics state at TargetTime for lag compensation.
	 *  Interpolates between history samples when TargetTime falls within the stored range,
	 *  and clamps to the oldest/newest available sample when TargetTime falls outside it.
	 *  Returns false only if there is no history available.
	 */
	bool GetStateAtTime(float TargetTime, FBSPhysicsState& OutState) const;

	/** Read-only access to the server-side history buffer */
	const TArray<FBSPhysicsState>& GetHistoryBuffer() const { return HistoryBuffer; }

private:

	/** Cached reference to the owning pawn's movement component */
	TObjectPtr<UChaosWheeledVehicleMovementComponent> VehicleMovement;

	/** Timer handle for the server-side broadcast */
	FTimerHandle ServerBroadcastTimer;

	/** Interpolation buffer on non-owning clients (up to InterpolationBufferSize entries) */
	TArray<FBSPhysicsState> InterpBuffer;

	/** Seconds consumed from the current interpolation interval */
	float InterpTimeOffset = 0.0f;

	/** History ring buffer used for server-side lag compensation */
	TArray<FBSPhysicsState> HistoryBuffer;

	/** Captures the vehicle's current physics state with a world-time timestamp */
	FBSPhysicsState CaptureCurrentState() const;

	/** Server timer callback: captures state and multicasts to all clients */
	void ServerBroadcastState();

	/** Multicast RPC that delivers physics state from server to all clients */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastPhysicsState(FBSPhysicsState State);

	/** Adds a new state to the interpolation buffer and drops the oldest if full */
	void PushInterpState(const FBSPhysicsState& State);

	/** Advances interpolation and applies the smoothed transform to the owner */
	void TickInterpolation(float DeltaTime);
};
