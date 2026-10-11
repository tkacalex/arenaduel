// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "CQTest.h"
#include "ArenaDuelTestUpgrades.h"
#include "Components/PIENetworkComponent.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"

#include "ArenaDuel/Characters/ArenaDuelCharacter.h"
#include "ArenaDuel/Game/ArenaDuelGameMode.h"
#include "ArenaDuel/Game/ArenaDuelGameState.h"
#include "ArenaDuel/Game/ArenaDuelZombieGameMode.h"
#include "ArenaDuel/Player/ArenaDuelPlayerState.h"
#include "ArenaDuel/Weapons/ArenaDuelWeaponComponent.h"
#include "EngineUtils.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FArenaDuelRoundRulesTest, "ArenaDuel.Phase8.RoundRules", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FArenaDuelRoundRulesTest::RunTest(const FString& Parameters)
{
	const AArenaDuelGameMode* Duel = GetDefault<AArenaDuelGameMode>();
	TestTrue(TEXT("A regular round lasts 90 seconds"), FMath::IsNearlyEqual(Duel->GetRoundDurationSeconds(), 90.0f));
	TestEqual(TEXT("Five round wins win the match"), Duel->GetRoundWinsToWinMatch(), 5);
	TestTrue(TEXT("The duel has a round clock"), Duel->UsesRoundTimer());
	TestFalse(TEXT("Survival, built on the duel classes, has none"), GetDefault<AArenaDuelZombieGameMode>()->UsesRoundTimer());
	TestEqual(TEXT("A full round reads 1:30"), AArenaDuelGameState::FormatRoundClock(90.0f), FString(TEXT("1:30")));
	TestEqual(TEXT("A started second still counts"), AArenaDuelGameState::FormatRoundClock(59.2f), FString(TEXT("1:00")));
	TestEqual(TEXT("The last fraction of a second reads 0:01"), AArenaDuelGameState::FormatRoundClock(0.4f), FString(TEXT("0:01")));
	TestEqual(TEXT("At the limit the clock reads 0:00"), AArenaDuelGameState::FormatRoundClock(0.0f), FString(TEXT("0:00")));
	TestEqual(TEXT("Nothing below zero"), AArenaDuelGameState::FormatRoundClock(-3.0f), FString(TEXT("0:00")));
	const AArenaDuelGameState* State = GetDefault<AArenaDuelGameState>();
	TestFalse(TEXT("A game state that was never given a clock shows none"), State->HasRoundTimer() || State->IsOvertime());
	return true;
}

namespace ArenaDuelRoundFlowTests
{
	static constexpr TCHAR GameModeClassPath[] = TEXT("/Game/ArenaDuel/Game/BP_ArenaDuelGameMode.BP_ArenaDuelGameMode_C");

	static AArenaDuelCharacter* PawnForSlot(UWorld* World, uint8 Slot)
	{
		if (!World) return nullptr;
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			APlayerController* Controller = It->Get();
			const AArenaDuelPlayerState* Player = Controller ? Controller->GetPlayerState<AArenaDuelPlayerState>() : nullptr;
			if (Player && Player->GetDuelSlot() == Slot) return Cast<AArenaDuelCharacter>(Controller->GetPawn());
		}
		return nullptr;
	}

	static int32 TotalWins(const AArenaDuelGameState* GameState)
	{
		int32 Wins = 0;
		if (GameState) for (APlayerState* Player : GameState->PlayerArray) if (const AArenaDuelPlayerState* Duel = Cast<AArenaDuelPlayerState>(Player)) Wins += Duel->GetRoundWins();
		return Wins;
	}

	static bool BothAliveInRound(UWorld* World)
	{
		const AArenaDuelGameState* GameState = World ? World->GetGameState<AArenaDuelGameState>() : nullptr;
		const AArenaDuelCharacter* A = PawnForSlot(World, 0);
		const AArenaDuelCharacter* B = PawnForSlot(World, 1);
		return GameState && GameState->IsRoundInProgress() && A && B && !A->IsDead() && !B->IsDead();
	}
}

struct FArenaDuelRoundFlowState : public FBasePIENetworkComponentState
{
	int32 Evaluated = 0;
	int32 RoundNumber = 0;
	float HealthBefore = 0.0f;
	int32 AmmoBefore = 0;
	FVector LocationBefore = FVector::ZeroVector;
	TWeakObjectPtr<AArenaDuelCharacter> PawnBefore;
};

NETWORK_TEST_CLASS(FArenaDuelRoundFlowNetworkTest, "ArenaDuel.Phase8.RoundFlow")
{
	FPIENetworkComponent<FArenaDuelRoundFlowState> Network{TestRunner, TestCommandBuilder, bInitializing};

	BEFORE_EACH()
	{
		SetMatchUpgradesForTests(true);
		UClass* GameModeClass = LoadClass<AGameModeBase>(nullptr, ArenaDuelRoundFlowTests::GameModeClassPath);
		FNetworkComponentBuilder<FArenaDuelRoundFlowState>()
			.WithClients(1)
			.AsListenServer()
			.WithGameMode(GameModeClass)
			.Build(Network);
	}

	TEST_METHOD(ClockRunsOutIntoOvertimeAndTheNextKillDecides)
	{
		Network
			.UntilServer(TEXT("Both duel players have pawns"), [](FArenaDuelRoundFlowState& State)
			{
				return ArenaDuelRoundFlowTests::PawnForSlot(State.World, 0) && ArenaDuelRoundFlowTests::PawnForSlot(State.World, 1);
			}, FTimespan::FromSeconds(10.0))
			.ThenServer(TEXT("Start a round with a two second limit"), [this](FArenaDuelRoundFlowState& State)
			{
				AArenaDuelGameMode* GameMode = State.World->GetAuthGameMode<AArenaDuelGameMode>();
				if (!GameMode) { TestRunner->AddError(TEXT("No duel game mode")); return; }
				GameMode->SetRoundTimingForTests(2.0f, 0.4f, 0.6f);
				GameMode->AdminRestartRound();
				State.Evaluated = GameMode->GetRoundsEvaluated();
			})
			.UntilServer(TEXT("The round runs with a clock on the server"), [this](FArenaDuelRoundFlowState& State)
			{
				const AArenaDuelGameState* GameState = State.World->GetGameState<AArenaDuelGameState>();
				const AArenaDuelGameMode* GameMode = State.World->GetAuthGameMode<AArenaDuelGameMode>();
				if (!ArenaDuelRoundFlowTests::BothAliveInRound(State.World) || !GameState->HasRoundTimer()) return false;
				if (!GameMode->IsRoundTimeLimitPending()) TestRunner->AddError(TEXT("The server holds no time limit for the running round"));
				if (GameState->GetRoundSecondsRemaining() <= 0.0f || GameState->GetRoundSecondsRemaining() > 2.01f) TestRunner->AddError(TEXT("The remaining time is not within the configured limit"));
				// A wound and a spent round must still be there after the limit.
				AArenaDuelCharacter* Pawn = ArenaDuelRoundFlowTests::PawnForSlot(State.World, 1);
				Pawn->ApplyServerDamage(30.0f);
				State.PawnBefore = Pawn;
				State.HealthBefore = Pawn->GetHealth();
				State.AmmoBefore = Pawn->GetWeaponComponent() ? Pawn->GetWeaponComponent()->GetCurrentMagazineAmmo() : 0;
				State.LocationBefore = Pawn->GetActorLocation();
				State.RoundNumber = GameState->GetRoundNumber();
				return true;
			}, FTimespan::FromSeconds(5.0))
			.UntilClient(TEXT("The client derives the same clock from the replicated end time"), 0, [](FArenaDuelRoundFlowState& State)
			{
				const AArenaDuelGameState* GameState = State.World->GetGameState<AArenaDuelGameState>();
				return GameState && (GameState->IsOvertime() || (GameState->HasRoundTimer() && GameState->GetRoundEndServerTime() > 0.0f && GameState->GetRoundSecondsRemaining() <= 2.01f));
			}, FTimespan::FromSeconds(5.0))
			.UntilServer(TEXT("The limit passes with both alive: overtime"), [](FArenaDuelRoundFlowState& State)
			{
				const AArenaDuelGameState* GameState = State.World->GetGameState<AArenaDuelGameState>();
				return GameState && GameState->IsOvertime();
			}, FTimespan::FromSeconds(6.0))
			.ThenServer(TEXT("Overtime changes nothing but the clock"), [this](FArenaDuelRoundFlowState& State)
			{
				const AArenaDuelGameState* GameState = State.World->GetGameState<AArenaDuelGameState>();
				const AArenaDuelGameMode* GameMode = State.World->GetAuthGameMode<AArenaDuelGameMode>();
				AArenaDuelCharacter* Pawn = ArenaDuelRoundFlowTests::PawnForSlot(State.World, 1);
				if (!GameState->IsRoundInProgress() || GameState->GetRoundNumber() != State.RoundNumber) TestRunner->AddError(TEXT("Overtime did not keep the same round running"));
				if (GameMode->GetRoundsEvaluated() != State.Evaluated || ArenaDuelRoundFlowTests::TotalWins(GameState) != 0) TestRunner->AddError(TEXT("The time limit alone awarded or evaluated a round"));
				if (GameMode->IsRoundTimeLimitPending()) TestRunner->AddError(TEXT("A time limit is still pending in overtime"));
				if (Pawn != State.PawnBefore.Get()) TestRunner->AddError(TEXT("Overtime replaced the pawn"));
				else
				{
					if (!FMath::IsNearlyEqual(Pawn->GetHealth(), State.HealthBefore, 0.1f)) TestRunner->AddError(TEXT("Overtime changed health"));
					if (Pawn->GetWeaponComponent() && Pawn->GetWeaponComponent()->GetCurrentMagazineAmmo() != State.AmmoBefore) TestRunner->AddError(TEXT("Overtime changed ammunition"));
					// Horizontal only: a pawn that has just spawned still settles onto the floor.
					if (FVector::Dist2D(Pawn->GetActorLocation(), State.LocationBefore) > 5.0f) TestRunner->AddError(TEXT("Overtime moved the player"));
				}
				if (GameState->GetRoundSecondsRemaining() != 0.0f) TestRunner->AddError(TEXT("Overtime still reports regular time"));
			})
			.UntilClient(TEXT("The client sees the overtime"), 0, [](FArenaDuelRoundFlowState& State)
			{
				const AArenaDuelGameState* GameState = State.World->GetGameState<AArenaDuelGameState>();
				return GameState && GameState->IsOvertime();
			}, FTimespan::FromSeconds(5.0))
			.ThenServer(TEXT("The next kill decides, once"), [this](FArenaDuelRoundFlowState& State)
			{
				AArenaDuelGameMode* GameMode = State.World->GetAuthGameMode<AArenaDuelGameMode>();
				const AArenaDuelGameState* GameState = State.World->GetGameState<AArenaDuelGameState>();
				AArenaDuelCharacter* Victim = ArenaDuelRoundFlowTests::PawnForSlot(State.World, 1);
				Victim->ApplyServerDamage(1000.0f);
				// Everything that could ask for a second evaluation of the same round.
				GameMode->HandlePlayerDeath(Victim);
				GameMode->AdminAwardRound(1);
				GameMode->ExpireRoundTimeForTests();
				const AArenaDuelPlayerState* Winner = GameMode->FindPlayerStateByDuelSlot(0);
				if (GameState->IsRoundInProgress() || GameState->IsOvertime() || GameState->HasRoundTimer()) TestRunner->AddError(TEXT("The kill in overtime did not end the round and its clock"));
				if (!Winner || Winner->GetRoundWins() != 1 || ArenaDuelRoundFlowTests::TotalWins(GameState) != 1) TestRunner->AddError(TEXT("The overtime kill did not award exactly one round to the survivor"));
				if (GameMode->GetRoundsEvaluated() != State.Evaluated + 1) TestRunner->AddError(TEXT("The round was evaluated more than once"));
				if (GameState->GetLastRoundWinnerSlot() != 0) TestRunner->AddError(TEXT("The round winner is not the survivor"));
			})
			.UntilServer(TEXT("The next round starts clean with a new clock"), [this](FArenaDuelRoundFlowState& State)
			{
				const AArenaDuelGameState* GameState = State.World->GetGameState<AArenaDuelGameState>();
				const AArenaDuelGameMode* GameMode = State.World->GetAuthGameMode<AArenaDuelGameMode>();
				if (!ArenaDuelRoundFlowTests::BothAliveInRound(State.World) || GameState->GetRoundNumber() != State.RoundNumber + 1) return false;
				const AArenaDuelCharacter* Pawn = ArenaDuelRoundFlowTests::PawnForSlot(State.World, 1);
				if (GameState->IsOvertime() || !GameState->HasRoundTimer() || !GameMode->IsRoundTimeLimitPending()) TestRunner->AddError(TEXT("The new round did not start with a fresh regular clock"));
				if (Pawn->GetHealth() < Pawn->GetMaxHealth() - 0.1f) TestRunner->AddError(TEXT("The new round did not restore health"));
				if (ArenaDuelRoundFlowTests::TotalWins(GameState) != 1) TestRunner->AddError(TEXT("Points changed between rounds"));
				return true;
			}, FTimespan::FromSeconds(6.0));
	}

	TEST_METHOD(KillAtTheLimitCountsOnceInEitherOrder)
	{
		Network
			.UntilServer(TEXT("Both duel players have pawns"), [](FArenaDuelRoundFlowState& State)
			{
				return ArenaDuelRoundFlowTests::PawnForSlot(State.World, 0) && ArenaDuelRoundFlowTests::PawnForSlot(State.World, 1);
			}, FTimespan::FromSeconds(10.0))
			.ThenServer(TEXT("Start a round with a long limit"), [](FArenaDuelRoundFlowState& State)
			{
				AArenaDuelGameMode* GameMode = State.World->GetAuthGameMode<AArenaDuelGameMode>();
				GameMode->SetRoundTimingForTests(30.0f, 0.4f, 0.6f);
				GameMode->AdminRestartRound();
			})
			.UntilServer(TEXT("Round is running"), [](FArenaDuelRoundFlowState& State) { return ArenaDuelRoundFlowTests::BothAliveInRound(State.World); }, FTimespan::FromSeconds(5.0))
			.ThenServer(TEXT("The kill lands first, the limit in the same frame after it"), [this](FArenaDuelRoundFlowState& State)
			{
				AArenaDuelGameMode* GameMode = State.World->GetAuthGameMode<AArenaDuelGameMode>();
				const AArenaDuelGameState* GameState = State.World->GetGameState<AArenaDuelGameState>();
				const int32 Evaluated = GameMode->GetRoundsEvaluated();
				ArenaDuelRoundFlowTests::PawnForSlot(State.World, 1)->ApplyServerDamage(1000.0f);
				GameMode->ExpireRoundTimeForTests();
				if (GameState->IsOvertime() || GameState->IsRoundInProgress()) TestRunner->AddError(TEXT("A limit after the kill reopened or extended the round"));
				if (ArenaDuelRoundFlowTests::TotalWins(GameState) != 1 || GameMode->GetRoundsEvaluated() != Evaluated + 1) TestRunner->AddError(TEXT("Kill then limit did not count exactly once"));
				if (GameMode->IsRoundTimeLimitPending()) TestRunner->AddError(TEXT("The decided round left its time limit pending"));
			})
			.UntilServer(TEXT("Next round is running"), [](FArenaDuelRoundFlowState& State)
			{
				const AArenaDuelGameState* GameState = State.World->GetGameState<AArenaDuelGameState>();
				return ArenaDuelRoundFlowTests::BothAliveInRound(State.World) && GameState->GetRoundNumber() >= 2 && GameState->HasRoundTimer();
			}, FTimespan::FromSeconds(6.0))
			.ThenServer(TEXT("The limit lands first, the kill in the same frame after it; a stale limit is ignored"), [this](FArenaDuelRoundFlowState& State)
			{
				AArenaDuelGameMode* GameMode = State.World->GetAuthGameMode<AArenaDuelGameMode>();
				const AArenaDuelGameState* GameState = State.World->GetGameState<AArenaDuelGameState>();
				const int32 Evaluated = GameMode->GetRoundsEvaluated();
				GameMode->ExpireRoundTimeForTests(true);
				if (GameState->IsOvertime()) TestRunner->AddError(TEXT("A time limit of an earlier round started overtime in this one"));
				GameMode->ExpireRoundTimeForTests();
				if (!GameState->IsOvertime() || !GameState->IsRoundInProgress()) TestRunner->AddError(TEXT("The limit with both alive did not start overtime"));
				ArenaDuelRoundFlowTests::PawnForSlot(State.World, 0)->ApplyServerDamage(1000.0f);
				const AArenaDuelPlayerState* Second = GameMode->FindPlayerStateByDuelSlot(1);
				if (GameState->IsRoundInProgress() || !Second || Second->GetRoundWins() != 1 || ArenaDuelRoundFlowTests::TotalWins(GameState) != 2 || GameMode->GetRoundsEvaluated() != Evaluated + 1)
					TestRunner->AddError(TEXT("Limit then kill did not count exactly once for the survivor"));
			});
	}

	TEST_METHOD(MatchEndNewMatchAndLeavingClearTheClock)
	{
		Network
			.UntilServer(TEXT("Both duel players have pawns"), [](FArenaDuelRoundFlowState& State)
			{
				return ArenaDuelRoundFlowTests::PawnForSlot(State.World, 0) && ArenaDuelRoundFlowTests::PawnForSlot(State.World, 1);
			}, FTimespan::FromSeconds(10.0))
			.ThenServer(TEXT("Fifth round win ends the match"), [this](FArenaDuelRoundFlowState& State)
			{
				AArenaDuelGameMode* GameMode = State.World->GetAuthGameMode<AArenaDuelGameMode>();
				const AArenaDuelGameState* GameState = State.World->GetGameState<AArenaDuelGameState>();
				GameMode->SetRoundTimingForTests(30.0f, 0.4f, 0.8f);
				GameMode->AdminRestartRound();
				AArenaDuelPlayerState* First = GameMode->FindPlayerStateByDuelSlot(0);
				if (!First) { TestRunner->AddError(TEXT("No player in slot 0")); return; }
				First->SetRoundWinsForDevelopment(4);
				if (!GameMode->IsRoundTimeLimitPending() || !GameState->HasRoundTimer()) TestRunner->AddError(TEXT("The round before the match point has no clock"));
				GameMode->AdminAwardRound(0);
				GameMode->AdminAwardRound(0);
				GameMode->ExpireRoundTimeForTests();
				if (!GameState->IsMatchComplete() || GameState->GetMatchWinnerSlot() != 0 || First->GetRoundWins() != 5) TestRunner->AddError(TEXT("The fifth win did not end the match 5 to 0"));
				if (GameMode->IsRoundTimeLimitPending() || GameState->HasRoundTimer() || GameState->IsOvertime()) TestRunner->AddError(TEXT("The match result still carries a round clock"));
			})
			.UntilServer(TEXT("The match returns to character selection with everything reset"), [this](FArenaDuelRoundFlowState& State)
			{
				const AArenaDuelGameState* GameState = State.World->GetGameState<AArenaDuelGameState>();
				const AArenaDuelGameMode* GameMode = State.World->GetAuthGameMode<AArenaDuelGameMode>();
				if (GameState->GetMatchPhase() != EArenaDuelMatchPhase::CharacterSelect) return false;
				if (GameState->IsMatchComplete() || ArenaDuelRoundFlowTests::TotalWins(GameState) != 0 || GameState->GetRoundNumber() != 1) TestRunner->AddError(TEXT("Selection after a match is not a clean start"));
				if (GameMode->IsRoundTimeLimitPending() || GameState->HasRoundTimer()) TestRunner->AddError(TEXT("A round clock runs in character selection"));
				return true;
			}, FTimespan::FromSeconds(6.0))
			.ThenServer(TEXT("A new match starts at round one with a full clock"), [this](FArenaDuelRoundFlowState& State)
			{
				AArenaDuelGameMode* GameMode = State.World->GetAuthGameMode<AArenaDuelGameMode>();
				const AArenaDuelGameState* GameState = State.World->GetGameState<AArenaDuelGameState>();
				GameMode->AdminResetMatch();
				if (!GameState->IsRoundInProgress() || GameState->GetRoundNumber() != 1 || !GameState->HasRoundTimer() || GameState->IsOvertime() || !GameMode->IsRoundTimeLimitPending()) TestRunner->AddError(TEXT("The new match did not start round one with a regular clock"));
				if (GameState->GetRoundSecondsRemaining() < 29.0f) TestRunner->AddError(TEXT("The new match did not get a full clock"));
				GameMode->ExpireRoundTimeForTests();
				if (!GameState->IsOvertime()) TestRunner->AddError(TEXT("Could not reach overtime for the leave check"));
			})
			.ThenServer(TEXT("A player leaving in overtime ends the duel without points or timers"), [this](FArenaDuelRoundFlowState& State)
			{
				AArenaDuelGameMode* GameMode = State.World->GetAuthGameMode<AArenaDuelGameMode>();
				const AArenaDuelGameState* GameState = State.World->GetGameState<AArenaDuelGameState>();
				const AArenaDuelPlayerState* Second = GameMode->FindPlayerStateByDuelSlot(1);
				AController* Leaving = Second ? Cast<AController>(Second->GetOwner()) : nullptr;
				if (!Leaving) { TestRunner->AddError(TEXT("No controller for slot 1")); return; }
				const int32 Evaluated = GameMode->GetRoundsEvaluated();
				GameMode->Logout(Leaving);
				if (GameState->GetMatchPhase() != EArenaDuelMatchPhase::CharacterSelect || GameState->IsRoundInProgress()) TestRunner->AddError(TEXT("Leaving did not return the duel to character selection"));
				if (GameState->HasRoundTimer() || GameState->IsOvertime() || GameMode->IsRoundTimeLimitPending()) TestRunner->AddError(TEXT("Leaving left a round clock behind"));
				if (ArenaDuelRoundFlowTests::TotalWins(GameState) != 0 || GameMode->GetRoundsEvaluated() != Evaluated) TestRunner->AddError(TEXT("Leaving awarded or evaluated a round"));
				// The limit of the abandoned round, should it still arrive, must do nothing.
				GameMode->ExpireRoundTimeForTests();
				GameMode->ExpireRoundTimeForTests(true);
				if (GameState->IsOvertime() || GameState->IsRoundInProgress()) TestRunner->AddError(TEXT("A late time limit acted after the duel was abandoned"));
			});
	}
};

#endif
