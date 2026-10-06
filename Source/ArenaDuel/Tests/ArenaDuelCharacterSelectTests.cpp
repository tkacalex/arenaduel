#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "CQTest.h"
#include "Components/PIENetworkComponent.h"
#include "Misc/AutomationTest.h"
#include "ArenaDuel/UI/ArenaDuelCharacterSelectWidget.h"
#include "ArenaDuel/Game/ArenaDuelGameMode.h"
#include "ArenaDuel/Game/ArenaDuelGameState.h"
#include "ArenaDuel/Player/ArenaDuelPlayerController.h"
#include "ArenaDuel/Characters/ArenaDuelCharacter.h"
#include "ArenaDuel/Weapons/ArenaDuelWeaponComponent.h"
#include "ArenaDuel/Abilities/ArenaDuelGA_ArcBarrier.h"
#include "ArenaDuel/Abilities/ArenaDuelGA_BurstLeap.h"
#include "AbilitySystemComponent.h"
#include "Components/CanvasPanel.h"
#include "GameFramework/CharacterMovementComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FArenaDuelCharacterSelectTreeTest, "ArenaDuel.CharacterSelect.NativeWidget", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FArenaDuelCharacterSelectTreeTest::RunTest(const FString& Parameters)
{
	UArenaDuelCharacterSelectWidget* Widget = NewObject<UArenaDuelCharacterSelectWidget>();
	TestTrue(TEXT("Native initialization builds the fullscreen tree before Slate"), Widget->Initialize());
	TestTrue(TEXT("Both panels, rosters, ready controls, central VS and root exist"), Widget->HasExpectedTree());
	TestTrue(TEXT("Widget supports keyboard focus"), Widget->IsFocusable());
	UWidget* OriginalRoot = Widget->GetRootWidget();
	TestFalse(TEXT("Repeated initialization is guarded"), Widget->Initialize());
	TestEqual(TEXT("No duplicate root after repeated initialization"), Widget->GetRootWidget(), OriginalRoot);
	TestTrue(TEXT("Slate builds the native tree"), Widget->TakeWidget()->GetChildren()->Num() > 0);
	TestEqual(TEXT("Shadow ability description reflects launch, not teleport"), UArenaDuelCharacterSelectWidget::GetPresentation(EArenaDuelCharacterArchetype::Shadow).PrimaryName, FString(TEXT("SHADOW STEP")));
	TestEqual(TEXT("Warden uses real implemented ability names"), UArenaDuelCharacterSelectWidget::GetPresentation(EArenaDuelCharacterArchetype::Warden).PrimaryName, FString(TEXT("ARC BARRIER")));
	return true;
}

struct FArenaDuelSelectNetworkState : public FBasePIENetworkComponentState {};

namespace ArenaDuelSelectTests
{
	AArenaDuelPlayerController* Controller(UWorld* World, uint8 DuelSlot)
	{
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			AArenaDuelPlayerController* PC = Cast<AArenaDuelPlayerController>(It->Get());
			const AArenaDuelPlayerState* PS = PC ? PC->GetPlayerState<AArenaDuelPlayerState>() : nullptr;
			if (PS && PS->GetDuelSlot() == DuelSlot) return PC;
		}
		return nullptr;
	}
	AArenaDuelPlayerState* Player(UWorld* World, uint8 DuelSlot)
	{
		for (APlayerState* Base : World->GetGameState()->PlayerArray)
			if (AArenaDuelPlayerState* PS = Cast<AArenaDuelPlayerState>(Base); PS && PS->GetDuelSlot() == DuelSlot) return PS;
		return nullptr;
	}
	AArenaDuelPlayerController* Local(UWorld* World)
	{
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
			if (AArenaDuelPlayerController* PC = Cast<AArenaDuelPlayerController>(It->Get()); PC && PC->IsLocalController()) return PC;
		return nullptr;
	}
}

NETWORK_TEST_CLASS(FArenaDuelCharacterSelectNetworkTest, "ArenaDuel.CharacterSelect.Network")
{
	FPIENetworkComponent<FArenaDuelSelectNetworkState> Network{TestRunner, TestCommandBuilder, bInitializing};
	BEFORE_EACH()
	{
		UClass* Mode = LoadClass<AGameModeBase>(nullptr, TEXT("/Game/ArenaDuel/Game/BP_ArenaDuelGameMode.BP_ArenaDuelGameMode_C"));
		FNetworkComponentBuilder<FArenaDuelSelectNetworkState>().WithClients(1).AsListenServer().WithGameMode(Mode).Build(Network);
	}
	TEST_METHOD(SelectionCountdownRoundAndRematch)
	{
		using namespace ArenaDuelSelectTests;
		Network
		.UntilServer(TEXT("Both players connect into inactive character selection"), [](FArenaDuelSelectNetworkState& State)
		{
			return Controller(State.World, 0) && Controller(State.World, 1) && Controller(State.World, 0)->GetPawn() && Controller(State.World, 1)->GetPawn();
		}, FTimespan::FromSeconds(10))
		.ThenServer(TEXT("Self-owned selection, Rift rejection, single-player ready lock, gameplay denial"), [this](FArenaDuelSelectNetworkState& State)
		{
			AArenaDuelGameState* GS = State.World->GetGameState<AArenaDuelGameState>();
			AArenaDuelPlayerController* Host = Controller(State.World, 0);
			AArenaDuelPlayerState* P1 = Player(State.World, 0);
			AArenaDuelPlayerState* P2 = Player(State.World, 1);
			if (GS->GetMatchPhase() != EArenaDuelMatchPhase::CharacterSelect || GS->IsRoundInProgress()) TestRunner->AddError(TEXT("Initial phase is not locked CharacterSelect"));
			if (P1->GetCharacterArchetype() != EArenaDuelCharacterArchetype::Shadow || P2->GetCharacterArchetype() != EArenaDuelCharacterArchetype::Shadow) TestRunner->AddError(TEXT("Default selection changed"));
			Host->ServerRequestCharacterSelection(EArenaDuelCharacterArchetype::Rift);
			if (P1->GetCharacterArchetype() != EArenaDuelCharacterArchetype::Shadow) TestRunner->AddError(TEXT("Rift was allowed"));
			Host->ServerRequestCharacterSelection(EArenaDuelCharacterArchetype::Warden);
			if (P1->GetCharacterArchetype() != EArenaDuelCharacterArchetype::Warden || P2->GetCharacterArchetype() != EArenaDuelCharacterArchetype::Shadow) TestRunner->AddError(TEXT("Public selection altered the wrong player"));
			AArenaDuelCharacter* Pawn = Cast<AArenaDuelCharacter>(Host->GetPawn());
			Pawn->ApplyServerDamage(50);
			Pawn->GetWeaponComponent()->StartFire();
			if (Pawn->GetHealth() != 100 || Pawn->GetWeaponComponent()->GetCurrentMagazineAmmo() != 30 || P1->TryActivatePrimaryAbility() || Pawn->GetCharacterMovement()->MovementMode != MOVE_None) TestRunner->AddError(TEXT("Selection leaked gameplay"));
			Host->ServerSetCharacterReady(true);
			Host->ServerRequestCharacterSelection(EArenaDuelCharacterArchetype::Shadow);
			if (GS->GetMatchPhase() != EArenaDuelMatchPhase::CharacterSelect || P1->GetCharacterArchetype() != EArenaDuelCharacterArchetype::Warden) TestRunner->AddError(TEXT("One ready started countdown or unlocked selection"));
			Host->ServerSetCharacterReady(false);
			Host->ServerSetCharacterReady(true);
		})
		.UntilClient(TEXT("Replicated host selection and local fullscreen menu are visible"), 0, [](FArenaDuelSelectNetworkState& State)
		{
			const AArenaDuelPlayerState* P1 = Player(State.World, 0);
			AArenaDuelPlayerController* PC = Local(State.World);
			return P1 && P1->IsCharacterReady() && P1->GetCharacterArchetype() == EArenaDuelCharacterArchetype::Warden && PC && PC->IsCharacterSelectOpen() && PC->GetCharacterSelectWidget() && PC->GetCharacterSelectWidget()->HasExpectedTree() && PC->bShowMouseCursor;
		}, FTimespan::FromSeconds(10))
		.ThenClient(TEXT("Remote client selects only itself and readies using real RPCs"), 0, [](FArenaDuelSelectNetworkState& State)
		{
			Local(State.World)->ServerRequestCharacterSelection(EArenaDuelCharacterArchetype::Warden);
			Local(State.World)->ServerRequestCharacterSelection(EArenaDuelCharacterArchetype::Shadow);
			Local(State.World)->ServerSetCharacterReady(true);
		})
		.UntilServer(TEXT("Both ready transition to authoritative countdown"), [](FArenaDuelSelectNetworkState& State)
		{
			return State.World->GetGameState<AArenaDuelGameState>()->GetMatchPhase() == EArenaDuelMatchPhase::Countdown;
		}, FTimespan::FromSeconds(5))
		.ThenServer(TEXT("Countdown locks selection, readiness and abilities"), [this](FArenaDuelSelectNetworkState& State)
		{
			Controller(State.World, 0)->ServerRequestCharacterSelection(EArenaDuelCharacterArchetype::Shadow);
			Controller(State.World, 1)->ServerSetCharacterReady(false);
			if (Player(State.World, 0)->GetCharacterArchetype() != EArenaDuelCharacterArchetype::Warden || !Player(State.World, 1)->IsCharacterReady() || Player(State.World, 0)->TryActivatePrimaryAbility()) TestRunner->AddError(TEXT("Countdown accepted locked action"));
		})
		.UntilClient(TEXT("Countdown is replicated with synchronized end timestamp"), 0, [](FArenaDuelSelectNetworkState& State)
		{
			const AArenaDuelGameState* GS = State.World->GetGameState<AArenaDuelGameState>();
			return GS->GetMatchPhase() == EArenaDuelMatchPhase::Countdown && GS->GetCountdownEndServerTime() > GS->GetServerWorldTimeSeconds() && Local(State.World)->IsCharacterSelectOpen();
		}, FTimespan::FromSeconds(5))
		.UntilServer(TEXT("Countdown completes into clean selected combat pawns"), [](FArenaDuelSelectNetworkState& State)
		{
			return State.World->GetGameState<AArenaDuelGameState>()->IsRoundInProgress();
		}, FTimespan::FromSeconds(10))
		.ThenServer(TEXT("Correct kit, health, ready reset and duplicate-free grant"), [this](FArenaDuelSelectNetworkState& State)
		{
			AArenaDuelPlayerState* P1 = Player(State.World, 0);
			UAbilitySystemComponent* ASC = P1->GetAbilitySystemComponent();
			if (P1->IsCharacterReady() || !ASC->FindAbilitySpecFromClass(UArenaDuelGA_ArcBarrier::StaticClass()) || !ASC->FindAbilitySpecFromClass(UArenaDuelGA_BurstLeap::StaticClass()) || ASC->GetActivatableAbilities().Num() != 2) TestRunner->AddError(TEXT("Selected kit / ready reset is incorrect"));
			for (uint8 Slot = 0; Slot < 2; ++Slot)
			{
				const AArenaDuelCharacter* Pawn = Cast<AArenaDuelCharacter>(Controller(State.World, Slot)->GetPawn());
				if (!Pawn || Pawn->IsDead() || Pawn->GetHealth() != 100 || Pawn->GetCharacterMovement()->MovementMode == MOVE_None) TestRunner->AddError(TEXT("Round did not restore health/movement"));
			}
		})
		.UntilClient(TEXT("Menu closes, cursor hides and local combat presentation resumes"), 0, [](FArenaDuelSelectNetworkState& State)
		{
			const AArenaDuelCharacter* Pawn = Cast<AArenaDuelCharacter>(Local(State.World)->GetPawn());
			return State.World->GetGameState<AArenaDuelGameState>()->IsRoundInProgress() && !Local(State.World)->IsCharacterSelectOpen() && !Local(State.World)->bShowMouseCursor && Pawn && Pawn->GetCharacterMovement()->MovementMode != MOVE_None;
		}, FTimespan::FromSeconds(5))
		.ThenServer(TEXT("Normal death enters round break without selection"), [this](FArenaDuelSelectNetworkState& State)
		{
			Cast<AArenaDuelCharacter>(Controller(State.World, 1)->GetPawn())->ApplyServerDamage(1000);
			if (Player(State.World, 0)->GetRoundWins() != 1 || State.World->GetGameState<AArenaDuelGameState>()->GetMatchPhase() != EArenaDuelMatchPhase::RoundBreak) TestRunner->AddError(TEXT("Normal round result failed"));
		})
		.UntilServer(TEXT("Normal three-second round restart preserves archetypes"), [](FArenaDuelSelectNetworkState& State)
		{
			const AArenaDuelGameState* GS = State.World->GetGameState<AArenaDuelGameState>();
			return GS->IsRoundInProgress() && GS->GetRoundNumber() == 2 && Player(State.World, 0)->GetCharacterArchetype() == EArenaDuelCharacterArchetype::Warden;
		}, FTimespan::FromSeconds(10))
		.ThenServer(TEXT("Fifth win keeps existing match result"), [this](FArenaDuelSelectNetworkState& State)
		{
			Player(State.World, 0)->SetRoundWinsForDevelopment(4);
			State.World->GetAuthGameMode<AArenaDuelGameMode>()->AdminAwardRound(0);
			const AArenaDuelGameState* GS = State.World->GetGameState<AArenaDuelGameState>();
			if (!GS->IsMatchComplete() || GS->GetMatchPhase() != EArenaDuelMatchPhase::MatchResult || Player(State.World, 0)->GetRoundWins() != 5) TestRunner->AddError(TEXT("First-to-five result changed"));
		})
		.UntilClient(TEXT("Match result reaches remote client before selection returns"), 0, [](FArenaDuelSelectNetworkState& State)
		{
			return State.World->GetGameState<AArenaDuelGameState>()->GetMatchPhase() == EArenaDuelMatchPhase::MatchResult && !Local(State.World)->IsCharacterSelectOpen();
		}, FTimespan::FromSeconds(5))
		.UntilServer(TEXT("After five seconds return to selection, 0:0, round one, no ready, same archetypes"), [](FArenaDuelSelectNetworkState& State)
		{
			const AArenaDuelGameState* GS = State.World->GetGameState<AArenaDuelGameState>();
			return GS->GetMatchPhase() == EArenaDuelMatchPhase::CharacterSelect && !GS->IsRoundInProgress() && !GS->IsMatchComplete() && GS->GetRoundNumber() == 1
				&& Player(State.World, 0)->GetRoundWins() == 0 && Player(State.World, 1)->GetRoundWins() == 0
				&& !Player(State.World, 0)->IsCharacterReady() && !Player(State.World, 1)->IsCharacterReady()
				&& Player(State.World, 0)->GetCharacterArchetype() == EArenaDuelCharacterArchetype::Warden && Player(State.World, 1)->GetCharacterArchetype() == EArenaDuelCharacterArchetype::Shadow;
		}, FTimespan::FromSeconds(10))
		.UntilClient(TEXT("Persistent fullscreen widget reopens for next selection"), 0, [](FArenaDuelSelectNetworkState& State)
		{
			return State.World->GetGameState<AArenaDuelGameState>()->GetMatchPhase() == EArenaDuelMatchPhase::CharacterSelect && Local(State.World)->IsCharacterSelectOpen();
		}, FTimespan::FromSeconds(5))
		.ThenClient(TEXT("Remote player changes character for the next match"), 0, [](FArenaDuelSelectNetworkState& State)
		{
			Local(State.World)->RequestCharacterSelection(EArenaDuelCharacterArchetype::Warden);
		})
		.UntilServer(TEXT("Second match selection updates only its owner"), [](FArenaDuelSelectNetworkState& State)
		{
			return Player(State.World, 1)->GetCharacterArchetype() == EArenaDuelCharacterArchetype::Warden && Player(State.World, 0)->GetCharacterArchetype() == EArenaDuelCharacterArchetype::Warden;
		}, FTimespan::FromSeconds(5))
		.ThenServer(TEXT("A disconnect during countdown cancels it without respawning the departed controller"), [this](FArenaDuelSelectNetworkState& State)
		{
			Controller(State.World, 0)->ServerSetCharacterReady(true);
			AArenaDuelPlayerController* Departing = Controller(State.World, 1);
			Departing->ServerSetCharacterReady(true);
			if (State.World->GetGameState<AArenaDuelGameState>()->GetMatchPhase() != EArenaDuelMatchPhase::Countdown) TestRunner->AddError(TEXT("Rematch could not enter countdown"));
			// Destroying the server controller invokes Unreal's actual Logout lifecycle.
			// Exactly this network-close diagnostic is expected; all unrelated errors still fail.
			TestRunner->AddExpectedError(TEXT("UEngine::BroadcastNetworkFailure: FailureType = ConnectionLost"), EAutomationExpectedErrorFlags::Contains, 1);
			Departing->Destroy();
		})
		.UntilServer(TEXT("Only the remaining duel side exists, not ready, locked in selection"), [](FArenaDuelSelectNetworkState& State)
		{
			const AArenaDuelGameState* GS = State.World->GetGameState<AArenaDuelGameState>();
			const AArenaDuelPlayerState* Remaining = Player(State.World, 0);
			return GS->GetMatchPhase() == EArenaDuelMatchPhase::CharacterSelect && !GS->IsRoundInProgress() && Remaining && !Remaining->IsCharacterReady() && !Player(State.World, 1);
		}, FTimespan::FromSeconds(5));
	}
};
#endif
