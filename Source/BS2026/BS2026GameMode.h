// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "BS2026GameMode.generated.h"

class ABSGameState;
class ABSPlayerState;

/**
 *  Game Mode for multiplayer vehicular combat.
 *
 *  Upgraded from AGameModeBase to AGameMode to gain the full
 *  match-state machine and RestartPlayer() support.
 *
 *  Responsibilities:
 *    - Sets ABSGameState and ABSPlayerState as the default classes.
 *    - Assigns arriving players to teams in round-robin order.
 *    - Delegates to ABSGameState for score tracking and kill feed.
 */
UCLASS(abstract)
class ABS2026GameMode : public AGameMode
{
	GENERATED_BODY()

public:

	ABS2026GameMode();

	// Assign the new player to a team when they log in
	virtual void PostLogin(APlayerController* NewPlayer) override;
};
