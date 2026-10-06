// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "CQTest.h"
#include "Components/PIENetworkComponent.h"
#include "Tests/AutomationEditorCommon.h"

#include "ArenaDuel/Characters/ArenaDuelCharacter.h"
#include "ArenaDuel/Game/ArenaDuelGameState.h"
#include "ArenaDuel/Player/ArenaDuelPlayerState.h"
#include "EngineUtils.h"
#include "GameFramework/GameModeBase.h"

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
			if (!Character->IsDead() && Character->GetHealth() >= Character->GetMaxHealth() - 0.1f)
			{
				++LivePlayers;
			}
		}
		return LivePlayers == 2;
	}
}

struct FArenaDuelPhase6RoundNetworkState : public FBasePIENetworkComponentState
{
	AArenaDuelCharacter* VictimPawn = nullptr;
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
};

#endif // WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
