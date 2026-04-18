// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameState.h"
#include "BSGameState.generated.h"

/** Phases a vehicular combat match can be in */
UENUM(BlueprintType)
enum class EBSMatchPhase : uint8
{
	WaitingToStart,
	InProgress,
	PostMatch
};

/** A single entry in the kill feed displayed to all players */
USTRUCT(BlueprintType)
struct FKillFeedEntry
{
	GENERATED_BODY()

	/** Display name of the player who scored the kill */
	UPROPERTY(BlueprintReadOnly, Category="Kill Feed")
	FString KillerName;

	/** Display name of the player whose vehicle was destroyed */
	UPROPERTY(BlueprintReadOnly, Category="Kill Feed")
	FString VictimName;

	/** Server world time when the kill occurred */
	UPROPERTY(BlueprintReadOnly, Category="Kill Feed")
	float Timestamp = 0.0f;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBSMatchPhaseChanged, EBSMatchPhase, NewPhase);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBSKillFeedUpdated, const FKillFeedEntry&, NewEntry);

/**
 *  Game State for multiplayer vehicular combat.
 *  Replicated to all clients; server is the only writer.
 *
 *  Tracks:
 *    - Match phase   (WaitingToStart → InProgress → PostMatch)
 *    - Per-team scores
 *    - Kill feed entries (latest MaxKillFeedEntries kills)
 */
UCLASS()
class ABSGameState : public AGameState
{
	GENERATED_BODY()

public:

	ABSGameState();

	// ── Replicated state ─────────────────────────────────────────────

	/** Current phase of the match */
	UPROPERTY(ReplicatedUsing=OnRep_MatchPhase, BlueprintReadOnly, Category="Match")
	EBSMatchPhase MatchPhase = EBSMatchPhase::WaitingToStart;

	/** Per-team score totals; index == team number */
	UPROPERTY(Replicated, BlueprintReadOnly, Category="Match")
	TArray<int32> TeamScores;

	/** Most-recent kill feed entries (max MaxKillFeedEntries entries) */
	UPROPERTY(ReplicatedUsing=OnRep_KillFeed, BlueprintReadOnly, Category="Match")
	TArray<FKillFeedEntry> KillFeed;

	// ── Client-side events ────────────────────────────────────────────

	/** Fired on clients whenever the match phase changes */
	UPROPERTY(BlueprintAssignable, Category="Match")
	FOnBSMatchPhaseChanged OnMatchPhaseChanged;

	/** Fired on clients whenever a new kill is added to the feed */
	UPROPERTY(BlueprintAssignable, Category="Match")
	FOnBSKillFeedUpdated OnKillFeedUpdated;

	// ── Server-side mutators ──────────────────────────────────────────

	/** Advances the match to a new phase (server only) */
	void SetMatchPhase(EBSMatchPhase NewPhase);

	/** Records a kill and broadcasts the updated kill feed (server only) */
	void RecordKill(const FString& KillerName, const FString& VictimName);

	/** Returns the score for TeamIndex, or 0 if the index is out of range */
	UFUNCTION(BlueprintPure, Category="Match")
	int32 GetTeamScore(int32 TeamIndex) const;

	/** Awards Amount points to TeamIndex (server only) */
	void AddTeamScore(int32 TeamIndex, int32 Amount = 1);

	/** Transitions to PostMatch and optionally travels all clients to the lobby map */
	void OnMatchEnd();

protected:

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:

	UFUNCTION()
	void OnRep_MatchPhase();

	UFUNCTION()
	void OnRep_KillFeed();

	/** Maximum number of kill feed entries retained */
	static constexpr int32 MaxKillFeedEntries = 10;
};
