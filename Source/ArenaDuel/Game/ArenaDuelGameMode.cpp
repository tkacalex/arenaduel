// Copyright Epic Games, Inc. All Rights Reserved.

#include "ArenaDuelGameMode.h"

#include "../Characters/ArenaDuelCharacter.h"
#include "ArenaDuelGameState.h"
#include "../Abilities/ArenaDuelVeilWall.h"
#include "../Abilities/ArenaDuelArcBarrier.h"
#include "../Abilities/ArenaDuelPhaseGateVisual.h"
#include "../Player/ArenaDuelPlayerController.h"
#include "../Player/ArenaDuelPlayerState.h"
#include "ArenaDuelMovementDebugHUD.h"
#include "AbilitySystemComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerStart.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "EngineUtils.h"

namespace
{
	constexpr float CharacterAutoReadyDelaySeconds = 12.0f;
}

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
	EnterCharacterSelect();
}

void AArenaDuelGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);
	if (NewPlayer)
	{
		AssignDuelSlot(NewPlayer->GetPlayerState<AArenaDuelPlayerState>());
		if (GetGameState<AArenaDuelGameState>() && GetGameState<AArenaDuelGameState>()->GetMatchPhase() == EArenaDuelMatchPhase::CharacterSelect)
		{
			StartCharacterAutoReadyTimer();
		}
		// Some startup paths create the pawn before PostLogin assigns the duel slot.
		// Correct that pawn immediately so slot 0 and slot 1 never share a start.
		if (AArenaDuelPlayerState* DuelPlayer = NewPlayer->GetPlayerState<AArenaDuelPlayerState>();
			DuelPlayer && DuelPlayer->GetDuelSlot() < 2)
		{
			AActor* ExpectedStart = FindPlayerStart(NewPlayer, TEXT(""));
			if (APawn* ExistingPawn = NewPlayer->GetPawn(); ExpectedStart && ExistingPawn
				&& !ExistingPawn->GetActorLocation().Equals(ExpectedStart->GetActorLocation(), 1.0f))
			{
				NewPlayer->UnPossess();
				ExistingPawn->Destroy();
				RestartPlayer(NewPlayer);
			}
		}
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
	JoiningPlayerState->SetDuelSlot(bSlotTaken[0] ? (bSlotTaken[1] ? 255 : 1) : 0);
}

void AArenaDuelGameMode::RestartPlayer(AController* NewPlayer)
{
	Super::RestartPlayer(NewPlayer);
	const AArenaDuelGameState* State = GetGameState<AArenaDuelGameState>();
	if (State && !State->IsRoundInProgress() && NewPlayer)
	{
		if (AArenaDuelCharacter* Character = Cast<AArenaDuelCharacter>(NewPlayer->GetPawn())) Character->SetRoundInputLocked(true);
	}
}

AActor* AArenaDuelGameMode::FindPlayerStart_Implementation(AController* Player, const FString& IncomingName)
{
	const AArenaDuelPlayerState* DuelPlayer = Player ? Player->GetPlayerState<AArenaDuelPlayerState>() : nullptr;
	const FName WantedTag = DuelPlayer && DuelPlayer->GetDuelSlot() < 2
		? (DuelPlayer->GetDuelSlot() == 0 ? FName(TEXT("ArenaCore_P1")) : FName(TEXT("ArenaCore_P2")))
		: NAME_None;
	if (WantedTag != NAME_None && GetWorld())
	{
		for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It)
		{
			if (It->PlayerStartTag == WantedTag) return *It;
		}
	}
	return Super::FindPlayerStart_Implementation(Player, IncomingName);
}

void AArenaDuelGameMode::Logout(AController* Exiting)
{
	if (const APlayerController* Controller = Cast<APlayerController>(Exiting))
		if (AArenaDuelPlayerState* Player = Controller->GetPlayerState<AArenaDuelPlayerState>())
		{
			Player->SetCharacterReadyAuthoritatively(false);
			Player->SetDuelSlot(255);
			AArenaDuelPhaseGateVisual::DestroyOwnedByPlayerState(GetWorld(), Player);
		}
	Super::Logout(Exiting);
	// No ranked forfeit here. A missing opponent safely returns the duel to selection.
	if (GetWorld() && !GetWorld()->bIsTearingDown) EnterCharacterSelect();
}

void AArenaDuelGameMode::EnterCharacterSelect()
{
	if (!HasAuthority() || !GetWorld()) return;
	ClearPendingRoundAndMatchTimers();
	bRoundRestartPending = false;
	AArenaDuelGameState* State = GetGameState<AArenaDuelGameState>();
	if (!State) return;
	for (APlayerState* Player : State->PlayerArray)
	{
		if (AArenaDuelPlayerState* DuelPlayer = Cast<AArenaDuelPlayerState>(Player))
		{
			DuelPlayer->SetRoundWinsForDevelopment(0);
			DuelPlayer->SetCharacterReadyAuthoritatively(false);
		}
	}
	State->SetMatchComplete(false, INDEX_NONE);
	State->SetRoundState(1, false, INDEX_NONE);
	State->SetMatchPhase(EArenaDuelMatchPhase::CharacterSelect);
	RestartDuelPlayers();
	StartCharacterAutoReadyTimer();
}

void AArenaDuelGameMode::StartCharacterAutoReadyTimer()
{
	AArenaDuelGameState* State = GetGameState<AArenaDuelGameState>();
	if (!HasAuthority() || !State || State->GetMatchPhase() != EArenaDuelMatchPhase::CharacterSelect) return;
	GetWorldTimerManager().ClearTimer(CharacterAutoReadyTimer);
	State->SetCharacterAutoReadyEndServerTime(State->GetServerWorldTimeSeconds() + CharacterAutoReadyDelaySeconds);
	GetWorldTimerManager().SetTimer(CharacterAutoReadyTimer, this, &ThisClass::AutoReadyCharacterSelectPlayers, CharacterAutoReadyDelaySeconds, false);
}

void AArenaDuelGameMode::AutoReadyCharacterSelectPlayers()
{
	AArenaDuelGameState* State = GetGameState<AArenaDuelGameState>();
	if (!HasAuthority() || !State || State->GetMatchPhase() != EArenaDuelMatchPhase::CharacterSelect) return;
	State->SetCharacterAutoReadyEndServerTime(0.0f);
	for (uint8 DuelSlot = 0; DuelSlot < 2; ++DuelSlot)
	{
		if (AArenaDuelPlayerState* Player = FindPlayerStateByDuelSlot(DuelSlot);
			Player && !Player->IsCharacterReady() && AArenaDuelPlayerState::IsImplementedArchetype(Player->GetCharacterArchetype()))
		{
			Player->SetCharacterReadyAuthoritatively(true);
		}
	}
	CheckBothReady();
}

void AArenaDuelGameMode::RequestCharacterSelection(APlayerController* Requester, EArenaDuelCharacterArchetype Archetype)
{
	const AArenaDuelGameState* State = GetGameState<AArenaDuelGameState>();
	AArenaDuelPlayerState* Player = Requester ? Requester->GetPlayerState<AArenaDuelPlayerState>() : nullptr;
	if (!HasAuthority() || !State || State->GetMatchPhase() != EArenaDuelMatchPhase::CharacterSelect || !Player || Player->GetOwner() != Requester || Player->GetDuelSlot() > 1 || Player->IsCharacterReady()) return;
	Player->SetCharacterArchetypeAuthoritatively(Archetype);
}

void AArenaDuelGameMode::RequestCharacterReady(APlayerController* Requester, bool bReady)
{
	const AArenaDuelGameState* State = GetGameState<AArenaDuelGameState>();
	AArenaDuelPlayerState* Player = Requester ? Requester->GetPlayerState<AArenaDuelPlayerState>() : nullptr;
	if (!HasAuthority() || !State || State->GetMatchPhase() != EArenaDuelMatchPhase::CharacterSelect || !Player || Player->GetOwner() != Requester || Player->GetDuelSlot() > 1) return;
	if (!AArenaDuelPlayerState::IsImplementedArchetype(Player->GetCharacterArchetype())) return;
	Player->SetCharacterReadyAuthoritatively(bReady);
	CheckBothReady();
}

void AArenaDuelGameMode::CheckBothReady()
{
	AArenaDuelGameState* State = GetGameState<AArenaDuelGameState>();
	AArenaDuelPlayerState* Left = FindPlayerStateByDuelSlot(0);
	AArenaDuelPlayerState* Right = FindPlayerStateByDuelSlot(1);
	if (!State || State->GetMatchPhase() != EArenaDuelMatchPhase::CharacterSelect || !Left || !Right || !Left->IsCharacterReady() || !Right->IsCharacterReady()) return;
	GetWorldTimerManager().ClearTimer(CharacterAutoReadyTimer);
	// Three numbered seconds plus a brief FIGHT beat, derived locally from one timestamp.
	State->SetMatchPhase(EArenaDuelMatchPhase::Countdown, State->GetServerWorldTimeSeconds() + 3.35f);
	GetWorldTimerManager().SetTimer(CharacterCountdownTimer, this, &AArenaDuelGameMode::StartSelectedMatch, 3.35f, false);
}

void AArenaDuelGameMode::StartSelectedMatch()
{
	AArenaDuelGameState* State = GetGameState<AArenaDuelGameState>();
	if (!State || State->GetMatchPhase() != EArenaDuelMatchPhase::Countdown) return;
	if (!FindPlayerStateByDuelSlot(0) || !FindPlayerStateByDuelSlot(1)) { EnterCharacterSelect(); return; }
	for (APlayerState* Player : State->PlayerArray)
		if (AArenaDuelPlayerState* DuelPlayer = Cast<AArenaDuelPlayerState>(Player)) DuelPlayer->SetCharacterReadyAuthoritatively(false);
	State->SetMatchComplete(false, INDEX_NONE);
	BeginRound(1, INDEX_NONE);
	RestartDuelPlayers();
}

void AArenaDuelGameMode::BeginRound(int32 RoundNumber, int32 LastWinnerSlot)
{
	AArenaDuelGameState* State = GetGameState<AArenaDuelGameState>();
	if (!HasAuthority() || !State || !GetWorld()) return;
	// A new serial retires whatever belonged to the round before: its time limit and its evaluation.
	++RoundSerial;
	GetWorldTimerManager().ClearTimer(RoundTimeLimitTimer);
	State->SetRoundState(RoundNumber, true, LastWinnerSlot);
	if (!UsesRoundTimer() || RoundDurationSeconds <= 0.0f) return;
	State->SetRoundClock(static_cast<float>(State->GetServerWorldTimeSeconds()) + RoundDurationSeconds);
	GetWorldTimerManager().SetTimer(RoundTimeLimitTimer, FTimerDelegate::CreateUObject(this, &AArenaDuelGameMode::HandleRoundTimeExpired, RoundSerial), RoundDurationSeconds, false);
}

void AArenaDuelGameMode::HandleRoundTimeExpired(int32 ForRoundSerial)
{
	AArenaDuelGameState* State = GetGameState<AArenaDuelGameState>();
	// A limit that belongs to an earlier round, or to a round that has already been decided, does nothing.
	if (!HasAuthority() || !State || ForRoundSerial != RoundSerial || bRoundRestartPending || !State->IsRoundInProgress() || State->IsOvertime()) return;
	TArray<AArenaDuelPlayerState*> Living;
	for (uint8 DuelSlot = 0; DuelSlot < 2; ++DuelSlot)
	{
		AArenaDuelPlayerState* Player = FindPlayerStateByDuelSlot(DuelSlot);
		const APlayerController* Controller = Player ? Cast<APlayerController>(Player->GetOwner()) : nullptr;
		const AArenaDuelCharacter* Character = Controller ? Cast<AArenaDuelCharacter>(Controller->GetPawn()) : nullptr;
		if (Character && !Character->IsDead()) Living.Add(Player);
	}
	// Interim rule: with both alive nothing is taken or changed, the round simply goes on until the next kill.
	if (Living.Num() >= 2) State->BeginOvertime();
	else EndRoundForDevelopment(Living.Num() == 1 ? Living[0] : nullptr);
}

bool AArenaDuelGameMode::IsRoundTimeLimitPending() const
{
	return GetWorld() && GetWorldTimerManager().IsTimerActive(RoundTimeLimitTimer);
}

void AArenaDuelGameMode::SetRoundTimingForTests(float InRoundDurationSeconds, float InRoundBreakSeconds, float InMatchResultSeconds)
{
	RoundDurationSeconds = FMath::Max(0.0f, InRoundDurationSeconds);
	RoundBreakSeconds = FMath::Max(0.1f, InRoundBreakSeconds);
	MatchResultSeconds = FMath::Max(0.1f, InMatchResultSeconds);
}

void AArenaDuelGameMode::ExpireRoundTimeForTests(bool bStale)
{
	HandleRoundTimeExpired(bStale ? RoundSerial - 1 : RoundSerial);
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
		if (ArenaPlayerState && !ArenaPlayerState->IsInactive() && ArenaPlayerState->GetDuelSlot() == DuelSlot) return ArenaPlayerState;
	}
	return nullptr;
}

void AArenaDuelGameMode::EndRoundForDevelopment(AArenaDuelPlayerState* WinningPlayerState)
{
	if (!HasAuthority() || bRoundRestartPending) return;
	AArenaDuelGameState* ArenaGameState = GetGameState<AArenaDuelGameState>();
	if (!ArenaGameState || !ArenaGameState->IsRoundInProgress()) return;
	// One evaluation per started round, whatever asks for it: a death, the time limit, the admin menu.
	if (EvaluatedRoundSerial == RoundSerial) return;
	EvaluatedRoundSerial = RoundSerial;
	++RoundsEvaluated;
	if (GetWorld()) GetWorldTimerManager().ClearTimer(RoundTimeLimitTimer);
	AArenaDuelVeilWall::DestroyAllForRound(GetWorld());
	AArenaDuelArcBarrier::DestroyAllForRound(GetWorld());
	AArenaDuelPhaseGateVisual::DestroyAllForRound(GetWorld());
	if (WinningPlayerState) WinningPlayerState->AwardRoundWin();
	const int32 WinnerSlot = WinningPlayerState ? static_cast<int32>(WinningPlayerState->GetDuelSlot()) : INDEX_NONE;
	ArenaGameState->SetRoundState(ArenaGameState->GetRoundNumber(), false, WinnerSlot);
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (APlayerController* PlayerController = It->Get())
		{
			if (AArenaDuelCharacter* Character = Cast<AArenaDuelCharacter>(PlayerController->GetPawn()))
			{
				if (UAbilitySystemComponent* ASC = Character->GetAbilitySystemComponent()) ASC->CancelAllAbilities();
				Character->SetRoundInputLocked(true);
			}
		}
	}
	bRoundRestartPending = true;

	const bool bMatchComplete = WinningPlayerState && WinningPlayerState->GetRoundWins() >= RoundWinsToWinMatch;
	if (bMatchComplete)
	{
		ArenaGameState->SetMatchComplete(true, WinnerSlot);
		ArenaGameState->SetMatchPhase(EArenaDuelMatchPhase::MatchResult);
		if (GetWorld()) GetWorldTimerManager().SetTimer(MatchResetTimer, this, &AArenaDuelGameMode::ResetMatchAndRestartPlayers, MatchResultSeconds, false);
	}
	else if (GetWorld())
	{
		GetWorldTimerManager().SetTimer(RoundRestartTimer, this, &AArenaDuelGameMode::StartNextRound, RoundBreakSeconds, false);
	}
}

void AArenaDuelGameMode::AdminRestartRound()
{
	if (!HasAuthority() || !GetWorld()) return;
	ClearPendingRoundAndMatchTimers();
	bRoundRestartPending = false;
	if (AArenaDuelGameState* ArenaGameState = GetGameState<AArenaDuelGameState>())
	{
		ArenaGameState->SetMatchComplete(false, INDEX_NONE);
		BeginRound(ArenaGameState->GetRoundNumber(), INDEX_NONE);
		RestartDuelPlayers();
	}
}

void AArenaDuelGameMode::AdminAdvanceRound()
{
	if (!HasAuthority() || !GetWorld()) return;
	ClearPendingRoundAndMatchTimers();
	bRoundRestartPending = false;
	if (AArenaDuelGameState* ArenaGameState = GetGameState<AArenaDuelGameState>())
	{
		ArenaGameState->SetMatchComplete(false, INDEX_NONE);
		BeginRound(ArenaGameState->GetRoundNumber() + 1, INDEX_NONE);
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
	ClearPendingRoundAndMatchTimers();
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
		ArenaGameState->SetMatchComplete(false, INDEX_NONE);
		BeginRound(1, INDEX_NONE);
		RestartDuelPlayers();
	}
}

void AArenaDuelGameMode::StartNextRound()
{
	if (!HasAuthority()) return;
	AArenaDuelGameState* ArenaGameState = GetGameState<AArenaDuelGameState>();
	if (!ArenaGameState || ArenaGameState->IsMatchComplete()) return;
	for (APlayerState* PlayerState : ArenaGameState->PlayerArray)
	{
		const AArenaDuelPlayerState* ArenaPlayerState = Cast<AArenaDuelPlayerState>(PlayerState);
		if (ArenaPlayerState && ArenaPlayerState->GetRoundWins() >= RoundWinsToWinMatch)
		{
			bRoundRestartPending = false;
			return;
		}
	}

	BeginRound(ArenaGameState->GetRoundNumber() + 1, ArenaGameState->GetLastRoundWinnerSlot());
	RestartDuelPlayers();
	bRoundRestartPending = false;
}

void AArenaDuelGameMode::ClearPendingRoundAndMatchTimers()
{
	GetWorldTimerManager().ClearTimer(RoundRestartTimer);
	GetWorldTimerManager().ClearTimer(MatchResetTimer);
	GetWorldTimerManager().ClearTimer(RoundTimeLimitTimer);
	GetWorldTimerManager().ClearTimer(CharacterCountdownTimer);
	GetWorldTimerManager().ClearTimer(CharacterAutoReadyTimer);
}

void AArenaDuelGameMode::ResetMatchAndRestartPlayers()
{
	EnterCharacterSelect();
}

void AArenaDuelGameMode::RestartDuelPlayers()
{
	if (!HasAuthority() || !GetWorld()) return;
	AArenaDuelVeilWall::DestroyAllForRound(GetWorld());
	AArenaDuelArcBarrier::DestroyAllForRound(GetWorld());
	AArenaDuelPhaseGateVisual::DestroyAllForRound(GetWorld());
	TArray<APlayerController*> Controllers;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (APlayerController* PlayerController = It->Get())
		{
			const AArenaDuelPlayerState* DuelPlayer = PlayerController->GetPlayerState<AArenaDuelPlayerState>();
			if (!DuelPlayer || DuelPlayer->GetDuelSlot() > 1 || PlayerController->IsActorBeingDestroyed()) continue;
			Controllers.Add(PlayerController);
			if (AArenaDuelPlayerState* PlayerState = PlayerController->GetPlayerState<AArenaDuelPlayerState>())
			{
				PlayerState->ResetAbilitiesForNewRound();
			}
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
