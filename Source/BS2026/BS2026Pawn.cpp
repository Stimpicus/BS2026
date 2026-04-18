// Copyright Epic Games, Inc. All Rights Reserved.

#include "BS2026Pawn.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "BS2026.h"
#include "TimerManager.h"
// Networking
#include "Networking/BSNetworkPredictionComponent.h"
// Abilities
#include "Abilities/BSAbilitySystemComponent.h"
#include "Abilities/BSHealthSet.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffectTypes.h"
// Game state (for kill recording)
#include "Networking/BSGameState.h"
#include "Networking/BSPlayerState.h"
#include "GameFramework/PlayerController.h"

#define LOCTEXT_NAMESPACE "VehiclePawn"

ABS2026Pawn::ABS2026Pawn()
{
	// ── Replication ───────────────────────────────────────────────────
	// bReplicates enables actor replication.
	// SetReplicateMovement(false) prevents the engine's naive Chaos physics
	// state replication which causes "ghost car" jitter; we own movement
	// replication through UBSNetworkPredictionComponent instead.
	bReplicates = true;
	SetReplicateMovement(false);
	NetUpdateFrequency = 60.0f;

	// ── Cameras ───────────────────────────────────────────────────────
	// construct the front camera boom
	FrontSpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("Front Spring Arm"));
	FrontSpringArm->SetupAttachment(GetMesh());
	FrontSpringArm->TargetArmLength = 0.0f;
	FrontSpringArm->bDoCollisionTest = false;
	FrontSpringArm->bEnableCameraRotationLag = true;
	FrontSpringArm->CameraRotationLagSpeed = 15.0f;
	FrontSpringArm->SetRelativeLocation(FVector(30.0f, 0.0f, 120.0f));

	FrontCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("Front Camera"));
	FrontCamera->SetupAttachment(FrontSpringArm);
	FrontCamera->bAutoActivate = false;

	// construct the back camera boom
	BackSpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("Back Spring Arm"));
	BackSpringArm->SetupAttachment(GetMesh());
	BackSpringArm->TargetArmLength = 650.0f;
	BackSpringArm->SocketOffset.Z = 150.0f;
	BackSpringArm->bDoCollisionTest = false;
	BackSpringArm->bInheritPitch = false;
	BackSpringArm->bInheritRoll = false;
	BackSpringArm->bEnableCameraRotationLag = true;
	BackSpringArm->CameraRotationLagSpeed = 2.0f;
	BackSpringArm->CameraLagMaxDistance = 50.0f;

	BackCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("Back Camera"));
	BackCamera->SetupAttachment(BackSpringArm);

	// ── Physics ───────────────────────────────────────────────────────
	// Configure the car mesh
	GetMesh()->SetSimulatePhysics(true);
	GetMesh()->SetCollisionProfileName(FName("Vehicle"));

	// get the Chaos Wheeled movement component
	ChaosVehicleMovement = CastChecked<UChaosWheeledVehicleMovementComponent>(GetVehicleMovement());

	// ── Network prediction ────────────────────────────────────────────
	NetworkPredictionComponent =
		CreateDefaultSubobject<UBSNetworkPredictionComponent>(TEXT("NetworkPrediction"));

	// ── Gameplay Ability System ───────────────────────────────────────
	AbilitySystemComponent =
		CreateDefaultSubobject<UBSAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	// Mixed replication: server applies effects authoritatively;
	// clients predict locally and receive corrections.
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	// Create the health attribute set as a subobject so it is properly garbage-collected.
	// It will be registered with the ASC in PossessedBy via AddAttributeSetSubobject.
	HealthSet = CreateDefaultSubobject<UBSHealthSet>(TEXT("HealthSet"));
}

UAbilitySystemComponent* ABS2026Pawn::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

void ABS2026Pawn::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	// GAS owner/avatar must be initialised on the server
	if (AbilitySystemComponent && HasAuthority())
	{
		AbilitySystemComponent->InitAbilityActorInfo(this, this);

		// Register the attribute set that was pre-created in the constructor
		AbilitySystemComponent->AddAttributeSetSubobject(HealthSet);

		// Grant all default abilities (fire weapon, respawn, etc.)
		AbilitySystemComponent->GrantStartupAbilities(DefaultAbilities);

		// Listen for health changes so we can record kills in the game state
		AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(
			UBSHealthSet::GetHealthAttribute())
			.AddUObject(this, &ABS2026Pawn::OnHealthChanged);
	}
}

void ABS2026Pawn::SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		// steering 
		EnhancedInputComponent->BindAction(SteeringAction, ETriggerEvent::Triggered, this, &ABS2026Pawn::Steering);
		EnhancedInputComponent->BindAction(SteeringAction, ETriggerEvent::Completed, this, &ABS2026Pawn::Steering);

		// throttle 
		EnhancedInputComponent->BindAction(ThrottleAction, ETriggerEvent::Triggered, this, &ABS2026Pawn::Throttle);
		EnhancedInputComponent->BindAction(ThrottleAction, ETriggerEvent::Completed, this, &ABS2026Pawn::Throttle);

		// brake 
		EnhancedInputComponent->BindAction(BrakeAction, ETriggerEvent::Triggered, this, &ABS2026Pawn::Brake);
		EnhancedInputComponent->BindAction(BrakeAction, ETriggerEvent::Started, this, &ABS2026Pawn::StartBrake);
		EnhancedInputComponent->BindAction(BrakeAction, ETriggerEvent::Completed, this, &ABS2026Pawn::StopBrake);

		// handbrake 
		EnhancedInputComponent->BindAction(HandbrakeAction, ETriggerEvent::Started, this, &ABS2026Pawn::StartHandbrake);
		EnhancedInputComponent->BindAction(HandbrakeAction, ETriggerEvent::Completed, this, &ABS2026Pawn::StopHandbrake);

		// look around 
		EnhancedInputComponent->BindAction(LookAroundAction, ETriggerEvent::Triggered, this, &ABS2026Pawn::LookAround);

		// toggle camera 
		EnhancedInputComponent->BindAction(ToggleCameraAction, ETriggerEvent::Triggered, this, &ABS2026Pawn::ToggleCamera);

		// reset the vehicle 
		EnhancedInputComponent->BindAction(ResetVehicleAction, ETriggerEvent::Triggered, this, &ABS2026Pawn::ResetVehicle);
	}
	else
	{
		UE_LOG(LogBS2026, Error, TEXT("'%s' Failed to find an Enhanced Input component! This template is built to use the Enhanced Input system. If you intend to use the legacy system, then you will need to update this C++ file."), *GetNameSafe(this));
	}
}

void ABS2026Pawn::BeginPlay()
{
	Super::BeginPlay();

	// Initialise GAS actor info on the client side as well
	if (AbilitySystemComponent && !HasAuthority())
	{
		AbilitySystemComponent->InitAbilityActorInfo(this, this);

		// Register the pre-created attribute set on clients too so
		// attribute replication notifies and change delegates work locally.
		AbilitySystemComponent->AddAttributeSetSubobject(HealthSet);
	}

	// set up the flipped check timer
	GetWorld()->GetTimerManager().SetTimer(FlipCheckTimer, this, &ABS2026Pawn::FlippedCheck, FlipCheckTime, true);
}

void ABS2026Pawn::EndPlay(EEndPlayReason::Type EndPlayReason)
{
	// clear the flipped check timer
	GetWorld()->GetTimerManager().ClearTimer(FlipCheckTimer);

	Super::EndPlay(EndPlayReason);
}

void ABS2026Pawn::Tick(float Delta)
{
	Super::Tick(Delta);

	// add some angular damping if the vehicle is in midair
	bool bMovingOnGround = ChaosVehicleMovement->IsMovingOnGround();
	GetMesh()->SetAngularDamping(bMovingOnGround ? 0.0f : 3.0f);

	// realign the camera yaw to face front
	float CameraYaw = BackSpringArm->GetRelativeRotation().Yaw;
	CameraYaw = FMath::FInterpTo(CameraYaw, 0.0f, Delta, 1.0f);

	BackSpringArm->SetRelativeRotation(FRotator(0.0f, CameraYaw, 0.0f));
}

void ABS2026Pawn::Steering(const FInputActionValue& Value)
{
	DoSteering(Value.Get<float>());
}

void ABS2026Pawn::Throttle(const FInputActionValue& Value)
{
	DoThrottle(Value.Get<float>());
}

void ABS2026Pawn::Brake(const FInputActionValue& Value)
{
	DoBrake(Value.Get<float>());
}

void ABS2026Pawn::StartBrake(const FInputActionValue& Value)
{
	DoBrakeStart();
}

void ABS2026Pawn::StopBrake(const FInputActionValue& Value)
{
	DoBrakeStop();
}

void ABS2026Pawn::StartHandbrake(const FInputActionValue& Value)
{
	DoHandbrakeStart();
}

void ABS2026Pawn::StopHandbrake(const FInputActionValue& Value)
{
	DoHandbrakeStop();
}

void ABS2026Pawn::LookAround(const FInputActionValue& Value)
{
	DoLookAround(Value.Get<float>());
}

void ABS2026Pawn::ToggleCamera(const FInputActionValue& Value)
{
	DoToggleCamera();
}

void ABS2026Pawn::ResetVehicle(const FInputActionValue& Value)
{
	DoResetVehicle();
}

void ABS2026Pawn::DoSteering(float SteeringValue)
{
	// Route through the prediction component so the server history buffer
	// stays in sync with the actual inputs that drove the simulation.
	NetworkPredictionComponent->SetSteeringInput(SteeringValue);
}

void ABS2026Pawn::DoThrottle(float ThrottleValue)
{
	NetworkPredictionComponent->SetThrottleInput(ThrottleValue);
	NetworkPredictionComponent->SetBrakeInput(0.0f);
}

void ABS2026Pawn::DoBrake(float BrakeValue)
{
	NetworkPredictionComponent->SetBrakeInput(BrakeValue);
	NetworkPredictionComponent->SetThrottleInput(0.0f);
}

void ABS2026Pawn::DoBrakeStart()
{
	BrakeLights(true);
}

void ABS2026Pawn::DoBrakeStop()
{
	BrakeLights(false);
	NetworkPredictionComponent->SetBrakeInput(0.0f);
}

void ABS2026Pawn::DoHandbrakeStart()
{
	NetworkPredictionComponent->SetHandbrakeInput(true);
	BrakeLights(true);
}

void ABS2026Pawn::DoHandbrakeStop()
{
	NetworkPredictionComponent->SetHandbrakeInput(false);
	BrakeLights(false);
}

void ABS2026Pawn::DoLookAround(float YawDelta)
{
	BackSpringArm->AddLocalRotation(FRotator(0.0f, YawDelta, 0.0f));
}

void ABS2026Pawn::DoToggleCamera()
{
	bFrontCameraActive = !bFrontCameraActive;
	FrontCamera->SetActive(bFrontCameraActive);
	BackCamera->SetActive(!bFrontCameraActive);
}

void ABS2026Pawn::DoResetVehicle()
{
	// reset to a location slightly above our current one
	FVector ResetLocation = GetActorLocation() + FVector(0.0f, 0.0f, 50.0f);

	// reset to our yaw. Ignore pitch and roll
	FRotator ResetRotation = GetActorRotation();
	ResetRotation.Pitch = 0.0f;
	ResetRotation.Roll = 0.0f;

	// teleport the actor to the reset spot and reset physics
	SetActorTransform(FTransform(ResetRotation, ResetLocation, FVector::OneVector), false, nullptr, ETeleportType::TeleportPhysics);

	GetMesh()->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
	GetMesh()->SetPhysicsLinearVelocity(FVector::ZeroVector);
}

void ABS2026Pawn::FlippedCheck()
{
	// check the difference in angle between the mesh's up vector and world up
	const float UpDot = FVector::DotProduct(FVector::UpVector, GetMesh()->GetUpVector());

	if (UpDot < FlipCheckMinDot)
	{
		// is this the second time we've checked that the vehicle is still flipped?
		if (bPreviousFlipCheck)
		{
			DoResetVehicle();
		}
		bPreviousFlipCheck = true;
	}
	else
	{
		bPreviousFlipCheck = false;
	}
}

void ABS2026Pawn::OnHealthChanged(const FOnAttributeChangeData& ChangeData)
{
	// Only the server records kills and propagates death to the game state
	if (!HasAuthority())
	{
		return;
	}

	if (ChangeData.NewValue <= 0.0f && ChangeData.OldValue > 0.0f)
	{
		// Record the kill in the game state
		if (ABSGameState* GS = GetWorld()->GetGameState<ABSGameState>())
		{
			FString KillerName = TEXT("Unknown");
			if (ChangeData.GEModData)
			{
				const FGameplayEffectContextHandle& Context =
					ChangeData.GEModData->EffectSpec.GetEffectContext();
				if (const AActor* InstigatorActor = Context.GetInstigator())
				{
					if (const APawn* InstigatorPawn = Cast<APawn>(InstigatorActor))
					{
						if (AController* InstigatorController = InstigatorPawn->GetController())
						{
							if (ABSPlayerState* KillerPS =
								InstigatorController->GetPlayerState<ABSPlayerState>())
							{
								KillerName = KillerPS->GetPlayerName();
								KillerPS->AddKill();
								KillerPS->AddVehicleScore(1);
							}
						}
					}
				}
			}

			FString VictimName = TEXT("Unknown");
			if (AController* Ctrl = GetController())
			{
				if (APlayerState* PS = Ctrl->PlayerState)
				{
					VictimName = PS->GetPlayerName();
					if (ABSPlayerState* BSPS = Cast<ABSPlayerState>(PS))
					{
						BSPS->AddDeath();
					}
				}
			}

			GS->RecordKill(KillerName, VictimName);
		}
	}
}

#undef LOCTEXT_NAMESPACE
