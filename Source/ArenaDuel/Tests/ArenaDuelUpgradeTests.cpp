// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "CQTest.h"
#include "ArenaDuelTestUpgrades.h"
#include "Components/PIENetworkComponent.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"

#include "AbilitySystemComponent.h"
#include "ArenaDuel/Abilities/ArenaDuelShadowCooldownEffects.h"
#include "ArenaDuel/Characters/ArenaDuelCharacter.h"
#include "ArenaDuel/Game/ArenaDuelGameMode.h"
#include "ArenaDuel/Game/ArenaDuelGameState.h"
#include "ArenaDuel/Game/ArenaDuelUpgrades.h"
#include "ArenaDuel/Game/ArenaDuelZombieGameMode.h"
#include "ArenaDuel/Player/ArenaDuelPlayerController.h"
#include "ArenaDuel/Player/ArenaDuelPlayerState.h"
#include "ArenaDuel/UI/ArenaDuelUpgradeWidget.h"
#include "ArenaDuel/Weapons/ArenaDuelWeaponComponent.h"
#include "Components/Button.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameModeBase.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FArenaDuelUpgradeRulesTest, "ArenaDuel.Phase8.UpgradeRules", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FArenaDuelUpgradeRulesTest::RunTest(const FString& Parameters)
{
	const AArenaDuelPlayerState* Player = GetDefault<AArenaDuelPlayerState>();
	TestEqual(TEXT("Three kinds of upgrade"), ArenaDuelUpgrades::KindCount, 3);
	TestEqual(TEXT("Three levels each"), Player->GetMaxUpgradeLevel(), 3);
	TestTrue(TEXT("A player without upgrades has the base values"), Player->GetReserveAmmoScale() == 1.0f && Player->GetReloadTimeScale() == 1.0f && Player->GetAbilityCooldownScale() == 1.0f);
	// Additive against the base value, never compounding.
	const float Reserve[4] = { 1.0f, 1.10f, 1.20f, 1.30f }, Faster[4] = { 1.0f, 0.95f, 0.90f, 0.85f };
	for (int32 Level = 0; Level <= 3; ++Level)
	{
		TestTrue(FString::Printf(TEXT("Reserve at level %d"), Level), FMath::IsNearlyEqual(Player->GetUpgradeScaleAtLevel(EArenaDuelUpgrade::Reserve, Level), Reserve[Level], 0.0001f));
		TestTrue(FString::Printf(TEXT("Reload at level %d"), Level), FMath::IsNearlyEqual(Player->GetUpgradeScaleAtLevel(EArenaDuelUpgrade::Reload, Level), Faster[Level], 0.0001f));
		TestTrue(FString::Printf(TEXT("Cooldowns at level %d"), Level), FMath::IsNearlyEqual(Player->GetUpgradeScaleAtLevel(EArenaDuelUpgrade::Ability, Level), Faster[Level], 0.0001f));
	}
	TestTrue(TEXT("Nothing beyond the highest level"), FMath::IsNearlyEqual(Player->GetUpgradeScaleAtLevel(EArenaDuelUpgrade::Reserve, 9), 1.30f, 0.0001f));
	TestEqual(TEXT("With nothing taken the automatic upgrade is the first in the fixed order"), Player->PickAutomaticUpgrade(), 0);
	TestTrue(TEXT("Ids outside the three kinds are refused"), !ArenaDuelUpgrades::IsValidId(3) && !ArenaDuelUpgrades::IsValidId(255) && ArenaDuelUpgrades::IsValidId(2));
	TestTrue(TEXT("The duel has upgrades and a ten second selection"), GetDefault<AArenaDuelGameMode>()->UsesMatchUpgrades() && FMath::IsNearlyEqual(GetDefault<AArenaDuelGameMode>()->GetUpgradeSelectSeconds(), 10.0f));
	TestFalse(TEXT("Survival, built on the duel classes, has no upgrades"), GetDefault<AArenaDuelZombieGameMode>()->UsesMatchUpgrades());
	TestEqual(TEXT("Card text, reserve"), UArenaDuelUpgradeWidget::DescribeLevel(EArenaDuelUpgrade::Reserve, 0.10f, 2), FString(TEXT("+20 % reserve")));
	TestEqual(TEXT("Card text, reload"), UArenaDuelUpgradeWidget::DescribeLevel(EArenaDuelUpgrade::Reload, 0.05f, 3), FString(TEXT("-15 % reload time")));
	TestEqual(TEXT("Card text, nothing yet"), UArenaDuelUpgradeWidget::DescribeLevel(EArenaDuelUpgrade::Ability, 0.05f, 0), FString(TEXT("no bonus")));
	return true;
}

namespace ArenaDuelUpgradeTests
{
	static constexpr TCHAR GameModeClassPath[] = TEXT("/Game/ArenaDuel/Game/BP_ArenaDuelGameMode.BP_ArenaDuelGameMode_C");
	static constexpr uint8 Reserve = static_cast<uint8>(EArenaDuelUpgrade::Reserve), Reload = static_cast<uint8>(EArenaDuelUpgrade::Reload), Ability = static_cast<uint8>(EArenaDuelUpgrade::Ability);

	static APlayerController* ControllerForSlot(UWorld* World, uint8 Slot)
	{
		if (!World) return nullptr;
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			APlayerController* Controller = It->Get();
			const AArenaDuelPlayerState* Player = Controller ? Controller->GetPlayerState<AArenaDuelPlayerState>() : nullptr;
			if (Player && Player->GetDuelSlot() == Slot) return Controller;
		}
		return nullptr;
	}
	static AArenaDuelCharacter* PawnForSlot(UWorld* World, uint8 Slot)
	{
		APlayerController* Controller = ControllerForSlot(World, Slot);
		return Controller ? Cast<AArenaDuelCharacter>(Controller->GetPawn()) : nullptr;
	}
	static AArenaDuelPlayerState* PlayerForSlot(UWorld* World, uint8 Slot)
	{
		APlayerController* Controller = ControllerForSlot(World, Slot);
		return Controller ? Controller->GetPlayerState<AArenaDuelPlayerState>() : nullptr;
	}
	static bool BothAliveInRound(UWorld* World, int32 RoundNumber)
	{
		const AArenaDuelGameState* GameState = World ? World->GetGameState<AArenaDuelGameState>() : nullptr;
		const AArenaDuelCharacter* A = PawnForSlot(World, 0);
		const AArenaDuelCharacter* B = PawnForSlot(World, 1);
		return GameState && GameState->IsRoundInProgress() && GameState->GetRoundNumber() == RoundNumber && A && B && !A->IsDead() && !B->IsDead();
	}
	static int32 Levels(const AArenaDuelPlayerState* Player)
	{
		// Reserve, reload, ability as one number, e.g. 120.
		return Player ? Player->GetUpgradeLevel(EArenaDuelUpgrade::Reserve) * 100 + Player->GetUpgradeLevel(EArenaDuelUpgrade::Reload) * 10 + Player->GetUpgradeLevel(EArenaDuelUpgrade::Ability) : -1;
	}
	/** The serial of the selection that is open, which every choice has to name. */
	static int32 OpenSelection(UWorld* World)
	{
		const AArenaDuelGameState* GameState = World ? World->GetGameState<AArenaDuelGameState>() : nullptr;
		return GameState ? GameState->GetUpgradeSelectSerial() : -1;
	}
	static void StartRound(UWorld* World, float SelectSeconds)
	{
		AArenaDuelGameMode* GameMode = World->GetAuthGameMode<AArenaDuelGameMode>();
		GameMode->SetRoundTimingForTests(30.0f, 0.4f, 0.6f);
		GameMode->SetUpgradeSelectSecondsForTests(SelectSeconds);
		GameMode->AdminResetMatch();
	}
}

struct FArenaDuelUpgradeState : public FBasePIENetworkComponentState
{
	int32 BaseReserve = 0;
	float BaseReload = 0.0f;
	int32 Evaluated = 0;
};

NETWORK_TEST_CLASS(FArenaDuelUpgradeNetworkTest, "ArenaDuel.Phase8.Upgrades")
{
	FPIENetworkComponent<FArenaDuelUpgradeState> Network{TestRunner, TestCommandBuilder, bInitializing};

	BEFORE_EACH()
	{
		SetMatchUpgradesForTests(true);
		UClass* GameModeClass = LoadClass<AGameModeBase>(nullptr, ArenaDuelUpgradeTests::GameModeClassPath);
		FNetworkComponentBuilder<FArenaDuelUpgradeState>()
			.WithClients(1)
			.AsListenServer()
			.WithGameMode(GameModeClass)
			.Build(Network);
	}

	TEST_METHOD(BothChooseDifferentUpgradesAndTheNextRoundHasThem)
	{
		using namespace ArenaDuelUpgradeTests;
		Network
			.UntilServer(TEXT("Both duel players have pawns"), [](FArenaDuelUpgradeState& State) { return PawnForSlot(State.World, 0) && PawnForSlot(State.World, 1); }, FTimespan::FromSeconds(10.0))
			.ThenServer(TEXT("Start a match with a long selection"), [](FArenaDuelUpgradeState& State) { StartRound(State.World, 20.0f); })
			.UntilServer(TEXT("Round one runs with base values"), [this](FArenaDuelUpgradeState& State)
			{
				if (!BothAliveInRound(State.World, 1)) return false;
				const UArenaDuelWeaponComponent* Weapon = PawnForSlot(State.World, 0)->GetWeaponComponent();
				State.BaseReserve = Weapon->GetCurrentDefinition().ReserveCapacity;
				State.BaseReload = Weapon->GetCurrentDefinition().ReloadDuration;
				if (Weapon->GetReserveAmmo() != State.BaseReserve || !FMath::IsNearlyEqual(Weapon->GetReloadDuration(), State.BaseReload)) TestRunner->AddError(TEXT("A fresh match does not start with base ammunition and reload time"));
				// Nothing can be taken while a round runs.
				AArenaDuelGameMode* GameMode = State.World->GetAuthGameMode<AArenaDuelGameMode>();
				GameMode->RequestUpgradeChoice(ControllerForSlot(State.World, 0), Reserve, OpenSelection(State.World));
				if (Levels(PlayerForSlot(State.World, 0)) != 0) TestRunner->AddError(TEXT("An upgrade was granted outside the selection"));
				return true;
			}, FTimespan::FromSeconds(6.0))
			.ThenServer(TEXT("A decided round opens the selection for both and locks everyone"), [this](FArenaDuelUpgradeState& State)
			{
				AArenaDuelGameMode* GameMode = State.World->GetAuthGameMode<AArenaDuelGameMode>();
				const AArenaDuelGameState* GameState = State.World->GetGameState<AArenaDuelGameState>();
				PawnForSlot(State.World, 1)->ApplyServerDamage(1000.0f);
				if (!GameState->IsUpgradeSelect() || !GameMode->IsUpgradeSelectionPending()) { TestRunner->AddError(TEXT("The decided round did not open the upgrade selection")); return; }
				if (GameState->IsRoundInProgress() || GameState->HasRoundTimer()) TestRunner->AddError(TEXT("The selection still counts as a running round"));
				if (GameState->GetUpgradeSelectSecondsRemaining() <= 0.0f || GameState->GetUpgradeSelectSecondsRemaining() > 20.01f) TestRunner->AddError(TEXT("The selection has no sensible end time"));
				// The winner too: nobody moves or fights during the selection.
				const AArenaDuelCharacter* Winner = PawnForSlot(State.World, 0);
				if (GameState->CanLivingCharacterMove(Winner) || Winner->GetCharacterMovement()->MovementMode != MOVE_None) TestRunner->AddError(TEXT("The round winner can move during the selection"));
				if (PlayerForSlot(State.World, 0)->HasChosenUpgrade() || PlayerForSlot(State.World, 1)->HasChosenUpgrade()) TestRunner->AddError(TEXT("A player counts as done before choosing"));
				// Requests that must change nothing.
				GameMode->RequestUpgradeChoice(ControllerForSlot(State.World, 0), 7, OpenSelection(State.World));
				GameMode->RequestUpgradeChoice(nullptr, Reserve, OpenSelection(State.World));
				if (Levels(PlayerForSlot(State.World, 0)) != 0 || Levels(PlayerForSlot(State.World, 1)) != 0) TestRunner->AddError(TEXT("An invalid request granted an upgrade"));
				// The host takes Supply; a second and a third request are ignored.
				GameMode->RequestUpgradeChoice(ControllerForSlot(State.World, 0), Reserve, OpenSelection(State.World));
				GameMode->RequestUpgradeChoice(ControllerForSlot(State.World, 0), Reserve, OpenSelection(State.World));
				GameMode->RequestUpgradeChoice(ControllerForSlot(State.World, 0), Reload, OpenSelection(State.World));
				if (Levels(PlayerForSlot(State.World, 0)) != 100 || !PlayerForSlot(State.World, 0)->HasChosenUpgrade()) TestRunner->AddError(TEXT("The host did not get exactly one level of Supply"));
				if (!GameState->IsUpgradeSelect()) TestRunner->AddError(TEXT("The selection closed before the second player chose"));
			})
			.UntilClient(TEXT("The client is shown the selection with the cursor"), 0, [](FArenaDuelUpgradeState& State)
			{
				const AArenaDuelGameState* GameState = State.World->GetGameState<AArenaDuelGameState>();
				const AArenaDuelPlayerController* Controller = Cast<AArenaDuelPlayerController>(State.World->GetFirstPlayerController());
				return GameState && GameState->IsUpgradeSelect() && Controller && Controller->IsUpgradeSelectOpen() && Controller->bShowMouseCursor && Controller->GetUpgradeWidget() && Controller->GetUpgradeWidget()->HasExpectedTree();
			}, FTimespan::FromSeconds(6.0))
			.ThenClient(TEXT("The client takes Reload through the real request, twice"), 0, [](FArenaDuelUpgradeState& State)
			{
				AArenaDuelPlayerController* Controller = Cast<AArenaDuelPlayerController>(State.World->GetFirstPlayerController());
				Controller->ChooseUpgrade(Reload);
				Controller->ChooseUpgrade(Ability);
			})
			.UntilServer(TEXT("Both have chosen: round two starts at once with a full clock and the score kept"), [this](FArenaDuelUpgradeState& State)
			{
				if (!BothAliveInRound(State.World, 2)) return false;
				const AArenaDuelGameState* GameState = State.World->GetGameState<AArenaDuelGameState>();
				const AArenaDuelGameMode* GameMode = State.World->GetAuthGameMode<AArenaDuelGameMode>();
				if (Levels(PlayerForSlot(State.World, 0)) != 100 || Levels(PlayerForSlot(State.World, 1)) != 10) TestRunner->AddError(FString::Printf(TEXT("Levels after the selection are %d and %d, expected 100 and 010"), Levels(PlayerForSlot(State.World, 0)), Levels(PlayerForSlot(State.World, 1))));
				if (GameMode->IsUpgradeSelectionPending() || GameState->IsUpgradeSelect()) TestRunner->AddError(TEXT("The selection is still open in the new round"));
				if (!GameState->HasRoundTimer() || GameState->GetRoundSecondsRemaining() < 29.0f) TestRunner->AddError(TEXT("The new round did not get the full clock"));
				if (PlayerForSlot(State.World, 0)->GetRoundWins() != 1 || PlayerForSlot(State.World, 1)->GetRoundWins() != 0) TestRunner->AddError(TEXT("The score changed across the selection"));
				// Effects on the freshly spawned pawns, per player.
				const UArenaDuelWeaponComponent* HostWeapon = PawnForSlot(State.World, 0)->GetWeaponComponent();
				const UArenaDuelWeaponComponent* ClientWeapon = PawnForSlot(State.World, 1)->GetWeaponComponent();
				if (HostWeapon->GetReserveAmmo() != FMath::RoundToInt(HostWeapon->GetCurrentDefinition().ReserveCapacity * 1.10f)) TestRunner->AddError(TEXT("Supply 1 did not give 10 % more reserve"));
				if (HostWeapon->GetCurrentMagazineAmmo() != HostWeapon->GetCurrentDefinition().MagazineCapacity) TestRunner->AddError(TEXT("Supply changed the magazine"));
				if (!FMath::IsNearlyEqual(HostWeapon->GetReloadDuration(), HostWeapon->GetCurrentDefinition().ReloadDuration)) TestRunner->AddError(TEXT("Supply changed the reload time"));
				if (!FMath::IsNearlyEqual(ClientWeapon->GetReloadDuration(), ClientWeapon->GetCurrentDefinition().ReloadDuration * 0.95f, 0.001f)) TestRunner->AddError(TEXT("Reload 1 did not shorten the reload by 5 %"));
				if (ClientWeapon->GetReserveAmmo() != ClientWeapon->GetCurrentDefinition().ReserveCapacity) TestRunner->AddError(TEXT("Reload changed the reserve"));
				if (GetDefault<UArenaDuelWeaponComponent>()->GetReserveCapacity(0) != GetDefault<UArenaDuelWeaponComponent>()->GetCurrentDefinition().ReserveCapacity) TestRunner->AddError(TEXT("The shared weapon definitions were changed"));
				// A request after the selection has closed is late.
				AArenaDuelGameMode* Rules = State.World->GetAuthGameMode<AArenaDuelGameMode>();
				Rules->RequestUpgradeChoice(ControllerForSlot(State.World, 0), Ability, OpenSelection(State.World));
				if (Levels(PlayerForSlot(State.World, 0)) != 100) TestRunner->AddError(TEXT("A late request granted an upgrade"));
				return true;
			}, FTimespan::FromSeconds(8.0))
			.UntilClient(TEXT("The client has its game input back and sees its shorter reload"), 0, [](FArenaDuelUpgradeState& State)
			{
				const AArenaDuelPlayerController* Controller = Cast<AArenaDuelPlayerController>(State.World->GetFirstPlayerController());
				const AArenaDuelCharacter* Pawn = Controller ? Cast<AArenaDuelCharacter>(Controller->GetPawn()) : nullptr;
				const AArenaDuelGameState* GameState = State.World->GetGameState<AArenaDuelGameState>();
				if (!Controller || !Pawn || !GameState || !GameState->IsRoundInProgress() || Controller->IsUpgradeSelectOpen() || Controller->bShowMouseCursor) return false;
				const UArenaDuelWeaponComponent* Weapon = Pawn->GetWeaponComponent();
				return Weapon && FMath::IsNearlyEqual(Weapon->GetReloadDuration(), Weapon->GetCurrentDefinition().ReloadDuration * 0.95f, 0.001f);
			}, FTimespan::FromSeconds(6.0))
			.ThenServer(TEXT("A match reset returns both to the base values"), [this](FArenaDuelUpgradeState& State)
			{
				State.World->GetAuthGameMode<AArenaDuelGameMode>()->AdminResetMatch();
				if (Levels(PlayerForSlot(State.World, 0)) != 0 || Levels(PlayerForSlot(State.World, 1)) != 0) TestRunner->AddError(TEXT("A match reset kept upgrade levels"));
			})
			.UntilServer(TEXT("The new match starts with base ammunition and reload time"), [this](FArenaDuelUpgradeState& State)
			{
				if (!BothAliveInRound(State.World, 1)) return false;
				for (uint8 Slot = 0; Slot < 2; ++Slot)
				{
					const UArenaDuelWeaponComponent* Weapon = PawnForSlot(State.World, Slot)->GetWeaponComponent();
					if (Weapon->GetReserveAmmo() != Weapon->GetCurrentDefinition().ReserveCapacity || !FMath::IsNearlyEqual(Weapon->GetReloadDuration(), Weapon->GetCurrentDefinition().ReloadDuration))
						TestRunner->AddError(TEXT("A pawn of the new match still carries an upgrade"));
				}
				return true;
			}, FTimespan::FromSeconds(6.0));
	}

	TEST_METHOD(NoChoiceInTimeGivesTheLowestAvailableAndMaxedOnesAreClosed)
	{
		using namespace ArenaDuelUpgradeTests;
		Network
			.UntilServer(TEXT("Both duel players have pawns"), [](FArenaDuelUpgradeState& State) { return PawnForSlot(State.World, 0) && PawnForSlot(State.World, 1); }, FTimespan::FromSeconds(10.0))
			.ThenServer(TEXT("Start a match with a short selection"), [](FArenaDuelUpgradeState& State) { StartRound(State.World, 1.2f); })
			.UntilServer(TEXT("Round one runs"), [](FArenaDuelUpgradeState& State) { return BothAliveInRound(State.World, 1); }, FTimespan::FromSeconds(6.0))
			.ThenServer(TEXT("Prepare levels, cooldown effect, and decide the round"), [this](FArenaDuelUpgradeState& State)
			{
				AArenaDuelGameMode* GameMode = State.World->GetAuthGameMode<AArenaDuelGameMode>();
				AArenaDuelPlayerState* Host = PlayerForSlot(State.World, 0);
				AArenaDuelPlayerState* Client = PlayerForSlot(State.World, 1);
				// Host: Supply full, Reload 1, Abilities 2. The level cap holds.
				for (int32 Step = 0; Step < 5; ++Step) Host->AddUpgradeLevelAuthoritatively(EArenaDuelUpgrade::Reserve);
				Host->AddUpgradeLevelAuthoritatively(EArenaDuelUpgrade::Reload);
				Host->AddUpgradeLevelAuthoritatively(EArenaDuelUpgrade::Ability);
				Host->AddUpgradeLevelAuthoritatively(EArenaDuelUpgrade::Ability);
				if (Levels(Host) != 312) TestRunner->AddError(TEXT("Levels did not stop at the maximum of three"));
				// A regular cooldown applied now is shorter by the upgraded share: 5 s Shadow Step at Abilities 2 is 4.5 s.
				UAbilitySystemComponent* ASC = Host->GetAbilitySystemComponent();
				ASC->ApplyGameplayEffectToSelf(GetDefault<UArenaDuelGE_ShadowStepCooldown>(), 1.0f, ASC->MakeEffectContext());
				const float Remaining = Host->GetShadowStepCooldownRemaining();
				if (!FMath::IsNearlyEqual(Remaining, 4.5f, 0.1f)) TestRunner->AddError(FString::Printf(TEXT("Cooldown with Abilities 2 is %.2f s, expected 4.5 s"), Remaining));
				// The other player's cooldown is untouched.
				UAbilitySystemComponent* OtherASC = Client->GetAbilitySystemComponent();
				OtherASC->ApplyGameplayEffectToSelf(GetDefault<UArenaDuelGE_ShadowStepCooldown>(), 1.0f, OtherASC->MakeEffectContext());
				if (!FMath::IsNearlyEqual(Client->GetShadowStepCooldownRemaining(), 5.0f, 0.1f)) TestRunner->AddError(TEXT("One player's upgrade shortened the other player's cooldown"));
				PawnForSlot(State.World, 1)->ApplyServerDamage(1000.0f);
				const AArenaDuelGameState* GameState = State.World->GetGameState<AArenaDuelGameState>();
				if (!GameState->IsUpgradeSelect()) { TestRunner->AddError(TEXT("No selection after the decided round")); return; }
				// A maxed kind is refused and does not use up the choice.
				GameMode->RequestUpgradeChoice(ControllerForSlot(State.World, 0), Reserve, OpenSelection(State.World));
				if (Levels(Host) != 312 || Host->HasChosenUpgrade()) TestRunner->AddError(TEXT("A maxed upgrade was accepted"));
			})
			.UntilServer(TEXT("Time runs out: each gets the lowest available, ties in the fixed order"), [this](FArenaDuelUpgradeState& State)
			{
				if (!BothAliveInRound(State.World, 2)) return false;
				// Host 3/1/2: Reload is the lowest that is left. Client 0/0/0: a tie, so the first, Supply.
				if (Levels(PlayerForSlot(State.World, 0)) != 322) TestRunner->AddError(FString::Printf(TEXT("Host levels are %d, expected 322"), Levels(PlayerForSlot(State.World, 0))));
				if (Levels(PlayerForSlot(State.World, 1)) != 100) TestRunner->AddError(FString::Printf(TEXT("Client levels are %d, expected 100"), Levels(PlayerForSlot(State.World, 1))));
				const UArenaDuelWeaponComponent* HostWeapon = PawnForSlot(State.World, 0)->GetWeaponComponent();
				if (HostWeapon->GetReserveAmmo() != FMath::RoundToInt(HostWeapon->GetCurrentDefinition().ReserveCapacity * 1.30f)) TestRunner->AddError(TEXT("Supply 3 is not 30 % more reserve"));
				if (!FMath::IsNearlyEqual(HostWeapon->GetReloadDuration(), HostWeapon->GetCurrentDefinition().ReloadDuration * 0.90f, 0.001f)) TestRunner->AddError(TEXT("Reload 2 is not 10 % shorter"));
				// A new round clears cooldowns as before; the upgrade does not bring them back.
				if (PlayerForSlot(State.World, 0)->GetShadowStepCooldownRemaining() > 0.0f) TestRunner->AddError(TEXT("A cooldown survived the round change"));
				return true;
			}, FTimespan::FromSeconds(8.0));
	}

	TEST_METHOD(NoSelectionWithoutAWinnerAtMatchEndOrAfterLeaving)
	{
		using namespace ArenaDuelUpgradeTests;
		Network
			.UntilServer(TEXT("Both duel players have pawns"), [](FArenaDuelUpgradeState& State) { return PawnForSlot(State.World, 0) && PawnForSlot(State.World, 1); }, FTimespan::FromSeconds(10.0))
			.ThenServer(TEXT("Start a match"), [](FArenaDuelUpgradeState& State) { StartRound(State.World, 20.0f); })
			.UntilServer(TEXT("Round one runs"), [](FArenaDuelUpgradeState& State) { return BothAliveInRound(State.World, 1); }, FTimespan::FromSeconds(6.0))
			.ThenServer(TEXT("A round without a winner has the plain break and no selection"), [this](FArenaDuelUpgradeState& State)
			{
				AArenaDuelGameMode* GameMode = State.World->GetAuthGameMode<AArenaDuelGameMode>();
				const AArenaDuelGameState* GameState = State.World->GetGameState<AArenaDuelGameState>();
				GameMode->EndRoundWithoutWinnerForTests();
				if (GameState->IsUpgradeSelect() || GameMode->IsUpgradeSelectionPending() || GameState->GetMatchPhase() != EArenaDuelMatchPhase::RoundBreak) TestRunner->AddError(TEXT("A round without a winner opened the selection"));
				GameMode->RequestUpgradeChoice(ControllerForSlot(State.World, 0), Reserve, OpenSelection(State.World));
				if (Levels(PlayerForSlot(State.World, 0)) != 0) TestRunner->AddError(TEXT("An upgrade was granted in the plain break"));
			})
			.UntilServer(TEXT("Round two runs"), [](FArenaDuelUpgradeState& State) { return BothAliveInRound(State.World, 2); }, FTimespan::FromSeconds(6.0))
			.ThenServer(TEXT("A player leaving during the selection ends the match and the upgrades"), [this](FArenaDuelUpgradeState& State)
			{
				AArenaDuelGameMode* GameMode = State.World->GetAuthGameMode<AArenaDuelGameMode>();
				const AArenaDuelGameState* GameState = State.World->GetGameState<AArenaDuelGameState>();
				AArenaDuelPlayerState* Host = PlayerForSlot(State.World, 0);
				PawnForSlot(State.World, 1)->ApplyServerDamage(1000.0f);
				GameMode->RequestUpgradeChoice(ControllerForSlot(State.World, 0), Ability, OpenSelection(State.World));
				if (!GameState->IsUpgradeSelect() || Levels(Host) != 1) { TestRunner->AddError(TEXT("Could not reach an open selection with one choice made")); return; }
				AController* Leaving = ControllerForSlot(State.World, 1);
				GameMode->Logout(Leaving);
				if (GameState->GetMatchPhase() != EArenaDuelMatchPhase::CharacterSelect || GameMode->IsUpgradeSelectionPending()) TestRunner->AddError(TEXT("Leaving did not close the selection and return to character selection"));
				if (Levels(Host) != 0 || Host->HasChosenUpgrade() || Host->GetRoundWins() != 0) TestRunner->AddError(TEXT("The abandoned match left upgrades or points behind"));
				GameMode->RequestUpgradeChoice(ControllerForSlot(State.World, 0), Reload, OpenSelection(State.World));
				if (Levels(Host) != 0) TestRunner->AddError(TEXT("An upgrade was granted in character selection"));
			});
	}

	TEST_METHOD(TheMatchDecidingRoundHasNoSelection)
	{
		using namespace ArenaDuelUpgradeTests;
		Network
			.UntilServer(TEXT("Both duel players have pawns"), [](FArenaDuelUpgradeState& State) { return PawnForSlot(State.World, 0) && PawnForSlot(State.World, 1); }, FTimespan::FromSeconds(10.0))
			.ThenServer(TEXT("Start a match"), [](FArenaDuelUpgradeState& State) { StartRound(State.World, 20.0f); })
			.UntilServer(TEXT("Round one runs"), [](FArenaDuelUpgradeState& State) { return BothAliveInRound(State.World, 1); }, FTimespan::FromSeconds(6.0))
			.ThenServer(TEXT("The fifth win goes straight to the match result"), [this](FArenaDuelUpgradeState& State)
			{
				AArenaDuelGameMode* GameMode = State.World->GetAuthGameMode<AArenaDuelGameMode>();
				const AArenaDuelGameState* GameState = State.World->GetGameState<AArenaDuelGameState>();
				AArenaDuelPlayerState* Host = PlayerForSlot(State.World, 0);
				Host->SetRoundWinsForDevelopment(4);
				Host->AddUpgradeLevelAuthoritatively(EArenaDuelUpgrade::Reload);
				PawnForSlot(State.World, 1)->ApplyServerDamage(1000.0f);
				if (!GameState->IsMatchComplete() || GameState->GetMatchPhase() != EArenaDuelMatchPhase::MatchResult) TestRunner->AddError(TEXT("The fifth win did not end the match"));
				if (GameState->IsUpgradeSelect() || GameMode->IsUpgradeSelectionPending()) TestRunner->AddError(TEXT("The match deciding round opened the selection"));
				GameMode->RequestUpgradeChoice(ControllerForSlot(State.World, 0), Reserve, OpenSelection(State.World));
				if (Levels(Host) != 10) TestRunner->AddError(TEXT("An upgrade was granted at the match result"));
			})
			.UntilServer(TEXT("Back in character selection the upgrades are gone"), [this](FArenaDuelUpgradeState& State)
			{
				const AArenaDuelGameState* GameState = State.World->GetGameState<AArenaDuelGameState>();
				if (GameState->GetMatchPhase() != EArenaDuelMatchPhase::CharacterSelect) return false;
				if (Levels(PlayerForSlot(State.World, 0)) != 0 || Levels(PlayerForSlot(State.World, 1)) != 0) TestRunner->AddError(TEXT("Upgrades survived the return to character selection"));
				return true;
			}, FTimespan::FromSeconds(6.0));
	}
};

#endif
