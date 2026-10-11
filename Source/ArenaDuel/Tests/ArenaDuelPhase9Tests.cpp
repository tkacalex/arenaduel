// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

// Phase 9A (docs/Phase9.md): the knife and cover, the knife's own attack time, a trigger held inside the switch
// time, the reload request, a third player in the duel, and upgrade requests that name their selection.

#include "CQTest.h"
#include "ArenaDuelTestUpgrades.h"
#include "Components/PIENetworkComponent.h"
#include "Misc/AutomationTest.h"

#include "ArenaDuel/Characters/ArenaDuelCharacter.h"
#include "ArenaDuel/Game/ArenaDuelGameMode.h"
#include "ArenaDuel/Game/ArenaDuelGameState.h"
#include "ArenaDuel/Game/ArenaDuelUpgrades.h"
#include "ArenaDuel/Game/ArenaDuelZombieGameMode.h"
#include "ArenaDuel/Game/ArenaDuelZombieGameState.h"
#include "ArenaDuel/Player/ArenaDuelLocalSettings.h"
#include "ArenaDuel/Player/ArenaDuelPlayerController.h"
#include "ArenaDuel/UI/ArenaDuelPlayerMenuWidget.h"
#include "ArenaDuel/Player/ArenaDuelPlayerState.h"
#include "ArenaDuel/Weapons/ArenaDuelWeaponComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"

// E09: Reset Defaults shows and keeps the default of every control, the resolution included.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FArenaDuelPhase9ResetDefaultsTest, "ArenaDuel.Phase9.Menu.ResetDefaults", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FArenaDuelPhase9ResetDefaultsTest::RunTest(const FString& Parameters)
{
	UArenaDuelPlayerMenuWidget* Widget = NewObject<UArenaDuelPlayerMenuWidget>();
	Widget->Initialize();
	if (!TestTrue(TEXT("The settings menu builds"), Widget->HasExpectedTree())) return false;
	FArenaDuelLocalSettings Changed;
	Changed.MouseSensitivity = 2.5f; Changed.ADSMultiplier = 0.5f; Changed.FOV = 104.0f; Changed.MasterVolume = 0.4f;
	Changed.WindowMode = 2; Changed.ResolutionX = 1600; Changed.ResolutionY = 900; Changed.bVSync = true; Changed.FPSLimit = 60;
	Widget->SetPendingForTests(Changed);
	TestEqual(TEXT("Changed settings are shown"), Widget->DescribeShownForTests(), FString(TEXT("Windowed|1600 x 900|On|60")));
	TestTrue(TEXT("Showing the settings does not change them"), Widget->GetPendingSettings().ResolutionX == 1600 && Widget->GetPendingSettings().WindowMode == 2 && Widget->GetPendingSettings().FPSLimit == 60 && FMath::IsNearlyEqual(Widget->GetPendingSettings().MouseSensitivity, 2.5f));
	Widget->ResetDefaultsForTests();
	const FArenaDuelLocalSettings Defaults;
	const FArenaDuelLocalSettings& Now = Widget->GetPendingSettings();
	TestEqual(TEXT("Reset shows the defaults, the resolution included"), Widget->DescribeShownForTests(), FString(TEXT("Borderless|1920 x 1080|Off|144")));
	TestTrue(TEXT("What Apply would take is the defaults"), Now.WindowMode == Defaults.WindowMode && Now.ResolutionX == Defaults.ResolutionX && Now.ResolutionY == Defaults.ResolutionY && Now.bVSync == Defaults.bVSync && Now.FPSLimit == Defaults.FPSLimit
		&& FMath::IsNearlyEqual(Now.MouseSensitivity, Defaults.MouseSensitivity) && FMath::IsNearlyEqual(Now.ADSMultiplier, Defaults.ADSMultiplier) && FMath::IsNearlyEqual(Now.FOV, Defaults.FOV) && FMath::IsNearlyEqual(Now.MasterVolume, Defaults.MasterVolume));
	return true;
}

namespace ArenaDuelPhase9Tests
{
	static constexpr TCHAR GameModeClassPath[] = TEXT("/Game/ArenaDuel/Game/BP_ArenaDuelGameMode.BP_ArenaDuelGameMode_C");

	static APlayerController* ControllerForSlot(UWorld* World, uint8 Slot)
	{
		if (!World) return nullptr;
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			APlayerController* Controller = It->Get();
			const AArenaDuelPlayerState* Player = Controller ? Controller->GetPlayerState<AArenaDuelPlayerState>() : nullptr;
			if (Player && Player->GetDuelSlot() == Slot && !Player->IsDuelSpectator()) return Controller;
		}
		return nullptr;
	}
	static AArenaDuelCharacter* PawnForSlot(UWorld* World, uint8 Slot)
	{
		APlayerController* Controller = ControllerForSlot(World, Slot);
		return Controller ? Cast<AArenaDuelCharacter>(Controller->GetPawn()) : nullptr;
	}
	static APlayerController* Watcher(UWorld* World)
	{
		if (!World) return nullptr;
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			APlayerController* Controller = It->Get();
			const AArenaDuelPlayerState* Player = Controller ? Controller->GetPlayerState<AArenaDuelPlayerState>() : nullptr;
			if (Player && Player->IsDuelSpectator()) return Controller;
		}
		return nullptr;
	}
	static int32 Controllers(UWorld* World)
	{
		int32 Count = 0;
		if (World) for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It) ++Count;
		return Count;
	}
	static bool BothAliveInRound(UWorld* World)
	{
		const AArenaDuelGameState* GameState = World ? World->GetGameState<AArenaDuelGameState>() : nullptr;
		const AArenaDuelCharacter* A = PawnForSlot(World, 0);
		const AArenaDuelCharacter* B = PawnForSlot(World, 1);
		return GameState && GameState->IsRoundInProgress() && A && B && !A->IsDead() && !B->IsDead();
	}
	static void StartMatch(UWorld* World, float SelectSeconds = 20.0f)
	{
		AArenaDuelGameMode* GameMode = World->GetAuthGameMode<AArenaDuelGameMode>();
		GameMode->SetRoundTimingForTests(60.0f, 0.4f, 0.6f);
		GameMode->SetUpgradeSelectSecondsForTests(SelectSeconds);
		GameMode->AdminResetMatch();
	}
	/** The five rays of a knife swing from where the attacker looks, as KnifeAttackAuthoritative builds them. */
	static FVector SwingEnd(const AArenaDuelCharacter* Attacker, int32 Ray)
	{
		const FArenaDuelKnifeDefinition& Knife = Attacker->GetWeaponComponent()->GetKnifeDefinition();
		const FRotationMatrix Aim(Attacker->GetControlRotation());
		const FVector2D Fan[] = { FVector2D(0.0f, 0.0f), FVector2D(1.0f, 0.0f), FVector2D(-1.0f, 0.0f), FVector2D(0.0f, 1.0f), FVector2D(0.0f, -1.0f) };
		return Attacker->GetPawnViewLocation() + Aim.GetUnitAxis(EAxis::X) * Knife.Range + (Aim.GetUnitAxis(EAxis::Y) * Fan[Ray].X + Aim.GetUnitAxis(EAxis::Z) * Fan[Ray].Y) * Knife.SwingHalfWidth;
	}
	/** Bit N is set when ray N of the swing would meet the victim's body with nothing of the world considered. */
	static int32 RaysOnBody(const AArenaDuelCharacter* Attacker, const AArenaDuelCharacter* Victim)
	{
		int32 Mask = 0;
		for (int32 Ray = 0; Ray < 5; ++Ray)
		{
			FHitResult Hit;
			if (Victim->TraceHitZones(Attacker->GetPawnViewLocation(), SwingEnd(Attacker, Ray), Hit)) Mask |= 1 << Ray;
		}
		return Mask;
	}
}

struct FArenaDuelPhase9State : public FBasePIENetworkComponentState
{
	double Mark = 0.0;
	float Lateral = 70.0f;
	float Health = 0.0f;
	int32 Ammo = 0;
	int32 Sequence = 0;
	int32 Serial = 0;
	FVector Base = FVector::ZeroVector;
	TWeakObjectPtr<AActor> Wall;
};

NETWORK_TEST_CLASS(FArenaDuelPhase9CombatTest, "ArenaDuel.Phase9.Combat")
{
	FPIENetworkComponent<FArenaDuelPhase9State> Network{TestRunner, TestCommandBuilder, bInitializing};

	BEFORE_EACH()
	{
		SetMatchUpgradesForTests(false);
		UClass* GameModeClass = LoadClass<AGameModeBase>(nullptr, ArenaDuelPhase9Tests::GameModeClassPath);
		FNetworkComponentBuilder<FArenaDuelPhase9State>()
			.WithClients(1)
			.AsListenServer()
			.WithGameMode(GameModeClass)
			.Build(Network);
	}

	AFTER_EACH()
	{
		SetMatchUpgradesForTests(true);
	}

	// E02: a side ray of the swing must not reach a body that stands behind cover the middle ray passes.
	TEST_METHOD(KnifeSideRaysStopAtCover)
	{
		using namespace ArenaDuelPhase9Tests;
		Network
			.UntilServer(TEXT("Both duel players have pawns"), [](FArenaDuelPhase9State& State) { return PawnForSlot(State.World, 0) && PawnForSlot(State.World, 1); }, FTimespan::FromSeconds(10.0))
			.ThenServer(TEXT("Start a round and take the knife"), [](FArenaDuelPhase9State& State) { StartMatch(State.World); })
			.UntilServer(TEXT("Round one runs"), [](FArenaDuelPhase9State& State) { return BothAliveInRound(State.World); }, FTimespan::FromSeconds(6.0))
			.ThenServer(TEXT("Attacker looks level along its own facing"), [this](FArenaDuelPhase9State& State)
			{
				AArenaDuelCharacter* Attacker = PawnForSlot(State.World, 0);
				AArenaDuelCharacter* Target = PawnForSlot(State.World, 1);
				// The test world has no floor. Both are held in place in the air, away from anything.
				Attacker->GetCharacterMovement()->DisableMovement();
				Target->GetCharacterMovement()->DisableMovement();
				Attacker->SetActorLocation(FVector(0.0f, 0.0f, 600.0f), false, nullptr, ETeleportType::TeleportPhysics);
				Attacker->GetController()->SetControlRotation(FRotator(0.0f, Attacker->GetActorRotation().Yaw, 0.0f));
				Attacker->GetWeaponComponent()->SelectSlot(EArenaDuelLoadoutSlot::Knife);
				if (Attacker->GetWeaponComponent()->GetActiveSlot() != EArenaDuelLoadoutSlot::Knife) TestRunner->AddError(TEXT("The attacker does not hold the knife"));
				State.Base = Attacker->GetActorLocation();
				State.Mark = 0.0;
			})
			// The victim is moved sideways in small steps until only the right-hand ray of the swing meets the body:
			// the place where the old code hit through cover. Bodies follow the pawn a frame later, so every
			// step is given a moment before it is judged.
			.UntilServer(TEXT("Victim stands where only the right ray reaches"), [this](FArenaDuelPhase9State& State)
			{
				AArenaDuelCharacter* Attacker = PawnForSlot(State.World, 0);
				AArenaDuelCharacter* Victim = PawnForSlot(State.World, 1);
				const double Now = State.World->GetTimeSeconds();
				if (State.Mark > 0.0 && Now - State.Mark < 0.2) return false;
				if (State.Mark > 0.0 && RaysOnBody(Attacker, Victim) == (1 << 1)) return true;
				if (State.Mark > 0.0) State.Lateral -= 2.0f;
				if (State.Lateral < 0.0f) { TestRunner->AddError(TEXT("No place found where only the right ray reaches the body")); return true; }
				const FRotationMatrix Aim(Attacker->GetControlRotation());
				const float Ahead = Attacker->GetWeaponComponent()->GetKnifeDefinition().Range * 0.8f;
				Victim->SetActorLocation(State.Base + Aim.GetUnitAxis(EAxis::X) * Ahead + Aim.GetUnitAxis(EAxis::Y) * State.Lateral, false, nullptr, ETeleportType::TeleportPhysics);
				State.Mark = Now;
				return false;
			}, FTimespan::FromSeconds(20.0))
			.ThenServer(TEXT("In the open that swing lands"), [this](FArenaDuelPhase9State& State)
			{
				AArenaDuelCharacter* Attacker = PawnForSlot(State.World, 0);
				AArenaDuelCharacter* Victim = PawnForSlot(State.World, 1);
				FHitResult World;
				if (State.World->LineTraceSingleByChannel(World, Attacker->GetPawnViewLocation(), SwingEnd(Attacker, 1), ECC_Visibility, FCollisionQueryParams(NAME_None, true, Attacker))) { TestRunner->AddError(TEXT("The test place is not open: something of the map is in the swing")); return; }
				const float Before = Victim->GetHealth();
				Attacker->GetWeaponComponent()->ClearActionTimesForTests();
				Attacker->GetWeaponComponent()->StartFire();
				if (!(Victim->GetHealth() < Before)) TestRunner->AddError(TEXT("A free knife hit at the edge of the swing no longer lands"));
				State.Health = Victim->GetHealth();
				// Cover: a thin wall half way, from just right of the middle ray outwards. The middle ray passes it.
				const FArenaDuelKnifeDefinition& Knife = Attacker->GetWeaponComponent()->GetKnifeDefinition();
				const FRotationMatrix Aim(Attacker->GetControlRotation());
				const float EdgeLateral = Knife.SwingHalfWidth * 0.25f;
				const FVector Centre = Attacker->GetPawnViewLocation() + Aim.GetUnitAxis(EAxis::X) * Knife.Range * 0.5f + Aim.GetUnitAxis(EAxis::Y) * (EdgeLateral + 100.0f);
				AStaticMeshActor* Wall = State.World->SpawnActor<AStaticMeshActor>(Centre, Attacker->GetControlRotation());
				if (!Wall) { TestRunner->AddError(TEXT("Could not spawn the cover")); return; }
				Wall->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
				Wall->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
				Wall->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));
				Wall->SetActorScale3D(FVector(0.04f, 2.0f, 4.0f));
				State.Wall = Wall;
				State.Mark = State.World->GetTimeSeconds();
			})
			.UntilServer(TEXT("The cover has settled"), [](FArenaDuelPhase9State& State) { return State.World->GetTimeSeconds() - State.Mark > 0.2; }, FTimespan::FromSeconds(3.0))
			.ThenServer(TEXT("Behind cover the same swing does nothing"), [this](FArenaDuelPhase9State& State)
			{
				AArenaDuelCharacter* Attacker = PawnForSlot(State.World, 0);
				AArenaDuelCharacter* Victim = PawnForSlot(State.World, 1);
				const FCollisionQueryParams Params(NAME_None, true, Attacker);
				FHitResult World;
				if (State.World->LineTraceSingleByChannel(World, Attacker->GetPawnViewLocation(), SwingEnd(Attacker, 0), ECC_Visibility, Params)) TestRunner->AddError(TEXT("The cover blocks the middle ray; the case under test needs it free"));
				if (!State.World->LineTraceSingleByChannel(World, Attacker->GetPawnViewLocation(), SwingEnd(Attacker, 1), ECC_Visibility, Params)) TestRunner->AddError(TEXT("The cover does not block the right ray"));
				if (RaysOnBody(Attacker, Victim) != (1 << 1)) TestRunner->AddError(TEXT("The victim moved between the two swings"));
				const int32 Sequence = Attacker->GetWeaponComponent()->GetLastShotSequence();
				Attacker->GetWeaponComponent()->ClearActionTimesForTests();
				Attacker->GetWeaponComponent()->StartFire();
				if (Attacker->GetWeaponComponent()->GetLastShotSequence() == Sequence) TestRunner->AddError(TEXT("The second swing was not made"));
				if (!FMath::IsNearlyEqual(Victim->GetHealth(), State.Health)) TestRunner->AddError(TEXT("A side ray of the knife hit through cover"));
				if (State.Wall.IsValid()) State.Wall->Destroy();
			});
	}

	// E03, E05: the knife keeps its own attack time across switches; only the firearm in the hands reloads.
	TEST_METHOD(KnifeAttackTimeSurvivesSwitchesAndReloadNeedsTheFirearm)
	{
		using namespace ArenaDuelPhase9Tests;
		Network
			.UntilServer(TEXT("Both duel players have pawns"), [](FArenaDuelPhase9State& State) { return PawnForSlot(State.World, 0) && PawnForSlot(State.World, 1); }, FTimespan::FromSeconds(10.0))
			.ThenServer(TEXT("Start a round"), [](FArenaDuelPhase9State& State) { StartMatch(State.World); })
			.UntilServer(TEXT("Round one runs"), [](FArenaDuelPhase9State& State) { return BothAliveInRound(State.World); }, FTimespan::FromSeconds(6.0))
			.ThenServer(TEXT("A reload request behind the knife is refused, with the firearm it is taken"), [this](FArenaDuelPhase9State& State)
			{
				UArenaDuelWeaponComponent* Weapon = PawnForSlot(State.World, 0)->GetWeaponComponent();
				// One round out of the magazine, so there is something to reload.
				Weapon->ClearActionTimesForTests();
				Weapon->StartFire();
				Weapon->StopFire();
				const int32 Magazine = Weapon->GetCurrentMagazineAmmo(), Reserve = Weapon->GetReserveAmmo();
				if (Magazine >= Weapon->GetCurrentDefinition().MagazineCapacity) { TestRunner->AddError(TEXT("No shot left the magazine to prepare the reload check")); return; }
				Weapon->SelectSlot(EArenaDuelLoadoutSlot::Knife);
				Weapon->RequestReloadForTests();
				if (Weapon->IsReloading() || Weapon->GetCurrentMagazineAmmo() != Magazine || Weapon->GetReserveAmmo() != Reserve) TestRunner->AddError(TEXT("A reload request reloaded the holstered firearm behind the knife"));
				if (Weapon->GetFlashbangsRemaining() > 0)
				{
					Weapon->SelectSlot(EArenaDuelLoadoutSlot::Flashbang);
					Weapon->RequestReloadForTests();
					if (Weapon->IsReloading()) TestRunner->AddError(TEXT("A reload request reloaded the holstered firearm behind the flashbang"));
				}
				// A heavy stab, then away from the knife and back at once.
				Weapon->SelectSlot(EArenaDuelLoadoutSlot::Knife);
				Weapon->ClearActionTimesForTests();
				State.Sequence = Weapon->GetLastShotSequence();
				Weapon->StartAim();
				if (Weapon->GetLastShotSequence() != State.Sequence + 1) { TestRunner->AddError(TEXT("The heavy stab was not made")); return; }
				State.Mark = State.World->GetTimeSeconds();
				Weapon->SelectSlot(EArenaDuelLoadoutSlot::Primary);
				Weapon->SelectSlot(EArenaDuelLoadoutSlot::Knife);
				if (Weapon->GetActiveSlot() != EArenaDuelLoadoutSlot::Knife) TestRunner->AddError(TEXT("The knife is not back in the hand"));
			})
			.UntilServer(TEXT("The switch time has passed, the stab's own time has not"), [this](FArenaDuelPhase9State& State)
			{
				UArenaDuelWeaponComponent* Weapon = PawnForSlot(State.World, 0)->GetWeaponComponent();
				const double Since = State.World->GetTimeSeconds() - State.Mark;
				if (Since < Weapon->GetSwitchSeconds() + 0.1) return false;
				if (Since > Weapon->GetKnifeDefinition().HeavyAttackInterval - 0.15) { TestRunner->AddError(TEXT("The test was too slow to try an attack inside the stab's time")); return true; }
				Weapon->StartFire();
				Weapon->StartAim();
				if (Weapon->GetLastShotSequence() != State.Sequence + 1) TestRunner->AddError(TEXT("Switching away and back shortened the knife's attack time"));
				return true;
			}, FTimespan::FromSeconds(3.0))
			.UntilServer(TEXT("After the stab's own time the knife strikes again"), [this](FArenaDuelPhase9State& State)
			{
				UArenaDuelWeaponComponent* Weapon = PawnForSlot(State.World, 0)->GetWeaponComponent();
				if (State.World->GetTimeSeconds() - State.Mark < Weapon->GetKnifeDefinition().HeavyAttackInterval + 0.05) return false;
				Weapon->StartFire();
				if (Weapon->GetLastShotSequence() != State.Sequence + 2) TestRunner->AddError(TEXT("The knife did not strike after its attack time"));
				// And the firearm in the hands does reload.
				Weapon->SelectSlot(EArenaDuelLoadoutSlot::Primary);
				Weapon->RequestReloadForTests();
				if (!Weapon->IsReloading()) TestRunner->AddError(TEXT("The firearm in the hands did not start to reload"));
				return true;
			}, FTimespan::FromSeconds(4.0));
	}

	// E04: a trigger pressed inside the switch time fires when the time is over, on the host and for a remote
	// player, and a trigger let go inside it does not fire at all.
	TEST_METHOD(TriggerHeldInsideTheSwitchTimeFiresWhenItEnds)
	{
		using namespace ArenaDuelPhase9Tests;
		Network
			.UntilServer(TEXT("Both duel players have pawns"), [](FArenaDuelPhase9State& State) { return PawnForSlot(State.World, 0) && PawnForSlot(State.World, 1); }, FTimespan::FromSeconds(10.0))
			.ThenServer(TEXT("Start a round"), [](FArenaDuelPhase9State& State) { StartMatch(State.World); })
			.UntilServer(TEXT("Round one runs"), [](FArenaDuelPhase9State& State) { return BothAliveInRound(State.World); }, FTimespan::FromSeconds(6.0))
			.ThenServer(TEXT("Host: switch to the firearm and press at once"), [this](FArenaDuelPhase9State& State)
			{
				UArenaDuelWeaponComponent* Weapon = PawnForSlot(State.World, 0)->GetWeaponComponent();
				Weapon->SelectSlot(EArenaDuelLoadoutSlot::Knife);
				Weapon->ClearActionTimesForTests();
				Weapon->SelectSlot(EArenaDuelLoadoutSlot::Primary);
				State.Ammo = Weapon->GetCurrentMagazineAmmo();
				State.Mark = State.World->GetTimeSeconds();
				Weapon->StartFire();
				if (Weapon->GetCurrentMagazineAmmo() != State.Ammo || Weapon->IsServerFiring()) TestRunner->AddError(TEXT("A shot left inside the switch time"));
				if (!Weapon->IsServerFirePending() || !Weapon->IsLocalFireHeld()) TestRunner->AddError(TEXT("The held trigger was forgotten inside the switch time"));
			})
			.UntilServer(TEXT("Host: the held trigger fires once the switch time is over"), [this](FArenaDuelPhase9State& State)
			{
				UArenaDuelWeaponComponent* Weapon = PawnForSlot(State.World, 0)->GetWeaponComponent();
				if (Weapon->GetCurrentMagazineAmmo() >= State.Ammo) return false;
				if (State.World->GetTimeSeconds() - State.Mark < Weapon->GetSwitchSeconds() - 0.02) TestRunner->AddError(TEXT("The shot left before the switch time was over"));
				if (!Weapon->IsServerFiring() && Weapon->GetCurrentDefinition().bAutomatic) TestRunner->AddError(TEXT("Automatic fire did not keep running"));
				Weapon->StopFire();
				if (Weapon->IsServerFiring() || Weapon->IsServerFirePending() || Weapon->IsLocalFireHeld()) TestRunner->AddError(TEXT("Letting go did not stop the fire"));
				return true;
			}, FTimespan::FromSeconds(3.0))
			.ThenServer(TEXT("Host: press and let go inside the switch time"), [](FArenaDuelPhase9State& State)
			{
				UArenaDuelWeaponComponent* Weapon = PawnForSlot(State.World, 0)->GetWeaponComponent();
				Weapon->SelectSlot(EArenaDuelLoadoutSlot::Knife);
				Weapon->ClearActionTimesForTests();
				Weapon->SelectSlot(EArenaDuelLoadoutSlot::Primary);
				State.Ammo = Weapon->GetCurrentMagazineAmmo();
				State.Mark = State.World->GetTimeSeconds();
				Weapon->StartFire();
				Weapon->StopFire();
			})
			.UntilServer(TEXT("Host: a trigger let go in time fires nothing"), [this](FArenaDuelPhase9State& State)
			{
				UArenaDuelWeaponComponent* Weapon = PawnForSlot(State.World, 0)->GetWeaponComponent();
				if (State.World->GetTimeSeconds() - State.Mark < Weapon->GetSwitchSeconds() + 0.3) return false;
				if (Weapon->GetCurrentMagazineAmmo() != State.Ammo || Weapon->IsServerFiring() || Weapon->IsServerFirePending()) TestRunner->AddError(TEXT("A trigger let go inside the switch time fired afterwards"));
				// For the remote player: note the magazine before the client acts.
				State.Ammo = PawnForSlot(State.World, 1)->GetWeaponComponent()->GetCurrentMagazineAmmo();
				return true;
			}, FTimespan::FromSeconds(3.0))
			.ThenClient(TEXT("Client: knife, back to the firearm, press at once and hold"), 0, [this](FArenaDuelPhase9State& State)
			{
				const APlayerController* Controller = State.World->GetFirstPlayerController();
				AArenaDuelCharacter* Pawn = Controller ? Cast<AArenaDuelCharacter>(Controller->GetPawn()) : nullptr;
				if (!Pawn) { TestRunner->AddError(TEXT("The client has no pawn")); return; }
				Pawn->GetWeaponComponent()->SelectSlot(EArenaDuelLoadoutSlot::Knife);
				Pawn->GetWeaponComponent()->SelectSlot(EArenaDuelLoadoutSlot::Primary);
				Pawn->GetWeaponComponent()->StartFire();
				if (!Pawn->GetWeaponComponent()->IsLocalFireHeld()) TestRunner->AddError(TEXT("The client does not hold the trigger"));
			})
			.UntilServer(TEXT("Server: the remote player's held trigger fires real shots"), [](FArenaDuelPhase9State& State)
			{
				const UArenaDuelWeaponComponent* Weapon = PawnForSlot(State.World, 1)->GetWeaponComponent();
				return Weapon->GetActiveSlot() == EArenaDuelLoadoutSlot::Primary && Weapon->GetCurrentMagazineAmmo() < State.Ammo;
			}, FTimespan::FromSeconds(5.0))
			.ThenClient(TEXT("Client: let go"), 0, [](FArenaDuelPhase9State& State)
			{
				const APlayerController* Controller = State.World->GetFirstPlayerController();
				if (AArenaDuelCharacter* Pawn = Controller ? Cast<AArenaDuelCharacter>(Controller->GetPawn()) : nullptr) Pawn->GetWeaponComponent()->StopFire();
			})
			.UntilServer(TEXT("Server: the remote player's fire stops"), [](FArenaDuelPhase9State& State)
			{
				const UArenaDuelWeaponComponent* Weapon = PawnForSlot(State.World, 1)->GetWeaponComponent();
				return !Weapon->IsServerFiring() && !Weapon->IsServerFirePending();
			}, FTimespan::FromSeconds(5.0));
	}

	// E06: with the pause menu open nothing of the weapon starts. E10: a refill has something to add only when ammunition is missing.
	TEST_METHOD(MenuLocksWeaponStartsAndRefillKnowsWhenItIsNeeded)
	{
		using namespace ArenaDuelPhase9Tests;
		Network
			.UntilServer(TEXT("Both duel players have pawns"), [](FArenaDuelPhase9State& State) { return PawnForSlot(State.World, 0) && PawnForSlot(State.World, 1); }, FTimespan::FromSeconds(10.0))
			.ThenServer(TEXT("Start a round"), [](FArenaDuelPhase9State& State) { StartMatch(State.World); })
			.UntilServer(TEXT("Round one runs"), [](FArenaDuelPhase9State& State) { return BothAliveInRound(State.World); }, FTimespan::FromSeconds(6.0))
			.ThenServer(TEXT("Menu open: no shot, no aim, no reload, no switch; closed: all of it again"), [this](FArenaDuelPhase9State& State)
			{
				AArenaDuelPlayerController* Controller = Cast<AArenaDuelPlayerController>(ControllerForSlot(State.World, 0));
				UArenaDuelWeaponComponent* Weapon = PawnForSlot(State.World, 0)->GetWeaponComponent();
				Weapon->ClearActionTimesForTests();
				if (!Weapon->IsAllAmmoFull()) TestRunner->AddError(TEXT("A fresh pawn does not count as fully loaded"));
				Weapon->StartFire();
				Weapon->StopFire();
				const int32 Ammo = Weapon->GetCurrentMagazineAmmo();
				if (Ammo >= Weapon->GetCurrentDefinition().MagazineCapacity) { TestRunner->AddError(TEXT("No shot left before the menu check")); return; }
				if (Weapon->IsAllAmmoFull()) TestRunner->AddError(TEXT("A magazine with a round missing counts as full"));
				Controller->OpenPlayerMenu();
				if (!Controller->IsPlayerMenuOpen()) { TestRunner->AddError(TEXT("The pause menu did not open")); return; }
				Weapon->StartFire();
				if (Weapon->GetCurrentMagazineAmmo() != Ammo || Weapon->IsLocalFireHeld() || Weapon->IsServerFiring() || Weapon->IsServerFirePending()) TestRunner->AddError(TEXT("A shot started behind the pause menu"));
				Weapon->StartAim();
				if (Weapon->IsAiming()) TestRunner->AddError(TEXT("Aiming started behind the pause menu"));
				Weapon->Reload();
				if (Weapon->IsReloading()) TestRunner->AddError(TEXT("A reload started behind the pause menu"));
				Weapon->SelectSlot(EArenaDuelLoadoutSlot::Knife);
				if (Weapon->GetActiveSlot() != EArenaDuelLoadoutSlot::Primary) TestRunner->AddError(TEXT("A weapon switch went through behind the pause menu"));
				Controller->ClosePlayerMenu();
				if (Controller->IsPlayerMenuOpen()) TestRunner->AddError(TEXT("The pause menu did not close"));
				Weapon->Reload();
				if (!Weapon->IsReloading()) TestRunner->AddError(TEXT("After closing the menu a reload does not start"));
				// What a refill does is what the shop sells: afterwards there is nothing left to add.
				Weapon->RefillAllAmmoForDevelopment();
				if (!Weapon->IsAllAmmoFull()) TestRunner->AddError(TEXT("After a refill the pawn does not count as fully loaded"));
			});
	}

	// E18: two lethal hits in one frame. The first death decides, in either order, and the round is decided once.
	TEST_METHOD(TwoLethalHitsInOneFrameGiveTheRoundToWhoeverDiedSecond)
	{
		using namespace ArenaDuelPhase9Tests;
		Network
			.UntilServer(TEXT("Both duel players have pawns"), [](FArenaDuelPhase9State& State) { return PawnForSlot(State.World, 0) && PawnForSlot(State.World, 1); }, FTimespan::FromSeconds(10.0))
			.ThenServer(TEXT("Start a round"), [](FArenaDuelPhase9State& State) { StartMatch(State.World); })
			.UntilServer(TEXT("Round one runs"), [](FArenaDuelPhase9State& State) { return BothAliveInRound(State.World); }, FTimespan::FromSeconds(6.0))
			.ThenServer(TEXT("Slot 1 first, then slot 0"), [this](FArenaDuelPhase9State& State)
			{
				AArenaDuelGameMode* GameMode = State.World->GetAuthGameMode<AArenaDuelGameMode>();
				const int32 Evaluated = GameMode->GetRoundsEvaluated();
				AArenaDuelCharacter* First = PawnForSlot(State.World, 0);
				AArenaDuelCharacter* Second = PawnForSlot(State.World, 1);
				Second->ApplyServerDamage(1000.0f);
				First->ApplyServerDamage(1000.0f);
				if (!Second->IsDead() || First->IsDead()) TestRunner->AddError(TEXT("The hit after the deciding death still counted"));
				if (GameMode->FindPlayerStateByDuelSlot(0)->GetRoundWins() != 1 || GameMode->FindPlayerStateByDuelSlot(1)->GetRoundWins() != 0 || GameMode->GetRoundsEvaluated() != Evaluated + 1) TestRunner->AddError(TEXT("Slot 0 did not get exactly one round"));
			})
			.UntilServer(TEXT("Round two runs"), [](FArenaDuelPhase9State& State) { return BothAliveInRound(State.World) && State.World->GetGameState<AArenaDuelGameState>()->GetRoundNumber() == 2; }, FTimespan::FromSeconds(8.0))
			.ThenServer(TEXT("Slot 0 first, then slot 1"), [this](FArenaDuelPhase9State& State)
			{
				AArenaDuelGameMode* GameMode = State.World->GetAuthGameMode<AArenaDuelGameMode>();
				const int32 Evaluated = GameMode->GetRoundsEvaluated();
				AArenaDuelCharacter* First = PawnForSlot(State.World, 0);
				AArenaDuelCharacter* Second = PawnForSlot(State.World, 1);
				First->ApplyServerDamage(1000.0f);
				Second->ApplyServerDamage(1000.0f);
				if (!First->IsDead() || Second->IsDead()) TestRunner->AddError(TEXT("The hit after the deciding death still counted (other order)"));
				if (GameMode->FindPlayerStateByDuelSlot(0)->GetRoundWins() != 1 || GameMode->FindPlayerStateByDuelSlot(1)->GetRoundWins() != 1 || GameMode->GetRoundsEvaluated() != Evaluated + 1) TestRunner->AddError(TEXT("Slot 1 did not get exactly one round"));
			});
	}

	// E07: a choice names its selection; one from an earlier selection is refused in a later one.
	TEST_METHOD(UpgradeChoiceFromAnEarlierSelectionIsRefused)
	{
		using namespace ArenaDuelPhase9Tests;
		static constexpr uint8 Reserve = static_cast<uint8>(EArenaDuelUpgrade::Reserve), Reload = static_cast<uint8>(EArenaDuelUpgrade::Reload);
		Network
			.UntilServer(TEXT("Both duel players have pawns"), [](FArenaDuelPhase9State& State) { return PawnForSlot(State.World, 0) && PawnForSlot(State.World, 1); }, FTimespan::FromSeconds(10.0))
			.ThenServer(TEXT("Start a match with upgrades and a long selection"), [](FArenaDuelPhase9State& State) { SetMatchUpgradesForTests(true); StartMatch(State.World, 30.0f); })
			.UntilServer(TEXT("Round one runs"), [](FArenaDuelPhase9State& State) { return BothAliveInRound(State.World); }, FTimespan::FromSeconds(6.0))
			.ThenServer(TEXT("First selection: both choose, naming it"), [this](FArenaDuelPhase9State& State)
			{
				AArenaDuelGameMode* GameMode = State.World->GetAuthGameMode<AArenaDuelGameMode>();
				const AArenaDuelGameState* GameState = State.World->GetGameState<AArenaDuelGameState>();
				const int32 Before = GameState->GetUpgradeSelectSerial();
				PawnForSlot(State.World, 1)->ApplyServerDamage(1000.0f);
				if (!GameState->IsUpgradeSelect()) { TestRunner->AddError(TEXT("The decided round did not open the selection")); return; }
				State.Serial = GameState->GetUpgradeSelectSerial();
				if (State.Serial != Before + 1) TestRunner->AddError(TEXT("A new selection did not get a new serial"));
				AArenaDuelPlayerState* Host = GameMode->FindPlayerStateByDuelSlot(0);
				// Wrong serials change nothing: the one before, the one after, none.
				GameMode->RequestUpgradeChoice(ControllerForSlot(State.World, 0), Reserve, State.Serial - 1);
				GameMode->RequestUpgradeChoice(ControllerForSlot(State.World, 0), Reserve, State.Serial + 1);
				GameMode->RequestUpgradeChoice(ControllerForSlot(State.World, 0), Reserve, 0);
				if (Host->HasChosenUpgrade() || Host->GetUpgradeLevel(EArenaDuelUpgrade::Reserve) != 0) TestRunner->AddError(TEXT("A choice with the wrong serial was taken"));
				GameMode->RequestUpgradeChoice(ControllerForSlot(State.World, 0), Reserve, State.Serial);
				GameMode->RequestUpgradeChoice(ControllerForSlot(State.World, 1), Reserve, State.Serial);
				if (Host->GetUpgradeLevel(EArenaDuelUpgrade::Reserve) != 1) TestRunner->AddError(TEXT("The choice naming the open selection was not taken"));
			})
			.UntilServer(TEXT("Round two runs"), [](FArenaDuelPhase9State& State)
			{
				const AArenaDuelGameState* GameState = State.World->GetGameState<AArenaDuelGameState>();
				return BothAliveInRound(State.World) && GameState->GetRoundNumber() == 2;
			}, FTimespan::FromSeconds(8.0))
			.ThenServer(TEXT("Second selection: a late choice from the first is refused, a current one taken"), [this](FArenaDuelPhase9State& State)
			{
				AArenaDuelGameMode* GameMode = State.World->GetAuthGameMode<AArenaDuelGameMode>();
				const AArenaDuelGameState* GameState = State.World->GetGameState<AArenaDuelGameState>();
				PawnForSlot(State.World, 1)->ApplyServerDamage(1000.0f);
				if (!GameState->IsUpgradeSelect() || GameState->GetUpgradeSelectSerial() != State.Serial + 1) { TestRunner->AddError(TEXT("The second selection did not open with the next serial")); return; }
				AArenaDuelPlayerState* Host = GameMode->FindPlayerStateByDuelSlot(0);
				GameMode->RequestUpgradeChoice(ControllerForSlot(State.World, 0), Reload, State.Serial);
				if (Host->HasChosenUpgrade() || Host->GetUpgradeLevel(EArenaDuelUpgrade::Reload) != 0) TestRunner->AddError(TEXT("A choice from the earlier selection used up the new one"));
				GameMode->RequestUpgradeChoice(ControllerForSlot(State.World, 0), Reload, State.Serial + 1);
				if (!Host->HasChosenUpgrade() || Host->GetUpgradeLevel(EArenaDuelUpgrade::Reload) != 1 || Host->GetUpgradeLevel(EArenaDuelUpgrade::Reserve) != 1) TestRunner->AddError(TEXT("The current choice was not taken exactly once"));
			});
	}
};

// E01: a third player only watches.
NETWORK_TEST_CLASS(FArenaDuelPhase9ThirdPlayerTest, "ArenaDuel.Phase9.ThirdPlayer")
{
	FPIENetworkComponent<FArenaDuelPhase9State> Network{TestRunner, TestCommandBuilder, bInitializing};

	BEFORE_EACH()
	{
		SetMatchUpgradesForTests(false);
		UClass* GameModeClass = LoadClass<AGameModeBase>(nullptr, ArenaDuelPhase9Tests::GameModeClassPath);
		FNetworkComponentBuilder<FArenaDuelPhase9State>()
			.WithClients(2)
			.AsListenServer()
			.WithGameMode(GameModeClass)
			.Build(Network);
	}

	AFTER_EACH()
	{
		SetMatchUpgradesForTests(true);
	}

	TEST_METHOD(WatchesWithoutAPawnAndLeavesWithoutEndingTheMatch)
	{
		using namespace ArenaDuelPhase9Tests;
		Network
			.UntilServer(TEXT("Three players are connected, two of them duel"), [](FArenaDuelPhase9State& State) { return Controllers(State.World) == 3 && PawnForSlot(State.World, 0) && PawnForSlot(State.World, 1) && Watcher(State.World); }, FTimespan::FromSeconds(15.0))
			.ThenServer(TEXT("The third has no place and no pawn; a match starts without them"), [this](FArenaDuelPhase9State& State)
			{
				AArenaDuelGameMode* GameMode = State.World->GetAuthGameMode<AArenaDuelGameMode>();
				APlayerController* Third = Watcher(State.World);
				const AArenaDuelPlayerState* ThirdState = Third->GetPlayerState<AArenaDuelPlayerState>();
				if (ThirdState->GetDuelSlot() < 2) TestRunner->AddError(TEXT("The third player holds a duel slot"));
				if (Third->GetPawn()) TestRunner->AddError(TEXT("The third player has a pawn"));
				StartMatch(State.World);
				GameMode->RestartPlayer(Third);
				if (Third->GetPawn()) TestRunner->AddError(TEXT("A restart gave the third player a pawn"));
			})
			.UntilServer(TEXT("Round one runs for the two"), [](FArenaDuelPhase9State& State) { return BothAliveInRound(State.World); }, FTimespan::FromSeconds(6.0))
			.ThenServer(TEXT("Nothing the third player does or suffers decides the round; leaving changes nothing"), [this](FArenaDuelPhase9State& State)
			{
				AArenaDuelGameMode* GameMode = State.World->GetAuthGameMode<AArenaDuelGameMode>();
				const AArenaDuelGameState* GameState = State.World->GetGameState<AArenaDuelGameState>();
				APlayerController* Third = Watcher(State.World);
				if (!Third) { TestRunner->AddError(TEXT("The third player is gone before the check")); return; }
				if (Third->GetPawn()) TestRunner->AddError(TEXT("The third player got a pawn with the round"));
				const int32 Evaluated = GameMode->GetRoundsEvaluated();
				const int32 Round = GameState->GetRoundNumber();
				// A character that belongs to neither duel player dies: no round is decided by it.
				if (AArenaDuelCharacter* Stray = State.World->SpawnActor<AArenaDuelCharacter>(PawnForSlot(State.World, 0)->GetClass(), FVector(0.0f, 0.0f, 5000.0f), FRotator::ZeroRotator))
				{
					GameMode->HandlePlayerDeath(Stray);
					Stray->Destroy();
				}
				if (!GameState->IsRoundInProgress() || GameMode->GetRoundsEvaluated() != Evaluated) TestRunner->AddError(TEXT("The death of someone outside the duel decided the round"));
				GameMode->Logout(Third);
				if (!GameState->IsRoundInProgress() || GameState->GetMatchPhase() != EArenaDuelMatchPhase::InRound || GameState->GetRoundNumber() != Round || GameMode->GetRoundsEvaluated() != Evaluated)
					TestRunner->AddError(TEXT("The third player leaving ended or reset the match"));
				if (!PawnForSlot(State.World, 0) || !PawnForSlot(State.World, 1)) TestRunner->AddError(TEXT("A duel player lost their pawn when the third left"));
				// The duel itself still works: slot 1 dies, slot 0 takes the round.
				PawnForSlot(State.World, 1)->ApplyServerDamage(1000.0f);
				const AArenaDuelPlayerState* First = GameMode->FindPlayerStateByDuelSlot(0);
				const AArenaDuelPlayerState* Second = GameMode->FindPlayerStateByDuelSlot(1);
				if (!First || !Second || First->GetRoundWins() != 1 || Second->GetRoundWins() != 0 || GameMode->GetRoundsEvaluated() != Evaluated + 1) TestRunner->AddError(TEXT("The kill between the two duel players was not counted once for the survivor"));
			});
	}
};

// E17: survival ends when the last living player leaves, so a dead player who stays can start again.
NETWORK_TEST_CLASS(FArenaDuelPhase9SurvivalLeaveTest, "ArenaDuel.Phase9.Survival")
{
	FPIENetworkComponent<FArenaDuelPhase9State> Network{TestRunner, TestCommandBuilder, bInitializing};

	BEFORE_EACH()
	{
		FNetworkComponentBuilder<FArenaDuelPhase9State>()
			.WithClients(1)
			.AsListenServer()
			.WithGameMode(AArenaDuelZombieGameMode::StaticClass())
			.Build(Network);
	}

	TEST_METHOD(LastLivingPlayerLeavingEndsTheRun)
	{
		Network
			.UntilServer(TEXT("Host and client have pawns in the run"), [](FArenaDuelPhase9State& State)
			{
				int32 Living = 0;
				for (FConstPlayerControllerIterator It = State.World->GetPlayerControllerIterator(); It; ++It) if (const AArenaDuelCharacter* Pawn = It->Get() ? Cast<AArenaDuelCharacter>(It->Get()->GetPawn()) : nullptr; Pawn && !Pawn->IsDead()) ++Living;
				return Living == 2;
			}, FTimespan::FromSeconds(15.0))
			.ThenServer(TEXT("The host dies, the client lives on, then leaves"), [this](FArenaDuelPhase9State& State)
			{
				AArenaDuelZombieGameMode* GameMode = State.World->GetAuthGameMode<AArenaDuelZombieGameMode>();
				const AArenaDuelZombieGameState* GameState = State.World->GetGameState<AArenaDuelZombieGameState>();
				if (!GameMode || !GameState) { TestRunner->AddError(TEXT("Not a survival world")); return; }
				APlayerController* Host = State.World->GetFirstPlayerController();
				APlayerController* Remote = nullptr;
				for (FConstPlayerControllerIterator It = State.World->GetPlayerControllerIterator(); It; ++It) if (It->Get() != Host) Remote = It->Get();
				AArenaDuelCharacter* HostPawn = Host ? Cast<AArenaDuelCharacter>(Host->GetPawn()) : nullptr;
				if (!HostPawn || !Remote) { TestRunner->AddError(TEXT("Host pawn or remote player missing")); return; }
				GameMode->EndRunIfNobodyStands(Remote);
				if (GameState->IsGameOver()) TestRunner->AddError(TEXT("A player leaving ended the run although the host still stands"));
				HostPawn->ApplyServerDamage(100000.0f);
				if (!HostPawn->IsDead()) { TestRunner->AddError(TEXT("The host could not be killed for the check")); return; }
				if (GameState->IsGameOver()) TestRunner->AddError(TEXT("The run ended although the remote player is alive"));
				GameMode->Logout(Remote);
				if (!GameState->IsGameOver()) TestRunner->AddError(TEXT("The last living player left and the run did not end"));
			});
	}
};

#endif
