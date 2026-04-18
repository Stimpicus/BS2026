// Copyright Epic Games, Inc. All Rights Reserved.

#include "BSNetworkPredictionComponent.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "TimerManager.h"
#include "Net/UnrealNetwork.h"

UBSNetworkPredictionComponent::UBSNetworkPredictionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true);
}

void UBSNetworkPredictionComponent::BeginPlay()
{
	Super::BeginPlay();

	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!OwnerPawn)
	{
		return;
	}

	VehicleMovement = OwnerPawn->FindComponentByClass<UChaosWheeledVehicleMovementComponent>();

	// Only the server needs to broadcast state
	if (GetOwner()->HasAuthority())
	{
		GetWorld()->GetTimerManager().SetTimer(
			ServerBroadcastTimer,
			this,
			&UBSNetworkPredictionComponent::ServerBroadcastState,
			ServerUpdateInterval,
			true
		);
	}
}

void UBSNetworkPredictionComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                                   FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!OwnerPawn)
	{
		return;
	}

	if (GetOwner()->HasAuthority())
	{
		// Server: record a history entry each tick for lag compensation
		FBSPhysicsState State = CaptureCurrentState();
		HistoryBuffer.Add(State);

		// Prune entries that are outside the rewind window
		const float Cutoff = GetWorld()->GetTimeSeconds() - MaxRewindTime;
		int32 ExpiredCount = 0;
		const int32 HistoryCount = HistoryBuffer.Num();
		while (ExpiredCount < HistoryCount && HistoryBuffer[ExpiredCount].Timestamp < Cutoff)
		{
			++ExpiredCount;
		}

		if (ExpiredCount > 0)
		{
			HistoryBuffer.RemoveAt(0, ExpiredCount, /*bAllowShrinking=*/false);
		}

		// Hard size cap as a safety net
		const int32 OverflowCount = HistoryBuffer.Num() - MaxHistorySize;
		if (OverflowCount > 0)
		{
			HistoryBuffer.RemoveAt(0, OverflowCount, /*bAllowShrinking=*/false);
		}
	}
	else if (!OwnerPawn->IsLocallyControlled())
	{
		// Non-owning clients: advance the smooth interpolation towards the latest server state
		TickInterpolation(DeltaTime);
	}
}

void UBSNetworkPredictionComponent::SetSteeringInput(float Value)
{
	if (VehicleMovement)
	{
		VehicleMovement->SetSteeringInput(Value);
	}
}

void UBSNetworkPredictionComponent::SetThrottleInput(float Value)
{
	if (VehicleMovement)
	{
		VehicleMovement->SetThrottleInput(Value);
	}
}

void UBSNetworkPredictionComponent::SetBrakeInput(float Value)
{
	if (VehicleMovement)
	{
		VehicleMovement->SetBrakeInput(Value);
	}
}

void UBSNetworkPredictionComponent::SetHandbrakeInput(bool bActive)
{
	if (VehicleMovement)
	{
		VehicleMovement->SetHandbrakeInput(bActive);
	}
}

bool UBSNetworkPredictionComponent::GetStateAtTime(float TargetTime, FBSPhysicsState& OutState) const
{
	if (HistoryBuffer.Num() == 0)
	{
		return false;
	}

	// Return the oldest entry if the requested time is before our history
	if (TargetTime <= HistoryBuffer[0].Timestamp)
	{
		OutState = HistoryBuffer[0];
		return true;
	}

	// Find the two bracketing entries and lerp between them
	for (int32 i = 1; i < HistoryBuffer.Num(); ++i)
	{
		if (HistoryBuffer[i].Timestamp >= TargetTime)
		{
			const FBSPhysicsState& A = HistoryBuffer[i - 1];
			const FBSPhysicsState& B = HistoryBuffer[i];
			const float Duration     = B.Timestamp - A.Timestamp;
			const float Alpha        = (Duration > KINDA_SMALL_NUMBER)
			                           ? (TargetTime - A.Timestamp) / Duration
			                           : 0.0f;

			OutState.Position        = FMath::Lerp(A.Position, B.Position, Alpha);
			OutState.Rotation        = FMath::Lerp(A.Rotation, B.Rotation, Alpha);
			OutState.LinearVelocity  = FMath::Lerp(A.LinearVelocity, B.LinearVelocity, Alpha);
			OutState.AngularVelocity = FMath::Lerp(A.AngularVelocity, B.AngularVelocity, Alpha);
			OutState.Timestamp       = TargetTime;
			return true;
		}
	}

	// Requested time is beyond our history — clamp to the most recent entry
	OutState = HistoryBuffer.Last();
	return true;
}

FBSPhysicsState UBSNetworkPredictionComponent::CaptureCurrentState() const
{
	FBSPhysicsState State;
	State.Timestamp = GetWorld()->GetTimeSeconds();

	if (UPrimitiveComponent* RootPrim = Cast<UPrimitiveComponent>(GetOwner()->GetRootComponent()))
	{
		State.Position        = GetOwner()->GetActorLocation();
		State.Rotation        = GetOwner()->GetActorRotation();
		State.LinearVelocity  = RootPrim->GetPhysicsLinearVelocity();
		State.AngularVelocity = RootPrim->GetPhysicsAngularVelocityInDegrees();
	}

	return State;
}

void UBSNetworkPredictionComponent::ServerBroadcastState()
{
	if (GetOwner()->HasAuthority())
	{
		MulticastPhysicsState(CaptureCurrentState());
	}
}

void UBSNetworkPredictionComponent::MulticastPhysicsState_Implementation(FBSPhysicsState State)
{
	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!OwnerPawn)
	{
		return;
	}

	// The locally-controlled pawn drives itself — skip incoming state for it
	if (OwnerPawn->IsLocallyControlled())
	{
		return;
	}

	// The server already has authoritative state
	if (GetOwner()->HasAuthority())
	{
		return;
	}

	PushInterpState(State);
}

void UBSNetworkPredictionComponent::PushInterpState(const FBSPhysicsState& State)
{
	// Discard out-of-order packets: MulticastPhysicsState is Unreliable so packets
	// can arrive out of order.  Non-monotonic timestamps would produce Duration <= 0
	// in TickInterpolation and stall interpolation entirely.
	if (InterpBuffer.Num() > 0 && State.Timestamp <= InterpBuffer.Last().Timestamp)
	{
		return;
	}

	InterpBuffer.Add(State);

	while (InterpBuffer.Num() > InterpolationBufferSize)
	{
		InterpBuffer.RemoveAt(0);
	}
}

void UBSNetworkPredictionComponent::TickInterpolation(float DeltaTime)
{
	if (InterpBuffer.Num() < 2)
	{
		return;
	}

	const FBSPhysicsState& A = InterpBuffer[0];
	const FBSPhysicsState& B = InterpBuffer[1];

	const float Duration = B.Timestamp - A.Timestamp;
	if (Duration <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	InterpTimeOffset += DeltaTime;
	const float Alpha = FMath::Clamp(InterpTimeOffset / Duration, 0.0f, 1.0f);

	const FVector  SmoothedPos = FMath::Lerp(A.Position, B.Position, Alpha);
	const FRotator SmoothedRot = FMath::Lerp(A.Rotation, B.Rotation, Alpha);

	GetOwner()->SetActorLocationAndRotation(
		SmoothedPos, SmoothedRot, false, nullptr, ETeleportType::TeleportPhysics);

	// Advance the buffer once we have consumed the first interval
	if (Alpha >= 1.0f)
	{
		InterpBuffer.RemoveAt(0);
		InterpTimeOffset = 0.0f;
	}
}
