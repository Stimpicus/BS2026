// Copyright Epic Games, Inc. All Rights Reserved.

#include "BSGameState.h"
#include "Net/UnrealNetwork.h"

ABSGameState::ABSGameState()
{
	// Initialise two teams with zero score
	TeamScores.Add(0);
	TeamScores.Add(0);
}

void ABSGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ABSGameState, MatchPhase);
	DOREPLIFETIME(ABSGameState, TeamScores);
	DOREPLIFETIME(ABSGameState, KillFeed);
}

void ABSGameState::SetMatchPhase(EBSMatchPhase NewPhase)
{
	if (!HasAuthority())
	{
		return;
	}

	MatchPhase = NewPhase;
	OnRep_MatchPhase();
}

void ABSGameState::RecordKill(const FString& KillerName, const FString& VictimName)
{
	if (!HasAuthority())
	{
		return;
	}

	FKillFeedEntry Entry;
	Entry.KillerName = KillerName;
	Entry.VictimName = VictimName;
	Entry.Timestamp  = GetWorld()->GetTimeSeconds();

	KillFeed.Add(Entry);

	// Trim to maximum size
	while (KillFeed.Num() > MaxKillFeedEntries)
	{
		KillFeed.RemoveAt(0);
	}

	// Notify locally on the server (clients receive via OnRep_KillFeed)
	OnRep_KillFeed();
}

int32 ABSGameState::GetTeamScore(int32 TeamIndex) const
{
	if (TeamScores.IsValidIndex(TeamIndex))
	{
		return TeamScores[TeamIndex];
	}
	return 0;
}

void ABSGameState::AddTeamScore(int32 TeamIndex, int32 Amount)
{
	if (HasAuthority() && TeamScores.IsValidIndex(TeamIndex))
	{
		TeamScores[TeamIndex] += Amount;
	}
}

void ABSGameState::OnMatchEnd()
{
	SetMatchPhase(EBSMatchPhase::PostMatch);
}

void ABSGameState::OnRep_MatchPhase()
{
	OnMatchPhaseChanged.Broadcast(MatchPhase);
}

void ABSGameState::OnRep_KillFeed()
{
	if (KillFeed.Num() > 0)
	{
		OnKillFeedUpdated.Broadcast(KillFeed.Last());
	}
}
