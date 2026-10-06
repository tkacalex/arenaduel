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

	EndRoundForDevelopment(WinningPlayerState);
}

AArenaDuelPlayerState* AArenaDuelGameMode::FindPlayerStateByDuelSlot(uint8 DuelSlot) const
{
	const AArenaDuelGameState* ArenaGameState = GetGameState<AArenaDuelGameState>();
	if (!ArenaGameState || DuelSlot > 1) return nullptr;
	for (APlayerState* PlayerState : ArenaGameState->PlayerArray)
	{
		AArenaDuelPlayerState* ArenaPlayerState = Cast<AArenaDuelPlayerState>(PlayerState);
		if (ArenaPlayerState && ArenaPlayerState->GetDuelSlot() == DuelSlot) return ArenaPlayerState;
	}
	return nullptr;
}

void AArenaDuelGameMode::EndRoundForDevelopment(AArenaDuelPlayerState* WinningPlayerState)
{
	if (!HasAuthority() || bRoundRestartPending) return;
	AArenaDuelGameState* ArenaGameState = GetGameState<AArenaDuelGameState>();
	if (!ArenaGameState || !ArenaGameState->IsRoundInProgress()) return;
	if (WinningPlayerState) WinningPlayerState->AwardRoundWin();
	const int32 WinnerSlot = WinningPlayerState ? static_cast<int32>(WinningPlayerState->GetDuelSlot()) : INDEX_NONE;
	ArenaGameState->SetRoundState(ArenaGameState->GetRoundNumber(), false, WinnerSlot);
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (APlayerController* PlayerController = It->Get())
		{
			if (AArenaDuelCharacter* Character = Cast<AArenaDuelCharacter>(PlayerController->GetPawn()))
			{
				Character->SetRoundInputLocked(true);
			}
		}
	}
	bRoundRestartPending = true;

	const bool bMatchComplete = WinningPlayerState && WinningPlayerState->GetRoundWins() >= 5;
	if (!bMatchComplete && GetWorld())
	{
		GetWorldTimerManager().SetTimer(RoundRestartTimer, this, &AArenaDuelGameMode::StartNextRound, 3.0f, false);
	}
}

void AArenaDuelGameMode::AdminRestartRound()
{
	if (!HasAuthority() || !GetWorld()) return;
	GetWorldTimerManager().ClearTimer(RoundRestartTimer);
	bRoundRestartPending = false;
	if (AArenaDuelGameState* ArenaGameState = GetGameState<AArenaDuelGameState>())
	{
		ArenaGameState->SetRoundState(ArenaGameState->GetRoundNumber(), true, INDEX_NONE);
		RestartDuelPlayers();
	}
}

void AArenaDuelGameMode::AdminAdvanceRound()
{
	if (!HasAuthority() || !GetWorld()) return;
	GetWorldTimerManager().ClearTimer(RoundRestartTimer);
	bRoundRestartPending = false;
	if (AArenaDuelGameState* ArenaGameState = GetGameState<AArenaDuelGameState>())
	{
		ArenaGameState->SetRoundState(ArenaGameState->GetRoundNumber() + 1, true, INDEX_NONE);
		RestartDuelPlayers();
	}
}

void AArenaDuelGameMode::AdminAwardRound(uint8 WinningDuelSlot)
{
	if (!HasAuthority() || !GetWorld()) return;
	AArenaDuelGameState* ArenaGameState = GetGameState<AArenaDuelGameState>();
	if (!ArenaGameState || !ArenaGameState->IsRoundInProgress() || bRoundRestartPending) return;
	AArenaDuelPlayerState* Winner = FindPlayerStateByDuelSlot(WinningDuelSlot);
	if (Winner) EndRoundForDevelopment(Winner);
}

void AArenaDuelGameMode::AdminResetMatch()
{
	if (!HasAuthority() || !GetWorld()) return;
	GetWorldTimerManager().ClearTimer(RoundRestartTimer);
	bRoundRestartPending = false;
	if (AArenaDuelGameState* ArenaGameState = GetGameState<AArenaDuelGameState>())
	{
		for (APlayerState* PlayerState : ArenaGameState->PlayerArray)
		{
			if (AArenaDuelPlayerState* ArenaPlayerState = Cast<AArenaDuelPlayerState>(PlayerState))
			{
				ArenaPlayerState->SetRoundWinsForDevelopment(0);
			}
		}
		ArenaGameState->SetRoundState(1, true, INDEX_NONE);
		RestartDuelPlayers();
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
