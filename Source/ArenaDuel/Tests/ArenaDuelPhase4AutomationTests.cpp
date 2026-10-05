#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "CQTest.h"
#include "Components/PIENetworkComponent.h"

#include "ArenaDuel/Characters/ArenaDuelCharacter.h"
#include "ArenaDuel/Characters/ArenaDuelCharacterMovementComponent.h"
#include "Engine/World.h"
#include "GameMapsSettings.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Tests/AutomationEditorCommon.h"
#include "UObject/UnrealType.h"
#include "EngineUtils.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/GameModeBase.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"

namespace ArenaDuelPhase4Tests
{
	static constexpr TCHAR MovementMap[] = TEXT("/Game/ArenaDuel/Maps/L_Phase4MovementTest");
	static constexpr TCHAR CharacterClass[] = TEXT("/Game/ArenaDuel/Characters/BP_ArenaDuelCharacter.BP_ArenaDuelCharacter_C");
	static constexpr TCHAR GameModeClass[] = TEXT("/Game/ArenaDuel/Game/BP_ArenaDuelGameMode.BP_ArenaDuelGameMode_C");
	static constexpr TCHAR InputRoot[] = TEXT("/Game/ArenaDuel/Input/");
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FArenaDuelPhase4MapAndAssetsTest, "ArenaDuel.Phase4.MapAndAssets", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FArenaDuelPhase4MapAndAssetsTest::RunTest(const FString& Parameters)
{
	FAutomationEditorCommonUtils::LoadMap(ArenaDuelPhase4Tests::MovementMap);
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	TestNotNull(TEXT("Phase 4 test map loaded"), World);
	if (!World)
	{
		return false;
	}

	TestEqual(TEXT("Phase 4 editor startup map"), UGameMapsSettings::GetGameDefaultMap(), FString(ArenaDuelPhase4Tests::MovementMap));
	TestEqual(TEXT("Phase 4 GameMode"), UGameMapsSettings::GetGlobalDefaultGameMode(), FString(ArenaDuelPhase4Tests::GameModeClass));

	for (const TCHAR* AssetName : { TEXT("IA_Move"), TEXT("IA_Look"), TEXT("IA_Jump"), TEXT("IA_Sprint"), TEXT("IA_Crouch"), TEXT("IMC_Gameplay") })
	{
		TestNotNull(FString::Printf(TEXT("Input asset %s"), AssetName), LoadObject<UObject>(nullptr, *FString::Printf(TEXT("%s%s.%s"), ArenaDuelPhase4Tests::InputRoot, AssetName, AssetName)));
	}

	UClass* CharacterClass = LoadClass<AArenaDuelCharacter>(nullptr, ArenaDuelPhase4Tests::CharacterClass);
	TestNotNull(TEXT("Blueprint Character class"), CharacterClass);
	if (!CharacterClass)
	{
		return false;
	}

	const AArenaDuelCharacter* CharacterCDO = CharacterClass->GetDefaultObject<AArenaDuelCharacter>();
	for (const TCHAR* PropertyName : { TEXT("DefaultMappingContext"), TEXT("MoveAction"), TEXT("LookAction"), TEXT("JumpAction"), TEXT("SprintAction"), TEXT("CrouchAction") })
	{
		const FObjectProperty* Property = FindFProperty<FObjectProperty>(AArenaDuelCharacter::StaticClass(), PropertyName);
		TestNotNull(FString::Printf(TEXT("Character property %s"), PropertyName), Property);
		if (Property)
		{
			TestNotNull(FString::Printf(TEXT("Character asset assignment %s"), PropertyName), Property->GetObjectPropertyValue_InContainer(CharacterCDO));
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FArenaDuelPhase4MovementStateTest, "ArenaDuel.Phase4.MovementState", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FArenaDuelPhase4MovementStateTest::RunTest(const FString& Parameters)
{
	UClass* CharacterClass = LoadClass<AArenaDuelCharacter>(nullptr, ArenaDuelPhase4Tests::CharacterClass);
	TestNotNull(TEXT("Character class loads"), CharacterClass);
	if (!CharacterClass || !GEditor)
	{
		return false;
	}

	UWorld* World = GEditor->GetEditorWorldContext().World();
	AArenaDuelCharacter* Character = World->SpawnActor<AArenaDuelCharacter>(CharacterClass, FVector::ZeroVector, FRotator::ZeroRotator);
	TestNotNull(TEXT("Test Character spawned"), Character);
	if (!Character)
	{
		return false;
	}

	UArenaDuelCharacterMovementComponent* Movement = Character->GetArenaDuelMovementComponent();
	TestNotNull(TEXT("Custom movement component exists"), Movement);
	if (!Movement)
	{
		Character->Destroy();
		return false;
	}

	Movement->SetMovementMode(MOVE_Walking);
	Movement->Velocity = FVector(800.0f, 0.0f, 0.0f);
	TestTrue(TEXT("Movement test starts grounded"), Movement->IsMovingOnGround());
	TestTrue(TEXT("Movement test has speed"), Movement->Velocity.Size2D() >= Movement->SlideMinSpeed);
	Movement->StartSprint();
	TestTrue(TEXT("Sprint raises configured speed"), Movement->GetMaxSpeed() > Movement->WalkSpeed);

	Movement->StartCrouchOrSlide();
	TestTrue(TEXT("Fast crouch enters slide"), Movement->IsSliding());
	const float SlideSpeed = Movement->Velocity.Size2D();
	TestTrue(TEXT("Slide preserves horizontal momentum"), SlideSpeed >= Movement->SlideMinSpeed);

	TestTrue(TEXT("Slide jump transitions to falling"), Movement->TrySlideJump());
	TestEqual(TEXT("Slide jump movement mode"), Movement->MovementMode, MOVE_Falling);
	TestTrue(TEXT("Slide jump retains upward impulse"), Movement->Velocity.Z > 0.0f);
	TestTrue(TEXT("Stamina is bounded"), Movement->MaxStamina > 0.0f && Movement->WallRunDrain > 0.0f && Movement->StaminaRegenRate > 0.0f);
	TestTrue(TEXT("Momentum cap is finite"), Movement->GlobalMomentumCap >= Movement->SprintSpeed);

	Character->Destroy();
	return true;
}

namespace ArenaDuelPhase4HardeningTests
{
	static constexpr TCHAR GameModeClass[] = TEXT("/Game/ArenaDuel/Game/BP_ArenaDuelGameMode.BP_ArenaDuelGameMode_C");

	static UEnhancedInputLocalPlayerSubsystem* GetLocalInputSubsystem(APlayerController* Controller)
	{
		return Controller && Controller->GetLocalPlayer() ? Controller->GetLocalPlayer()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
	}

	static AArenaDuelCharacter* SpawnCharacter(UWorld* World, const FVector& Location)
	{
		UClass* CharacterClass = LoadClass<AArenaDuelCharacter>(nullptr, ArenaDuelPhase4Tests::CharacterClass);
		AArenaDuelCharacter* Character = World && CharacterClass ? World->SpawnActor<AArenaDuelCharacter>(CharacterClass, Location, FRotator::ZeroRotator) : nullptr;
		if (Character)
		{
			Character->GetCharacterMovement()->SetUpdatedComponent(Character->GetCapsuleComponent());
			Character->SpawnDefaultController();
		}
		return Character;
	}

	static UArenaDuelCharacterMovementComponent* Movement(AArenaDuelCharacter* Character)
	{
		return Character ? Character->GetArenaDuelMovementComponent() : nullptr;
	}

	static bool IsFinite(const FVector& Value)
	{
		return FMath::IsFinite(Value.X) && FMath::IsFinite(Value.Y) && FMath::IsFinite(Value.Z);
	}

	static void AdvanceCustomMovement(UArenaDuelCharacterMovementComponent* Movement, int32 Steps, float DeltaSeconds = 0.016f)
	{
		if (!Movement)
		{
			return;
		}
		for (int32 Index = 0; Index < Steps; ++Index)
		{
			Movement->UpdateCharacterStateBeforeMovement(DeltaSeconds);
			if (Movement->MovementMode == MOVE_Custom)
			{
				Movement->PhysCustom(DeltaSeconds, Index);
			}
			else if (Movement->IsFalling())
			{
				Movement->PhysFalling(DeltaSeconds, Index);
			}
		}
	}

	static bool IsCapsuleOverlapping(AArenaDuelCharacter* Character)
	{
		if (!Character || !Character->GetWorld() || !Character->GetCapsuleComponent())
		{
			return true;
		}
		const UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
		const FCollisionShape Shape = FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(), Capsule->GetScaledCapsuleHalfHeight());
		FCollisionQueryParams Params(SCENE_QUERY_STAT(ArenaDuelTestOverlap), false, Character);
		return Character->GetWorld()->OverlapBlockingTestByChannel(Character->GetActorLocation(), Character->GetActorQuat(), ECC_Pawn, Shape, Params);
	}

	static AArenaDuelCharacter* SpawnFalling(UWorld* World, const FVector& Location, const FVector& InitialVelocity)
	{
		AArenaDuelCharacter* Character = SpawnCharacter(World, Location);
		if (UArenaDuelCharacterMovementComponent* Move = Movement(Character))
		{
			Move->SetMovementMode(MOVE_Falling);
			Move->Velocity = InitialVelocity;
		}
		return Character;
	}
}

#define ARENA_PHASE4_COMPONENT_TEST(TestName, TestPath, Body) \
IMPLEMENT_SIMPLE_AUTOMATION_TEST(TestName, TestPath, EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter) \
bool TestName::RunTest(const FString& Parameters) Body

ARENA_PHASE4_COMPONENT_TEST(FArenaDuelPhase4SprintTest, "ArenaDuel.Phase4.Sprint", {
	FAutomationEditorCommonUtils::LoadMap(ArenaDuelPhase4Tests::MovementMap);
	AArenaDuelCharacter* Character = ArenaDuelPhase4HardeningTests::SpawnCharacter(GEditor->GetEditorWorldContext().World(), FVector::ZeroVector);
	UArenaDuelCharacterMovementComponent* Move = ArenaDuelPhase4HardeningTests::Movement(Character);
	TestNotNull(TEXT("Sprint Character"), Character);
	TestNotNull(TEXT("Sprint movement"), Move);
	if (Move) { Move->SetMovementMode(MOVE_Walking); Move->Velocity = FVector(800.0f, 0.0f, 0.0f); Move->StartSprint(); TestTrue(TEXT("Sprint intent retained"), Move->WantsSprintIntent()); TestEqual(TEXT("Sprint speed"), Move->GetMaxSpeed(), Move->SprintSpeed); } return true;
})

ARENA_PHASE4_COMPONENT_TEST(FArenaDuelPhase4CrouchTest, "ArenaDuel.Phase4.Crouch", {
	FAutomationEditorCommonUtils::LoadMap(ArenaDuelPhase4Tests::MovementMap);
	UArenaDuelCharacterMovementComponent* Move = ArenaDuelPhase4HardeningTests::Movement(ArenaDuelPhase4HardeningTests::SpawnCharacter(GEditor->GetEditorWorldContext().World(), FVector::ZeroVector));
	TestNotNull(TEXT("Crouch movement"), Move);
	if (Move) { Move->StartCrouchOrSlide(); TestTrue(TEXT("Crouch intent retained"), Move->WantsCrouchSlideIntent()); Move->StopCrouchOrSlide(); TestFalse(TEXT("Crouch intent released"), Move->WantsCrouchSlideIntent()); } return true;
})

ARENA_PHASE4_COMPONENT_TEST(FArenaDuelPhase4SlideTest, "ArenaDuel.Phase4.Slide", {
	FAutomationEditorCommonUtils::LoadMap(ArenaDuelPhase4Tests::MovementMap);
	UArenaDuelCharacterMovementComponent* Move = ArenaDuelPhase4HardeningTests::Movement(ArenaDuelPhase4HardeningTests::SpawnCharacter(GEditor->GetEditorWorldContext().World(), FVector::ZeroVector));
	Move->SetMovementMode(MOVE_Walking); Move->Velocity = FVector(800.0f, 0.0f, 0.0f); Move->StartCrouchOrSlide();
	TestTrue(TEXT("Slide entered above threshold"), Move->IsSliding()); TestTrue(TEXT("Slide is capped"), Move->Velocity.Size2D() <= Move->GlobalMomentumCap); return true;
})

ARENA_PHASE4_COMPONENT_TEST(FArenaDuelPhase4SlideJumpTest, "ArenaDuel.Phase4.SlideJump", {
	FAutomationEditorCommonUtils::LoadMap(ArenaDuelPhase4Tests::MovementMap);
	UArenaDuelCharacterMovementComponent* Move = ArenaDuelPhase4HardeningTests::Movement(ArenaDuelPhase4HardeningTests::SpawnCharacter(GEditor->GetEditorWorldContext().World(), FVector::ZeroVector));
	Move->SetMovementMode(MOVE_Walking); Move->Velocity = FVector(800.0f, 0.0f, 0.0f); Move->StartCrouchOrSlide(); const bool bJumped = Move->TrySlideJump();
	TestTrue(TEXT("Slide jump accepted"), bJumped); TestEqual(TEXT("Slide jump falls"), Move->MovementMode, MOVE_Falling); TestTrue(TEXT("Slide jump rises"), Move->Velocity.Z > 0.0f); return true;
})

ARENA_PHASE4_COMPONENT_TEST(FArenaDuelPhase4AirControlTrajectoryTest, "ArenaDuel.Phase4.AirControlTrajectory", {
	FAutomationEditorCommonUtils::LoadMap(ArenaDuelPhase4Tests::MovementMap); UWorld* World = GEditor->GetEditorWorldContext().World();
	AArenaDuelCharacter* NoInput = ArenaDuelPhase4HardeningTests::SpawnFalling(World, FVector(0.0f, 0.0f, 300.0f), FVector(900.0f, 0.0f, 0.0f));
	AArenaDuelCharacter* Lateral = ArenaDuelPhase4HardeningTests::SpawnFalling(World, FVector(0.0f, 0.0f, 300.0f), FVector(900.0f, 0.0f, 0.0f));
	UArenaDuelCharacterMovementComponent* A = ArenaDuelPhase4HardeningTests::Movement(NoInput); UArenaDuelCharacterMovementComponent* B = ArenaDuelPhase4HardeningTests::Movement(Lateral);
	TestNotNull(TEXT("No-input movement"), A); TestNotNull(TEXT("Lateral movement"), B);
	if (A && B) { for (int32 Index = 0; Index < 20; ++Index) { Lateral->AddMovementInput(FVector::YAxisVector, 1.0f); A->TickComponent(0.016f, LEVELTICK_All, nullptr); B->TickComponent(0.016f, LEVELTICK_All, nullptr); } const FVector DeltaA = NoInput->GetActorLocation() - FVector(0.0f, 0.0f, 300.0f); const FVector DeltaB = Lateral->GetActorLocation() - FVector(0.0f, 0.0f, 300.0f); TestTrue(TEXT("Air control creates lateral displacement"), FMath::Abs(DeltaB.Y - DeltaA.Y) > 1.0f); TestTrue(TEXT("Forward momentum is retained"), B->Velocity.X > 0.0f); TestTrue(TEXT("Air speed is capped"), B->Velocity.Size2D() <= B->GlobalMomentumCap + 1.0f); TestTrue(TEXT("Air velocity is finite"), ArenaDuelPhase4HardeningTests::IsFinite(B->Velocity)); TestTrue(TEXT("Air location is finite"), ArenaDuelPhase4HardeningTests::IsFinite(Lateral->GetActorLocation())); }
	return true;
})

ARENA_PHASE4_COMPONENT_TEST(FArenaDuelPhase4StaminaLifecycleTest, "ArenaDuel.Phase4.StaminaLifecycle", {
	FAutomationEditorCommonUtils::LoadMap(ArenaDuelPhase4Tests::MovementMap); UWorld* World = GEditor->GetEditorWorldContext().World(); AArenaDuelCharacter* Character = ArenaDuelPhase4HardeningTests::SpawnFalling(World, FVector(800.0f, 390.0f, 100.0f), FVector(900.0f, 0.0f, 0.0f)); UArenaDuelCharacterMovementComponent* Move = ArenaDuelPhase4HardeningTests::Movement(Character); TestNotNull(TEXT("Stamina movement"), Move); if (!Move) return false;
	Move->StartSprint(); Move->UpdateCharacterStateBeforeMovement(0.016f); TestTrue(TEXT("Wall run starts for stamina test"), Move->IsWallRunning()); const float Initial = Move->GetStamina(); Move->PhysCustom(0.25f, 0); const float DuringRun = Move->GetStamina(); TestTrue(TEXT("Wall run drains stamina"), DuringRun < Initial); TestTrue(TEXT("Stamina remains nonnegative"), DuringRun >= 0.0f); Move->ConsumeStamina(Move->GetMaxStamina()); Move->PhysCustom(0.016f, 0); TestFalse(TEXT("Exhaustion exits wall run"), Move->IsWallRunning()); const float Drained = Move->GetStamina(); Move->TickComponent(Move->StaminaRegenDelay * 0.5f, LEVELTICK_All, nullptr); TestTrue(TEXT("Regen delay is respected"), Move->GetStamina() <= Drained); Move->TickComponent(Move->StaminaRegenDelay * 2.0f, LEVELTICK_All, nullptr); TestTrue(TEXT("Regen begins after delay"), Move->GetStamina() > Drained); const float Regenerated = Move->GetStamina(); Move->ConsumeStamina(10.0f); Move->TickComponent(Move->StaminaRegenDelay * 0.5f, LEVELTICK_All, nullptr); TestTrue(TEXT("Second use resets delay"), Move->GetStamina() <= Regenerated - 9.0f); Move->TickComponent(Move->StaminaRegenDelay * 2.0f, LEVELTICK_All, nullptr); TestTrue(TEXT("Second regeneration begins"), Move->GetStamina() > Regenerated - 10.0f); TestTrue(TEXT("Stamina remains capped"), Move->GetStamina() <= Move->GetMaxStamina()); return true;
})

ARENA_PHASE4_COMPONENT_TEST(FArenaDuelPhase4WallRunEntryTest, "ArenaDuel.Phase4.WallRunEntry", {
	FAutomationEditorCommonUtils::LoadMap(ArenaDuelPhase4Tests::MovementMap); UWorld* World = GEditor->GetEditorWorldContext().World(); AArenaDuelCharacter* Character = ArenaDuelPhase4HardeningTests::SpawnFalling(World, FVector(800.0f, 390.0f, 100.0f), FVector(900.0f, 0.0f, 0.0f)); UArenaDuelCharacterMovementComponent* Move = ArenaDuelPhase4HardeningTests::Movement(Character); Move->StartSprint(); Move->UpdateCharacterStateBeforeMovement(0.016f); TestTrue(TEXT("Valid wall enters wall run"), Move->IsWallRunning()); TestEqual(TEXT("Wall run custom mode"), Move->CustomMovementMode, static_cast<uint8>(EArenaDuelCustomMovementMode::WallRun)); return true;
})

ARENA_PHASE4_COMPONENT_TEST(FArenaDuelPhase4WallRunInvalidCasesTest, "ArenaDuel.Phase4.WallRunInvalidCases", {
	FAutomationEditorCommonUtils::LoadMap(ArenaDuelPhase4Tests::MovementMap); UWorld* World = GEditor->GetEditorWorldContext().World();
	AArenaDuelCharacter* NoWall = ArenaDuelPhase4HardeningTests::SpawnFalling(World, FVector(0.0f, 0.0f, 300.0f), FVector(900.0f, 0.0f, 0.0f)); UArenaDuelCharacterMovementComponent* NoWallMove = ArenaDuelPhase4HardeningTests::Movement(NoWall); NoWallMove->StartSprint(); NoWallMove->UpdateCharacterStateBeforeMovement(0.016f); TestFalse(TEXT("No wall rejects wall run"), NoWallMove->IsWallRunning());
	AArenaDuelCharacter* Slow = ArenaDuelPhase4HardeningTests::SpawnFalling(World, FVector(800.0f, 390.0f, 100.0f), FVector(200.0f, 0.0f, 0.0f)); UArenaDuelCharacterMovementComponent* SlowMove = ArenaDuelPhase4HardeningTests::Movement(Slow); SlowMove->StartSprint(); SlowMove->UpdateCharacterStateBeforeMovement(0.016f); TestFalse(TEXT("Insufficient speed rejects wall run"), SlowMove->IsWallRunning());
	AArenaDuelCharacter* Empty = ArenaDuelPhase4HardeningTests::SpawnFalling(World, FVector(800.0f, 390.0f, 100.0f), FVector(900.0f, 0.0f, 0.0f)); UArenaDuelCharacterMovementComponent* EmptyMove = ArenaDuelPhase4HardeningTests::Movement(Empty); EmptyMove->ConsumeStamina(EmptyMove->GetMaxStamina()); EmptyMove->StartSprint(); EmptyMove->UpdateCharacterStateBeforeMovement(0.016f); TestFalse(TEXT("Empty stamina rejects wall run"), EmptyMove->IsWallRunning()); return true;
})

ARENA_PHASE4_COMPONENT_TEST(FArenaDuelPhase4WallRunExitTest, "ArenaDuel.Phase4.WallRunExit", {
	FAutomationEditorCommonUtils::LoadMap(ArenaDuelPhase4Tests::MovementMap); UWorld* World = GEditor->GetEditorWorldContext().World(); AArenaDuelCharacter* Character = ArenaDuelPhase4HardeningTests::SpawnFalling(World, FVector(800.0f, 390.0f, 100.0f), FVector(900.0f, 0.0f, 0.0f)); UArenaDuelCharacterMovementComponent* Move = ArenaDuelPhase4HardeningTests::Movement(Character); Move->StartSprint(); Move->UpdateCharacterStateBeforeMovement(0.016f); TestTrue(TEXT("Exit test enters wall run"), Move->IsWallRunning()); Character->SetActorLocation(FVector(800.0f, 0.0f, 300.0f)); Move->PhysCustom(0.016f, 0); TestFalse(TEXT("Lost wall exits"), Move->IsWallRunning()); TestTrue(TEXT("Exit velocity remains finite"), ArenaDuelPhase4HardeningTests::IsFinite(Move->Velocity)); return true;
})

ARENA_PHASE4_COMPONENT_TEST(FArenaDuelPhase4WallRunReattachTest, "ArenaDuel.Phase4.WallRunReattach", {
	FAutomationEditorCommonUtils::LoadMap(ArenaDuelPhase4Tests::MovementMap); UWorld* World = GEditor->GetEditorWorldContext().World(); AArenaDuelCharacter* Character = ArenaDuelPhase4HardeningTests::SpawnFalling(World, FVector(800.0f, 390.0f, 100.0f), FVector(900.0f, 0.0f, 0.0f)); UArenaDuelCharacterMovementComponent* Move = ArenaDuelPhase4HardeningTests::Movement(Character); Move->StartSprint(); Move->UpdateCharacterStateBeforeMovement(0.016f); TestTrue(TEXT("Reattach test enters wall run"), Move->IsWallRunning()); Move->TryWallJump(); Move->SetMovementMode(MOVE_Falling); Move->Velocity = FVector(900.0f, 0.0f, 0.0f); Move->UpdateCharacterStateBeforeMovement(0.0f); TestFalse(TEXT("Same wall lockout applies"), Move->IsWallRunning()); Move->UpdateCharacterStateBeforeMovement(Move->WallReattachCooldown + 0.01f); TestTrue(TEXT("Same wall can reattach after cooldown"), Move->IsWallRunning()); return true;
})

ARENA_PHASE4_COMPONENT_TEST(FArenaDuelPhase4WallJumpBehaviorTest, "ArenaDuel.Phase4.WallJumpBehavior", {
	FAutomationEditorCommonUtils::LoadMap(ArenaDuelPhase4Tests::MovementMap); UWorld* World = GEditor->GetEditorWorldContext().World(); AArenaDuelCharacter* Character = ArenaDuelPhase4HardeningTests::SpawnFalling(World, FVector(800.0f, 390.0f, 100.0f), FVector(900.0f, 0.0f, 0.0f)); UArenaDuelCharacterMovementComponent* Move = ArenaDuelPhase4HardeningTests::Movement(Character); Move->StartSprint(); Move->UpdateCharacterStateBeforeMovement(0.016f); const bool bJumped = Move->TryWallJump(); TestTrue(TEXT("Wall jump accepted"), bJumped); TestFalse(TEXT("Wall jump exits wall run"), Move->IsWallRunning()); TestEqual(TEXT("Wall jump falls"), Move->MovementMode, MOVE_Falling); TestTrue(TEXT("Wall jump rises"), Move->Velocity.Z > 0.0f); TestTrue(TEXT("Wall jump moves away from wall"), Move->Velocity.Y < 0.0f); TestTrue(TEXT("Wall jump velocity is finite"), ArenaDuelPhase4HardeningTests::IsFinite(Move->Velocity)); return true;
})

ARENA_PHASE4_COMPONENT_TEST(FArenaDuelPhase4VaultProgressionTest, "ArenaDuel.Phase4.VaultProgression", {
	FAutomationEditorCommonUtils::LoadMap(ArenaDuelPhase4Tests::MovementMap); UWorld* World = GEditor->GetEditorWorldContext().World(); AArenaDuelCharacter* Character = ArenaDuelPhase4HardeningTests::SpawnFalling(World, FVector(930.0f, 0.0f, 88.0f), FVector(800.0f, 0.0f, 0.0f)); UArenaDuelCharacterMovementComponent* Move = ArenaDuelPhase4HardeningTests::Movement(Character); const FVector Start = Character->GetActorLocation(); Move->UpdateCharacterStateBeforeMovement(0.016f); TestEqual(TEXT("Vault starts custom traversal"), Move->CustomMovementMode, static_cast<uint8>(EArenaDuelCustomMovementMode::Vault)); bool bMoved = false; for (int32 Index = 0; Index < 20 && Move->MovementMode == MOVE_Custom; ++Index) { const FVector Before = Character->GetActorLocation(); Move->PhysCustom(0.016f, Index); bMoved |= !Character->GetActorLocation().Equals(Before, 0.1f); } AddInfo(FString::Printf(TEXT("Vault final location: %s"), *Character->GetActorLocation().ToString())); TestTrue(TEXT("Vault progresses over multiple frames"), bMoved); TestTrue(TEXT("Vault crosses obstacle"), Character->GetActorLocation().X > 1050.0f); TestTrue(TEXT("Vault ends in valid mode"), Move->MovementMode == MOVE_Walking || Move->MovementMode == MOVE_Falling); TestFalse(TEXT("Vault destination is clear"), ArenaDuelPhase4HardeningTests::IsCapsuleOverlapping(Character)); TestTrue(TEXT("Vault transform is finite"), ArenaDuelPhase4HardeningTests::IsFinite(Character->GetActorLocation()) && ArenaDuelPhase4HardeningTests::IsFinite(Move->Velocity)); return true;
})

ARENA_PHASE4_COMPONENT_TEST(FArenaDuelPhase4VaultInvalidCasesTest, "ArenaDuel.Phase4.VaultInvalidCases", {
	FAutomationEditorCommonUtils::LoadMap(ArenaDuelPhase4Tests::MovementMap); UWorld* World = GEditor->GetEditorWorldContext().World(); AArenaDuelCharacter* Character = ArenaDuelPhase4HardeningTests::SpawnFalling(World, FVector(1300.0f, 0.0f, 100.0f), FVector(800.0f, 0.0f, 0.0f)); UArenaDuelCharacterMovementComponent* Move = ArenaDuelPhase4HardeningTests::Movement(Character); Move->UpdateCharacterStateBeforeMovement(0.016f); TestFalse(TEXT("No suitable low obstacle rejects vault"), Move->MovementMode == MOVE_Custom && Move->CustomMovementMode == static_cast<uint8>(EArenaDuelCustomMovementMode::Vault)); TestFalse(TEXT("Rejected vault does not overlap"), ArenaDuelPhase4HardeningTests::IsCapsuleOverlapping(Character)); return true;
})

ARENA_PHASE4_COMPONENT_TEST(FArenaDuelPhase4MantleProgressionTest, "ArenaDuel.Phase4.MantleProgression", {
	FAutomationEditorCommonUtils::LoadMap(ArenaDuelPhase4Tests::MovementMap); UWorld* World = GEditor->GetEditorWorldContext().World(); AArenaDuelCharacter* Character = ArenaDuelPhase4HardeningTests::SpawnFalling(World, FVector(1350.0f, 0.0f, 120.0f), FVector(800.0f, 0.0f, 0.0f)); UArenaDuelCharacterMovementComponent* Move = ArenaDuelPhase4HardeningTests::Movement(Character); const FVector Start = Character->GetActorLocation(); Move->UpdateCharacterStateBeforeMovement(0.016f); TestEqual(TEXT("Mantle starts custom traversal"), Move->CustomMovementMode, static_cast<uint8>(EArenaDuelCustomMovementMode::Mantle)); bool bMovedUp = false; for (int32 Index = 0; Index < 30 && Move->MovementMode == MOVE_Custom; ++Index) { const FVector Before = Character->GetActorLocation(); Move->PhysCustom(0.016f, Index); bMovedUp |= Character->GetActorLocation().Z > Before.Z; } TestTrue(TEXT("Mantle moves upward"), bMovedUp); TestTrue(TEXT("Mantle moves forward"), Character->GetActorLocation().X > Start.X); TestTrue(TEXT("Mantle ends in valid mode"), Move->MovementMode == MOVE_Walking || Move->MovementMode == MOVE_Falling); TestFalse(TEXT("Mantle destination is clear"), ArenaDuelPhase4HardeningTests::IsCapsuleOverlapping(Character)); return true;
})

ARENA_PHASE4_COMPONENT_TEST(FArenaDuelPhase4MantleInvalidCasesTest, "ArenaDuel.Phase4.MantleInvalidCases", {
	FAutomationEditorCommonUtils::LoadMap(ArenaDuelPhase4Tests::MovementMap); UWorld* World = GEditor->GetEditorWorldContext().World(); AArenaDuelCharacter* Character = ArenaDuelPhase4HardeningTests::SpawnFalling(World, FVector(900.0f, 0.0f, 100.0f), FVector(800.0f, 0.0f, 0.0f)); UArenaDuelCharacterMovementComponent* Move = ArenaDuelPhase4HardeningTests::Movement(Character); Move->UpdateCharacterStateBeforeMovement(0.016f); TestFalse(TEXT("Low obstacle is not incorrectly mantled"), Move->MovementMode == MOVE_Custom && Move->CustomMovementMode == static_cast<uint8>(EArenaDuelCustomMovementMode::Mantle)); TestFalse(TEXT("Rejected mantle does not overlap"), ArenaDuelPhase4HardeningTests::IsCapsuleOverlapping(Character)); return true;
})

ARENA_PHASE4_COMPONENT_TEST(FArenaDuelPhase4TraversalCollisionSafetyTest, "ArenaDuel.Phase4.TraversalCollisionSafety", {
	FAutomationEditorCommonUtils::LoadMap(ArenaDuelPhase4Tests::MovementMap); UWorld* World = GEditor->GetEditorWorldContext().World(); AArenaDuelCharacter* VaultCharacter = ArenaDuelPhase4HardeningTests::SpawnFalling(World, FVector(930.0f, 0.0f, 88.0f), FVector(800.0f, 0.0f, 0.0f)); UArenaDuelCharacterMovementComponent* VaultMove = ArenaDuelPhase4HardeningTests::Movement(VaultCharacter); VaultMove->UpdateCharacterStateBeforeMovement(0.016f); for (int32 Index = 0; Index < 20 && VaultMove->MovementMode == MOVE_Custom; ++Index) VaultMove->PhysCustom(0.016f, Index); TestFalse(TEXT("Vault never leaves capsule embedded"), ArenaDuelPhase4HardeningTests::IsCapsuleOverlapping(VaultCharacter)); AArenaDuelCharacter* MantleCharacter = ArenaDuelPhase4HardeningTests::SpawnFalling(World, FVector(1350.0f, 0.0f, 120.0f), FVector(800.0f, 0.0f, 0.0f)); UArenaDuelCharacterMovementComponent* MantleMove = ArenaDuelPhase4HardeningTests::Movement(MantleCharacter); MantleMove->UpdateCharacterStateBeforeMovement(0.016f); for (int32 Index = 0; Index < 30 && MantleMove->MovementMode == MOVE_Custom; ++Index) MantleMove->PhysCustom(0.016f, Index); TestFalse(TEXT("Mantle never leaves capsule embedded"), ArenaDuelPhase4HardeningTests::IsCapsuleOverlapping(MantleCharacter)); return true;
})

#undef ARENA_PHASE4_COMPONENT_TEST

struct FArenaDuelPhase4NetworkState : public FBasePIENetworkComponentState
{
	AArenaDuelCharacter* OwnedPawn = nullptr;
};

NETWORK_TEST_CLASS(FArenaDuelPhase4NetworkTest, "ArenaDuel.Phase4.Network")
{
	FPIENetworkComponent<FArenaDuelPhase4NetworkState> Network{TestRunner, TestCommandBuilder, bInitializing};

	BEFORE_EACH()
	{
		UClass* GameModeClass = LoadClass<AGameModeBase>(nullptr, ArenaDuelPhase4HardeningTests::GameModeClass);
		FNetworkComponentBuilder<FArenaDuelPhase4NetworkState>()
			.WithClients(1)
			.AsListenServer()
			.WithGameMode(GameModeClass)
			.Build(Network);
	}

	TEST_METHOD(SprintAndCrouchIntent)
	{
		Network
			.UntilClient(TEXT("Wait for owned pawn"), 0, [](FArenaDuelPhase4NetworkState& State)
			{
				APlayerController* Controller = State.World->GetFirstPlayerController();
				State.OwnedPawn = Controller ? Cast<AArenaDuelCharacter>(Controller->GetPawn()) : nullptr;
				return State.OwnedPawn != nullptr;
			}, FTimespan::FromSeconds(5.0))
			.ThenClient(TEXT("Inject sprint intent"), 0, [](FArenaDuelPhase4NetworkState& State)
			{
				if (State.OwnedPawn)
				{
					State.OwnedPawn->GetArenaDuelMovementComponent()->StartSprint();
				}
				if (APlayerController* Controller = State.World->GetFirstPlayerController())
				{
					if (UEnhancedInputLocalPlayerSubsystem* Input = ArenaDuelPhase4HardeningTests::GetLocalInputSubsystem(Controller))
					{
						Input->StartContinuousInputInjectionForAction(LoadObject<UInputAction>(nullptr, TEXT("/Game/ArenaDuel/Input/IA_Move.IA_Move")), FInputActionValue(FVector2D(0.0f, 1.0f)), {}, {});
						Input->StartContinuousInputInjectionForAction(LoadObject<UInputAction>(nullptr, TEXT("/Game/ArenaDuel/Input/IA_Sprint.IA_Sprint")), FInputActionValue(true), {}, {});
					}
				}
			})
			.UntilServer(TEXT("Server reconstructs sprint intent"), [](FArenaDuelPhase4NetworkState& State)
			{
				for (TActorIterator<AArenaDuelCharacter> It(State.World); It; ++It)
				{
					if (const UArenaDuelCharacterMovementComponent* Move = It->GetArenaDuelMovementComponent())
					{
						if (Move->WantsSprintIntent()) return true;
					}
				}
				return false;
			}, FTimespan::FromSeconds(5.0))
			.ThenClient(TEXT("Inject crouch slide intent"), 0, [](FArenaDuelPhase4NetworkState& State)
			{
				if (APlayerController* Controller = State.World->GetFirstPlayerController())
				{
					if (UEnhancedInputLocalPlayerSubsystem* Input = ArenaDuelPhase4HardeningTests::GetLocalInputSubsystem(Controller))
					{
						Input->StartContinuousInputInjectionForAction(LoadObject<UInputAction>(nullptr, TEXT("/Game/ArenaDuel/Input/IA_Crouch.IA_Crouch")), FInputActionValue(true), {}, {});
					}
				}
			})
			.UntilServer(TEXT("Server reconstructs crouch slide intent"), [](FArenaDuelPhase4NetworkState& State)
			{
				for (TActorIterator<AArenaDuelCharacter> It(State.World); It; ++It)
				{
					if (const UArenaDuelCharacterMovementComponent* Move = It->GetArenaDuelMovementComponent())
					{
						if (Move->WantsCrouchSlideIntent()) return true;
					}
				}
				return false;
			}, FTimespan::FromSeconds(5.0));
	}
};

#endif
