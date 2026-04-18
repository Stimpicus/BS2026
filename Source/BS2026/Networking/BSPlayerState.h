// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "BSPlayerState.generated.h"

/**
 *  Player State for multiplayer vehicular combat.
 *  All properties are replicated to every client so scoreboards and
 *  team-colour logic can read them without server round-trips.
 */
UCLASS()
class ABSPlayerState : public APlayerState
{
	GENERATED_BODY()

public:

	ABSPlayerState();

	// ── Replicated per-player stats ───────────────────────────────────

	/** Team this player belongs to (0-based; 0 or 1 for two-team modes) */
	UPROPERTY(Replicated, BlueprintReadOnly, Category="Player")
	int32 TeamIndex = 0;

	/** Total kills scored this session */
	UPROPERTY(Replicated, BlueprintReadOnly, Category="Player")
	int32 Kills = 0;

	/** Total times this player's vehicle was destroyed this session */
	UPROPERTY(Replicated, BlueprintReadOnly, Category="Player")
	int32 Deaths = 0;

	/** Accumulated match score (kills + objectives, etc.) */
	UPROPERTY(Replicated, BlueprintReadOnly, Category="Player")
	int32 VehicleScore = 0;

	// ── Server-side mutators ──────────────────────────────────────────

	/** Increments the kill counter (server only) */
	void AddKill();

	/** Increments the death counter (server only) */
	void AddDeath();

	/** Adds Amount to the player's score (server only) */
	void AddVehicleScore(int32 Amount);

	/** Assigns this player to a team (server only) */
	void SetTeamIndex(int32 NewTeamIndex);

protected:

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
