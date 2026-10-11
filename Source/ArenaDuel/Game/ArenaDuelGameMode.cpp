// Copyright Epic Games, Inc. All Rights Reserved.

#include "ArenaDuelGameMode.h"

#include "../ArenaDuel.h"

#include "../Characters/ArenaDuelCharacter.h"
#include "ArenaDuelGameState.h"
#include "../Abilities/ArenaDuelVeilWall.h"
#include "../Abilities/ArenaDuelArcBarrier.h"
#include "../Abilities/ArenaDuelPhaseGateVisual.h"
#include "../Player/ArenaDuelPlayerController.h"
#include "../Player/ArenaDuelPlayerState.h"
#include "../Weapons/ArenaDuelWeaponComponent.h"
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
	// 0 falls back to the plain round break after every round. For tests of that break and for comparison in play tests.
	TAutoConsoleVariable<int32> CVarUpgradesEnabled(TEXT("ArenaDuel.Upgrades.Enabled"), 1, TEXT("Server: 1 offers match upgrades after a decided round, 0 uses the plain round break"));

#if !UE_BUILD_SHIPPING
	// Round flow from the console, on the server, for checks with two players: no need to wait out 90 seconds.
	AArenaDuelGameMode* DuelRules(UWorld* World) { return World ? World->GetAuthGameMode<AArenaDuelGameMode>() : nullptr; }
	FAutoConsoleCommandWithWorldAndArgs CmdRoundTimeLeft(TEXT("ArenaDuel.Round.TimeLeft"), TEXT("Server: set the regular time left in the running round, in seconds"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (AArenaDuelGameMode* Rules = DuelRules(World); Rules && Args.Num() > 0) Rules->SetRoundTimeRemainingForDevelopment(FCString::Atof(*Args[0]));
		}));
	FAutoConsoleCommandWithWorldAndArgs CmdRoundKill(TEXT("ArenaDuel.Round.Kill"), TEXT("Server: kill the player in duel slot 0 or 1 through the normal damage path"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (AArenaDuelGameMode* Rules = DuelRules(World); Rules && Args.Num() > 0) Rules->KillDuelSlotForDevelopment(static_cast<uint8>(FMath::Clamp(FCString::Atoi(*Args[0]), 0, 1)));
		}));
#endif
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
		const AArenaDuelPlayerState* Joined = NewPlayer->GetPlayerState<AArenaDuelPlayerState>();
		// One line per change of who is in the match, so a session between separate processes can be read from the host's log.
		UE_LOG(LogArenaDuel, Log, TEXT("Match: %s joined, %s"), *GetNameSafe(NewPlayer), Joined && Joined->IsDuelSpectator() ? TEXT("watching (both duel places are taken)") : *FString::Printf(TEXT("duel slot %d"), Joined ? static_cast<int32>(Joined->GetDuelSlot()) : -1));
		if (IsLimitedToDuelPlayers() && (!Joined || Joined->GetDuelSlot() > 1))
		{
			// Both places are taken: this player watches. The engine gave them a pawn before the slot was
			// known; it goes, and no later restart gives them another (RestartPlayer).
			if (APawn* Extra = NewPlayer->GetPawn())
			{
				NewPlayer->UnPossess();
				Extra->Destroy();
			}
			return;
		}
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
	JoiningPlayerState->SetDuelSpectator(bSlotTaken[0] && bSlotTaken[1]);
}

void AArenaDuelGameMode::RestartPlayer(AController* NewPlayer)
{
	// A player who has been told they are not one of the two gets no pawn. (A player who has not been given a
	// slot yet is let through: the engine starts a joining player before PostLogin assigns it.)
	if (const AArenaDuelPlayerState* Asked = NewPlayer ? NewPlayer->GetPlayerState<AArenaDuelPlayerState>() : nullptr; Asked && IsLimitedToDuelPlayers() && Asked->IsDuelSpectator()) return;
	Super::RestartPlayer(NewPlayer);
	// The new pawn starts with what its player has taken so far in this match.
	if (const AArenaDuelCharacter* Spawned = NewPlayer ? Cast<AArenaDuelCharacter>(NewPlayer->GetPawn()) : nullptr; Spawned && Spawned->GetWeaponComponent()) Spawned->GetWeaponComponent()->ApplyMatchUpgrades();
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
	bool bWasDuelPlayer = false;
	if (const APlayerController* Controller = Cast<APlayerController>(Exiting))
		if (AArenaDuelPlayerState* Player = Controller->GetPlayerState<AArenaDuelPlayerState>())
		{
			bWasDuelPlayer = Player->GetDuelSlot() < 2;
			Player->SetCharacterReadyAuthoritatively(false);
			Player->SetDuelSlot(255);
			AArenaDuelPhaseGateVisual::DestroyOwnedByPlayerState(GetWorld(), Player);
		}
	UE_LOG(LogArenaDuel, Log, TEXT("Match: %s left, %s"), *GetNameSafe(Exiting), bWasDuelPlayer ? TEXT("a duel player: back to character selection") : TEXT("not a duel player: the match goes on"));
	Super::Logout(Exiting);
	// No ranked forfeit here. A missing opponent safely returns the duel to selection. Someone who was only
	// watching leaves without touching the match.
	if (bWasDuelPlayer && GetWorld() && !GetWorld()->bIsTearingDown) EnterCharacterSelect();
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
			// Upgrades belong to one match: they end with it, however it ends.
			DuelPlayer->ResetUpgradesAuthoritatively();
			DuelPlayer->SetCharacterReadyAuthoritatively(false);
		}
	}
	State->SetMatchComplete(false, INDEX_NONE);
	State->SetRoundState(1, false, INDEX_NONE);
	State->SetMatchPhase(EArenaDuelMatchPhase::CharacterSelect);
	UE_LOG(LogArenaDuel, Log, TEXT("Match: character selection, score and upgrades cleared"));
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
	UE_LOG(LogArenaDuel, Log, TEXT("Match: round %d begins"), RoundNumber);
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
	// The selection that follows a decided round is as short as the break, unless a test sets it itself.
	UpgradeSelectSeconds = FMath::Max(0.1f, InRoundBreakSeconds);
	RoundDurationSeconds = FMath::Max(0.0f, InRoundDurationSeconds);
	RoundBreakSeconds = FMath::Max(0.1f, InRoundBreakSeconds);
	MatchResultSeconds = FMath::Max(0.1f, InMatchResultSeconds);
}

void AArenaDuelGameMode::SetRoundTimeRemainingForDevelopment(float Seconds)
{
	AArenaDuelGameState* State = GetGameState<AArenaDuelGameState>();
	if (!HasAuthority() || !State || !GetWorld() || !UsesRoundTimer() || !State->IsRoundInProgress() || State->IsOvertime() || bRoundRestartPending) return;
	const float Left = FMath::Max(0.1f, Seconds);
	State->SetRoundClock(static_cast<float>(State->GetServerWorldTimeSeconds()) + Left);
	GetWorldTimerManager().SetTimer(RoundTimeLimitTimer, FTimerDelegate::CreateUObject(this, &AArenaDuelGameMode::HandleRoundTimeExpired, RoundSerial), Left, false);
}

void AArenaDuelGameMode::KillDuelSlotForDevelopment(uint8 DuelSlot)
{
	const AArenaDuelPlayerState* Player = FindPlayerStateByDuelSlot(DuelSlot);
	const APlayerController* Controller = Player ? Cast<APlayerController>(Player->GetOwner()) : nullptr;
	if (AArenaDuelCharacter* Character = Controller ? Cast<AArenaDuelCharacter>(Controller->GetPawn()) : nullptr; Character && !Character->IsDead()) Character->ApplyServerDamage(100000.0f);
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

	// Only the two duel players decide a round: the one who died loses it, the other one wins it if alive.
	AArenaDuelPlayerState* LosingPlayerState = DeadCharacter->GetPlayerState<AArenaDuelPlayerState>();
	if (!LosingPlayerState || LosingPlayerState->GetDuelSlot() > 1) return;
	AArenaDuelPlayerState* WinningPlayerState = FindPlayerStateByDuelSlot(1 - LosingPlayerState->GetDuelSlot());
	const AController* WinningController = WinningPlayerState ? Cast<AController>(WinningPlayerState->GetOwner()) : nullptr;
	const AArenaDuelCharacter* WinningCharacter = WinningController ? Cast<AArenaDuelCharacter>(WinningController->GetPawn()) : nullptr;
	if (!WinningCharacter || WinningCharacter->IsDead()) WinningPlayerState = nullptr;

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
	UE_LOG(LogArenaDuel, Log, TEXT("Match: round %d decided, winner slot %d"), ArenaGameState->GetRoundNumber(), WinnerSlot);
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
		// A decided round is followed by the upgrade selection; a round without a winner by the plain break.
		if (WinningPlayerState && UsesMatchUpgrades() && CVarUpgradesEnabled.GetValueOnGameThread() != 0) BeginUpgradeSelection();
		else GetWorldTimerManager().SetTimer(RoundRestartTimer, this, &AArenaDuelGameMode::StartNextRound, RoundBreakSeconds, false);
	}
}

void AArenaDuelGameMode::BeginUpgradeSelection()
{
	AArenaDuelGameState* State = GetGameState<AArenaDuelGameState>();
	if (!HasAuthority() || !State || !GetWorld()) return;
	bool bAnythingToChoose = false;
	for (uint8 DuelSlot = 0; DuelSlot < 2; ++DuelSlot)
	{
		if (AArenaDuelPlayerState* Player = FindPlayerStateByDuelSlot(DuelSlot))
		{
			// A player who has everything is done at once.
			Player->SetUpgradeChosenAuthoritatively(!Player->HasAnyUpgradeAvailable());
			bAnythingToChoose |= Player->HasAnyUpgradeAvailable();
		}
	}
	if (!bAnythingToChoose)
	{
		GetWorldTimerManager().SetTimer(RoundRestartTimer, this, &AArenaDuelGameMode::StartNextRound, RoundBreakSeconds, false);
		return;
	}
	// Nobody moves during the selection, the round winner included.
	State->BeginUpgradeSelect(static_cast<float>(State->GetServerWorldTimeSeconds()) + UpgradeSelectSeconds);
	UE_LOG(LogArenaDuel, Log, TEXT("Match: upgrade selection %d opens for %.0f s"), State->GetUpgradeSelectSerial(), UpgradeSelectSeconds);
	GetWorldTimerManager().SetTimer(UpgradeSelectTimer, this, &AArenaDuelGameMode::FinishUpgradeSelection, UpgradeSelectSeconds, false);
}

void AArenaDuelGameMode::RequestUpgradeChoice(APlayerController* Requester, uint8 UpgradeId, int32 SelectionSerial)
{
	const AArenaDuelGameState* State = GetGameState<AArenaDuelGameState>();
	AArenaDuelPlayerState* Player = Requester ? Requester->GetPlayerState<AArenaDuelPlayerState>() : nullptr;
	// Only a duel player, only for themselves, only while the selection is open, only one upgrade, only a
	// known kind that is not at its maximum. Anything else, a second or a late request included, changes nothing.
	if (!HasAuthority() || !State || !State->IsUpgradeSelect() || !IsUpgradeSelectionPending()) return;
	// The request names the selection it was made in. One from an earlier selection, however late it
	// arrives, is not a choice in this one.
	if (SelectionSerial != State->GetUpgradeSelectSerial())
	{
		UE_LOG(LogArenaDuel, Log, TEXT("Match: upgrade choice for selection %d refused, selection %d is open"), SelectionSerial, State->GetUpgradeSelectSerial());
		return;
	}
	if (!Player || Player->GetOwner() != Requester || Player->GetDuelSlot() > 1 || Player->HasChosenUpgrade()) return;
	if (!ArenaDuelUpgrades::IsValidId(UpgradeId) || !Player->AddUpgradeLevelAuthoritatively(static_cast<EArenaDuelUpgrade>(UpgradeId))) return;
	Player->SetUpgradeChosenAuthoritatively(true);
	UE_LOG(LogArenaDuel, Log, TEXT("Match: duel slot %d took upgrade %d in selection %d"), static_cast<int32>(Player->GetDuelSlot()), static_cast<int32>(UpgradeId), SelectionSerial);
	const AArenaDuelPlayerState* Left = FindPlayerStateByDuelSlot(0);
	const AArenaDuelPlayerState* Right = FindPlayerStateByDuelSlot(1);
	if (Left && Right && Left->HasChosenUpgrade() && Right->HasChosenUpgrade()) FinishUpgradeSelection();
}

void AArenaDuelGameMode::FinishUpgradeSelection()
{
	const AArenaDuelGameState* State = GetGameState<AArenaDuelGameState>();
	if (!HasAuthority() || !State || !State->IsUpgradeSelect() || !GetWorld()) return;
	GetWorldTimerManager().ClearTimer(UpgradeSelectTimer);
	for (uint8 DuelSlot = 0; DuelSlot < 2; ++DuelSlot)
	{
		AArenaDuelPlayerState* Player = FindPlayerStateByDuelSlot(DuelSlot);
		if (!Player || Player->HasChosenUpgrade()) continue;
		// Too late: the player gets an available upgrade with the lowest own level, ties in the fixed order.
		if (const int32 Automatic = Player->PickAutomaticUpgrade(); Automatic != INDEX_NONE)
		{
			Player->AddUpgradeLevelAuthoritatively(static_cast<EArenaDuelUpgrade>(Automatic));
			UE_LOG(LogArenaDuel, Log, TEXT("Match: duel slot %d did not choose in time and got upgrade %d"), static_cast<int32>(DuelSlot), Automatic);
		}
		Player->SetUpgradeChosenAuthoritatively(true);
	}
	StartNextRound();
}

bool AArenaDuelGameMode::IsUpgradeSelectionPending() const
{
	return GetWorld() && GetWorldTimerManager().IsTimerActive(UpgradeSelectTimer);
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
				ArenaPlayerState->ResetUpgradesAuthoritatively();
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
	GetWorldTimerManager().ClearTimer(UpgradeSelectTimer);
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
