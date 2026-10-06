// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "CQTest.h"
#include "Components/PIENetworkComponent.h"
#include "Misc/AutomationTest.h"
#include "ArenaDuel/Abilities/ArenaDuelGameplayAbility.h"
#include "ArenaDuel/Abilities/ArenaDuelGameplayTags.h"
#include "ArenaDuel/Abilities/ArenaDuelGA_ShadowStep.h"
#include "ArenaDuel/Abilities/ArenaDuelGA_VeilWall.h"
#include "ArenaDuel/Abilities/ArenaDuelGA_ArcBarrier.h"
#include "ArenaDuel/Abilities/ArenaDuelGA_BurstLeap.h"
#include "ArenaDuel/Abilities/ArenaDuelArcBarrier.h"
#include "ArenaDuel/Abilities/ArenaDuelShadowCooldownEffects.h"
#include "ArenaDuel/Abilities/ArenaDuelVeilWall.h"
#include "ArenaDuel/Player/ArenaDuelPlayerState.h"
#include "ArenaDuel/Characters/ArenaDuelCharacter.h"
#include "ArenaDuel/Game/ArenaDuelGameMode.h"
#include "ArenaDuel/Game/ArenaDuelGameState.h"
#include "ArenaDuel/Weapons/ArenaDuelWeaponComponent.h"
#include "ArenaDuel/UI/ArenaDuelAdminTypes.h"
#include "ArenaDuel/Player/ArenaDuelPlayerController.h"
#include "AbilitySystemComponent.h"
#include "Components/PrimitiveComponent.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"
#include "EngineUtils.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/GameState.h"
#include "GameFramework/PlayerController.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FArenaDuelPhase7ShadowAbilityContractTest, "ArenaDuel.Phase7.ShadowAbilityContract", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FArenaDuelPhase7ShadowAbilityContractTest::RunTest(const FString& Parameters)
{
	const UArenaDuelGA_ShadowStep* ShadowStep = GetDefault<UArenaDuelGA_ShadowStep>();
	const UArenaDuelGA_VeilWall* VeilWallAbility = GetDefault<UArenaDuelGA_VeilWall>();
	TestTrue(TEXT("Shadow Step uses predicted GAS execution"), ShadowStep && ShadowStep->GetNetExecutionPolicy() == EGameplayAbilityNetExecutionPolicy::LocalPredicted);
	TestTrue(TEXT("Veil Wall uses predicted GAS execution"), VeilWallAbility && VeilWallAbility->GetNetExecutionPolicy() == EGameplayAbilityNetExecutionPolicy::LocalPredicted);
	TestTrue(TEXT("Shadow Step exposes its native ability and cooldown tags"), ShadowStep && ShadowStep->GetAssetTags().HasTagExact(TAG_Ability_Shadow_ShadowStep.GetTag()) && ShadowStep->GetCooldownTags()->HasTagExact(TAG_Cooldown_Shadow_ShadowStep.GetTag()));
	TestTrue(TEXT("Veil Wall exposes its native ability and cooldown tags"), VeilWallAbility && VeilWallAbility->GetAssetTags().HasTagExact(TAG_Ability_Shadow_VeilWall.GetTag()) && VeilWallAbility->GetCooldownTags()->HasTagExact(TAG_Cooldown_Shadow_VeilWall.GetTag()));
	const UArenaDuelGA_ArcBarrier* ArcBarrier = GetDefault<UArenaDuelGA_ArcBarrier>();
	const UArenaDuelGA_BurstLeap* BurstLeap = GetDefault<UArenaDuelGA_BurstLeap>();
	TestTrue(TEXT("Warden abilities use predicted GAS execution"), ArcBarrier && ArcBarrier->GetNetExecutionPolicy() == EGameplayAbilityNetExecutionPolicy::LocalPredicted && BurstLeap && BurstLeap->GetNetExecutionPolicy() == EGameplayAbilityNetExecutionPolicy::LocalPredicted);
	TestTrue(TEXT("Warden ability and cooldown tags are registered"), ArcBarrier && ArcBarrier->GetAssetTags().HasTagExact(TAG_Ability_Warden_ArcBarrier.GetTag()) && ArcBarrier->GetCooldownTags()->HasTagExact(TAG_Cooldown_Warden_ArcBarrier.GetTag()) && BurstLeap && BurstLeap->GetAssetTags().HasTagExact(TAG_Ability_Warden_BurstLeap.GetTag()) && BurstLeap->GetCooldownTags()->HasTagExact(TAG_Cooldown_Warden_BurstLeap.GetTag()));

	const UGameplayEffect* StepCooldown = GetDefault<UArenaDuelGE_ShadowStepCooldown>();
	const UGameplayEffect* WallCooldown = GetDefault<UArenaDuelGE_VeilWallCooldown>();
	const UGameplayEffect* BarrierCooldown = GetDefault<UArenaDuelGE_ArcBarrierCooldown>();
	const UGameplayEffect* LeapCooldown = GetDefault<UArenaDuelGE_BurstLeapCooldown>();
	float StepDuration = 0.0f;
	float WallDuration = 0.0f;
	float BarrierDuration = 0.0f;
	float LeapDuration = 0.0f;
	const bool bHasStepDuration = StepCooldown && StepCooldown->DurationMagnitude.GetStaticMagnitudeIfPossible(1.0f, StepDuration);
	const bool bHasWallDuration = WallCooldown && WallCooldown->DurationMagnitude.GetStaticMagnitudeIfPossible(1.0f, WallDuration);
	const bool bHasBarrierDuration = BarrierCooldown && BarrierCooldown->DurationMagnitude.GetStaticMagnitudeIfPossible(1.0f, BarrierDuration);
	const bool bHasLeapDuration = LeapCooldown && LeapCooldown->DurationMagnitude.GetStaticMagnitudeIfPossible(1.0f, LeapDuration);
	TestTrue(TEXT("Shadow Step cooldown is a five second duration effect"), StepCooldown && StepCooldown->DurationPolicy == EGameplayEffectDurationType::HasDuration && bHasStepDuration && FMath::IsNearlyEqual(StepDuration, 5.0f));
	TestTrue(TEXT("Veil Wall cooldown is a twelve second duration effect"), WallCooldown && WallCooldown->DurationPolicy == EGameplayEffectDurationType::HasDuration && bHasWallDuration && FMath::IsNearlyEqual(WallDuration, 12.0f));
	TestTrue(TEXT("Arc Barrier cooldown is a fourteen second duration effect"), BarrierCooldown && BarrierCooldown->DurationPolicy == EGameplayEffectDurationType::HasDuration && bHasBarrierDuration && FMath::IsNearlyEqual(BarrierDuration, 14.0f));
	TestTrue(TEXT("Burst Leap cooldown is a seven second duration effect"), LeapCooldown && LeapCooldown->DurationPolicy == EGameplayEffectDurationType::HasDuration && bHasLeapDuration && FMath::IsNearlyEqual(LeapDuration, 7.0f));
	TestTrue(TEXT("Cooldown effects grant the corresponding owned tags"), StepCooldown && StepCooldown->GetGrantedTags().HasTagExact(TAG_Cooldown_Shadow_ShadowStep.GetTag()) && WallCooldown && WallCooldown->GetGrantedTags().HasTagExact(TAG_Cooldown_Shadow_VeilWall.GetTag()));

	AArenaDuelPlayerState* PlayerState = NewObject<AArenaDuelPlayerState>(GetTransientPackage());
	TestNotNull(TEXT("PlayerState can be created for persistent ability grant check"), PlayerState);
	if (PlayerState)
	{
		TestTrue(TEXT("Default archetype is Shadow"), PlayerState->GetCharacterArchetype() == EArenaDuelCharacterArchetype::Shadow);
		PlayerState->GrantCharacterAbilities();
		PlayerState->GrantCharacterAbilities();
		UAbilitySystemComponent* ASC = PlayerState->GetAbilitySystemComponent();
		TestTrue(TEXT("PlayerState ASC has exactly one Shadow Step spec"), ASC && ASC->FindAbilitySpecFromClass(UArenaDuelGA_ShadowStep::StaticClass()));
		TestTrue(TEXT("PlayerState ASC has exactly one Veil Wall spec"), ASC && ASC->FindAbilitySpecFromClass(UArenaDuelGA_VeilWall::StaticClass()));
		if (ASC)
		{
			int32 ShadowSpecCount = 0;
			int32 WallSpecCount = 0;
			for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
			{
				if (Spec.Ability && Spec.Ability->GetClass() == UArenaDuelGA_ShadowStep::StaticClass()) ++ShadowSpecCount;
				if (Spec.Ability && Spec.Ability->GetClass() == UArenaDuelGA_VeilWall::StaticClass()) ++WallSpecCount;
			}
			TestEqual(TEXT("Repeated grant does not duplicate Shadow Step"), ShadowSpecCount, 1);
			TestEqual(TEXT("Repeated grant does not duplicate Veil Wall"), WallSpecCount, 1);
		}
	}

	const AArenaDuelVeilWall* Wall = GetDefault<AArenaDuelVeilWall>();
	TestTrue(TEXT("Veil Wall replicates and has the requested temporary lifetime"), Wall && Wall->GetIsReplicated() && FMath::IsNearlyEqual(Wall->InitialLifeSpan, 3.0f));
	TestTrue(TEXT("Veil Wall development dimensions are approximately 450 wide by 250 high"), Wall && FMath::IsNearlyEqual(Wall->GetVisualScale().Y * 100.0f, 450.0f) && FMath::IsNearlyEqual(Wall->GetVisualScale().Z * 100.0f, 250.0f));
	if (Wall)
	{
		TArray<UPrimitiveComponent*> Primitives;
		Wall->GetComponents<UPrimitiveComponent>(Primitives);
		TestTrue(TEXT("Veil Wall contains a visual primitive"), Primitives.Num() > 0);
		for (const UPrimitiveComponent* Primitive : Primitives)
		{
			TestEqual(TEXT("Veil Wall visuals have no movement or hitscan collision"), Primitive->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
		}
	}
	const AArenaDuelArcBarrier* Barrier = GetDefault<AArenaDuelArcBarrier>();
	TestTrue(TEXT("Arc Barrier replicates and has a five second lifetime"), Barrier && Barrier->GetIsReplicated() && FMath::IsNearlyEqual(Barrier->InitialLifeSpan, 5.0f));
	TestTrue(TEXT("Arc Barrier development dimensions are 425 by 245 units"), Barrier && Barrier->GetBarrierHalfExtents().Equals(FVector(12.5f, 212.5f, 122.5f)));
	if (Barrier)
	{
		const UPrimitiveComponent* Collision = Cast<UPrimitiveComponent>(Barrier->GetRootComponent());
		TestTrue(TEXT("Arc Barrier physically blocks Pawn movement"), Collision && Collision->GetCollisionEnabled() == ECollisionEnabled::QueryAndPhysics && Collision->GetCollisionResponseToChannel(ECC_Pawn) == ECR_Block);
		TestTrue(TEXT("Arc Barrier blocks current Visibility hitscan"), Collision && Collision->GetCollisionResponseToChannel(ECC_Visibility) == ECR_Block);
		const UStaticMeshComponent* Visual = Barrier->FindComponentByClass<UStaticMeshComponent>();
		TestTrue(TEXT("Arc Barrier visual does not duplicate collision"), Visual && Visual->GetCollisionEnabled() == ECollisionEnabled::NoCollision);
	}
	return true;
}

struct FArenaDuelPhase7NetworkState : public FBasePIENetworkComponentState
{
	FVector InitialServerLocations[2] = { FVector::ZeroVector, FVector::ZeroVector };
	bool bInitialServerLocationCaptured[2] = { false, false };
};

namespace ArenaDuelPhase7NetworkTests
{
	static constexpr TCHAR GameModeClassPath[] = TEXT("/Game/ArenaDuel/Game/BP_ArenaDuelGameMode.BP_ArenaDuelGameMode_C");

	static int32 CountAbilitySpecs(const AArenaDuelPlayerState* PlayerState, UClass* AbilityClass)
	{
		const UAbilitySystemComponent* ASC = PlayerState ? PlayerState->GetAbilitySystemComponent() : nullptr;
		if (!ASC || !AbilityClass) return 0;
		int32 Count = 0;
		for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
		{
			if (Spec.Ability && Spec.Ability->GetClass() == AbilityClass) ++Count;
		}
		return Count;
	}

	static AArenaDuelCharacter* FindRemoteCharacter(UWorld* World)
	{
		if (!World) return nullptr;
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			APlayerController* Controller = It->Get();
			if (Controller && !Controller->IsLocalController()) return Cast<AArenaDuelCharacter>(Controller->GetPawn());
		}
		return nullptr;
	}

	static int32 CountWalls(UWorld* World)
	{
		int32 Count = 0;
		for (TActorIterator<AArenaDuelVeilWall> It(World); It; ++It) ++Count;
		return Count;
	}
}

NETWORK_TEST_CLASS(FArenaDuelPhase7WardenNetworkSmokeTest, "ArenaDuel.Phase7.WardenNetworkSmoke")
{
	FPIENetworkComponent<FArenaDuelPhase7NetworkState> Network{TestRunner, TestCommandBuilder, bInitializing};

	BEFORE_EACH()
	{
		UClass* GameModeClass = LoadClass<AGameModeBase>(nullptr, ArenaDuelPhase7NetworkTests::GameModeClassPath);
		FNetworkComponentBuilder<FArenaDuelPhase7NetworkState>().WithClients(1).AsListenServer().WithGameMode(GameModeClass).Build(Network);
	}

	TEST_METHOD(WardenKitBarrierLeapAndPersistentArchetype)
	{
		Network
			.UntilServer(TEXT("Assign Warden kit to Player 2 while Player 1 remains Shadow"), [](FArenaDuelPhase7NetworkState& State)
			{
				AArenaDuelGameMode* GameMode = State.World->GetAuthGameMode<AArenaDuelGameMode>();
				AArenaDuelPlayerState* P1 = GameMode ? GameMode->FindPlayerStateByDuelSlot(0) : nullptr;
				AArenaDuelPlayerState* P2 = GameMode ? GameMode->FindPlayerStateByDuelSlot(1) : nullptr;
				if (!P1 || !P2) return false;
				if (State.World->GetGameState<AArenaDuelGameState>()->IsCharacterSelectVisible()) GameMode->AdminRestartRound();
				P2->SetCharacterArchetypeForDevelopment(EArenaDuelCharacterArchetype::Warden);
				return P1->GetCharacterArchetype() == EArenaDuelCharacterArchetype::Shadow && P2->GetCharacterArchetype() == EArenaDuelCharacterArchetype::Warden;
			}, FTimespan::FromSeconds(10.0))
			.UntilClient(TEXT("Owning client observes Warden archetype and exactly the Warden kit"), 0, [](FArenaDuelPhase7NetworkState& State)
			{
				for (TActorIterator<AArenaDuelCharacter> It(State.World); It; ++It)
				{
					if (!It->IsLocallyControlled()) continue;
					AArenaDuelPlayerState* PS = It->GetPlayerState<AArenaDuelPlayerState>();
					return PS && PS->GetCharacterArchetype() == EArenaDuelCharacterArchetype::Warden
						&& ArenaDuelPhase7NetworkTests::CountAbilitySpecs(PS, UArenaDuelGA_ArcBarrier::StaticClass()) == 1
						&& ArenaDuelPhase7NetworkTests::CountAbilitySpecs(PS, UArenaDuelGA_BurstLeap::StaticClass()) == 1
						&& ArenaDuelPhase7NetworkTests::CountAbilitySpecs(PS, UArenaDuelGA_ShadowStep::StaticClass()) == 0
						&& ArenaDuelPhase7NetworkTests::CountAbilitySpecs(PS, UArenaDuelGA_VeilWall::StaticClass()) == 0;
				}
				return false;
			}, FTimespan::FromSeconds(10.0))
			.ThenClient(TEXT("Client activates predicted Arc Barrier"), 0, [this](FArenaDuelPhase7NetworkState& State)
			{
				for (TActorIterator<AArenaDuelCharacter> It(State.World); It; ++It)
				{
					if (!It->IsLocallyControlled()) continue;
					AArenaDuelPlayerState* PS = It->GetPlayerState<AArenaDuelPlayerState>();
					if (AArenaDuelPlayerController* PC = Cast<AArenaDuelPlayerController>(It->GetController()))
					{
						PC->ServerExecuteAdminCommand(EArenaDuelAdminCommand::SetArchetypeShadow, PS ? PS->GetDuelSlot() : 1, 0.0f);
					}
					if (!PS || !PS->TryActivatePrimaryAbility()) TestRunner->AddError(TEXT("Generic primary slot did not activate Warden Arc Barrier"));
					else if (PS->TryActivatePrimaryAbility()) TestRunner->AddError(TEXT("Arc Barrier cooldown did not reject an immediate second activation"));
					return;
				}
				TestRunner->AddError(TEXT("No locally controlled Warden Character on client"));
			})
			.UntilServer(TEXT("Server confirms replicated physical barrier and authoritative hitscan damage"), [](FArenaDuelPhase7NetworkState& State)
			{
				AArenaDuelCharacter* Warden = ArenaDuelPhase7NetworkTests::FindRemoteCharacter(State.World);
				AArenaDuelPlayerState* PS = Warden ? Warden->GetPlayerState<AArenaDuelPlayerState>() : nullptr;
				AArenaDuelArcBarrier* Barrier = nullptr;
				for (TActorIterator<AArenaDuelArcBarrier> It(State.World); It; ++It) { Barrier = *It; break; }
				if (!Warden || !PS || !Barrier || Barrier->GetOwner() != PS || PS->GetPrimaryAbilityCooldownRemaining() < 13.0f) return false;
				if (Barrier->GetBarrierHealth() >= 350.0f)
				{
					if (UArenaDuelWeaponComponent* Weapon = Warden->GetWeaponComponent()) Weapon->StartFire();
					return false;
				}
				if (Warden->GetHealth() < 99.9f) return false;
				for (FConstPlayerControllerIterator It = State.World->GetPlayerControllerIterator(); It; ++It)
				{
					const APlayerController* OtherController = It->Get();
					const AArenaDuelPlayerState* OtherState = OtherController ? OtherController->GetPlayerState<AArenaDuelPlayerState>() : nullptr;
					const AArenaDuelCharacter* OtherCharacter = OtherController ? Cast<AArenaDuelCharacter>(OtherController->GetPawn()) : nullptr;
					if (OtherState && OtherState != PS && OtherCharacter && OtherCharacter->GetHealth() < 99.9f) return false;
				}
				if (UArenaDuelWeaponComponent* Weapon = Warden->GetWeaponComponent()) Weapon->StopFire();
				return Barrier->GetBarrierHealth() < 350.0f;
			}, FTimespan::FromSeconds(5.0))
			.UntilClient(TEXT("Arc Barrier replicates to the owning client with matching owner and blocking channels"), 0, [](FArenaDuelPhase7NetworkState& State)
			{
				AArenaDuelCharacter* LocalCharacter = nullptr;
				for (TActorIterator<AArenaDuelCharacter> It(State.World); It; ++It) if (It->IsLocallyControlled()) { LocalCharacter = *It; break; }
				AArenaDuelPlayerState* PS = LocalCharacter ? LocalCharacter->GetPlayerState<AArenaDuelPlayerState>() : nullptr;
				for (TActorIterator<AArenaDuelArcBarrier> It(State.World); It; ++It)
				{
					const UPrimitiveComponent* Collision = Cast<UPrimitiveComponent>(It->GetRootComponent());
					return PS && It->GetOwner() == PS && Collision
						&& Collision->GetCollisionResponseToChannel(ECC_Pawn) == ECR_Block
						&& Collision->GetCollisionResponseToChannel(ECC_Visibility) == ECR_Block;
				}
				return false;
			}, FTimespan::FromSeconds(5.0))
			.ThenClient(TEXT("Client activates predicted Warden Burst Leap with cooldown"), 0, [this](FArenaDuelPhase7NetworkState& State)
			{
				for (TActorIterator<AArenaDuelCharacter> It(State.World); It; ++It)
				{
					if (!It->IsLocallyControlled()) continue;
					AArenaDuelPlayerState* PS = It->GetPlayerState<AArenaDuelPlayerState>();
					if (!PS || !PS->TryActivateSecondaryAbility()) TestRunner->AddError(TEXT("Generic secondary slot did not activate Warden Burst Leap"));
					else if (PS->TryActivateSecondaryAbility()) TestRunner->AddError(TEXT("Burst Leap cooldown did not reject an immediate second activation"));
					return;
				}
				TestRunner->AddError(TEXT("No locally controlled Warden Character for Burst Leap"));
			})
			.UntilServer(TEXT("Server observes Burst Leap movement and cooldown without damage"), [](FArenaDuelPhase7NetworkState& State)
			{
				AArenaDuelCharacter* Warden = ArenaDuelPhase7NetworkTests::FindRemoteCharacter(State.World);
				AArenaDuelPlayerState* PS = Warden ? Warden->GetPlayerState<AArenaDuelPlayerState>() : nullptr;
				return Warden && PS && PS->GetSecondaryAbilityCooldownRemaining() > 6.0f && Warden->GetVelocity().Z > 100.0f && Warden->GetHealth() >= 99.9f;
			}, FTimespan::FromSeconds(3.0))
			.ThenServer(TEXT("Archetype and Warden kit survive a round reset without duplicates and with ready cooldowns"), [](FArenaDuelPhase7NetworkState& State)
			{
				AArenaDuelCharacter* Warden = ArenaDuelPhase7NetworkTests::FindRemoteCharacter(State.World);
				AArenaDuelPlayerState* PS = Warden ? Warden->GetPlayerState<AArenaDuelPlayerState>() : nullptr;
				AArenaDuelGameMode* GameMode = State.World->GetAuthGameMode<AArenaDuelGameMode>();
				if (GameMode) GameMode->AdminRestartRound();
				if (!PS || PS->GetCharacterArchetype() != EArenaDuelCharacterArchetype::Warden) return;
				PS->ResetAbilitiesForNewRound();
			})
			.UntilServer(TEXT("Warden specs remain unique after respawn"), [](FArenaDuelPhase7NetworkState& State)
			{
				AArenaDuelCharacter* Warden = ArenaDuelPhase7NetworkTests::FindRemoteCharacter(State.World);
				AArenaDuelPlayerState* PS = Warden ? Warden->GetPlayerState<AArenaDuelPlayerState>() : nullptr;
				return PS && PS->GetCharacterArchetype() == EArenaDuelCharacterArchetype::Warden
					&& ArenaDuelPhase7NetworkTests::CountAbilitySpecs(PS, UArenaDuelGA_ArcBarrier::StaticClass()) == 1
					&& ArenaDuelPhase7NetworkTests::CountAbilitySpecs(PS, UArenaDuelGA_BurstLeap::StaticClass()) == 1
					&& ArenaDuelPhase7NetworkTests::CountAbilitySpecs(PS, UArenaDuelGA_ShadowStep::StaticClass()) == 0
					&& ArenaDuelPhase7NetworkTests::CountAbilitySpecs(PS, UArenaDuelGA_VeilWall::StaticClass()) == 0
					&& PS->GetPrimaryAbilityCooldownRemaining() <= KINDA_SMALL_NUMBER
					&& PS->GetSecondaryAbilityCooldownRemaining() <= KINDA_SMALL_NUMBER;
			}, FTimespan::FromSeconds(5.0))
			.ThenServer(TEXT("Fresh match reset preserves Warden, then Warden to Shadow kit replacement removes old specs"), [](FArenaDuelPhase7NetworkState& State)
			{
				AArenaDuelGameMode* GameMode = State.World->GetAuthGameMode<AArenaDuelGameMode>();
				AArenaDuelPlayerState* PS = GameMode ? GameMode->FindPlayerStateByDuelSlot(1) : nullptr;
				if (!PS || !GameMode) return;
				GameMode->AdminResetMatch();
				if (PS->GetCharacterArchetype() != EArenaDuelCharacterArchetype::Warden)
				{
					TestRunner->AddError(TEXT("Fresh match reset unexpectedly changed Warden archetype"));
					return;
				}
				PS->SetCharacterArchetypeForDevelopment(EArenaDuelCharacterArchetype::Shadow);
			})
			.UntilServer(TEXT("Switching back to Shadow leaves only the Shadow kit"), [](FArenaDuelPhase7NetworkState& State)
			{
				AArenaDuelGameMode* GameMode = State.World->GetAuthGameMode<AArenaDuelGameMode>();
				AArenaDuelPlayerState* PS = GameMode ? GameMode->FindPlayerStateByDuelSlot(1) : nullptr;
				return PS && PS->GetCharacterArchetype() == EArenaDuelCharacterArchetype::Shadow
					&& ArenaDuelPhase7NetworkTests::CountAbilitySpecs(PS, UArenaDuelGA_ShadowStep::StaticClass()) == 1
					&& ArenaDuelPhase7NetworkTests::CountAbilitySpecs(PS, UArenaDuelGA_VeilWall::StaticClass()) == 1
					&& ArenaDuelPhase7NetworkTests::CountAbilitySpecs(PS, UArenaDuelGA_ArcBarrier::StaticClass()) == 0
					&& ArenaDuelPhase7NetworkTests::CountAbilitySpecs(PS, UArenaDuelGA_BurstLeap::StaticClass()) == 0;
			}, FTimespan::FromSeconds(5.0));
	}
};

NETWORK_TEST_CLASS(FArenaDuelPhase7ShadowNetworkSmokeTest, "ArenaDuel.Phase7.ShadowNetwork")
{
	FPIENetworkComponent<FArenaDuelPhase7NetworkState> Network{TestRunner, TestCommandBuilder, bInitializing};

	BEFORE_EACH()
	{
		UClass* GameModeClass = LoadClass<AGameModeBase>(nullptr, ArenaDuelPhase7NetworkTests::GameModeClassPath);
		FNetworkComponentBuilder<FArenaDuelPhase7NetworkState>()
			.WithClients(1)
			.AsListenServer()
			.WithGameMode(GameModeClass)
			.Build(Network);
	}

	TEST_METHOD(ClientAbilitiesArePredictedServerValidatedAndResetForRound)
	{
		Network
			.UntilServer(TEXT("Wait for both server PlayerStates with exactly one Shadow ability spec each"), [](FArenaDuelPhase7NetworkState& State)
			{
				if (State.World->GetGameState<AArenaDuelGameState>()->IsCharacterSelectVisible()) State.World->GetAuthGameMode<AArenaDuelGameMode>()->AdminRestartRound();
				int32 Count = 0;
				for (APlayerState* BaseState : State.World->GetGameState()->PlayerArray)
				{
					AArenaDuelPlayerState* PlayerState = Cast<AArenaDuelPlayerState>(BaseState);
					if (!PlayerState) continue;
					++Count;
					const uint8 Slot = PlayerState->GetDuelSlot();
					if (Slot < 2)
					{
						if (!State.bInitialServerLocationCaptured[Slot])
						{
							if (AArenaDuelCharacter* Character = Cast<AArenaDuelCharacter>(PlayerState->GetPawn()))
							{
								State.InitialServerLocations[Slot] = Character->GetActorLocation();
								State.bInitialServerLocationCaptured[Slot] = true;
							}
						}
					}
					if (ArenaDuelPhase7NetworkTests::CountAbilitySpecs(PlayerState, UArenaDuelGA_ShadowStep::StaticClass()) != 1
						|| ArenaDuelPhase7NetworkTests::CountAbilitySpecs(PlayerState, UArenaDuelGA_VeilWall::StaticClass()) != 1) return false;
				}
				return Count == 2;
			}, FTimespan::FromSeconds(10.0))
			.ThenClient(TEXT("Owning client activates Shadow Step once and immediate reuse is rejected"), 0, [this](FArenaDuelPhase7NetworkState& State)
			{
				AArenaDuelCharacter* LocalCharacter = nullptr;
				for (TActorIterator<AArenaDuelCharacter> It(State.World); It; ++It) if (It->IsLocallyControlled()) { LocalCharacter = *It; break; }
				if (!LocalCharacter || !LocalCharacter->GetAbilitySystemComponent())
				{
					TestRunner->AddError(TEXT("Client could not resolve its controlled Character/ASC"));
					return;
				}
				AArenaDuelPlayerState* PlayerState = LocalCharacter->GetPlayerState<AArenaDuelPlayerState>();
				if (!PlayerState) TestRunner->AddError(TEXT("Client PlayerState was not available for the controlled Character"));
				if (!LocalCharacter->GetAbilitySystemComponent()->TryActivateAbilityByClass(UArenaDuelGA_ShadowStep::StaticClass()))
				{
					TestRunner->AddError(TEXT("Owning client could not activate Shadow Step"));
					return;
				}
				if (LocalCharacter->GetAbilitySystemComponent()->TryActivateAbilityByClass(UArenaDuelGA_ShadowStep::StaticClass()))
				{
					TestRunner->AddError(TEXT("Immediate second Shadow Step was not rejected by its GAS cooldown"));
				}
			})
			.UntilServer(TEXT("Server observes predicted dash movement and authoritative cooldown"), [](FArenaDuelPhase7NetworkState& State)
			{
				AArenaDuelCharacter* Character = ArenaDuelPhase7NetworkTests::FindRemoteCharacter(State.World);
				AArenaDuelPlayerState* PlayerState = Character ? Character->GetPlayerState<AArenaDuelPlayerState>() : nullptr;
				return Character && PlayerState->GetShadowStepCooldownRemaining() > 4.0f
					&& PlayerState->GetDuelSlot() < 2
					&& State.bInitialServerLocationCaptured[PlayerState->GetDuelSlot()]
					&& FVector::Dist2D(Character->GetActorLocation(), State.InitialServerLocations[PlayerState->GetDuelSlot()]) > 40.0f;
			}, FTimespan::FromSeconds(4.0))
			.ThenClient(TEXT("Owning client activates Veil Wall and immediate reuse is rejected"), 0, [this](FArenaDuelPhase7NetworkState& State)
			{
				for (TActorIterator<AArenaDuelCharacter> It(State.World); It; ++It)
				{
					if (!It->IsLocallyControlled()) continue;
					UAbilitySystemComponent* ASC = It->GetAbilitySystemComponent();
					if (!ASC || !ASC->TryActivateAbilityByClass(UArenaDuelGA_VeilWall::StaticClass()))
					{
						TestRunner->AddError(TEXT("Owning client could not activate Veil Wall"));
						return;
					}
					if (ASC->TryActivateAbilityByClass(UArenaDuelGA_VeilWall::StaticClass())) TestRunner->AddError(TEXT("Immediate second Veil Wall was not rejected by its GAS cooldown"));
					return;
				}
				TestRunner->AddError(TEXT("Client has no locally controlled Character for Veil Wall"));
			})
			.UntilServer(TEXT("Server spawns one replicated collisionless Veil Wall"), [](FArenaDuelPhase7NetworkState& State)
			{
				if (ArenaDuelPhase7NetworkTests::CountWalls(State.World) != 1) return false;
				for (TActorIterator<AArenaDuelVeilWall> It(State.World); It; ++It)
				{
					TArray<UPrimitiveComponent*> Primitives;
					It->GetComponents<UPrimitiveComponent>(Primitives);
					if (Primitives.IsEmpty()) return false;
					for (const UPrimitiveComponent* Primitive : Primitives) if (Primitive->GetCollisionEnabled() != ECollisionEnabled::NoCollision) return false;
				}
				AArenaDuelCharacter* OwnerCharacter = ArenaDuelPhase7NetworkTests::FindRemoteCharacter(State.World);
				AArenaDuelPlayerState* OwnerState = OwnerCharacter ? OwnerCharacter->GetPlayerState<AArenaDuelPlayerState>() : nullptr;
				if (!OwnerState || OwnerState->GetVeilWallCooldownRemaining() <= 11.0f) return false;
				for (TActorIterator<AArenaDuelCharacter> It(State.World); It; ++It) if (It->GetHealth() < 99.9f) return false;
				return true;
			}, FTimespan::FromSeconds(4.0))
			.UntilClient(TEXT("Owning client receives the visual-only replicated wall"), 0, [](FArenaDuelPhase7NetworkState& State)
			{
				if (ArenaDuelPhase7NetworkTests::CountWalls(State.World) != 1) return false;
				for (TActorIterator<AArenaDuelVeilWall> It(State.World); It; ++It)
				{
					TArray<UPrimitiveComponent*> Primitives;
					It->GetComponents<UPrimitiveComponent>(Primitives);
					if (Primitives.IsEmpty()) return false;
					for (const UPrimitiveComponent* Primitive : Primitives) if (Primitive->GetCollisionEnabled() != ECollisionEnabled::NoCollision) return false;
				}
				return true;
			}, FTimespan::FromSeconds(4.0))
			.UntilServer(TEXT("Temporary wall expires"), [](FArenaDuelPhase7NetworkState& State)
			{
				return ArenaDuelPhase7NetworkTests::CountWalls(State.World) == 0;
			}, FTimespan::FromSeconds(5.0))
			.UntilClient(TEXT("Replicated temporary wall expires on owning client"), 0, [](FArenaDuelPhase7NetworkState& State)
			{
				return ArenaDuelPhase7NetworkTests::CountWalls(State.World) == 0;
			}, FTimespan::FromSeconds(5.0))
			.ThenServer(TEXT("Enter authoritative round break and clear cooldowns for gate validation"), [this](FArenaDuelPhase7NetworkState& State)
			{
				AArenaDuelCharacter* RemoteCharacter = ArenaDuelPhase7NetworkTests::FindRemoteCharacter(State.World);
				AArenaDuelPlayerState* RemoteState = RemoteCharacter ? RemoteCharacter->GetPlayerState<AArenaDuelPlayerState>() : nullptr;
				AArenaDuelGameMode* GameMode = State.World->GetAuthGameMode<AArenaDuelGameMode>();
				if (!RemoteState || !GameMode)
				{
					TestRunner->AddError(TEXT("Unable to start round-break ability rejection check"));
					return;
				}
				GameMode->AdminAwardRound(RemoteState->GetDuelSlot() == 0 ? 1 : 0);
				RemoteState->ResetAbilitiesForNewRound();
				const AArenaDuelGameState* GameState = State.World->GetGameState<AArenaDuelGameState>();
				if (!GameState || GameState->IsRoundInProgress()) TestRunner->AddError(TEXT("GameMode did not enter the authoritative round-break state"));
			})
			.ThenServer(TEXT("Round-break server rejects Q and E without applying gameplay effects"), [this](FArenaDuelPhase7NetworkState& State)
			{
				AArenaDuelCharacter* RemoteCharacter = ArenaDuelPhase7NetworkTests::FindRemoteCharacter(State.World);
				AArenaDuelPlayerState* RemoteState = RemoteCharacter ? RemoteCharacter->GetPlayerState<AArenaDuelPlayerState>() : nullptr;
				UAbilitySystemComponent* ASC = RemoteCharacter ? RemoteCharacter->GetAbilitySystemComponent() : nullptr;
				if (ASC)
				{
					ASC->TryActivateAbilityByClass(UArenaDuelGA_ShadowStep::StaticClass());
					ASC->TryActivateAbilityByClass(UArenaDuelGA_VeilWall::StaticClass());
				}
				if (!RemoteState || !ASC || RemoteState->GetShadowStepCooldownRemaining() > KINDA_SMALL_NUMBER
					|| RemoteState->GetVeilWallCooldownRemaining() > KINDA_SMALL_NUMBER || ArenaDuelPhase7NetworkTests::CountWalls(State.World) != 0)
				{
					TestRunner->AddError(TEXT("A Shadow ability produced gameplay effects while the server round was inactive"));
				}
			})
			.ThenServer(TEXT("Restart current round without awarding a win"), [](FArenaDuelPhase7NetworkState& State)
			{
				if (AArenaDuelGameMode* GameMode = State.World->GetAuthGameMode<AArenaDuelGameMode>()) GameMode->AdminRestartRound();
			})
			.UntilServer(TEXT("Respawn preserves one spec per ability and clears cooldowns"), [](FArenaDuelPhase7NetworkState& State)
			{
				AArenaDuelCharacter* RemoteCharacter = ArenaDuelPhase7NetworkTests::FindRemoteCharacter(State.World);
				AArenaDuelPlayerState* PlayerState = RemoteCharacter ? RemoteCharacter->GetPlayerState<AArenaDuelPlayerState>() : nullptr;
				return PlayerState && PlayerState->GetPawn()
					&& ArenaDuelPhase7NetworkTests::CountAbilitySpecs(PlayerState, UArenaDuelGA_ShadowStep::StaticClass()) == 1
					&& ArenaDuelPhase7NetworkTests::CountAbilitySpecs(PlayerState, UArenaDuelGA_VeilWall::StaticClass()) == 1
					&& PlayerState->GetShadowStepCooldownRemaining() <= KINDA_SMALL_NUMBER
					&& PlayerState->GetVeilWallCooldownRemaining() <= KINDA_SMALL_NUMBER;
			}, FTimespan::FromSeconds(5.0))
			.ThenServer(TEXT("Dead PlayerState avatar cannot activate Shadow abilities"), [this](FArenaDuelPhase7NetworkState& State)
			{
				AArenaDuelCharacter* RemoteCharacter = ArenaDuelPhase7NetworkTests::FindRemoteCharacter(State.World);
				if (!RemoteCharacter)
				{
					TestRunner->AddError(TEXT("Server could not resolve the remote Character for death validation"));
					return;
				}
				RemoteCharacter->AdminKill();
				UAbilitySystemComponent* ASC = RemoteCharacter->GetAbilitySystemComponent();
				const bool bDeadAfterAdminKill = RemoteCharacter->IsDead();
				if (ASC)
				{
					// TryActivateAbilityByClass may report a false positive after GAS rejects activation; assert that no committed gameplay effect occurred.
					ASC->TryActivateAbilityByClass(UArenaDuelGA_ShadowStep::StaticClass());
					ASC->TryActivateAbilityByClass(UArenaDuelGA_VeilWall::StaticClass());
				}
				const bool bCooldownApplied = RemoteCharacter->GetPlayerState<AArenaDuelPlayerState>()
					&& (RemoteCharacter->GetPlayerState<AArenaDuelPlayerState>()->GetShadowStepCooldownRemaining() > KINDA_SMALL_NUMBER
						|| RemoteCharacter->GetPlayerState<AArenaDuelPlayerState>()->GetVeilWallCooldownRemaining() > KINDA_SMALL_NUMBER);
				const int32 WallsAfterDeadActivationAttempt = ArenaDuelPhase7NetworkTests::CountWalls(State.World);
				if (!bDeadAfterAdminKill || !ASC || bCooldownApplied || WallsAfterDeadActivationAttempt != 0)
				{
					TestRunner->AddError(FString::Printf(TEXT("Dead ability gate failed: dead=%d ASC=%d cooldownApplied=%d wallCount=%d"), bDeadAfterAdminKill, ASC != nullptr, bCooldownApplied, WallsAfterDeadActivationAttempt));
				}
			});
	}
};

#endif // WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
