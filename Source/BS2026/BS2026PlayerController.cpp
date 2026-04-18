// Copyright Epic Games, Inc. All Rights Reserved.

#include "BS2026PlayerController.h"
#include "BS2026Pawn.h"
#include "BS2026UI.h"
#include "EnhancedInputSubsystems.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "Blueprint/UserWidget.h"
#include "BS2026.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerStart.h"
#include "Widgets/Input/SVirtualJoystick.h"
#include "Abilities/BSAbilitySystemComponent.h"
#include "Abilities/BSHealthSet.h"
#include "AbilitySystemComponent.h"

void ABS2026PlayerController::BeginPlay()
{
	Super::BeginPlay();

	// ensure we're attached to the vehicle pawn so that World Partition streaming works correctly
	bAttachToPawn = true;

	// only spawn UI on local player controllers
	if (IsLocalPlayerController())
	{
		if (ShouldUseTouchControls())
		{
			// spawn the mobile controls widget
			MobileControlsWidget = CreateWidget<UUserWidget>(this, MobileControlsWidgetClass);

			if (MobileControlsWidget)
			{
				MobileControlsWidget->AddToPlayerScreen(0);
			}
			else
			{
				UE_LOG(LogBS2026, Error, TEXT("Could not spawn mobile controls widget."));
			}
		}

		// spawn the UI widget and add it to the viewport
		VehicleUI = CreateWidget<UBS2026UI>(this, VehicleUIClass);

		if (VehicleUI)
		{
			VehicleUI->AddToViewport();
		}
		else
		{
			UE_LOG(LogBS2026, Error, TEXT("Could not spawn vehicle UI widget."));
		}
	}
}

void ABS2026PlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	// only add IMCs for local player controllers
	if (IsLocalPlayerController())
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		{
			for (UInputMappingContext* CurrentContext : DefaultMappingContexts)
			{
				Subsystem->AddMappingContext(CurrentContext, 0);
			}

			// only add these IMCs if we're not using mobile touch input
			if (!ShouldUseTouchControls())
			{
				for (UInputMappingContext* CurrentContext : MobileExcludedMappingContexts)
				{
					Subsystem->AddMappingContext(CurrentContext, 0);
				}
			}
		}
	}
}

void ABS2026PlayerController::Tick(float Delta)
{
	Super::Tick(Delta);

	// Speed and gear are physics simulation values; they cannot be GAS attributes
	// because they change every frame based on the Chaos solver state.
	// Health is delivered via OnHealthAttributeChanged (bound in OnPossess).
	if (IsValid(VehiclePawn) && IsValid(VehicleUI))
	{
		VehicleUI->UpdateSpeed(VehiclePawn->GetChaosVehicleMovement()->GetForwardSpeed());
		VehicleUI->UpdateGear(VehiclePawn->GetChaosVehicleMovement()->GetCurrentGear());
	}
}

void ABS2026PlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	// Unbind from the previously possessed vehicle pawn, if any.
	if (IsValid(VehiclePawn))
	{
		VehiclePawn->OnDestroyed.RemoveDynamic(this, &ABS2026PlayerController::OnPawnDestroyed);

		if (IsLocalPlayerController())
		{
			if (UBSAbilitySystemComponent* PreviousASC =
				Cast<UBSAbilitySystemComponent>(VehiclePawn->GetAbilitySystemComponent()))
			{
				PreviousASC->GetGameplayAttributeValueChangeDelegate(UBSHealthSet::GetHealthAttribute())
					.RemoveAll(this);
			}
		}
	}

	// Only treat the possessed pawn as a vehicle when it is actually an ABS2026Pawn.
	VehiclePawn = Cast<ABS2026Pawn>(InPawn);
	if (!IsValid(VehiclePawn))
	{
		return;
	}
	// subscribe to the pawn's OnDestroyed delegate (fallback for non-GAS paths)
	VehiclePawn->OnDestroyed.AddDynamic(this, &ABS2026PlayerController::OnPawnDestroyed);

	// Subscribe to GAS health attribute changes so the HUD updates without per-frame polling.
	// This only binds on the local client; the server does not need HUD updates.
	if (IsLocalPlayerController())
	{
		if (UBSAbilitySystemComponent* ASC =
			Cast<UBSAbilitySystemComponent>(VehiclePawn->GetAbilitySystemComponent()))
		{
			ASC->GetGameplayAttributeValueChangeDelegate(UBSHealthSet::GetHealthAttribute())
				.AddUObject(this, &ABS2026PlayerController::OnHealthAttributeChanged);
		}
	}
}

void ABS2026PlayerController::OnPawnDestroyed(AActor* DestroyedPawn)
{
	// In multiplayer, UGA_RespawnVehicle handles server-authoritative respawn before
	// the pawn is fully destroyed.  This fallback is retained for single-player
	// levels and any map that has not granted the respawn ability.
	//
	// Guard: only the server (or standalone) should spawn and possess a replacement
	// vehicle.  On remote clients this delegate fires too, but spawning here would
	// create a non-replicated local pawn and can double-trigger respawn when
	// UGA_RespawnVehicle is also active.
	const ENetMode NetMode = GetNetMode();
	if (NetMode != NM_DedicatedServer && NetMode != NM_ListenServer && NetMode != NM_Standalone)
	{
		return;
	}

	if (!VehiclePawnClass)
	{
		return;
	}

	TArray<AActor*> ActorList;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), APlayerStart::StaticClass(), ActorList);

	if (ActorList.Num() > 0)
	{
		const FTransform SpawnTransform = ActorList[0]->GetActorTransform();

		if (ABS2026Pawn* RespawnedVehicle =
			GetWorld()->SpawnActor<ABS2026Pawn>(VehiclePawnClass, SpawnTransform))
		{
			Possess(RespawnedVehicle);
		}
	}
}

void ABS2026PlayerController::OnHealthAttributeChanged(const FOnAttributeChangeData& ChangeData)
{
	// Forward the new health value to the HUD widget without polling every tick.
	if (IsValid(VehicleUI))
	{
		VehicleUI->UpdateHealth(ChangeData.NewValue);
	}
}

bool ABS2026PlayerController::ShouldUseTouchControls() const
{
	return SVirtualJoystick::ShouldDisplayTouchInterface() || bForceTouchControls;
}
