// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "GameplayEffectTypes.h"
#include "BS2026PlayerController.generated.h"

class UInputMappingContext;
class ABS2026Pawn;
class UBS2026UI;

/**
 *  Vehicle Player Controller class
 *  Handles input mapping and user interface
 */
UCLASS(abstract, Config="Game")
class ABS2026PlayerController : public APlayerController
{
	GENERATED_BODY()

protected:

	/** Input Mapping Contexts */
	UPROPERTY(EditAnywhere, Category ="Input|Input Mappings")
	TArray<UInputMappingContext*> DefaultMappingContexts;

	/** Input Mapping Contexts */
	UPROPERTY(EditAnywhere, Category="Input|Input Mappings")
	TArray<UInputMappingContext*> MobileExcludedMappingContexts;

	/** Mobile controls widget to spawn */
	UPROPERTY(EditAnywhere, Category="Input|Touch Controls")
	TSubclassOf<UUserWidget> MobileControlsWidgetClass;

	/** Pointer to the mobile controls widget */
	UPROPERTY()
	TObjectPtr<UUserWidget> MobileControlsWidget;

	/** If true, the player will use UMG touch controls even if not playing on mobile platforms */
	UPROPERTY(EditAnywhere, Config, Category = "Input|Touch Controls")
	bool bForceTouchControls = false;

	/** If true, the optional steering wheel input mapping context will be registered */
	UPROPERTY(EditAnywhere, Category = "Input|Steering Wheel Controls")
	bool bUseSteeringWheelControls = false;

	/** Optional Input Mapping Context to be used for steering wheel input.
	 *  This is added alongside the default Input Mapping Context and does not block other forms of input.
	 */
	UPROPERTY(EditAnywhere, Category = "Input|Steering Wheel Controls", meta = (EditCondition = "bUseSteeringWheelControls"))
	UInputMappingContext* SteeringWheelInputMappingContext;

	/**
	 *  Type of vehicle to automatically respawn when it's destroyed.
	 *  NOTE: In the multiplayer path this is handled by UGA_RespawnVehicle.
	 *  This field is kept as a fallback for single-player / non-GAS scenarios.
	 */
	UPROPERTY(EditAnywhere, Category="Vehicle|Respawn")
	TSubclassOf<ABS2026Pawn> VehiclePawnClass;

	/** Pointer to the controlled vehicle pawn */
	TObjectPtr<ABS2026Pawn> VehiclePawn;

	/** Type of the UI to spawn */
	UPROPERTY(EditAnywhere, Category="Vehicle|UI")
	TSubclassOf<UBS2026UI> VehicleUIClass;

	/** Pointer to the UI widget */
	UPROPERTY()
	TObjectPtr<UBS2026UI> VehicleUI;

protected:

	/** Gameplay initialization */
	virtual void BeginPlay() override;

	/** Input setup */
	virtual void SetupInputComponent() override;

public:

	/**
	 *  Update vehicle UI on tick.
	 *  Speed and gear are polled from the physics simulation each frame because
	 *  they are not GAS attributes; health changes are delivered via the GAS
	 *  attribute-change delegate registered in OnPossess.
	 */
	virtual void Tick(float Delta) override;

protected:

	/** Pawn setup */
	virtual void OnPossess(APawn* InPawn) override;

	/**
	 *  Fallback respawn handler for single-player / non-GAS scenarios.
	 *  In multiplayer the UGA_RespawnVehicle ability handles respawn
	 *  server-authoritatively before this delegate fires.
	 */
	UFUNCTION()
	void OnPawnDestroyed(AActor* DestroyedPawn);

	/** Returns true if the player should use UMG touch controls */
	bool ShouldUseTouchControls() const;

	/**
	 *  GAS attribute-change callback for Health.
	 *  Bound in OnPossess; forwards the new value to the HUD widget
	 *  without needing to poll every frame.
	 */
	void OnHealthAttributeChanged(const FOnAttributeChangeData& ChangeData);
};
