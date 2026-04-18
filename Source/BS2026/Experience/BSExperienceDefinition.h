// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "BSExperienceDefinition.generated.h"

class ABS2026Pawn;
class AGameModeBase;

/** How the winner of a match is determined */
UENUM(BlueprintType)
enum class EBSWinCondition : uint8
{
	/** First team to reach ScoreLimit wins */
	ScoreLimit,
	/** Match ends after TimeLimitSeconds; highest score wins */
	TimeLimit,
	/** Last vehicle standing wins */
	LastManStanding
};

/**
 *  Data-only asset that fully describes a game mode "experience".
 *
 *  Place one of these assets in your Content Browser for each mode variant
 *  (e.g. "DA_Experience_Deathmatch", "DA_Experience_CaptureTheFlag").
 *  The lobby UI reads the list of experiences, displays their DisplayName /
 *  Description to the player, and passes the selected asset to the game mode
 *  at match start.
 *
 *  Advantages over hard-coded Blueprint GameMode chains:
 *    - All configuration is data-driven — no need to subclass for each variant.
 *    - Easy to add, reorder, or disable experiences without code.
 *    - Supports console variable overrides for QA / live-ops.
 */
UCLASS(BlueprintType)
class UBSExperienceDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:

	// ── Identity ──────────────────────────────────────────────────────

	/** Short name shown in the lobby UI */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Experience")
	FText DisplayName;

	/** Longer description shown in the lobby UI */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Experience", meta=(MultiLine))
	FText Description;

	// ── Core configuration ────────────────────────────────────────────

	/** Game mode class to use for this experience */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Experience")
	TSubclassOf<AGameModeBase> GameModeClass;

	/** Default pawn class players will spawn with */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Experience")
	TSubclassOf<ABS2026Pawn> DefaultPawnClass;

	/** Absolute content path to the map for this experience */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Experience")
	FSoftObjectPath CombatMap;

	// ── Win condition ─────────────────────────────────────────────────

	/** How the match winner is determined */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Experience|Win Condition")
	EBSWinCondition WinCondition = EBSWinCondition::ScoreLimit;

	/** Score required to win (used when WinCondition == ScoreLimit) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Experience|Win Condition",
		meta=(EditCondition="WinCondition==EBSWinCondition::ScoreLimit", ClampMin="1"))
	int32 ScoreLimit = 20;

	/** Time limit in seconds (used when WinCondition == TimeLimit) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Experience|Win Condition",
		meta=(EditCondition="WinCondition==EBSWinCondition::TimeLimit", ClampMin="60",
		      Units="s"))
	float TimeLimitSeconds = 600.0f;

	// ── Player limits ─────────────────────────────────────────────────

	/** Maximum number of players for this experience */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Experience|Players",
		meta=(ClampMin="2", ClampMax="16"))
	int32 MaxPlayers = 8;

	/** Number of teams (1 = free-for-all) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Experience|Players",
		meta=(ClampMin="1", ClampMax="4"))
	int32 NumTeams = 2;
};
