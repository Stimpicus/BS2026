// Copyright Epic Games, Inc. All Rights Reserved.

#include "BS2026GameMode.h"
#include "BS2026PlayerController.h"

ABS2026GameMode::ABS2026GameMode()
{
	PlayerControllerClass = ABS2026PlayerController::StaticClass();
}
