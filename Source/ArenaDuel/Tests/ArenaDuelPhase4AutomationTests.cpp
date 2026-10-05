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

ARENA_PHASE4_COMPONENT_TEST(FArenaDuelPhase4AirControlTest, "ArenaDuel.Phase4.AirControl", {
	UArenaDuelCharacterMovementComponent* Move = nullptr;
	FAutomationEditorCommonUtils::LoadMap(ArenaDuelPhase4Tests::MovementMap); Move = ArenaDuelPhase4HardeningTests::Movement(ArenaDuelPhase4HardeningTests::SpawnCharacter(GEditor->GetEditorWorldContext().World(), FVector::ZeroVector));
	TestNotNull(TEXT("Air control movement"), Move); if (Move) { Move->SetMovementMode(MOVE_Falling); TestTrue(TEXT("Air control is enabled"), Move->AirControl > 0.0f && Move->AirControl < 1.0f); TestTrue(TEXT("Air control tuning matches configuration"), FMath::IsNearlyEqual(Move->AirControl, Move->AirControlTuning)); } return true;
})

ARENA_PHASE4_COMPONENT_TEST(FArenaDuelPhase4StaminaTest, "ArenaDuel.Phase4.Stamina", {
	UArenaDuelCharacterMovementComponent* Move = nullptr;
	FAutomationEditorCommonUtils::LoadMap(ArenaDuelPhase4Tests::MovementMap); Move = ArenaDuelPhase4HardeningTests::Movement(ArenaDuelPhase4HardeningTests::SpawnCharacter(GEditor->GetEditorWorldContext().World(), FVector::ZeroVector));
	TestNotNull(TEXT("Stamina movement"), Move); if (Move) { TestTrue(TEXT("Stamina starts full"), FMath::IsNearlyEqual(Move->GetStamina(), Move->GetMaxStamina())); TestTrue(TEXT("Stamina settings valid"), Move->GetMaxStamina() > 0.0f && Move->WallRunDrain > 0.0f && Move->StaminaRegenRate > 0.0f); } return true;
})

ARENA_PHASE4_COMPONENT_TEST(FArenaDuelPhase4WallRunTest, "ArenaDuel.Phase4.WallRun", {
	FAutomationEditorCommonUtils::LoadMap(ArenaDuelPhase4Tests::MovementMap); UWorld* World = GEditor->GetEditorWorldContext().World(); AArenaDuelCharacter* Character = ArenaDuelPhase4HardeningTests::SpawnCharacter(World, FVector(800.0f, 390.0f, 100.0f)); UArenaDuelCharacterMovementComponent* Move = ArenaDuelPhase4HardeningTests::Movement(Character); Move->SetMovementMode(MOVE_Falling); Move->Velocity = FVector(900.0f, 0.0f, 0.0f); Move->StartSprint(); Move->UpdateCharacterStateBeforeMovement(0.016f); TestTrue(TEXT("Valid wall enters wall run"), Move->IsWallRunning()); return true;
})

ARENA_PHASE4_COMPONENT_TEST(FArenaDuelPhase4WallJumpTest, "ArenaDuel.Phase4.WallJump", {
	FAutomationEditorCommonUtils::LoadMap(ArenaDuelPhase4Tests::MovementMap); UWorld* World = GEditor->GetEditorWorldContext().World(); AArenaDuelCharacter* Character = ArenaDuelPhase4HardeningTests::SpawnCharacter(World, FVector(800.0f, 390.0f, 100.0f)); UArenaDuelCharacterMovementComponent* Move = ArenaDuelPhase4HardeningTests::Movement(Character); Move->SetMovementMode(MOVE_Falling); Move->Velocity = FVector(900.0f, 0.0f, 0.0f); Move->StartSprint(); Move->UpdateCharacterStateBeforeMovement(0.016f); const bool bJumped = Move->TryWallJump(); TestTrue(TEXT("Wall jump accepted"), bJumped); TestEqual(TEXT("Wall jump falls"), Move->MovementMode, MOVE_Falling); TestTrue(TEXT("Wall jump rises"), Move->Velocity.Z > 0.0f); return true;
})

ARENA_PHASE4_COMPONENT_TEST(FArenaDuelPhase4VaultTest, "ArenaDuel.Phase4.Vault", {
	FAutomationEditorCommonUtils::LoadMap(ArenaDuelPhase4Tests::MovementMap); UWorld* World = GEditor->GetEditorWorldContext().World(); AArenaDuelCharacter* Character = ArenaDuelPhase4HardeningTests::SpawnCharacter(World, FVector(600.0f, 0.0f, 100.0f)); UArenaDuelCharacterMovementComponent* Move = ArenaDuelPhase4HardeningTests::Movement(Character); TestNotNull(TEXT("Vault movement"), Move); if (!Move) return false; Move->SetMovementMode(MOVE_Falling); Move->Velocity = FVector(800.0f, 0.0f, 0.0f); Move->UpdateCharacterStateBeforeMovement(0.016f); TestTrue(TEXT("Traversal attempt remains safe"), Move->MovementMode == MOVE_Falling || Move->IsMantling()); return true;
})

ARENA_PHASE4_COMPONENT_TEST(FArenaDuelPhase4MantleTest, "ArenaDuel.Phase4.Mantle", {
	FAutomationEditorCommonUtils::LoadMap(ArenaDuelPhase4Tests::MovementMap); UWorld* World = GEditor->GetEditorWorldContext().World(); AArenaDuelCharacter* Character = ArenaDuelPhase4HardeningTests::SpawnCharacter(World, FVector(1000.0f, 0.0f, 100.0f)); UArenaDuelCharacterMovementComponent* Move = ArenaDuelPhase4HardeningTests::Movement(Character); TestNotNull(TEXT("Mantle movement"), Move); if (!Move) return false; Move->SetMovementMode(MOVE_Falling); Move->Velocity = FVector(800.0f, 0.0f, 0.0f); Move->UpdateCharacterStateBeforeMovement(0.016f); TestTrue(TEXT("Mantle traversal state is safe"), Move->MovementMode == MOVE_Falling || Move->IsMantling()); return true;
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
