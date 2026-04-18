// Copyright Epic Games, Inc. All Rights Reserved.

#include "BS2026GameMode.h"
#include "BS2026PlayerController.h"
#include "Networking/BSGameState.h"
#include "Networking/BSPlayerState.h"
#include "GameFramework/PlayerController.h"

ABS2026GameMode::ABS2026GameMode()
{
	PlayerControllerClass = ABS2026PlayerController::StaticClass();

	// Use our replicated game state and player state
	GameStateClass   = ABSGameState::StaticClass();
	PlayerStateClass = ABSPlayerState::StaticClass();
}

void ABS2026GameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	// Assign team by round-robin based on the current player count
	if (ABSPlayerState* BSPS = NewPlayer->GetPlayerState<ABSPlayerState>())
	{
		const int32 PlayerCount = GameState ? GameState->PlayerArray.Num() : 1;
		// Alternate between team 0 and team 1
		BSPS->SetTeamIndex((PlayerCount - 1) % 2);
	}
}
