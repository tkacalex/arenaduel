// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "CQTest.h"
#include "Components/PIENetworkComponent.h"
#include "Tests/AutomationEditorCommon.h"

#include "ArenaDuel/Characters/ArenaDuelCharacter.h"
#include "ArenaDuel/Characters/ArenaDuelCharacterMovementComponent.h"
#include "ArenaDuel/Game/ArenaDuelGameState.h"
#include "ArenaDuel/Game/ArenaDuelGameMode.h"
#include "ArenaDuel/Player/ArenaDuelPlayerState.h"
#include "ArenaDuel/Weapons/ArenaDuelWeaponComponent.h"
#include "EngineUtils.h"
#include "GameFramework/GameModeBase.h"
#include "ArenaDuel/Abilities/ArenaDuelVeilWall.h"

namespace ArenaDuelPhase6RoundTests
{
	static constexpr TCHAR GameModeClassPath[] = TEXT("/Game/ArenaDuel/Game/BP_ArenaDuelGameMode.BP_ArenaDuelGameMode_C");

	static int32 CountWins(const AArenaDuelGameState* GameState)
	{
		if (!GameState) return 0;
		int32 TotalWins = 0;
		for (APlayerState* PlayerState : GameState->PlayerArray)
		{
			if (const AArenaDuelPlayerState* DuelState = Cast<AArenaDuelPlayerState>(PlayerState))
			{
				TotalWins += DuelState->GetRoundWins();
			}
		}
		return TotalWins;
	}

	static bool BothPlayersReset(UWorld* World)
	{
		if (!World) return false;
		int32 LivePlayers = 0;
		for (TActorIterator<AArenaDuelCharacter> It(World); It; ++It)
		{
			const AArenaDuelCharacter* Character = *It;
			if (!Character->IsDead() && Character->GetHealth() >= Character->GetMaxHealth() - 0.1f
				&& Character->GetCharacterMovement()->MovementMode != MOVE_None)
			{
				++LivePlayers;
			}
		}
		return LivePlayers == 2;
	}

	static APlayerController* FindControllerForSlot(UWorld* World, uint8 Slot)
	{
		if (!World) return nullptr;
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			APlayerController* Controller = It->Get();
			const AArenaDuelPlayerState* State = Controller ? Controller->GetPlayerState<AArenaDuelPlayerState>() : nullptr;
			if (State && State->GetDuelSlot() == Slot) return Controller;
		}
		return nullptr;
	}

	static int32 CountVeilWalls(UWorld* World)
	{
		int32 Count = 0;
		if (World) for (TActorIterator<AArenaDuelVeilWall> It(World); It; ++It) ++Count;
		return Count;
	}
}

struct FArenaDuelPhase6RoundNetworkState : public FBasePIENetworkComponentState
{
	AArenaDuelCharacter* VictimPawn = nullptr;
	AArenaDuelCharacter* InitialPawns[2] = { nullptr, nullptr };
	double MatchEndTime = 0.0;
};

NETWORK_TEST_CLASS(FArenaDuelPhase6RoundNetworkTest, "ArenaDuel.Phase6.Network")
{
	FPIENetworkComponent<FArenaDuelPhase6RoundNetworkState> Network{TestRunner, TestCommandBuilder, bInitializing};

	BEFORE_EACH()
	{
		UClass* GameModeClass = LoadClass<AGameModeBase>(nullptr, ArenaDuelPhase6RoundTests::GameModeClassPath);
		FNetworkComponentBuilder<FArenaDuelPhase6RoundNetworkState>()
			.WithClients(1)
			.AsListenServer()
			.WithGameMode(GameModeClass)
			.Build(Network);
	}

	TEST_METHOD(DeathAwardsRoundAndRestartsBothPlayers)
	{
		Network
			.UntilServer(TEXT("Wait for two server pawns"), [](FArenaDuelPhase6RoundNetworkState& State)
			{
				int32 ControlledPlayers = 0;
				for (FConstPlayerControllerIterator It = State.World->GetPlayerControllerIterator(); It; ++It)
				{
					if (APlayerController* Controller = It->Get())
					{
						if (AArenaDuelCharacter* Pawn = Cast<AArenaDuelCharacter>(Controller->GetPawn()))
						{
							++ControlledPlayers;
							if (!State.VictimPawn) State.VictimPawn = Pawn;
						}
					}
				}
				return ControlledPlayers == 2 && State.VictimPawn;
			}, FTimespan::FromSeconds(10.0))
			.ThenServer(TEXT("Apply lethal server damage"), [](FArenaDuelPhase6RoundNetworkState& State)
			{
				if (State.VictimPawn) State.VictimPawn->ApplyServerDamage(1000.0f);
			})
			.ThenServer(TEXT("Both pawns are locked during the round break"), [this](FArenaDuelPhase6RoundNetworkState& State)
			{
				const AArenaDuelGameState* GameState = State.World->GetGameState<AArenaDuelGameState>();
				if (!GameState || GameState->IsRoundInProgress()) TestRunner->AddError(TEXT("Server did not end the round before the break"));
				for (TActorIterator<AArenaDuelCharacter> It(State.World); It; ++It)
				{
					AArenaDuelCharacter* Character = *It;
					if (Character->IsDead()) continue;
					UArenaDuelWeaponComponent* Weapon = Character->GetWeaponComponent();
					if (Character->GetCharacterMovement()->MovementMode != MOVE_None)
					{
						TestRunner->AddError(TEXT("Surviving pawn movement was not disabled for the round break"));
					}
					if (Weapon)
					{
						const EArenaDuelWeaponId OriginalWeapon = Weapon->GetCurrentWeaponId();
						Weapon->StartFire();
						Weapon->StartAim();
						Weapon->Reload();
						Weapon->EquipWeapon((static_cast<int32>(OriginalWeapon) + 1) % Weapon->GetWeaponDefinitionCount());
						if (Weapon->IsFireHeld() || Weapon->IsAiming() || Weapon->IsReloading() || Weapon->GetCurrentWeaponId() != OriginalWeapon)
						{
							TestRunner->AddError(TEXT("Server accepted combat input during the round break"));
						}
					}
				}
			})
			.UntilClient(TEXT("Client receives round-end input lock"), 0, [](FArenaDuelPhase6RoundNetworkState& State)
			{
				const AArenaDuelGameState* GameState = State.World->GetGameState<AArenaDuelGameState>();
				if (!GameState || GameState->IsRoundInProgress()) return false;
				for (TActorIterator<AArenaDuelCharacter> It(State.World); It; ++It)
				{
					if (It->GetCharacterMovement()->MovementMode != MOVE_None) return false;
				}
				return true;
			}, FTimespan::FromSeconds(2.0))
			.UntilServer(TEXT("Server awards and restarts the round"), [](FArenaDuelPhase6RoundNetworkState& State)
			{
				const AArenaDuelGameState* GameState = State.World->GetGameState<AArenaDuelGameState>();
				return GameState && GameState->GetRoundNumber() == 2 && GameState->IsRoundInProgress()
					&& ArenaDuelPhase6RoundTests::CountWins(GameState) == 1
					&& ArenaDuelPhase6RoundTests::BothPlayersReset(State.World);
			}, FTimespan::FromSeconds(8.0))
			.UntilClient(TEXT("Client receives round score and both reset pawns"), 0, [](FArenaDuelPhase6RoundNetworkState& State)
			{
				const AArenaDuelGameState* GameState = State.World->GetGameState<AArenaDuelGameState>();
				return GameState && GameState->GetRoundNumber() == 2 && GameState->IsRoundInProgress()
					&& ArenaDuelPhase6RoundTests::CountWins(GameState) == 1
					&& ArenaDuelPhase6RoundTests::BothPlayersReset(State.World);
			}, FTimespan::FromSeconds(10.0));
	}

	TEST_METHOD(FirstToFiveShowsMatchResultThenStartsFreshMatchForEitherWinner)
	{
		Network
			.UntilServer(TEXT("Wait for both duel slots"), [](FArenaDuelPhase6RoundNetworkState& State)
			{
				return ArenaDuelPhase6RoundTests::FindControllerForSlot(State.World, 0)
					&& ArenaDuelPhase6RoundTests::FindControllerForSlot(State.World, 1);
			}, FTimespan::FromSeconds(10.0))
			.ThenServer(TEXT("Reach five wins for Player 1 and end match"), [](FArenaDuelPhase6RoundNetworkState& State)
			{
				AArenaDuelGameMode* GameMode = State.World->GetAuthGameMode<AArenaDuelGameMode>();
				AArenaDuelGameState* GameState = State.World->GetGameState<AArenaDuelGameState>();
				AArenaDuelPlayerState* P1 = GameMode ? GameMode->FindPlayerStateByDuelSlot(0) : nullptr;
				AArenaDuelPlayerState* P2 = GameMode ? GameMode->FindPlayerStateByDuelSlot(1) : nullptr;
				if (!GameMode || !GameState || !P1 || !P2) return;
				P1->SetRoundWinsForDevelopment(4);
				P2->SetRoundWinsForDevelopment(3);
				for (uint8 Slot = 0; Slot < 2; ++Slot)
				{
					APlayerController* Controller = ArenaDuelPhase6RoundTests::FindControllerForSlot(State.World, Slot);
					State.InitialPawns[Slot] = Controller ? Cast<AArenaDuelCharacter>(Controller->GetPawn()) : nullptr;
				}
				State.World->SpawnActor<AArenaDuelVeilWall>(FVector(300.0f, 0.0f, 120.0f), FRotator::ZeroRotator);
				GameMode->AdminAwardRound(0);
				State.MatchEndTime = State.World->GetTimeSeconds();
			})
			.ThenServer(TEXT("Replicated match result is authoritative and locks both players"), [this](FArenaDuelPhase6RoundNetworkState& State)
			{
				const AArenaDuelGameState* GameState = State.World->GetGameState<AArenaDuelGameState>();
				const AArenaDuelGameMode* GameMode = State.World->GetAuthGameMode<AArenaDuelGameMode>();
				const AArenaDuelPlayerState* P1 = GameMode ? GameMode->FindPlayerStateByDuelSlot(0) : nullptr;
				const AArenaDuelPlayerState* P2 = GameMode ? GameMode->FindPlayerStateByDuelSlot(1) : nullptr;
				if (!GameState || !GameState->IsMatchComplete() || GameState->GetMatchWinnerSlot() != 0 || GameState->IsRoundInProgress()
					|| !P1 || P1->GetRoundWins() != 5 || !P2 || P2->GetRoundWins() != 3)
				{
					TestRunner->AddError(TEXT("Player 1 match completion did not preserve and publish the 5:3 result."));
				}
				for (uint8 Slot = 0; Slot < 2; ++Slot)
				{
					APlayerController* Controller = ArenaDuelPhase6RoundTests::FindControllerForSlot(State.World, Slot);
					const AArenaDuelCharacter* Pawn = Controller ? Cast<AArenaDuelCharacter>(Controller->GetPawn()) : nullptr;
					if (!Pawn || Pawn->GetCharacterMovement()->MovementMode != MOVE_None) TestRunner->AddError(TEXT("A player was not movement-locked during the result display."));
				}
			})
			.UntilClient(TEXT("Client receives match winner and inactive round"), 0, [](FArenaDuelPhase6RoundNetworkState& State)
			{
				const AArenaDuelGameState* GameState = State.World->GetGameState<AArenaDuelGameState>();
				return GameState && GameState->IsMatchComplete() && GameState->GetMatchWinnerSlot() == 0 && !GameState->IsRoundInProgress();
			}, FTimespan::FromSeconds(3.0))
			.UntilServer(TEXT("No normal three-second restart occurs"), [this](FArenaDuelPhase6RoundNetworkState& State)
			{
				if (State.World->GetTimeSeconds() - State.MatchEndTime < 3.2) return false;
				const AArenaDuelGameState* GameState = State.World->GetGameState<AArenaDuelGameState>();
				for (uint8 Slot = 0; Slot < 2; ++Slot)
				{
					APlayerController* Controller = ArenaDuelPhase6RoundTests::FindControllerForSlot(State.World, Slot);
					if (!Controller || Controller->GetPawn() != State.InitialPawns[Slot]) TestRunner->AddError(TEXT("A pawn restarted on the normal round timer during the match result."));
				}
				if (!GameState || !GameState->IsMatchComplete() || GameState->IsRoundInProgress()) TestRunner->AddError(TEXT("Match result ended before the five-second display elapsed."));
				return true;
			}, FTimespan::FromSeconds(4.0))
			.UntilServer(TEXT("Player 1 winner resets to a clean 0:0 match"), [this](FArenaDuelPhase6RoundNetworkState& State)
			{
				const AArenaDuelGameState* GameState = State.World->GetGameState<AArenaDuelGameState>();
				const AArenaDuelGameMode* GameMode = State.World->GetAuthGameMode<AArenaDuelGameMode>();
				const AArenaDuelPlayerState* P1 = GameMode ? GameMode->FindPlayerStateByDuelSlot(0) : nullptr;
				const AArenaDuelPlayerState* P2 = GameMode ? GameMode->FindPlayerStateByDuelSlot(1) : nullptr;
				return GameState && !GameState->IsMatchComplete() && GameState->GetMatchWinnerSlot() == INDEX_NONE
					&& GameState->IsRoundInProgress() && GameState->GetRoundNumber() == 1
					&& P1 && P1->GetRoundWins() == 0 && P2 && P2->GetRoundWins() == 0
					&& ArenaDuelPhase6RoundTests::BothPlayersReset(State.World)
					&& ArenaDuelPhase6RoundTests::CountVeilWalls(State.World) == 0
					&& FMath::IsNearlyZero(P1->GetShadowStepCooldownRemaining())
					&& FMath::IsNearlyZero(P1->GetVeilWallCooldownRemaining())
					&& FMath::IsNearlyZero(P2->GetShadowStepCooldownRemaining())
					&& FMath::IsNearlyZero(P2->GetVeilWallCooldownRemaining());
			}, FTimespan::FromSeconds(8.0))
			.ThenServer(TEXT("Reach five wins for Player 2 and end match"), [](FArenaDuelPhase6RoundNetworkState& State)
			{
				AArenaDuelGameMode* GameMode = State.World->GetAuthGameMode<AArenaDuelGameMode>();
				AArenaDuelPlayerState* P2 = GameMode ? GameMode->FindPlayerStateByDuelSlot(1) : nullptr;
				if (!GameMode || !P2) return;
				P2->SetRoundWinsForDevelopment(4);
				GameMode->AdminAwardRound(1);
				State.MatchEndTime = State.World->GetTimeSeconds();
			})
			.ThenServer(TEXT("Player 2 is the replicated winner"), [this](FArenaDuelPhase6RoundNetworkState& State)
			{
				const AArenaDuelGameState* GameState = State.World->GetGameState<AArenaDuelGameState>();
				const AArenaDuelGameMode* GameMode = State.World->GetAuthGameMode<AArenaDuelGameMode>();
				const AArenaDuelPlayerState* P2 = GameMode ? GameMode->FindPlayerStateByDuelSlot(1) : nullptr;
				if (!GameState || !GameState->IsMatchComplete() || GameState->GetMatchWinnerSlot() != 1 || !P2 || P2->GetRoundWins() != 5)
				{
					TestRunner->AddError(TEXT("Player 2 match winner state was not published correctly."));
				}
			})
			.UntilServer(TEXT("Player 2 winner also starts a clean fresh match"), [](FArenaDuelPhase6RoundNetworkState& State)
			{
				const AArenaDuelGameState* GameState = State.World->GetGameState<AArenaDuelGameState>();
				const AArenaDuelGameMode* GameMode = State.World->GetAuthGameMode<AArenaDuelGameMode>();
				const AArenaDuelPlayerState* P1 = GameMode ? GameMode->FindPlayerStateByDuelSlot(0) : nullptr;
				const AArenaDuelPlayerState* P2 = GameMode ? GameMode->FindPlayerStateByDuelSlot(1) : nullptr;
				return GameState && !GameState->IsMatchComplete() && GameState->GetMatchWinnerSlot() == INDEX_NONE
					&& GameState->IsRoundInProgress() && GameState->GetRoundNumber() == 1
					&& P1 && P1->GetRoundWins() == 0 && P2 && P2->GetRoundWins() == 0
					&& ArenaDuelPhase6RoundTests::BothPlayersReset(State.World);
			}, FTimespan::FromSeconds(8.0));
	}
};

#endif // WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
