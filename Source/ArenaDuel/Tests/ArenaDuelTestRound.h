#pragma once

#include "Engine/World.h"
#include "ArenaDuel/Game/ArenaDuelGameState.h"
#include "ArenaDuel/Player/ArenaDuelPlayerController.h"
#include "ArenaDuel/Player/ArenaDuelPlayerState.h"

namespace ArenaDuelTestRound
{
	// Server side. Movement and combat input stay locked until both players are ready in the character
	// select lobby and the countdown has finished. Tests that drive input call this until it returns true
	// instead of waiting for the lobby's auto-ready timer.
	inline bool EnsureRoundInProgress(UWorld* World)
	{
		const AArenaDuelGameState* GameState = World ? World->GetGameState<AArenaDuelGameState>() : nullptr;
		if (!GameState || GameState->IsRoundInProgress()) return GameState != nullptr;
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			AArenaDuelPlayerController* Controller = Cast<AArenaDuelPlayerController>(It->Get());
			const AArenaDuelPlayerState* PlayerState = Controller ? Controller->GetPlayerState<AArenaDuelPlayerState>() : nullptr;
			if (PlayerState && !PlayerState->IsCharacterReady()) Controller->ServerSetCharacterReady(true);
		}
		return false;
	}
}
