// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

// Phase 9A (docs/Phase9.md): the knife and cover, the knife's own attack time, a trigger held inside the switch
// time, the reload request, a third player in the duel, and upgrade requests that name their selection.

#include "CQTest.h"
#include <limits>
#include "ArenaDuelTestUpgrades.h"
#include "Components/PIENetworkComponent.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformProcess.h"

#include "ArenaDuel/Characters/ArenaDuelCharacter.h"
#include "ArenaDuel/Game/ArenaDuelGameMode.h"
#include "ArenaDuel/Game/ArenaDuelGameState.h"
#include "ArenaDuel/Game/ArenaDuelUpgrades.h"
#include "ArenaDuel/Game/ArenaDuelZombieGameMode.h"
#include "ArenaDuel/Game/ArenaDuelZombieGameState.h"
#include "ArenaDuel/Game/ArenaDuelZombieTypes.h"
#include "ArenaDuel/Characters/ArenaDuelZombie.h"
#include "Components/CapsuleComponent.h"
#include "ArenaDuel/Player/ArenaDuelLocalSettings.h"
#include "ArenaDuel/Player/ArenaDuelPlayerController.h"
#include "ArenaDuel/UI/ArenaDuelPlayerMenuWidget.h"
#include "ArenaDuel/Player/ArenaDuelPlayerState.h"
#include "ArenaDuel/Weapons/ArenaDuelWeaponComponent.h"
#include "Camera/CameraComponent.h"
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

// E12: a capsule of any height is centred on the floor under the picked point.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FArenaDuelPhase9SpawnCentreTest, "ArenaDuel.Phase9.Survival.SpawnCentre", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FArenaDuelPhase9SpawnCentreTest::RunTest(const FString& Parameters)
{
	const FVector Picked(100.0f, -40.0f, 96.0f);   // what PickSpawnLocation answers over a floor at 0
	TestTrue(TEXT("An ordinary zombie stands where it stood"), FMath::IsNearlyEqual(AArenaDuelZombieGameMode::SpawnCentreForCapsule(Picked, 96.0f).Z, 98.0, 0.01));
	TestTrue(TEXT("A capsule twice as tall has its centre twice as high"), FMath::IsNearlyEqual(AArenaDuelZombieGameMode::SpawnCentreForCapsule(Picked, 192.0f).Z, 194.0, 0.01));
	TestTrue(TEXT("Its feet are on the floor, not in it"), AArenaDuelZombieGameMode::SpawnCentreForCapsule(Picked, 250.0f).Z - 250.0 > 0.0);
	TestTrue(TEXT("The place on the floor is kept"), FMath::IsNearlyEqual(AArenaDuelZombieGameMode::SpawnCentreForCapsule(Picked, 192.0f).X, 100.0) && FMath::IsNearlyEqual(AArenaDuelZombieGameMode::SpawnCentreForCapsule(Picked, 192.0f).Y, -40.0));
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

	// E08: while the eye eases into a crouch the shot leaves from the eased eye, on the server and for the owner.
	TEST_METHOD(ShotOriginFollowsTheEasedEyeThroughACrouch)
	{
		using namespace ArenaDuelPhase9Tests;
		Network
			.UntilServer(TEXT("Both duel players have pawns"), [](FArenaDuelPhase9State& State) { return PawnForSlot(State.World, 0) && PawnForSlot(State.World, 1); }, FTimespan::FromSeconds(10.0))
			.ThenServer(TEXT("Start a round"), [](FArenaDuelPhase9State& State) { StartMatch(State.World); })
			.UntilServer(TEXT("Round one runs"), [](FArenaDuelPhase9State& State) { return BothAliveInRound(State.World); }, FTimespan::FromSeconds(6.0))
			.ThenServer(TEXT("The remote player's pawn crouches on the server"), [this](FArenaDuelPhase9State& State)
			{
				AArenaDuelCharacter* Pawn = PawnForSlot(State.World, 1);
				// Held in the air and treated as walking, so the crouch itself is all that moves the capsule.
				Pawn->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
				Pawn->GetCharacterMovement()->GravityScale = 0.0f;
				Pawn->GetCharacterMovement()->bCrouchMaintainsBaseLocation = true;
				// Away from the other pawn: the two share the origin of the empty test world, and a capsule that starts
				// inside another cannot be moved down by the crouch.
				Pawn->SetActorLocation(FVector(3000.0f, 3000.0f, 800.0f), false, nullptr, ETeleportType::TeleportPhysics);
				const double EyeBefore = Pawn->GetPawnViewLocation().Z;
				if (!FMath::IsNearlyZero(Pawn->GetEyeEaseOffset(), 0.01f)) TestRunner->AddError(TEXT("The eye is not at rest before the crouch"));
				Pawn->GetCharacterMovement()->bWantsToCrouch = true;
				Pawn->GetCharacterMovement()->Crouch(false);
				if (!Pawn->bIsCrouched) { TestRunner->AddError(TEXT("The pawn did not crouch")); return; }
				// The capsule has shrunk at once; the eye, and with it the shot origin, has not moved yet.
				if (Pawn->GetEyeEaseOffset() < 5.0f) TestRunner->AddError(TEXT("The server does not ease the eye of a remote player's pawn"));
				if (!FMath::IsNearlyEqual(Pawn->GetPawnViewLocation().Z, EyeBefore, 1.5)) TestRunner->AddError(FString::Printf(TEXT("The shot origin jumped by %.1f cm with the crouch"), Pawn->GetPawnViewLocation().Z - EyeBefore));
				State.Health = Pawn->GetEyeEaseOffset();
				State.Mark = State.World->GetTimeSeconds();
			})
			.UntilServer(TEXT("The eased offset shrinks and ends at the settled camera height"), [this](FArenaDuelPhase9State& State)
			{
				const AArenaDuelCharacter* Pawn = PawnForSlot(State.World, 1);
				const double Age = State.World->GetTimeSeconds() - State.Mark;
				if (Age < 0.08) return false;
				if (Age < 0.3 && !(Pawn->GetEyeEaseOffset() < State.Health)) TestRunner->AddError(TEXT("The eye does not ease down"));
				if (Age < 1.0) return false;
				if (!FMath::IsNearlyZero(Pawn->GetEyeEaseOffset(), 0.01f)) TestRunner->AddError(TEXT("The eye did not settle"));
				if (!FMath::IsNearlyEqual(Pawn->GetPawnViewLocation().Z - Pawn->GetActorLocation().Z, Pawn->GetFirstPersonCamera()->GetRelativeLocation().Z, 12.0)) TestRunner->AddError(TEXT("Settled, the shot origin is not at the camera height"));
				return true;
			}, FTimespan::FromSeconds(4.0));
	}

	// E08, what is left of it: the owner's word about its eye moves the server's clock only within limits, and
	// the eased eye decides what a shot fired just over cover does.
	TEST_METHOD(OwnerEyeHintIsTakenWithinLimitsAndTheEasedEyeDecidesAShotOverCover)
	{
		using namespace ArenaDuelPhase9Tests;
		Network
			.UntilServer(TEXT("Both duel players have pawns"), [](FArenaDuelPhase9State& State) { return PawnForSlot(State.World, 0) && PawnForSlot(State.World, 1); }, FTimespan::FromSeconds(10.0))
			.ThenServer(TEXT("Start a round"), [](FArenaDuelPhase9State& State) { StartMatch(State.World); })
			.UntilServer(TEXT("Round one runs"), [](FArenaDuelPhase9State& State) { return BothAliveInRound(State.World); }, FTimespan::FromSeconds(6.0))
			.ThenServer(TEXT("Remote player's pawn: what its owner says about the eye is taken within limits"), [this](FArenaDuelPhase9State& State)
			{
				AArenaDuelCharacter* Pawn = PawnForSlot(State.World, 1);
				Pawn->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
				Pawn->GetCharacterMovement()->GravityScale = 0.0f;
				Pawn->SetActorLocation(FVector(-3000.0f, 3000.0f, 800.0f), false, nullptr, ETeleportType::TeleportPhysics);
				Pawn->GetCharacterMovement()->bCrouchMaintainsBaseLocation = true;
				Pawn->GetCharacterMovement()->bWantsToCrouch = true;
				Pawn->GetCharacterMovement()->Crouch(false);
				if (!Pawn->bIsCrouched || Pawn->GetEyeEaseOffset() < 40.0f) { TestRunner->AddError(TEXT("The pawn did not crouch with its eye still up")); return; }
				const float Limit = Pawn->GetEyeEaseHintMaxSeconds(), Start = Pawn->GetEyeEaseOffset();
				// The owner says its eye is 40 ms further along: taken as said, and the eye is lower by what 40 ms are worth.
				Pawn->ApplyOwnerEyeEaseHint(Pawn->GetCrouchEaseAge() + 0.04f, 1000.0f);
				if (!FMath::IsNearlyEqual(Pawn->GetCrouchEaseHint(), 0.04f, 0.005f)) TestRunner->AddError(FString::Printf(TEXT("A small hint was taken as %.3f s instead of 0.040"), Pawn->GetCrouchEaseHint()));
				if (!(Pawn->GetEyeEaseOffset() < Start - 10.0f && Pawn->GetEyeEaseOffset() > Start - 25.0f)) TestRunner->AddError(FString::Printf(TEXT("A hint of 40 ms moved the eye from %.1f to %.1f"), Start, Pawn->GetEyeEaseOffset()));
				// Nearly a second further along: no more than the limit.
				Pawn->ApplyOwnerEyeEaseHint(0.9f, 1000.0f);
				if (Pawn->GetCrouchEaseHint() > Limit + 0.001f || Pawn->GetCrouchEaseHint() < Limit - 0.02f) TestRunner->AddError(FString::Printf(TEXT("A large hint moved the clock by %.3f s; the limit is %.3f"), Pawn->GetCrouchEaseHint(), Limit));
				const float AtLimit = Pawn->GetEyeEaseOffset();
				if (AtLimit < 5.0f) TestRunner->AddError(TEXT("At the limit the eye is already settled: the limit does not bound anything"));
				// Not in an ease at all, negative, not a number: nothing.
				for (const float Nonsense : { 5.0f, -3.0f, std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity() })
				{
					Pawn->ApplyOwnerEyeEaseHint(Nonsense, Nonsense);
					if (!FMath::IsNearlyZero(Pawn->GetCrouchEaseHint()) || !FMath::IsNearlyZero(Pawn->GetSlideEaseHint())) TestRunner->AddError(TEXT("A value from outside the ease was taken as a hint"));
				}
				if (Pawn->GetEyeEaseOffset() > 48.5f) TestRunner->AddError(TEXT("The eye is above where the ease started"));
				// A new ease drops what was said about the last one.
				Pawn->ApplyOwnerEyeEaseHint(Pawn->GetCrouchEaseAge() + 0.05f, 1000.0f);
				const bool bTaken = FMath::IsNearlyEqual(Pawn->GetCrouchEaseHint(), 0.05f, 0.005f);
				Pawn->GetCharacterMovement()->bWantsToCrouch = false;
				Pawn->GetCharacterMovement()->UnCrouch(false);
				if (!bTaken || !FMath::IsNearlyZero(Pawn->GetCrouchEaseHint())) TestRunner->AddError(TEXT("A hint outlived the ease it was given for"));
				// The host's own pawn takes no hint: it is its own owner.
				AArenaDuelCharacter* Host = PawnForSlot(State.World, 0);
				Host->ApplyOwnerEyeEaseHint(0.05f, 0.05f);
				if (!FMath::IsNearlyZero(Host->GetCrouchEaseHint()) || !FMath::IsNearlyZero(Host->GetSlideEaseHint())) TestRunner->AddError(TEXT("The host's own pawn took a hint"));
			})
			.ThenServer(TEXT("Host's pawn stands behind cover a hand lower than its standing eye"), [this](FArenaDuelPhase9State& State)
			{
				AArenaDuelCharacter* Pawn = PawnForSlot(State.World, 0);
				Pawn->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
				Pawn->GetCharacterMovement()->GravityScale = 0.0f;
				Pawn->SetActorLocation(FVector(3000.0f, 3000.0f, 800.0f), false, nullptr, ETeleportType::TeleportPhysics);
				Pawn->GetController()->SetControlRotation(FRotator::ZeroRotator);
				// The top of the wall is 12 cm below the standing eye: a standing shot passes over it; crouched, the
				// eye settles 48 cm lower, 36 cm below the top.
				const float StandingEye = Pawn->GetPawnViewLocation().Z;
				AStaticMeshActor* Wall = State.World->SpawnActor<AStaticMeshActor>(FVector(3150.0f, 3000.0f, StandingEye - 12.0f - 100.0f), FRotator::ZeroRotator);
				Wall->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
				Wall->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
				Wall->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));
				Wall->SetActorScale3D(FVector(0.2f, 6.0f, 2.0f));
				State.Wall = Wall;
				State.Base = Pawn->GetActorLocation();
				State.Mark = State.World->GetTimeSeconds();
				State.Sequence = 0;
			})
			.UntilServer(TEXT("Standing over the wall; crouching, the shot follows the eye down behind it"), [this](FArenaDuelPhase9State& State)
			{
				AArenaDuelCharacter* Pawn = PawnForSlot(State.World, 0);
				UArenaDuelWeaponComponent* Weapon = Pawn->GetWeaponComponent();
				const double Age = State.World->GetTimeSeconds() - State.Mark;
				const auto Shoot = [Weapon]() { const int32 Before = Weapon->GetLastShotSequence(); Weapon->ClearActionTimesForTests(); Weapon->StartFire(); Weapon->StopFire(); return Weapon->GetLastShotSequence() != Before; };
				// Held where it was put: the test world has no floor.
				if (State.Sequence == 0)
				{
					if (Age < 0.25) return false;
					Pawn->SetActorLocation(State.Base, false, nullptr, ETeleportType::TeleportPhysics);
					if (!Shoot() || Weapon->GetLastShotResult() != EArenaDuelShotResult::Miss) TestRunner->AddError(TEXT("A standing shot did not pass over the cover"));
					Pawn->GetCharacterMovement()->bCrouchMaintainsBaseLocation = true;
					Pawn->GetCharacterMovement()->bWantsToCrouch = true;
					Pawn->GetCharacterMovement()->Crouch(false);
					if (!Pawn->bIsCrouched || Pawn->GetEyeEaseOffset() < 40.0f) { TestRunner->AddError(TEXT("The host pawn did not crouch with its eye still up")); return true; }
					// In the first moment the eye, and the shot, are where they were.
					if (!Shoot() || Weapon->GetLastShotResult() != EArenaDuelShotResult::Miss) TestRunner->AddError(TEXT("In the first moment of the crouch the shot did not pass over the cover the eye still looks over"));
					State.Mark = State.World->GetTimeSeconds();
					State.Sequence = 1;
					return false;
				}
				if (State.Sequence == 1)
				{
					// Somewhere on the way down: whatever the eye does, the shot does.
					if (Age < 0.05) return false;
					State.Sequence = 2;
					const float Left = Pawn->GetEyeEaseOffset();
					const bool bEyeAbove = Left > 36.0f;
					if (FMath::Abs(Left - 36.0f) > 1.5f)
					{
						if (!Shoot()) { TestRunner->AddError(TEXT("No shot left during the ease")); return true; }
						const bool bOver = Weapon->GetLastShotResult() == EArenaDuelShotResult::Miss;
						if (bOver != bEyeAbove) TestRunner->AddError(FString::Printf(TEXT("With %.1f cm of the ease left the shot %s the cover although the eye is %s its top"), Left, bOver ? TEXT("passed over") : TEXT("hit"), bEyeAbove ? TEXT("above") : TEXT("below")));
					}
					return false;
				}
				if (Age < 1.2) return false;
				if (!Pawn->bIsCrouched) { TestRunner->AddError(TEXT("The host pawn did not stay crouched")); return true; }
				if (!FMath::IsNearlyZero(Pawn->GetEyeEaseOffset(), 0.05f)) TestRunner->AddError(TEXT("The eye did not settle"));
				if (!Shoot() || Weapon->GetLastShotResult() != EArenaDuelShotResult::World) TestRunner->AddError(TEXT("Settled in the crouch, a shot did not hit the cover in front of the eye"));
				if (State.Wall.IsValid()) State.Wall->Destroy();
				return true;
			}, FTimespan::FromSeconds(6.0));
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

	// E04 with latency: a client hears of its own switch later than it asked for it. A trigger pressed in between
	// goes by the item asked for: after asking for the firearm it holds fire, after asking for the knife it does not.
	TEST_METHOD(ClientTriggerGoesByTheItemItAskedFor)
	{
		using namespace ArenaDuelPhase9Tests;
		const auto ClientWeapon = [](FArenaDuelPhase9State& State) -> UArenaDuelWeaponComponent*
		{
			const APlayerController* Controller = State.World->GetFirstPlayerController();
			const AArenaDuelCharacter* Pawn = Controller ? Cast<AArenaDuelCharacter>(Controller->GetPawn()) : nullptr;
			return Pawn ? Pawn->GetWeaponComponent() : nullptr;
		};
		Network
			.UntilServer(TEXT("Both duel players have pawns"), [](FArenaDuelPhase9State& State) { return PawnForSlot(State.World, 0) && PawnForSlot(State.World, 1); }, FTimespan::FromSeconds(10.0))
			.ThenServer(TEXT("Start a round"), [](FArenaDuelPhase9State& State) { StartMatch(State.World); })
			.UntilServer(TEXT("Round one runs"), [](FArenaDuelPhase9State& State) { return BothAliveInRound(State.World); }, FTimespan::FromSeconds(6.0))
			.UntilClient(TEXT("Client: the round runs here too"), 0, [ClientWeapon](FArenaDuelPhase9State& State)
			{
				const AArenaDuelGameState* GameState = State.World->GetGameState<AArenaDuelGameState>();
				return GameState && GameState->IsRoundInProgress() && ClientWeapon(State) && ClientWeapon(State)->GetActiveSlot() == EArenaDuelLoadoutSlot::Primary;
			}, FTimespan::FromSeconds(6.0))
			.ThenClient(TEXT("Client: ask for the knife and press at once"), 0, [this, ClientWeapon](FArenaDuelPhase9State& State)
			{
				UArenaDuelWeaponComponent* Weapon = ClientWeapon(State);
				Weapon->SelectSlot(EArenaDuelLoadoutSlot::Knife);
				if (Weapon->GetActiveSlot() != EArenaDuelLoadoutSlot::Primary) TestRunner->AddError(TEXT("The client already has the server's answer; the case under test needs it pending"));
				if (Weapon->GetExpectedSlot() != EArenaDuelLoadoutSlot::Knife) TestRunner->AddError(TEXT("The client does not go by the knife it asked for"));
				Weapon->StartFire();
				if (Weapon->IsLocalFireHeld()) TestRunner->AddError(TEXT("A press after asking for the knife held the firearm's trigger"));
			})
			.UntilClient(TEXT("Client: the knife arrives"), 0, [ClientWeapon](FArenaDuelPhase9State& State) { return ClientWeapon(State)->GetActiveSlot() == EArenaDuelLoadoutSlot::Knife; }, FTimespan::FromSeconds(5.0))
			.ThenServer(TEXT("Server: note the magazine and let the knife's times pass"), [](FArenaDuelPhase9State& State)
			{
				UArenaDuelWeaponComponent* Weapon = PawnForSlot(State.World, 1)->GetWeaponComponent();
				State.Ammo = Weapon->GetCurrentMagazineAmmo();
				Weapon->ClearActionTimesForTests();
			})
			.ThenClient(TEXT("Client: ask for the firearm and press at once, while the knife is still shown"), 0, [this, ClientWeapon](FArenaDuelPhase9State& State)
			{
				UArenaDuelWeaponComponent* Weapon = ClientWeapon(State);
				Weapon->SelectSlot(EArenaDuelLoadoutSlot::Primary);
				if (Weapon->GetActiveSlot() != EArenaDuelLoadoutSlot::Knife) TestRunner->AddError(TEXT("The client already has the server's answer; the case under test needs it pending"));
				Weapon->StartFire();
				if (!Weapon->IsLocalFireHeld()) TestRunner->AddError(TEXT("A press right after asking for the firearm was taken for a knife stroke"));
			})
			.UntilServer(TEXT("Server: the held trigger fires real shots once the switch time is over"), [](FArenaDuelPhase9State& State)
			{
				const UArenaDuelWeaponComponent* Weapon = PawnForSlot(State.World, 1)->GetWeaponComponent();
				return Weapon->GetActiveSlot() == EArenaDuelLoadoutSlot::Primary && Weapon->GetCurrentMagazineAmmo() < State.Ammo;
			}, FTimespan::FromSeconds(5.0))
			.ThenClient(TEXT("Client: let go"), 0, [ClientWeapon](FArenaDuelPhase9State& State) { ClientWeapon(State)->StopFire(); })
			.UntilClient(TEXT("Client: the answer has arrived and nothing is expected any more"), 0, [ClientWeapon](FArenaDuelPhase9State& State)
			{
				const UArenaDuelWeaponComponent* Weapon = ClientWeapon(State);
				return Weapon->GetActiveSlot() == EArenaDuelLoadoutSlot::Primary && Weapon->GetExpectedSlot() == EArenaDuelLoadoutSlot::Primary && !Weapon->IsLocalFireHeld();
			}, FTimespan::FromSeconds(5.0));
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

// The server's rate of fire for automatic weapons: the configured rounds per minute, at any frame rate, with no
// shot left out and none added, and nothing fired after the fire was cut off.
namespace ArenaDuelPhase9FireRate
{
	static void SetServerFrameRate(float Fps)
	{
		if (IConsoleVariable* MaxFps = IConsoleManager::Get().FindConsoleVariable(TEXT("t.MaxFPS"))) MaxFps->Set(Fps);
	}
	static TArray<int32> AutomaticWeapons()
	{
		TArray<int32> Found;
		const UArenaDuelWeaponComponent* Defaults = GetDefault<UArenaDuelWeaponComponent>();
		for (int32 Index = 0; Index < Defaults->GetWeaponDefinitionCount(); ++Index) if (Defaults->GetWeaponDefinition(Index)->bAutomatic) Found.Add(Index);
		return Found;
	}
	static double Interval(const UArenaDuelWeaponComponent* Weapon) { return 60.0 / FMath::Max(static_cast<double>(Weapon->GetCurrentDefinition().RoundsPerMinute), 1.0); }
	/** Shots a held trigger may and must have fired in this many seconds: one at once, then one per interval. */
	static int32 ShotsDue(double Seconds, double ShotInterval) { return FMath::FloorToInt32(Seconds / ShotInterval) + 1; }
}

NETWORK_TEST_CLASS(FArenaDuelPhase9FireRateTest, "ArenaDuel.Phase9.FireRate")
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
		ArenaDuelPhase9FireRate::SetServerFrameRate(0.0f);
		SetMatchUpgradesForTests(true);
	}

	// One held burst of 1.5 s per automatic weapon and frame rate: 30, 60 and 144 frames per second, a frame rate
	// that jumps between 25 and 200 from frame to frame, and 60 with one hitch of a quarter of a second.
	TEST_METHOD(HeldFireKeepsTheConfiguredRateAtAnyFrameRate)
	{
		using namespace ArenaDuelPhase9Tests;
		using namespace ArenaDuelPhase9FireRate;
		Network
			.UntilServer(TEXT("Both duel players have pawns"), [](FArenaDuelPhase9State& State) { return PawnForSlot(State.World, 0) && PawnForSlot(State.World, 1); }, FTimespan::FromSeconds(10.0))
			.ThenServer(TEXT("Start a round"), [](FArenaDuelPhase9State& State) { StartMatch(State.World); })
			.UntilServer(TEXT("Round one runs"), [](FArenaDuelPhase9State& State) { return BothAliveInRound(State.World); }, FTimespan::FromSeconds(6.0));
		// Frame rate: above zero as given, -1 jumping, -2 at 60 with a hitch.
		for (const int32 WeaponIndex : AutomaticWeapons()) for (const float Fps : { 30.0f, 60.0f, 144.0f, -1.0f, -2.0f })
		{
			Network
				.ThenServer(TEXT("Take the weapon, fill it and hold the trigger"), [WeaponIndex, Fps](FArenaDuelPhase9State& State)
				{
					UArenaDuelWeaponComponent* Weapon = PawnForSlot(State.World, 0)->GetWeaponComponent();
					SetServerFrameRate(Fps > 0.0f ? Fps : 60.0f);
					Weapon->StopFire();
					Weapon->GrantAllWeaponsForDevelopment();
					Weapon->EquipWeapon(WeaponIndex);
					Weapon->RefillAllAmmoForDevelopment();
					State.Mark = 0.0;
					State.Serial = 0;
				})
				.UntilServer(TEXT("The burst runs for a second and a half and has every shot that was due"), [this, WeaponIndex, Fps](FArenaDuelPhase9State& State)
				{
					UArenaDuelWeaponComponent* Weapon = PawnForSlot(State.World, 0)->GetWeaponComponent();
					const double Now = State.World->GetTimeSeconds();
					if (State.Mark <= 0.0)
					{
						// A few frames at the new frame rate first, and the last shot's own interval, then press.
						if (++State.Serial < 12 || Weapon->GetEquippedWeaponIndex() != WeaponIndex) return false;
						Weapon->ClearActionTimesForTests();
						State.Sequence = Weapon->GetLastShotSequence();
						State.Ammo = Weapon->GetCurrentMagazineAmmo();
						State.Health = 0.0f;
						Weapon->StartFire();
						State.Mark = Now;
						if (Weapon->GetLastShotSequence() != State.Sequence + 1) TestRunner->AddError(TEXT("The first shot did not leave with the press"));
						return false;
					}
					if (Fps == -1.0f) SetServerFrameRate((++State.Serial % 2) == 0 ? 25.0f : 200.0f);
					if (Fps == -2.0f && State.Health == 0.0f && Now - State.Mark > 0.6)
					{
						// One long frame. The shots that fell into it may be made up for only in part.
						State.Health = 1.0f;
						State.Lateral = static_cast<float>(Weapon->GetLastShotSequence());
						FPlatformProcess::Sleep(0.25f);
						return false;
					}
					if (Fps == -2.0f && State.Health == 1.0f)
					{
						State.Health = 2.0f;
						const int32 InOneFrame = Weapon->GetLastShotSequence() - static_cast<int32>(State.Lateral);
						if (InOneFrame > 3) TestRunner->AddError(FString::Printf(TEXT("%d shots left in the frame after a hitch"), InOneFrame));
					}
					if (Now - State.Mark < 1.5) return false;
					const double Held = Now - State.Mark;
					const int32 Shots = Weapon->GetLastShotSequence() - State.Sequence;
					const int32 Due = ShotsDue(Held, Interval(Weapon));
					Weapon->StopFire();
					const FString Case = FString::Printf(TEXT("weapon %d, %s: %d shots in %.3f s, %d due at %.0f rounds per minute"), WeaponIndex, Fps > 0.0f ? *FString::Printf(TEXT("%.0f fps"), Fps) : (Fps == -1.0f ? TEXT("jumping frame rate") : TEXT("a hitch")), Shots, Held, Due, Weapon->GetCurrentDefinition().RoundsPerMinute);
					UE_LOG(LogTemp, Display, TEXT("FireRate %s"), *Case);
					// With a hitch the shots of at most the long frame may be missing, less what was made up for.
					const int32 MayMiss = Fps == -2.0f ? FMath::CeilToInt32(0.25 / Interval(Weapon)) + 1 : 1;
					// -FireRateMeasureOnly on the command line: log every case and judge none, to measure a build as it is.
					if (FParse::Param(FCommandLine::Get(), TEXT("FireRateMeasureOnly"))) return true;
					if (Shots > Due) TestRunner->AddError(FString::Printf(TEXT("More shots than the rate allows (%s)"), *Case));
					if (Shots < Due - MayMiss) TestRunner->AddError(FString::Printf(TEXT("Shots were left out (%s)"), *Case));
					if (State.Ammo - Weapon->GetCurrentMagazineAmmo() != Shots) TestRunner->AddError(FString::Printf(TEXT("Rounds used and shots fired differ (%s)"), *Case));
					return true;
				}, FTimespan::FromSeconds(8.0));
		}
	}

	// Everything that cuts a burst off: letting go, pressing again and again, a switch, a reload, the last round,
	// the end of the round. No shot after the cut, none more than the rate allows, none below zero.
	TEST_METHOD(NothingFiresAfterTheFireIsCutOffAndNothingExtraFromPressingAgain)
	{
		using namespace ArenaDuelPhase9Tests;
		using namespace ArenaDuelPhase9FireRate;
		const int32 WeaponIndex = AutomaticWeapons().Num() > 0 ? AutomaticWeapons()[0] : 0;
		const auto Wait = [](double Seconds) { return [Seconds](FArenaDuelPhase9State& State) { return State.World->GetTimeSeconds() - State.Mark >= Seconds; }; };
		Network
			.UntilServer(TEXT("Both duel players have pawns"), [](FArenaDuelPhase9State& State) { return PawnForSlot(State.World, 0) && PawnForSlot(State.World, 1); }, FTimespan::FromSeconds(10.0))
			.ThenServer(TEXT("Start a round and take an automatic weapon"), [WeaponIndex](FArenaDuelPhase9State& State)
			{
				StartMatch(State.World);
				SetServerFrameRate(60.0f);
			})
			.UntilServer(TEXT("Round one runs"), [](FArenaDuelPhase9State& State) { return BothAliveInRound(State.World); }, FTimespan::FromSeconds(6.0))
			.ThenServer(TEXT("Hold, then let go"), [WeaponIndex](FArenaDuelPhase9State& State)
			{
				UArenaDuelWeaponComponent* Weapon = PawnForSlot(State.World, 0)->GetWeaponComponent();
				Weapon->GrantAllWeaponsForDevelopment();
				Weapon->EquipWeapon(WeaponIndex);
				Weapon->RefillAllAmmoForDevelopment();
				Weapon->ClearActionTimesForTests();
				Weapon->StartFire();
				State.Mark = State.World->GetTimeSeconds();
			})
			.UntilServer(TEXT("A third of a second of fire"), Wait(0.33), FTimespan::FromSeconds(3.0))
			.ThenServer(TEXT("Let go"), [](FArenaDuelPhase9State& State)
			{
				UArenaDuelWeaponComponent* Weapon = PawnForSlot(State.World, 0)->GetWeaponComponent();
				Weapon->StopFire();
				State.Sequence = Weapon->GetLastShotSequence();
				State.Mark = State.World->GetTimeSeconds();
			})
			.UntilServer(TEXT("Nothing fires after letting go"), [this](FArenaDuelPhase9State& State)
			{
				UArenaDuelWeaponComponent* Weapon = PawnForSlot(State.World, 0)->GetWeaponComponent();
				if (State.World->GetTimeSeconds() - State.Mark < 0.4) return false;
				if (Weapon->GetLastShotSequence() != State.Sequence) TestRunner->AddError(TEXT("A shot left after the trigger was let go"));
				// Now press and let go on every frame for a second.
				Weapon->RefillAllAmmoForDevelopment();
				State.Sequence = Weapon->GetLastShotSequence();
				State.Mark = State.World->GetTimeSeconds();
				Weapon->StartFire();
				return true;
			}, FTimespan::FromSeconds(3.0))
			.UntilServer(TEXT("Pressing again and again gives no shot more than holding would"), [this](FArenaDuelPhase9State& State)
			{
				UArenaDuelWeaponComponent* Weapon = PawnForSlot(State.World, 0)->GetWeaponComponent();
				const double Held = State.World->GetTimeSeconds() - State.Mark;
				Weapon->StopFire();
				if (Held < 1.0) { Weapon->StartFire(); return false; }
				const int32 Shots = Weapon->GetLastShotSequence() - State.Sequence, Due = ShotsDue(Held, Interval(Weapon));
				if (Shots > Due) TestRunner->AddError(FString::Printf(TEXT("Pressing again and again fired %d shots where %d were due"), Shots, Due));
				if (Shots < Due / 2) TestRunner->AddError(FString::Printf(TEXT("Pressing again and again fired only %d shots where %d were due"), Shots, Due));
				// A switch in the middle of a burst.
				Weapon->RefillAllAmmoForDevelopment();
				Weapon->StartFire();
				State.Mark = State.World->GetTimeSeconds();
				return true;
			}, FTimespan::FromSeconds(4.0))
			.UntilServer(TEXT("A fifth of a second of fire"), Wait(0.2), FTimespan::FromSeconds(3.0))
			.ThenServer(TEXT("Switch to the knife"), [this](FArenaDuelPhase9State& State)
			{
				UArenaDuelWeaponComponent* Weapon = PawnForSlot(State.World, 0)->GetWeaponComponent();
				Weapon->SelectSlot(EArenaDuelLoadoutSlot::Knife);
				if (Weapon->GetActiveSlot() != EArenaDuelLoadoutSlot::Knife || Weapon->IsServerFiring()) TestRunner->AddError(TEXT("The switch did not stop the fire"));
				State.Sequence = Weapon->GetLastShotSequence();
				State.Mark = State.World->GetTimeSeconds();
			})
			.UntilServer(TEXT("Nothing fires after the switch"), [this, WeaponIndex](FArenaDuelPhase9State& State)
			{
				UArenaDuelWeaponComponent* Weapon = PawnForSlot(State.World, 0)->GetWeaponComponent();
				if (State.World->GetTimeSeconds() - State.Mark < 0.4) return false;
				if (Weapon->GetLastShotSequence() != State.Sequence) TestRunner->AddError(TEXT("A shot left after the switch to the knife"));
				// Back to the firearm; a reload in the middle of a burst.
				Weapon->SelectSlot(EArenaDuelLoadoutSlot::Primary);
				Weapon->ClearActionTimesForTests();
				Weapon->StartFire();
				State.Mark = State.World->GetTimeSeconds();
				return true;
			}, FTimespan::FromSeconds(3.0))
			.UntilServer(TEXT("A fifth of a second of fire again"), Wait(0.2), FTimespan::FromSeconds(3.0))
			.ThenServer(TEXT("Reload"), [this](FArenaDuelPhase9State& State)
			{
				UArenaDuelWeaponComponent* Weapon = PawnForSlot(State.World, 0)->GetWeaponComponent();
				Weapon->Reload();
				if (!Weapon->IsReloading() || Weapon->IsServerFiring()) TestRunner->AddError(TEXT("The reload did not start or did not stop the fire"));
				State.Sequence = Weapon->GetLastShotSequence();
				State.Mark = State.World->GetTimeSeconds();
			})
			.UntilServer(TEXT("Nothing fires during the reload; then the last three rounds"), [this](FArenaDuelPhase9State& State)
			{
				UArenaDuelWeaponComponent* Weapon = PawnForSlot(State.World, 0)->GetWeaponComponent();
				if (Weapon->IsReloading())
				{
					if (Weapon->GetLastShotSequence() != State.Sequence) TestRunner->AddError(TEXT("A shot left during the reload"));
					return false;
				}
				// Three rounds left, the trigger held: three shots, then the reload the empty magazine starts.
				Weapon->ConsumeAmmoForTests(Weapon->GetCurrentMagazineAmmo() - 3);
				State.Sequence = Weapon->GetLastShotSequence();
				State.Ammo = Weapon->GetReserveAmmo();
				Weapon->ClearActionTimesForTests();
				Weapon->StartFire();
				State.Mark = State.World->GetTimeSeconds();
				return true;
			}, FTimespan::FromSeconds(8.0))
			.UntilServer(TEXT("Exactly the rounds that were there, then a reload"), [this](FArenaDuelPhase9State& State)
			{
				UArenaDuelWeaponComponent* Weapon = PawnForSlot(State.World, 0)->GetWeaponComponent();
				if (State.World->GetTimeSeconds() - State.Mark < 0.8) return false;
				const int32 Shots = Weapon->GetLastShotSequence() - State.Sequence;
				if (Shots != 3) TestRunner->AddError(FString::Printf(TEXT("%d shots left from a magazine of three"), Shots));
				if (Weapon->GetCurrentMagazineAmmo() < 0) TestRunner->AddError(TEXT("The magazine went below zero"));
				if (!Weapon->IsReloading() && Weapon->GetCurrentMagazineAmmo() == 0) TestRunner->AddError(TEXT("The empty magazine did not start a reload"));
				Weapon->StopFire();
				return !Weapon->IsReloading();
			}, FTimespan::FromSeconds(8.0))
			.ThenServer(TEXT("Hold again, then the round ends"), [this](FArenaDuelPhase9State& State)
			{
				UArenaDuelWeaponComponent* Weapon = PawnForSlot(State.World, 0)->GetWeaponComponent();
				Weapon->ClearActionTimesForTests();
				Weapon->StartFire();
				PawnForSlot(State.World, 1)->ApplyServerDamage(1000.0f);
				if (State.World->GetGameState<AArenaDuelGameState>()->IsRoundInProgress()) TestRunner->AddError(TEXT("The round did not end with the kill"));
				State.Sequence = Weapon->GetLastShotSequence();
				State.Mark = State.World->GetTimeSeconds();
			})
			.UntilServer(TEXT("Nothing fires after the round has ended"), [this](FArenaDuelPhase9State& State)
			{
				const AArenaDuelCharacter* Pawn = PawnForSlot(State.World, 0);
				if (!Pawn || State.World->GetTimeSeconds() - State.Mark < 0.3) return false;
				// The pawn of the next round is a new one; the check is for the one that held the trigger.
				if (State.World->GetGameState<AArenaDuelGameState>()->GetRoundNumber() == 1 && Pawn->GetWeaponComponent()->GetLastShotSequence() != State.Sequence) TestRunner->AddError(TEXT("A shot left after the round had ended"));
				return true;
			}, FTimespan::FromSeconds(3.0))
			.UntilServer(TEXT("Round two runs"), [](FArenaDuelPhase9State& State) { return BothAliveInRound(State.World) && State.World->GetGameState<AArenaDuelGameState>()->GetRoundNumber() == 2; }, FTimespan::FromSeconds(8.0))
			.ThenServer(TEXT("Hold, then the shooter dies"), [this, WeaponIndex](FArenaDuelPhase9State& State)
			{
				AArenaDuelCharacter* Shooter = PawnForSlot(State.World, 0);
				UArenaDuelWeaponComponent* Weapon = Shooter->GetWeaponComponent();
				Weapon->ClearActionTimesForTests();
				Weapon->StartFire();
				const int32 Sequence = Weapon->GetLastShotSequence();
				Shooter->ApplyServerDamage(1000.0f);
				if (!Shooter->IsDead()) TestRunner->AddError(TEXT("The shooter did not die"));
				Weapon->FireAuthoritativeForTests();
				if (Weapon->GetLastShotSequence() != Sequence || Weapon->IsServerFiring()) TestRunner->AddError(TEXT("A dead player's weapon fired or kept the trigger"));
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

	// E10: the shop, as the server runs it: full ammunition costs nothing, missing ammunition is paid for once and delivered.
	TEST_METHOD(ShopChargesForAmmunitionOnlyWhenSomeIsMissing)
	{
		Network
			.UntilServer(TEXT("The run is in a break and the host has a pawn"), [](FArenaDuelPhase9State& State)
			{
				const AArenaDuelZombieGameState* GameState = State.World->GetGameState<AArenaDuelZombieGameState>();
				const APlayerController* Host = State.World->GetFirstPlayerController();
				return GameState && GameState->IsIntermission() && Host && Cast<AArenaDuelCharacter>(Host->GetPawn());
			}, FTimespan::FromSeconds(20.0))
			.ThenServer(TEXT("Buy ammunition full, with rounds missing, again, and without points"), [this](FArenaDuelPhase9State& State)
			{
				AArenaDuelZombieGameMode* GameMode = State.World->GetAuthGameMode<AArenaDuelZombieGameMode>();
				APlayerController* Host = State.World->GetFirstPlayerController();
				AArenaDuelPlayerState* Player = Host->GetPlayerState<AArenaDuelPlayerState>();
				UArenaDuelWeaponComponent* Weapon = Cast<AArenaDuelCharacter>(Host->GetPawn())->GetWeaponComponent();
				const int32 Cost = GameMode->GetPurchaseCost(Host, EArenaDuelSurvivalPurchase::Ammo);
				if (Cost <= 0) { TestRunner->AddError(TEXT("Ammunition has no price to check")); return; }
				Player->AddSurvivalPoints(Cost * 3 - Player->GetSurvivalPoints());
				const int32 Points = Player->GetSurvivalPoints();
				const int32 Capacity = Weapon->GetCurrentDefinition().MagazineCapacity;
				if (!Weapon->IsAllAmmoFull() || Weapon->GetCurrentMagazineAmmo() != Capacity) { TestRunner->AddError(TEXT("The pawn does not start fully loaded")); return; }
				// Full: refused, nothing charged, nothing changed.
				if (GameMode->TryPurchase(Host, EArenaDuelSurvivalPurchase::Ammo)) TestRunner->AddError(TEXT("Ammunition was sold to a fully loaded player"));
				if (Player->GetSurvivalPoints() != Points) TestRunner->AddError(FString::Printf(TEXT("Full ammunition cost %d points"), Points - Player->GetSurvivalPoints()));
				// Five rounds missing: sold, charged once, delivered.
				Weapon->ConsumeAmmoForTests(5);
				if (Weapon->GetCurrentMagazineAmmo() != Capacity - 5 || Weapon->IsAllAmmoFull()) { TestRunner->AddError(TEXT("Could not take rounds out of the magazine")); return; }
				if (!GameMode->TryPurchase(Host, EArenaDuelSurvivalPurchase::Ammo)) TestRunner->AddError(TEXT("Ammunition was refused although rounds were missing"));
				if (Player->GetSurvivalPoints() != Points - Cost) TestRunner->AddError(FString::Printf(TEXT("The purchase cost %d points instead of %d"), Points - Player->GetSurvivalPoints(), Cost));
				if (Weapon->GetCurrentMagazineAmmo() != Capacity || !Weapon->IsAllAmmoFull()) TestRunner->AddError(TEXT("The paid ammunition was not delivered"));
				// Again at once: full, so refused and free.
				if (GameMode->TryPurchase(Host, EArenaDuelSurvivalPurchase::Ammo) || Player->GetSurvivalPoints() != Points - Cost) TestRunner->AddError(TEXT("A second purchase right after was sold or charged"));
				// Rounds missing but not enough points: refused, nothing delivered.
				Weapon->ConsumeAmmoForTests(3);
				Player->AddSurvivalPoints(Cost - 1 - Player->GetSurvivalPoints());
				if (GameMode->TryPurchase(Host, EArenaDuelSurvivalPurchase::Ammo) || Player->GetSurvivalPoints() != Cost - 1 || Weapon->GetCurrentMagazineAmmo() != Capacity - 3) TestRunner->AddError(TEXT("A purchase without enough points went through"));
			});
	}

	// E11, R03: a zombie killed while running, while standing, and by a head shot. The server keeps the speed it
	// had, every machine decides clip or ragdoll from the same data, and that data is complete when a client is
	// told of the death.
	TEST_METHOD(ZombieDeathDataIsKeptAndTheSameOnServerAndClient)
	{
		Network
			.UntilServer(TEXT("Host and client are in the run"), [](FArenaDuelPhase9State& State)
			{
				int32 Players = 0;
				for (FConstPlayerControllerIterator It = State.World->GetPlayerControllerIterator(); It; ++It) if (It->Get() && It->Get()->GetPawn()) ++Players;
				return Players == 2;
			}, FTimespan::FromSeconds(15.0))
			.ThenServer(TEXT("Three zombies die: running, standing, standing with a head shot"), [this](FArenaDuelPhase9State& State)
			{
				AArenaDuelZombieGameMode* GameMode = State.World->GetAuthGameMode<AArenaDuelZombieGameMode>();
				AController* Killer = State.World->GetFirstPlayerController();
				struct FCase { FVector Speed; EArenaDuelShotResult Zone; bool bBuckle; };
				const FCase Cases[] = { { FVector(450.0f, 0.0f, 0.0f), EArenaDuelShotResult::Body, false }, { FVector::ZeroVector, EArenaDuelShotResult::Body, true }, { FVector::ZeroVector, EArenaDuelShotResult::Head, false } };
				int32 Index = 0;
				for (const FCase& Case : Cases)
				{
					AArenaDuelZombie* Zombie = GameMode->SpawnZombieAt(EArenaDuelZombieType::Normal, FVector(900.0f, -600.0f + 600.0f * Index, 400.0f), FRotator::ZeroRotator);
					++Index;
					if (!Zombie) { TestRunner->AddError(TEXT("A zombie could not be spawned for the death check")); continue; }
					Zombie->GetCharacterMovement()->Velocity = Case.Speed;
					Zombie->TakeWeaponHit(100000.0f, 1.0f, Case.Zone, Killer, Zombie->GetActorLocation(), FVector(1.0f, 0.0f, 0.0f));
					if (!Zombie->IsDead()) { TestRunner->AddError(TEXT("The zombie survived the killing hit")); continue; }
					// The body is stopped by now; what counts is what it had.
					if (!Zombie->GetDeathVelocity().Equals(Case.Speed, 1.5)) TestRunner->AddError(FString::Printf(TEXT("Case %d: the speed at death was kept as %s"), Index, *Zombie->GetDeathVelocity().ToCompactString()));
					if (Zombie->ShouldBuckleBeforeRagdoll() != Case.bBuckle) TestRunner->AddError(FString::Printf(TEXT("Case %d: wrong choice between buckling and dropping on the server"), Index));
					if (!Zombie->WasDeathNotified() || !Zombie->GetDeathVelocityAtNotify().Equals(Case.Speed, 1.5) || Zombie->WasDeathHeadshotAtNotify() != (Case.Zone == EArenaDuelShotResult::Head)) TestRunner->AddError(FString::Printf(TEXT("Case %d: the server's own death handling did not see the death data"), Index));
					if (!Case.bBuckle && !Zombie->WasRagdollRequested()) TestRunner->AddError(FString::Printf(TEXT("Case %d: a zombie that drops at once was not handed to physics at once"), Index));
				}
			})
			.UntilClient(TEXT("The client has the three deaths with complete data and the same choices"), 0, [this](FArenaDuelPhase9State& State)
			{
				int32 Dead = 0, Running = 0, Buckling = 0, Headshots = 0, HandedOver = 0;
				for (TActorIterator<AArenaDuelZombie> It(State.World); It; ++It)
				{
					if (!It->IsDead() || !It->WasDeathNotified()) continue;
					++Dead;
					// Complete at the moment of the notification: what the client saw then is what it has now.
					if (!It->GetDeathVelocityAtNotify().Equals(It->GetDeathVelocity(), 0.01) || It->WasDeathHeadshotAtNotify() != It->IsDeathHeadshot()) TestRunner->AddError(TEXT("A client was told of a death before the death data had arrived"));
					if (It->GetDeathHitDirection().IsNearlyZero()) TestRunner->AddError(TEXT("A client has a death without the direction of the hit"));
					if (It->GetDeathVelocityAtNotify().Size2D() > 400.0) { ++Running; if (It->ShouldBuckleBeforeRagdoll()) TestRunner->AddError(TEXT("The client lets a runner at speed buckle")); }
					else if (It->WasDeathHeadshotAtNotify()) { ++Headshots; if (It->ShouldBuckleBeforeRagdoll()) TestRunner->AddError(TEXT("The client lets a head shot buckle")); }
					else if (It->ShouldBuckleBeforeRagdoll()) ++Buckling;
					if (It->WasRagdollRequested()) ++HandedOver;
				}
				if (Dead < 3 || HandedOver < 3) return false;
				if (Running != 1 || Buckling != 1 || Headshots != 1) TestRunner->AddError(FString::Printf(TEXT("The client has %d running, %d buckling and %d head shot deaths instead of one each"), Running, Buckling, Headshots));
				return true;
			}, FTimespan::FromSeconds(10.0));
	}

	// E12: real spawns and rescues of the largest enemy against floor, ceiling, wall and an occupied place.
	TEST_METHOD(BigEnemiesSpawnAndAreRescuedOnlyWhereTheirCapsuleFits)
	{
		Network
			.UntilServer(TEXT("The run has a host"), [](FArenaDuelPhase9State& State) { return State.World->GetFirstPlayerController() && State.World->GetAuthGameMode<AArenaDuelZombieGameMode>(); }, FTimespan::FromSeconds(15.0))
			.ThenServer(TEXT("Free place, low ceiling, wall, occupied place"), [this](FArenaDuelPhase9State& State)
			{
				AArenaDuelZombieGameMode* GameMode = State.World->GetAuthGameMode<AArenaDuelZombieGameMode>();
				const int32 BossIndex = static_cast<int32>(EArenaDuelZombieType::Boss);
				if (!GameMode->GetTypeConfigs().IsValidIndex(BossIndex)) { TestRunner->AddError(TEXT("No boss configuration")); return; }
				const float Scale = GameMode->GetTypeConfigs()[BossIndex].Scale;
				const UCapsuleComponent* Template = GetDefault<AArenaDuelZombie>()->GetCapsuleComponent();
				const float Radius = Template->GetUnscaledCapsuleRadius(), Half = Template->GetUnscaledCapsuleHalfHeight();
				if (Scale < 1.2f) { TestRunner->AddError(TEXT("The boss is not large enough for this check to mean anything")); return; }
				UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
				const auto Block = [&State, Cube](const FVector& Centre, const FVector& Size)
				{
					AStaticMeshActor* Actor = State.World->SpawnActor<AStaticMeshActor>(Centre, FRotator::ZeroRotator);
					Actor->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
					Actor->GetStaticMeshComponent()->SetStaticMesh(Cube);
					Actor->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));
					Actor->SetActorScale3D(Size / 100.0);
				};
				// Far from the players, a floor with its top at z = 0, and four places on it 1500 cm apart.
				const FVector Origin(30000.0f, 0.0f, 0.0f);
				Block(Origin + FVector(0.0f, 0.0f, -50.0f), FVector(8000.0f, 8000.0f, 100.0f));
				const auto Place = [&Origin](int32 Index) { return Origin + FVector(-2250.0f + 1500.0f * Index, 0.0f, 96.0f); };   // what PickSpawnLocation answers
				const FVector Free = Place(0), LowCeiling = Place(1), BesideWall = Place(2), Occupied = Place(3), FreeAgain = Origin + FVector(0.0f, 2500.0f, 96.0f);
				// A ceiling an ordinary zombie fits under and the boss does not.
				const float CeilingHeight = 2.0f * Half + 0.5f * (2.0f * Half * Scale - 2.0f * Half);
				Block(FVector(LowCeiling.X, LowCeiling.Y, CeilingHeight + 50.0f), FVector(600.0f, 600.0f, 100.0f));
				// A wall whose face is nearer than the boss is wide and farther than an ordinary zombie is.
				const float WallDistance = 0.5f * (Radius + Radius * Scale);
				Block(FVector(BesideWall.X + WallDistance + 50.0f, BesideWall.Y, 300.0f), FVector(100.0f, 600.0f, 600.0f));
				const auto FeetAboveFloor = [](const AArenaDuelZombie* Zombie) { return Zombie->GetActorLocation().Z - Zombie->GetCapsuleComponent()->GetScaledCapsuleHalfHeight(); };

				AArenaDuelZombie* Boss = GameMode->SpawnZombieAt(EArenaDuelZombieType::Boss, Free, FRotator::ZeroRotator);
				if (!Boss) { TestRunner->AddError(TEXT("The boss did not spawn on a free place")); return; }
				if (!FMath::IsNearlyEqual(Boss->GetCapsuleComponent()->GetScaledCapsuleHalfHeight(), Half * Scale, 1.0f)) TestRunner->AddError(TEXT("The spawned boss does not have its scaled capsule"));
				if (FeetAboveFloor(Boss) < 0.0 || FeetAboveFloor(Boss) > 8.0) TestRunner->AddError(FString::Printf(TEXT("The boss's feet are %.1f cm above the floor after the spawn"), FeetAboveFloor(Boss)));
				if (!FMath::IsNearlyEqual(Boss->GetActorLocation().X, Free.X, 5.0) || !FMath::IsNearlyEqual(Boss->GetActorLocation().Y, Free.Y, 5.0)) TestRunner->AddError(TEXT("The boss did not spawn at the place asked for"));

				if (GameMode->SpawnZombieAt(EArenaDuelZombieType::Boss, LowCeiling, FRotator::ZeroRotator)) TestRunner->AddError(TEXT("The boss spawned under a ceiling lower than itself"));
				if (GameMode->SpawnZombieAt(EArenaDuelZombieType::Boss, BesideWall, FRotator::ZeroRotator)) TestRunner->AddError(TEXT("The boss spawned with its side in a wall"));
				AArenaDuelZombie* Sitter = GameMode->SpawnZombieAt(EArenaDuelZombieType::Normal, Occupied, FRotator::ZeroRotator);
				if (!Sitter) { TestRunner->AddError(TEXT("An ordinary zombie did not spawn on a free place")); return; }
				if (GameMode->SpawnZombieAt(EArenaDuelZombieType::Boss, Occupied, FRotator::ZeroRotator)) TestRunner->AddError(TEXT("The boss spawned on a place another enemy stands on"));
				// It is the scaled size that decides: the same two tight places take an ordinary zombie.
				AArenaDuelZombie* UnderCeiling = GameMode->SpawnZombieAt(EArenaDuelZombieType::Normal, LowCeiling, FRotator::ZeroRotator);
				AArenaDuelZombie* AtWall = GameMode->SpawnZombieAt(EArenaDuelZombieType::Normal, BesideWall, FRotator::ZeroRotator);
				if (!UnderCeiling || !AtWall) TestRunner->AddError(TEXT("An ordinary zombie was refused where only the boss is too large"));
				if (UnderCeiling) UnderCeiling->Destroy();
				if (AtWall) AtWall->Destroy();

				// Rescue: refused where the boss does not fit, and a refusal moves nothing and counts nothing.
				const FVector Before = Boss->GetActorLocation();
				const int32 Done = GameMode->GetRescuesDone(), Refused = GameMode->GetRescuesRefused();
				if (GameMode->RescueZombieTo(Boss, LowCeiling, FRotator::ZeroRotator)) TestRunner->AddError(TEXT("The boss was rescued to under a low ceiling"));
				if (GameMode->RescueZombieTo(Boss, BesideWall, FRotator::ZeroRotator)) TestRunner->AddError(TEXT("The boss was rescued into a wall"));
				if (GameMode->RescueZombieTo(Boss, Occupied, FRotator::ZeroRotator)) TestRunner->AddError(TEXT("The boss was rescued onto another enemy"));
				if (!Boss->GetActorLocation().Equals(Before, 0.5)) TestRunner->AddError(TEXT("A refused rescue moved the boss"));
				if (GameMode->GetRescuesDone() != Done || GameMode->GetRescuesRefused() != Refused + 3) TestRunner->AddError(TEXT("Refused rescues were counted as done, or not counted as refused"));
				if (!GameMode->RescueZombieTo(Boss, FreeAgain, FRotator::ZeroRotator)) { TestRunner->AddError(TEXT("The boss was not rescued to a free place")); return; }
				if (GameMode->GetRescuesDone() != Done + 1) TestRunner->AddError(TEXT("A rescue that worked was not counted"));
				if (!FMath::IsNearlyEqual(Boss->GetActorLocation().X, FreeAgain.X, 5.0) || !FMath::IsNearlyEqual(Boss->GetActorLocation().Y, FreeAgain.Y, 5.0)) TestRunner->AddError(TEXT("The rescued boss is not where it was sent"));
				if (FeetAboveFloor(Boss) < 0.0 || FeetAboveFloor(Boss) > 8.0) TestRunner->AddError(FString::Printf(TEXT("The boss's feet are %.1f cm above the floor after the rescue"), FeetAboveFloor(Boss)));
				if (Boss->GetSecondsWithoutProgress() > 0.01f) TestRunner->AddError(TEXT("A rescue did not give the boss a fresh start"));
			});
	}
};

#endif
