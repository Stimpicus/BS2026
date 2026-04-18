// Copyright Epic Games, Inc. All Rights Reserved.

#include "BSPlayerState.h"
#include "Net/UnrealNetwork.h"

ABSPlayerState::ABSPlayerState()
{
}

void ABSPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ABSPlayerState, TeamIndex);
	DOREPLIFETIME(ABSPlayerState, Kills);
	DOREPLIFETIME(ABSPlayerState, Deaths);
	DOREPLIFETIME(ABSPlayerState, VehicleScore);
}

void ABSPlayerState::AddKill()
{
	if (HasAuthority())
	{
		++Kills;
	}
}

void ABSPlayerState::AddDeath()
{
	if (HasAuthority())
	{
		++Deaths;
	}
}

void ABSPlayerState::AddVehicleScore(int32 Amount)
{
	if (HasAuthority())
	{
		VehicleScore += Amount;
	}
}

void ABSPlayerState::SetTeamIndex(int32 NewTeamIndex)
{
	if (HasAuthority())
	{
		TeamIndex = NewTeamIndex;
	}
}
