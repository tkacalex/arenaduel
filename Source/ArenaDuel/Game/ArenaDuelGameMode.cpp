// Copyright Epic Games, Inc. All Rights Reserved.

#include "ArenaDuelGameMode.h"

#include "../Characters/ArenaDuelCharacter.h"
#include "ArenaDuelGameState.h"
#include "../Player/ArenaDuelPlayerController.h"
#include "../Player/ArenaDuelPlayerState.h"
#include "ArenaDuelMovementDebugHUD.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "TimerManager.h"
#include "Engine/World.h"

AArenaDuelGameMode::AArenaDuelGameMode()
{
	GameStateClass = AArenaDuelGameState::StaticClass();
	PlayerControllerClass = AArenaDuelPlayerController::StaticClass();
	PlayerStateClass = AArenaDuelPlayerState::StaticClass();
	DefaultPawnClass = AArenaDuelCharacter::StaticClass();
	HUDClass = AArenaDuelMovementDebugHUD::StaticClass();
}

void AArenaDuelGameMode::BeginPlay()
{
	Super::BeginPlay();
	if (AArenaDuelGameState* ArenaGameState = GetGameState<AArenaDuelGameState>())
	{
		ArenaGameState->SetRoundState(1, true, INDEX_NONE);
	}
}

void AArenaDuelGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);
	if (NewPlayer)
	{
		AssignDuelSlot(NewPlayer->GetPlayerState<AArenaDuelPlayerState>());
	}
}

void AArenaDuelGameMode::AssignDuelSlot(AArenaDuelPlayerState* JoiningPlayerState)
{
	AArenaDuelGameState* ArenaGameState = GetGameState<AArenaDuelGameState>();
	if (!JoiningPlayerState || !ArenaGameState) return;

	bool bSlotTaken[2] = { false, false };
	for (APlayerState* PlayerState : ArenaGameState->PlayerArray)
	{
		const AArenaDuelPlayerState* ArenaPlayerState = Cast<AArenaDuelPlayerState>(PlayerState);
		if (ArenaPlayerState && ArenaPlayerState != JoiningPlayerState && ArenaPlayerState->GetDuelSlot() < 2)
		{
			bSlotTaken[ArenaPlayerState->GetDuelSlot()] = true;
		}
	}
	JoiningPlayerState->SetDuelSlot(bSlotTaken[0] ? 1 : 0);
}

void AArenaDuelGameMode::HandlePlayerDeath(AArenaDuelCharacter* DeadCharacter)
{
	if (!HasAuthority() || !DeadCharacter || bRoundRestartPending) return;

	AArenaDuelGameState* ArenaGameState = GetGameState<AArenaDuelGameState>();
	if (!ArenaGameState || !ArenaGameState->IsRoundInProgress()) return;

	AArenaDuelPlayerState* LosingPlayerState = DeadCharacter->GetPlayerState<AArenaDuelPlayerState>();
	AArenaDuelPlayerState* WinningPlayerState = nullptr;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PlayerController = It->Get();
		AArenaDuelPlayerState* CandidateState = PlayerController ? PlayerController->GetPlayerState<AArenaDuelPlayerState>() : nullptr;
		AArenaDuelCharacter* CandidateCharacter = PlayerController ? Cast<AArenaDuelCharacter>(PlayerController->GetPawn()) : nullptr;
		if (CandidateState && CandidateState != LosingPlayerState && CandidateCharacter && !CandidateCharacter->IsDead())
		{
			WinningPlayerState = CandidateState;
			break;
		}
	}

	const int32 WinnerSlot = WinningPlayerState ? static_cast<int32>(WinningPlayerState->GetDuelSlot()) : INDEX_NONE;
	if (WinningPlayerState)
	{
		WinningPlayerState->AwardRoundWin();
	}
	ArenaGameState->SetRoundState(ArenaGameState->GetRoundNumber(), false, WinnerSlot);
	bRoundRestartPending = true;

	const bool bMatchComplete = WinningPlayerState && WinningPlayerState->GetRoundWins() >= 5;
	if (!bMatchComplete && GetWorld())
	{
		GetWorldTimerManager().SetTimer(RoundRestartTimer, this, &AArenaDuelGameMode::StartNextRound, 3.0f, false);
	}
}

void AArenaDuelGameMode::StartNextRound()
{
	if (!HasAuthority()) return;
	AArenaDuelGameState* ArenaGameState = GetGameState<AArenaDuelGameState>();
	if (!ArenaGameState) return;
	for (APlayerState* PlayerState : ArenaGameState->PlayerArray)
	{
		const AArenaDuelPlayerState* ArenaPlayerState = Cast<AArenaDuelPlayerState>(PlayerState);
		if (ArenaPlayerState && ArenaPlayerState->GetRoundWins() >= 5)
		{
			bRoundRestartPending = false;
			return;
		}
	}

	ArenaGameState->SetRoundState(ArenaGameState->GetRoundNumber() + 1, true, ArenaGameState->GetLastRoundWinnerSlot());
	RestartDuelPlayers();
	bRoundRestartPending = false;
}

void AArenaDuelGameMode::RestartDuelPlayers()
{
	if (!HasAuthority() || !GetWorld()) return;
	TArray<APlayerController*> Controllers;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (APlayerController* PlayerController = It->Get())
		{
			Controllers.Add(PlayerController);
			if (APawn* OldPawn = PlayerController->GetPawn())
			{
				PlayerController->UnPossess();
				OldPawn->Destroy();
			}
		}
	}
	for (APlayerController* PlayerController : Controllers)
	{
		if (IsValid(PlayerController))
		{
			RestartPlayer(PlayerController);
		}
	}
}
