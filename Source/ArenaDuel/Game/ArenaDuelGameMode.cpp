// Copyright Epic Games, Inc. All Rights Reserved.

#include "ArenaDuelGameMode.h"

#include "../Characters/ArenaDuelCharacter.h"
#include "ArenaDuelGameState.h"
#include "../Player/ArenaDuelPlayerController.h"
#include "../Player/ArenaDuelPlayerState.h"
#include "ArenaDuelMovementDebugHUD.h"

AArenaDuelGameMode::AArenaDuelGameMode()
{
	GameStateClass = AArenaDuelGameState::StaticClass();
	PlayerControllerClass = AArenaDuelPlayerController::StaticClass();
	PlayerStateClass = AArenaDuelPlayerState::StaticClass();
	DefaultPawnClass = AArenaDuelCharacter::StaticClass();
	HUDClass = AArenaDuelMovementDebugHUD::StaticClass();
}
