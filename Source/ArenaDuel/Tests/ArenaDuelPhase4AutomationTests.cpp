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
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Engine/TextRenderActor.h"
#include "Engine/StaticMeshActor.h"
#include "GameFramework/PlayerStart.h"
#include "Components/SkyAtmosphereComponent.h"
#include "ArenaDuel/Game/ArenaDuelMovementDebugHUD.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/GameStateBase.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/NetConnection.h"
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

	const FString ConfiguredMap = UGameMapsSettings::GetGameDefaultMap();
	TestTrue(TEXT("Configured development map remains a project-owned ArenaDuel map"), ConfiguredMap == ArenaDuelPhase4Tests::MovementMap || ConfiguredMap == TEXT("/Game/ArenaDuel/Maps/L_ArenaCore"));
	TestEqual(TEXT("Phase 4 GameMode"), UGameMapsSettings::GetGlobalDefaultGameMode(), FString(ArenaDuelPhase4Tests::GameModeClass));
	bool bDirectional = false;
	bool bSkyLight = false;
	bool bSkyAtmosphere = false;
	bool bSafetyFloor = false;
	bool bLeftWall = false;
	bool bRightWall = false;
	bool bVault = false;
	bool bMantle = false;
	bool bCentralObstacles = false;
	bool bRamp = false;
	bool bRaisedPlatform = false;
	bool bSlideTunnel = false;
	bool bSlideMarkers = false;
	bool bLabelsOriented = true;
	AStaticMeshActor* SafetyFloorActor = nullptr;
	TArray<AStaticMeshActor*> SafetyWallActors;
	AStaticMeshActor* NorthSafetyWall = nullptr;
	AStaticMeshActor* SouthSafetyWall = nullptr;
	AStaticMeshActor* WestSafetyWall = nullptr;
	AStaticMeshActor* EastSafetyWall = nullptr;
	int32 PlayerStarts = 0;
	int32 Labels = 0;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		const FString Label = It->GetActorLabel();
		bDirectional |= Cast<ADirectionalLight>(*It) != nullptr && It->GetActorLabel() == TEXT("Phase4_DirectionalLight");
		bSkyLight |= Cast<ASkyLight>(*It) != nullptr && It->GetActorLabel() == TEXT("Phase4_SkyLight");
		bSkyAtmosphere |= Cast<ASkyAtmosphere>(*It) != nullptr && It->GetActorLabel() == TEXT("Phase4_SkyAtmosphere");
		bSafetyFloor |= Label == TEXT("Phase4_LongSprintLane") && It->GetActorScale3D().X >= 70.0f && It->GetActorScale3D().Y >= 20.0f;
		if (Label == TEXT("Phase4_LongSprintLane"))
		{
			SafetyFloorActor = Cast<AStaticMeshActor>(*It);
		}
		if (Label == TEXT("Phase4_NorthSafetyWall") || Label == TEXT("Phase4_SouthSafetyWall") || Label == TEXT("Phase4_WestSafetyWall") || Label == TEXT("Phase4_EastSafetyWall"))
		{
			if (AStaticMeshActor* SafetyWall = Cast<AStaticMeshActor>(*It))
			{
				SafetyWallActors.Add(SafetyWall);
				if (Label == TEXT("Phase4_NorthSafetyWall")) NorthSafetyWall = SafetyWall;
				if (Label == TEXT("Phase4_SouthSafetyWall")) SouthSafetyWall = SafetyWall;
				if (Label == TEXT("Phase4_WestSafetyWall")) WestSafetyWall = SafetyWall;
				if (Label == TEXT("Phase4_EastSafetyWall")) EastSafetyWall = SafetyWall;
			}
		}
		bLeftWall |= Label == TEXT("Phase4_LeftWall");
		bRightWall |= Label == TEXT("Phase4_RightWall");
		bVault |= Label == TEXT("Phase4_VaultObstacle");
		bMantle |= Label == TEXT("Phase4_MantleLedge");
		bCentralObstacles |= Label == TEXT("Phase4_CentralCrateA");
		bRamp |= Label == TEXT("Phase4_Ramp");
		bRaisedPlatform |= Label == TEXT("Phase4_RaisedPlatform");
		bSlideTunnel |= Label == TEXT("Phase4_SlideTunnelRoof");
		bSlideMarkers |= Label == TEXT("Phase4_LabelSlide5m") || Label == TEXT("Phase4_LabelSlide7m");
		PlayerStarts += Cast<APlayerStart>(*It) != nullptr ? 1 : 0;
		if (Cast<ATextRenderActor>(*It))
		{
			Labels++;
			if (Label.StartsWith(TEXT("Phase4_Label")))
			{
				const FRotator Rotation = It->GetActorRotation();
				const FVector Scale = It->GetActorScale3D();
				bLabelsOriented &= FMath::Abs(FRotator::NormalizeAxis(Rotation.Pitch)) < 1.0f && FMath::Abs(FRotator::NormalizeAxis(Rotation.Roll)) < 1.0f && Scale.GetAbsMax() > 0.1f;
			}
		}
	}
	TestTrue(TEXT("Phase 4 map has actor Phase4_DirectionalLight"), bDirectional);
	TestTrue(TEXT("Phase 4 map has actor Phase4_SkyLight"), bSkyLight);
	TestTrue(TEXT("Phase 4 map has actor Phase4_SkyAtmosphere"), bSkyAtmosphere);
	TestTrue(TEXT("Phase 4 map has a large safety floor"), bSafetyFloor);
	bool bSafetyWallsInsideFloor = SafetyFloorActor != nullptr && SafetyWallActors.Num() == 4;
	if (bSafetyWallsInsideFloor)
	{
		const FBox FloorBounds = SafetyFloorActor->GetComponentsBoundingBox(true);
		for (const AStaticMeshActor* SafetyWall : SafetyWallActors)
		{
			bSafetyWallsInsideFloor &= FloorBounds.IsInsideXY(SafetyWall->GetActorLocation());
		}
	}
	TestTrue(TEXT("Safety wall centers are inside the playable floor bounds"), bSafetyWallsInsideFloor);
	bool bPerimeterCoversFloor = SafetyFloorActor && NorthSafetyWall && SouthSafetyWall && WestSafetyWall && EastSafetyWall;
	if (bPerimeterCoversFloor)
	{
		const FBox FloorBounds = SafetyFloorActor->GetComponentsBoundingBox(true);
		const float RequiredXExtent = FloorBounds.GetExtent().X * 0.99f;
		const float RequiredYExtent = FloorBounds.GetExtent().Y * 0.99f;
		bPerimeterCoversFloor =
			NorthSafetyWall->GetComponentsBoundingBox(true).GetExtent().X >= RequiredXExtent &&
			SouthSafetyWall->GetComponentsBoundingBox(true).GetExtent().X >= RequiredXExtent &&
			WestSafetyWall->GetComponentsBoundingBox(true).GetExtent().Y >= RequiredYExtent &&
			EastSafetyWall->GetComponentsBoundingBox(true).GetExtent().Y >= RequiredYExtent;
	}
	TestTrue(TEXT("Safety perimeter wall extents cover the full playable floor"), bPerimeterCoversFloor);
	TestTrue(TEXT("Phase 4 map has left and right wall-run walls"), bLeftWall && bRightWall);
	TestTrue(TEXT("Phase 4 map has vault and mantle stations"), bVault && bMantle);
	TestTrue(TEXT("Phase 4 map has a central obstacle cluster"), bCentralObstacles);
	TestTrue(TEXT("Phase 4 map has a ramp and raised platform"), bRamp && bRaisedPlatform);
	TestTrue(TEXT("Phase 4 map has a slide tunnel"), bSlideTunnel);
	TestTrue(TEXT("Phase 4 map has slide distance markers"), bSlideMarkers);
	TestTrue(TEXT("Phase 4 map has two PlayerStarts"), PlayerStarts >= 2);
	TestTrue(TEXT("Phase 4 map has development labels"), Labels >= 10);
	TestTrue(TEXT("Phase 4 labels are upright with readable scale"), bLabelsOriented);
	UClass* GameModeClass = LoadClass<AGameModeBase>(nullptr, ArenaDuelPhase4Tests::GameModeClass);
	TestTrue(TEXT("Development HUD is configured on the native GameMode"), GameModeClass && GameModeClass->GetDefaultObject<AGameModeBase>()->HUDClass == AArenaDuelMovementDebugHUD::StaticClass());

	for (const TCHAR* AssetName : { TEXT("IA_Move"), TEXT("IA_Look"), TEXT("IA_Jump"), TEXT("IA_Sprint"), TEXT("IA_Crouch"), TEXT("IA_Slide"), TEXT("IMC_Gameplay") })
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
	for (const TCHAR* PropertyName : { TEXT("DefaultMappingContext"), TEXT("MoveAction"), TEXT("LookAction"), TEXT("JumpAction"), TEXT("SprintAction"), TEXT("CrouchAction"), TEXT("SlideAction") })
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
	FAutomationEditorCommonUtils::LoadMap(ArenaDuelPhase4Tests::MovementMap);
	UClass* CharacterClass = LoadClass<AArenaDuelCharacter>(nullptr, ArenaDuelPhase4Tests::CharacterClass);
	TestNotNull(TEXT("Character class loads"), CharacterClass);
	if (!CharacterClass || !GEditor)
	{
		return false;
	}

	UWorld* World = GEditor->GetEditorWorldContext().World();
	AArenaDuelCharacter* Character = World->SpawnActor<AArenaDuelCharacter>(CharacterClass, FVector(0.0f, 0.0f, 88.0f), FRotator::ZeroRotator);
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
	Movement->SetUpdatedComponent(Character->GetCapsuleComponent());

	Movement->SetMovementMode(MOVE_Walking);
	Movement->CurrentFloor.bWalkableFloor = true;
	Movement->CurrentFloor.FloorDist = 0.0f;
	Movement->Velocity = FVector(800.0f, 0.0f, 0.0f);
	TestTrue(TEXT("Movement test starts grounded"), Movement->IsMovingOnGround());
	TestTrue(TEXT("Movement test has speed"), Movement->Velocity.Size2D() >= Movement->SlideMinSpeed);
	Movement->StartSprint();
	TestTrue(TEXT("Sprint raises configured speed"), Movement->GetMaxSpeed() > Movement->WalkSpeed);

	Movement->StartSlide();
	TestTrue(TEXT("Fast crouch enters slide"), Movement->IsSliding());
	const float SlideSpeed = Movement->Velocity.Size2D();
	TestTrue(TEXT("Slide entry receives a meaningful speed kick"), SlideSpeed > Movement->SprintSpeed);
	TestTrue(TEXT("Slide entry remains capped"), SlideSpeed <= Movement->GlobalMomentumCap);
	const FVector SlideStart = Character->GetActorLocation();
	int32 SlideFrames = 0;
	while (Movement->IsSliding() && SlideFrames < 180 && FVector::Dist2D(Character->GetActorLocation(), SlideStart) < 760.0f)
	{
		const FVector RuntimeVelocity = Movement->Velocity;
		Movement->PhysCustom(0.016f, SlideFrames);
		if (Movement->IsSliding())
		{
			FHitResult SlideHit;
			Character->GetRootComponent()->MoveComponent(RuntimeVelocity * 0.016f, Character->GetActorQuat(), true, &SlideHit);
		}
		++SlideFrames;
	}
	const float SlideDistance = FVector::Dist2D(Character->GetActorLocation(), SlideStart);
	AddInfo(FString::Printf(TEXT("Measured full-speed slide distance: %.1f uu over %d frames"), SlideDistance, SlideFrames));
	TestTrue(TEXT("Full-speed slide travels at least 450 uu"), SlideDistance >= 450.0f);
	TestTrue(TEXT("Full-speed slide remains within 800 uu"), SlideDistance <= 800.0f);
	Movement->StopSlide();
	TestFalse(TEXT("Full-speed slide exits cleanly"), Movement->IsSliding());
	Movement->SetMovementMode(MOVE_Walking);
	Movement->Velocity = FVector(800.0f, 0.0f, 0.0f);
	Movement->StartSprint();
	Movement->StartSlide();
	const float SpeedBeforeFriction = SlideSpeed;
	Movement->PhysCustom(0.1f, 0);
	TestTrue(TEXT("Slide friction reduces speed over time"), Movement->Velocity.Size2D() < SpeedBeforeFriction);
	Movement->Velocity = FVector(410.0f, 0.0f, 0.0f);
	Movement->PhysCustom(0.1f, 0);
	TestTrue(TEXT("Slide persists through minimum duration"), Movement->IsSliding());
	Movement->PhysCustom(0.35f, 0);
	TestFalse(TEXT("Slide eventually exits after minimum duration"), Movement->IsSliding());
	Movement->SetMovementMode(MOVE_Walking);
	Movement->Velocity = FVector(800.0f, 0.0f, 0.0f);
	Movement->StartSlide();
	const float SlideJumpSpeed = Movement->Velocity.Size2D();

	TestTrue(TEXT("Slide jump transitions to falling"), Movement->TrySlideJump());
	TestEqual(TEXT("Slide jump movement mode"), Movement->MovementMode, MOVE_Falling);
	TestTrue(TEXT("Slide jump retains upward impulse"), Movement->Velocity.Z > 0.0f);
	TestTrue(TEXT("Slide jump retains meaningful horizontal momentum"), Movement->Velocity.Size2D() >= SlideJumpSpeed * 0.85f);
	TestTrue(TEXT("Stamina is bounded"), Movement->MaxStamina > 0.0f && Movement->WallRunDrain > 0.0f && Movement->StaminaRegenRate > 0.0f);
	TestTrue(TEXT("Momentum cap is finite"), Movement->GlobalMomentumCap >= Movement->SprintSpeed);

	Character->Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FArenaDuelPhase4SlideBufferTest, "ArenaDuel.Phase4.SlideInputBuffer", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

namespace ArenaDuelPhase4HardeningTests
{
	static AArenaDuelCharacter* SpawnCharacter(UWorld* World, const FVector& Location);
}

bool FArenaDuelPhase4SlideBufferTest::RunTest(const FString& Parameters)
{
	FAutomationEditorCommonUtils::LoadMap(ArenaDuelPhase4Tests::MovementMap);
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	AArenaDuelCharacter* SlowCharacter = ArenaDuelPhase4HardeningTests::SpawnCharacter(World, FVector(-300.0f, 0.0f, 1.0f));
	AArenaDuelCharacter* BufferedCharacter = ArenaDuelPhase4HardeningTests::SpawnCharacter(World, FVector(-100.0f, 0.0f, 1.0f));
	TestNotNull(TEXT("Slide buffer slow character spawned"), SlowCharacter);
	TestNotNull(TEXT("Slide buffer sprint character spawned"), BufferedCharacter);
	if (!SlowCharacter || !BufferedCharacter)
	{
		return false;
	}
	SlowCharacter->GetCharacterMovement()->SetUpdatedComponent(SlowCharacter->GetCapsuleComponent());
	BufferedCharacter->GetCharacterMovement()->SetUpdatedComponent(BufferedCharacter->GetCapsuleComponent());
	SlowCharacter->SpawnDefaultController();
	BufferedCharacter->SpawnDefaultController();

	UArenaDuelCharacterMovementComponent* SlowMovement = SlowCharacter->GetArenaDuelMovementComponent();
	SlowMovement->SetMovementMode(MOVE_Walking);
	SlowMovement->StartSlide();
	TestFalse(TEXT("Slow Ctrl press does not slide"), SlowMovement->IsSliding());
	TestFalse(TEXT("Slow Ctrl press does not queue slide"), SlowMovement->IsSlideQueued());
	TestFalse(TEXT("Slow Ctrl press does not crouch"), SlowCharacter->IsCrouched());
	TestTrue(TEXT("Slow Ctrl press retains slide intent"), SlowMovement->WantsSlideIntent());
	SlowMovement->StopSlide();

	UArenaDuelCharacterMovementComponent* BufferedMovement = BufferedCharacter->GetArenaDuelMovementComponent();
	BufferedMovement->SetMovementMode(MOVE_Walking);
	BufferedMovement->StartSprint();
	BufferedMovement->Velocity = FVector(600.0f, 0.0f, 0.0f);
	BufferedMovement->StartSlide();
	TestFalse(TEXT("Early Ctrl press does not immediately crouch"), BufferedMovement->IsCrouching());
	TestTrue(TEXT("Early Ctrl press queues slide"), BufferedMovement->IsSlideQueued());
	BufferedMovement->StopSlide();
	BufferedMovement->SetMovementMode(MOVE_Walking);
	BufferedMovement->Velocity = FVector(720.0f, 0.0f, 0.0f);
	BufferedMovement->StartSlide();
	TestTrue(TEXT("Valid speed enters slide immediately"), BufferedMovement->IsSliding());
	BufferedMovement->StopSlide();

	TestFalse(TEXT("Releasing Ctrl cancels queued slide"), BufferedMovement->IsSlideQueued());

	SlowCharacter->Destroy();
	BufferedCharacter->Destroy();
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
		FVector SpawnLocation = Location;
		if (SpawnLocation.Z < 50.0f)
		{
			SpawnLocation.Z = 88.0f;
		}
		AArenaDuelCharacter* Character = World && CharacterClass ? World->SpawnActor<AArenaDuelCharacter>(CharacterClass, SpawnLocation, FRotator::ZeroRotator) : nullptr;
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
	AArenaDuelCharacter* Character = ArenaDuelPhase4HardeningTests::SpawnCharacter(GEditor->GetEditorWorldContext().World(), FVector::ZeroVector);
	UArenaDuelCharacterMovementComponent* Move = ArenaDuelPhase4HardeningTests::Movement(Character);
	TestNotNull(TEXT("Crouch movement"), Move);
	if (Move && Character) { Move->NavAgentProps.bCanCrouch = true; Move->SetMovementMode(MOVE_Walking); Character->Crouch(); Move->TickComponent(0.016f, LEVELTICK_All, nullptr); TestTrue(TEXT("Native crouch enters crouched state"), Character->IsCrouched()); TestFalse(TEXT("Native crouch never enters slide"), Move->IsSliding()); Character->UnCrouch(); Move->TickComponent(0.016f, LEVELTICK_All, nullptr); TestFalse(TEXT("Native crouch release uncrouches"), Character->IsCrouched()); } return true;
})

ARENA_PHASE4_COMPONENT_TEST(FArenaDuelPhase4SlideTest, "ArenaDuel.Phase4.Slide", {
	FAutomationEditorCommonUtils::LoadMap(ArenaDuelPhase4Tests::MovementMap);
	UArenaDuelCharacterMovementComponent* Move = ArenaDuelPhase4HardeningTests::Movement(ArenaDuelPhase4HardeningTests::SpawnCharacter(GEditor->GetEditorWorldContext().World(), FVector::ZeroVector));
	Move->SetMovementMode(MOVE_Walking); Move->Velocity = FVector(800.0f, 0.0f, 0.0f); Move->StartSlide();
	TestTrue(TEXT("Slide entered above threshold"), Move->IsSliding()); TestTrue(TEXT("Slide is capped"), Move->Velocity.Size2D() <= Move->GlobalMomentumCap); return true;
})

ARENA_PHASE4_COMPONENT_TEST(FArenaDuelPhase4SlideJumpTest, "ArenaDuel.Phase4.SlideJump", {
	FAutomationEditorCommonUtils::LoadMap(ArenaDuelPhase4Tests::MovementMap);
	UArenaDuelCharacterMovementComponent* Move = ArenaDuelPhase4HardeningTests::Movement(ArenaDuelPhase4HardeningTests::SpawnCharacter(GEditor->GetEditorWorldContext().World(), FVector::ZeroVector));
	Move->SetMovementMode(MOVE_Walking); Move->Velocity = FVector(800.0f, 0.0f, 0.0f); Move->StartSlide(); const bool bJumped = Move->TrySlideJump();
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
	AArenaDuelCharacter* RemotePawn = nullptr;
	AStaticMeshActor* FloorActor = nullptr;
	AStaticMeshActor* LeftWallActor = nullptr;
	AStaticMeshActor* RightWallActor = nullptr;
	AStaticMeshActor* VaultActor = nullptr;
	FVector ClientStart = FVector::ZeroVector;
	FVector ServerStartLocation = FVector::ZeroVector;
	bool bServerStartValid = false;
	bool bClientStartCaptured = false;
	int32 StableConvergenceFrames = 0;
	FVector RemoteStartLocation = FVector::ZeroVector;
	FVector RemoteStartVelocity = FVector::ZeroVector;
	EMovementMode RemoteStartMovementMode = MOVE_None;
	uint8 RemoteStartCustomMovementMode = 0;
	bool RemoteStartSprintIntent = false;
	bool RemoteStartCrouchIntent = false;
	float InitialClientStamina = 0.0f;
	float InitialServerStamina = 0.0f;
	float LowestClientStamina = 0.0f;
	float LowestServerStamina = 0.0f;
	int32 ServerSettleFrames = 0;
	bool bSlideJumpInjected = false;
	bool bServerAdvancedMovementObserved = false;
};

namespace ArenaDuelPhase4NetworkTests
{
	struct FAuthoritativeSnapshot
	{
		bool bValid = false;
		bool bStaminaValid = false;
		float Stamina = 0.0f;
		FVector Location = FVector::ZeroVector;
		FVector Velocity = FVector::ZeroVector;
		EMovementMode MovementMode = MOVE_None;
		uint8 CustomMovementMode = 0;
		bool bSprintIntent = false;
		bool bCrouchIntent = false;
	};

	static FAuthoritativeSnapshot LastServerSnapshot;

	static int32 StartScenarioId(const FVector& Location)
	{
		if (FMath::IsNearlyEqual(Location.X, -2500.0f)) return 1;
		if (FMath::IsNearlyEqual(Location.X, -1200.0f)) return 2;
		if (FMath::IsNearlyEqual(Location.X, 500.0f) && FMath::IsNearlyEqual(Location.Y, 390.0f)) return 3;
		if (FMath::IsNearlyEqual(Location.X, 820.0f)) return 4;
		return 0;
	}

	static bool StartLocationFromScenario(int32 ScenarioId, FVector& OutLocation)
	{
		switch (ScenarioId)
		{
		case 1: OutLocation = FVector(-2500.0f, 0.0f, 100.0f); return true;
		case 2: OutLocation = FVector(-1200.0f, 0.0f, 100.0f); return true;
		case 3: OutLocation = FVector(500.0f, 390.0f, 100.0f); return true;
		case 4: OutLocation = FVector(820.0f, 0.0f, 100.0f); return true;
		default: return false;
		}
	}

	static void ConfigureFixtureMesh(AStaticMeshActor& Actor, const FVector& Location, const FVector& Scale);

	static UInputAction* LoadAction(const TCHAR* Path)
	{
		return LoadObject<UInputAction>(nullptr, Path);
	}

	static AArenaDuelCharacter* FindServerClientPawn(FArenaDuelPhase4NetworkState& State)
	{
		if (State.ClientConnections.Num() > 0 && State.ClientConnections[0])
		{
			APlayerController* ClientController = State.ClientConnections[0]->PlayerController;
			for (TActorIterator<AArenaDuelCharacter> It(State.World); It; ++It)
			{
				if (*It && ClientController && ((*It)->GetController() == ClientController || ((*It)->GetPlayerState() && (*It)->GetPlayerState() == ClientController->PlayerState)))
				{
					return *It;
				}
			}
			if (ClientController)
			{
				return Cast<AArenaDuelCharacter>(ClientController->GetPawn());
			}
			if (AArenaDuelCharacter* ViewPawn = Cast<AArenaDuelCharacter>(State.ClientConnections[0]->ViewTarget))
			{
				return ViewPawn;
			}
		}
		return nullptr;
	}

	static AArenaDuelCharacter* FindRemoteCharacter(UWorld* World, AArenaDuelCharacter* Owned)
	{
		if (!World || !Owned)
		{
			return nullptr;
		}
		APlayerState* RemotePlayerState = nullptr;
		if (AGameStateBase* GameState = World->GetGameState())
		{
			for (const TObjectPtr<APlayerState>& PlayerStateObject : GameState->PlayerArray)
			{
				APlayerState* PlayerState = PlayerStateObject.Get();
				if (PlayerState && PlayerState != Owned->GetPlayerState() && (!RemotePlayerState || PlayerState->GetPlayerId() < RemotePlayerState->GetPlayerId()))
				{
					RemotePlayerState = PlayerState;
				}
			}
		}
		if (!RemotePlayerState)
		{
			return nullptr;
		}
		for (TActorIterator<AArenaDuelCharacter> It(World); It; ++It)
		{
			if (*It != Owned && (*It)->GetPlayerState() == RemotePlayerState)
			{
				return *It;
			}
		}
		if (AController* OwnedController = Owned->GetController())
		{
			for (TActorIterator<AArenaDuelCharacter> It(World); It; ++It)
			{
				if (*It != Owned && (*It)->GetController() && (*It)->GetController() != OwnedController)
				{
					return *It;
				}
			}
		}
		return nullptr;
	}

	static void PrepareClient(FArenaDuelPhase4NetworkState& State)
	{
		APlayerController* Controller = State.World->GetFirstPlayerController();
		State.OwnedPawn = Controller ? Cast<AArenaDuelCharacter>(Controller->GetPawn()) : nullptr;
		State.RemotePawn = FindRemoteCharacter(State.World, State.OwnedPawn);
		if (State.OwnedPawn)
		{
			Controller->SetControlRotation(FRotator(0.0f, 0.0f, 0.0f));
			if (State.InitialClientStamina <= 0.0f)
			{
				State.InitialClientStamina = State.OwnedPawn->GetArenaDuelMovementComponent()->GetStamina();
				State.LowestClientStamina = State.InitialClientStamina;
			}
		}
		if (State.RemotePawn && State.RemoteStartMovementMode == MOVE_None && State.RemotePawn->GetCharacterMovement()->MovementMode == MOVE_Walking)
		{
			State.RemoteStartLocation = State.RemotePawn->GetActorLocation();
			State.RemoteStartVelocity = State.RemotePawn->GetCharacterMovement()->Velocity;
			State.RemoteStartMovementMode = State.RemotePawn->GetCharacterMovement()->MovementMode;
			State.RemoteStartCustomMovementMode = State.RemotePawn->GetCharacterMovement()->CustomMovementMode;
			State.RemoteStartSprintIntent = State.RemotePawn->GetArenaDuelMovementComponent()->WantsSprintIntent();
			State.RemoteStartCrouchIntent = State.RemotePawn->GetArenaDuelMovementComponent()->WantsSlideIntent();
		}
		if (State.FloorActor) ConfigureFixtureMesh(*State.FloorActor, FVector(0.0f, 0.0f, -50.0f), FVector(80.0f, 8.0f, 1.0f));
		if (State.LeftWallActor) ConfigureFixtureMesh(*State.LeftWallActor, FVector(800.0f, 450.0f, 180.0f), FVector(8.0f, 0.4f, 3.0f));
		if (State.RightWallActor) ConfigureFixtureMesh(*State.RightWallActor, FVector(800.0f, -450.0f, 180.0f), FVector(8.0f, 0.4f, 3.0f));
		if (State.VaultActor) ConfigureFixtureMesh(*State.VaultActor, FVector(1100.0f, 0.0f, 50.0f), FVector(1.0f, 4.0f, 2.0f));
	}

	static bool HasInputMapping(FArenaDuelPhase4NetworkState& State)
	{
		APlayerController* Controller = State.World ? State.World->GetFirstPlayerController() : nullptr;
		UEnhancedInputLocalPlayerSubsystem* Input = ArenaDuelPhase4HardeningTests::GetLocalInputSubsystem(Controller);
		UInputMappingContext* Context = LoadObject<UInputMappingContext>(nullptr, TEXT("/Game/ArenaDuel/Input/IMC_Gameplay.IMC_Gameplay"));
		return Input && Context && Input->HasMappingContext(Context);
	}

	static bool HasInitialReplication(FArenaDuelPhase4NetworkState& State)
	{
		const FVector Location = State.OwnedPawn ? State.OwnedPawn->GetActorLocation() : FVector::ZeroVector;
		if (!State.bServerStartValid && State.OwnedPawn && State.OwnedPawn->GetPlayerState())
		{
			FVector ReplicatedStartLocation;
			if (StartLocationFromScenario(FMath::RoundToInt(State.OwnedPawn->GetPlayerState()->GetScore()), ReplicatedStartLocation))
			{
				State.ServerStartLocation = ReplicatedStartLocation;
				State.bServerStartValid = true;
			}
		}
		const bool bConvergedToServerStart = State.bServerStartValid && FVector::Dist2D(Location, State.ServerStartLocation) <= 150.0f;
		if (State.OwnedPawn && ArenaDuelPhase4HardeningTests::IsFinite(Location) && bConvergedToServerStart)
		{
			if (!State.bClientStartCaptured)
			{
				State.ClientStart = Location;
				State.bClientStartCaptured = true;
			}
			return true;
		}
		return false;
	}

	static bool HasFixtureGeometry(FArenaDuelPhase4NetworkState& State)
	{
		return State.FloorActor && State.LeftWallActor && State.RightWallActor && State.VaultActor && HasInitialReplication(State);
	}

	static bool PlaceServerPawn(FArenaDuelPhase4NetworkState& State, const FVector& Location, const FVector& InitialVelocity = FVector::ZeroVector)
	{
		if (AArenaDuelCharacter* Pawn = FindServerClientPawn(State))
		{
			Pawn->SetActorLocation(Location, false, nullptr, ETeleportType::TeleportPhysics);
			Pawn->SetActorRotation(FRotator::ZeroRotator);
			if (UArenaDuelCharacterMovementComponent* Movement = Pawn->GetArenaDuelMovementComponent())
			{
				Movement->StopSprint();
				Movement->StopSlide();
				Movement->Velocity = InitialVelocity;
				Movement->SetMovementMode(MOVE_Walking);
			}
			Pawn->ForceNetUpdate();
			if (Pawn->GetPlayerState())
			{
				Pawn->GetPlayerState()->SetScore(static_cast<float>(StartScenarioId(Location)));
				Pawn->GetPlayerState()->ForceNetUpdate();
			}
			State.ServerStartLocation = Location;
			State.bServerStartValid = true;
			State.bClientStartCaptured = false;
			State.StableConvergenceFrames = 0;
			State.ServerSettleFrames = 0;
			State.RemoteStartMovementMode = MOVE_None;
			return true;
		}
		return false;
	}

	static void StartInput(FArenaDuelPhase4NetworkState& State, bool bSprint = true)
	{
		if (UEnhancedInputLocalPlayerSubsystem* Input = ArenaDuelPhase4HardeningTests::GetLocalInputSubsystem(State.World->GetFirstPlayerController()))
		{
			if (bSprint)
			{
				Input->StartContinuousInputInjectionForAction(LoadAction(TEXT("/Game/ArenaDuel/Input/IA_Sprint.IA_Sprint")), FInputActionValue(true), {}, {});
				Input->InjectInputForAction(LoadAction(TEXT("/Game/ArenaDuel/Input/IA_Sprint.IA_Sprint")), FInputActionValue(true), {}, {});
			}
			Input->StartContinuousInputInjectionForAction(LoadAction(TEXT("/Game/ArenaDuel/Input/IA_Move.IA_Move")), FInputActionValue(FVector2D(0.0f, 1.0f)), {}, {});
			Input->InjectInputForAction(LoadAction(TEXT("/Game/ArenaDuel/Input/IA_Move.IA_Move")), FInputActionValue(FVector2D(0.0f, 1.0f)), {}, {});
		}
	}

	static void StartCrouch(FArenaDuelPhase4NetworkState& State)
	{
		if (UEnhancedInputLocalPlayerSubsystem* Input = ArenaDuelPhase4HardeningTests::GetLocalInputSubsystem(State.World->GetFirstPlayerController()))
		{
			Input->StartContinuousInputInjectionForAction(LoadAction(TEXT("/Game/ArenaDuel/Input/IA_Crouch.IA_Crouch")), FInputActionValue(true), {}, {});
			Input->InjectInputForAction(LoadAction(TEXT("/Game/ArenaDuel/Input/IA_Crouch.IA_Crouch")), FInputActionValue(true), {}, {});
		}
	}

	static void StartSlide(FArenaDuelPhase4NetworkState& State)
	{
		if (UEnhancedInputLocalPlayerSubsystem* Input = ArenaDuelPhase4HardeningTests::GetLocalInputSubsystem(State.World->GetFirstPlayerController()))
		{
			UInputAction* SlideAction = LoadAction(TEXT("/Game/ArenaDuel/Input/IA_Slide.IA_Slide"));
			Input->StartContinuousInputInjectionForAction(SlideAction, FInputActionValue(true), {}, {});
			Input->InjectInputForAction(SlideAction, FInputActionValue(true), {}, {});
		}
	}

	static void InjectJump(FArenaDuelPhase4NetworkState& State)
	{
		if (UEnhancedInputLocalPlayerSubsystem* Input = ArenaDuelPhase4HardeningTests::GetLocalInputSubsystem(State.World->GetFirstPlayerController()))
		{
			Input->InjectInputForAction(LoadAction(TEXT("/Game/ArenaDuel/Input/IA_Jump.IA_Jump")), FInputActionValue(true), {}, {});
		}
	}

	static void StartHeldJump(FArenaDuelPhase4NetworkState& State)
	{
		if (UEnhancedInputLocalPlayerSubsystem* Input = ArenaDuelPhase4HardeningTests::GetLocalInputSubsystem(State.World->GetFirstPlayerController()))
		{
			UInputAction* JumpAction = LoadAction(TEXT("/Game/ArenaDuel/Input/IA_Jump.IA_Jump"));
			Input->StartContinuousInputInjectionForAction(JumpAction, FInputActionValue(true), {}, {});
			Input->InjectInputForAction(JumpAction, FInputActionValue(true), {}, {});
		}
	}

	static void StopHeldJump(FArenaDuelPhase4NetworkState& State)
	{
		if (UEnhancedInputLocalPlayerSubsystem* Input = ArenaDuelPhase4HardeningTests::GetLocalInputSubsystem(State.World->GetFirstPlayerController()))
		{
			Input->StopContinuousInputInjectionForAction(LoadAction(TEXT("/Game/ArenaDuel/Input/IA_Jump.IA_Jump")));
		}
	}

	static void StopAllInput(FArenaDuelPhase4NetworkState& State)
	{
		if (UEnhancedInputLocalPlayerSubsystem* Input = ArenaDuelPhase4HardeningTests::GetLocalInputSubsystem(State.World->GetFirstPlayerController()))
		{
			Input->StopContinuousInputInjectionForAction(LoadAction(TEXT("/Game/ArenaDuel/Input/IA_Move.IA_Move")));
			Input->StopContinuousInputInjectionForAction(LoadAction(TEXT("/Game/ArenaDuel/Input/IA_Sprint.IA_Sprint")));
			Input->StopContinuousInputInjectionForAction(LoadAction(TEXT("/Game/ArenaDuel/Input/IA_Crouch.IA_Crouch")));
			Input->StopContinuousInputInjectionForAction(LoadAction(TEXT("/Game/ArenaDuel/Input/IA_Slide.IA_Slide")));
			Input->StopContinuousInputInjectionForAction(LoadAction(TEXT("/Game/ArenaDuel/Input/IA_Jump.IA_Jump")));
		}
		if (State.OwnedPawn)
		{
			State.OwnedPawn->GetArenaDuelMovementComponent()->StopSprint();
			State.OwnedPawn->GetArenaDuelMovementComponent()->StopSlide();
		}
	}

	static bool IsFinite(const FVector& Value)
	{
		return FMath::IsFinite(Value.X) && FMath::IsFinite(Value.Y) && FMath::IsFinite(Value.Z);
	}

	static bool HasConverged(FArenaDuelPhase4NetworkState& State)
	{
		if (!State.OwnedPawn || !LastServerSnapshot.bValid)
		{
			return false;
		}
		const UArenaDuelCharacterMovementComponent* ClientMove = State.OwnedPawn->GetArenaDuelMovementComponent();
		const bool bFrameConverged = ClientMove && IsFinite(State.OwnedPawn->GetActorLocation()) && IsFinite(LastServerSnapshot.Location) && IsFinite(ClientMove->Velocity) && IsFinite(LastServerSnapshot.Velocity) && FVector::Dist2D(State.OwnedPawn->GetActorLocation(), LastServerSnapshot.Location) <= 200.0f && FVector::Dist2D(ClientMove->Velocity, LastServerSnapshot.Velocity) <= 350.0f && ClientMove->MovementMode == LastServerSnapshot.MovementMode && ClientMove->CustomMovementMode == LastServerSnapshot.CustomMovementMode;
		State.StableConvergenceFrames = bFrameConverged ? State.StableConvergenceFrames + 1 : 0;
		return State.StableConvergenceFrames >= 5;
	}

	static void CaptureServerSnapshot(FArenaDuelPhase4NetworkState& State)
	{
		AArenaDuelCharacter* ServerPawn = FindServerClientPawn(State);
		if (!ServerPawn || !ServerPawn->GetArenaDuelMovementComponent())
		{
			return;
		}
		const UArenaDuelCharacterMovementComponent* Movement = ServerPawn->GetArenaDuelMovementComponent();
		LastServerSnapshot.bValid = true;
		LastServerSnapshot.Location = ServerPawn->GetActorLocation();
		LastServerSnapshot.Velocity = Movement->Velocity;
		LastServerSnapshot.MovementMode = Movement->MovementMode;
		LastServerSnapshot.CustomMovementMode = Movement->CustomMovementMode;
		LastServerSnapshot.bSprintIntent = Movement->WantsSprintIntent();
		LastServerSnapshot.bCrouchIntent = Movement->WantsSlideIntent();
	}

	static bool CaptureServerSnapshotAfterSettle(FArenaDuelPhase4NetworkState& State)
	{
		if (++State.ServerSettleFrames < 10)
		{
			return false;
		}
		CaptureServerSnapshot(State);
		return LastServerSnapshot.bValid;
	}

	static bool IsVault(AArenaDuelCharacter* Pawn);

	static bool IsVaultComplete(AArenaDuelCharacter* Pawn)
	{
		if (!Pawn)
		{
			return false;
		}
		const UCharacterMovementComponent* Movement = Pawn->GetCharacterMovement();
		return Movement && (Movement->IsMovingOnGround() || Movement->IsFalling()) && !IsVault(Pawn) && Pawn->GetActorLocation().X > 1050.0f && !ArenaDuelPhase4HardeningTests::IsCapsuleOverlapping(Pawn);
	}

	static void CaptureRemoteBaseline(FArenaDuelPhase4NetworkState& State)
	{
		if (State.RemoteStartMovementMode != MOVE_None)
		{
			return;
		}
		AArenaDuelCharacter* Owned = State.OwnedPawn;
		if (!Owned && State.World)
		{
			Owned = FindServerClientPawn(State);
		}
		if (AArenaDuelCharacter* Remote = FindRemoteCharacter(State.World, Owned))
		{
			if (Remote->GetCharacterMovement()->MovementMode != MOVE_Walking)
			{
				return;
			}
			State.RemoteStartLocation = Remote->GetActorLocation();
			State.RemoteStartVelocity = Remote->GetCharacterMovement()->Velocity;
			State.RemoteStartMovementMode = Remote->GetCharacterMovement()->MovementMode;
			State.RemoteStartCustomMovementMode = Remote->GetCharacterMovement()->CustomMovementMode;
			State.RemoteStartSprintIntent = Remote->GetArenaDuelMovementComponent()->WantsSprintIntent();
			State.RemoteStartCrouchIntent = Remote->GetArenaDuelMovementComponent()->WantsSlideIntent();
		}
	}

	static bool RemotePawnIsIsolated(FArenaDuelPhase4NetworkState& State)
	{
		CaptureRemoteBaseline(State);
		AArenaDuelCharacter* RemotePawn = State.RemotePawn;
		if (!RemotePawn)
		{
			RemotePawn = FindRemoteCharacter(State.World, FindServerClientPawn(State));
		}
		if (!RemotePawn)
		{
			return false;
		}
		const UArenaDuelCharacterMovementComponent* Movement = RemotePawn->GetArenaDuelMovementComponent();
		const bool bIsolated = Movement && FVector::Dist2D(RemotePawn->GetActorLocation(), State.RemoteStartLocation) <= 100.0f && FVector::Dist2D(Movement->Velocity, State.RemoteStartVelocity) <= 100.0f && Movement->MovementMode == State.RemoteStartMovementMode && Movement->CustomMovementMode == State.RemoteStartCustomMovementMode && Movement->WantsSprintIntent() == State.RemoteStartSprintIntent && Movement->WantsSlideIntent() == State.RemoteStartCrouchIntent;
		return bIsolated;
	}

	static bool IsVault(AArenaDuelCharacter* Pawn)
	{
		if (!Pawn)
		{
			return false;
		}
		const UArenaDuelCharacterMovementComponent* Movement = Pawn->GetArenaDuelMovementComponent();
		return Movement && Movement->MovementMode == MOVE_Custom && Movement->CustomMovementMode == static_cast<uint8>(EArenaDuelCustomMovementMode::Vault);
	}

	static void ConfigureFixtureMesh(AStaticMeshActor& Actor, const FVector& Location, const FVector& Scale)
	{
		if (Actor.HasAuthority())
		{
			Actor.SetReplicates(true);
		}
		Actor.GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
		Actor.GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
		Actor.GetStaticMeshComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Actor.GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));
		Actor.GetStaticMeshComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
		Actor.SetActorEnableCollision(true);
		Actor.GetStaticMeshComponent()->RecreatePhysicsState();
		Actor.SetActorLocation(Location);
		Actor.SetActorScale3D(Scale);
	}

}

NETWORK_TEST_CLASS(FArenaDuelPhase4NetworkTest, "ArenaDuel.Phase4.Network")
{
	FPIENetworkComponent<FArenaDuelPhase4NetworkState> Network{TestRunner, TestCommandBuilder, bInitializing};

	BEFORE_EACH()
	{
		ArenaDuelPhase4NetworkTests::LastServerSnapshot = {};
		FAutomationEditorCommonUtils::LoadMap(ArenaDuelPhase4Tests::MovementMap);
		UClass* GameModeClass = LoadClass<AGameModeBase>(nullptr, ArenaDuelPhase4HardeningTests::GameModeClass);
		FNetworkComponentBuilder<FArenaDuelPhase4NetworkState>()
			.WithClients(1)
			.AsListenServer()
			.WithGameMode(GameModeClass)
			.Build(Network);
		Network.SpawnAndReplicate<AStaticMeshActor, &FArenaDuelPhase4NetworkState::FloorActor>([](AStaticMeshActor& Actor) { ArenaDuelPhase4NetworkTests::ConfigureFixtureMesh(Actor, FVector(0.0f, 0.0f, -50.0f), FVector(80.0f, 8.0f, 1.0f)); });
		Network.SpawnAndReplicate<AStaticMeshActor, &FArenaDuelPhase4NetworkState::LeftWallActor>([](AStaticMeshActor& Actor) { ArenaDuelPhase4NetworkTests::ConfigureFixtureMesh(Actor, FVector(800.0f, 450.0f, 180.0f), FVector(8.0f, 0.4f, 3.0f)); });
		Network.SpawnAndReplicate<AStaticMeshActor, &FArenaDuelPhase4NetworkState::RightWallActor>([](AStaticMeshActor& Actor) { ArenaDuelPhase4NetworkTests::ConfigureFixtureMesh(Actor, FVector(800.0f, -450.0f, 180.0f), FVector(8.0f, 0.4f, 3.0f)); });
		Network.SpawnAndReplicate<AStaticMeshActor, &FArenaDuelPhase4NetworkState::VaultActor>([](AStaticMeshActor& Actor) { ArenaDuelPhase4NetworkTests::ConfigureFixtureMesh(Actor, FVector(1100.0f, 0.0f, 50.0f), FVector(1.0f, 4.0f, 2.0f)); });
	}

	TEST_METHOD(SprintAndCrouchIntent)
	{
		Network
			.UntilServer(TEXT("Place sprint fixture"), [](FArenaDuelPhase4NetworkState& State) { return ArenaDuelPhase4NetworkTests::PlaceServerPawn(State, FVector(-2500.0f, 0.0f, 100.0f)); }, FTimespan::FromSeconds(5.0))
			.UntilClient(TEXT("Wait for pawns, geometry and Enhanced Input"), 0, [](FArenaDuelPhase4NetworkState& State) { ArenaDuelPhase4NetworkTests::PrepareClient(State); return State.OwnedPawn && State.RemotePawn && ArenaDuelPhase4NetworkTests::HasInputMapping(State) && ArenaDuelPhase4NetworkTests::HasFixtureGeometry(State); }, FTimespan::FromSeconds(5.0))
			.ThenClient(TEXT("Inject sprint intent"), 0, [](FArenaDuelPhase4NetworkState& State)
			{
				ArenaDuelPhase4NetworkTests::StartInput(State);
			})
			.UntilClient(TEXT("Client retains sprint intent"), 0, [](FArenaDuelPhase4NetworkState& State) { return State.OwnedPawn && State.OwnedPawn->GetArenaDuelMovementComponent()->WantsSprintIntent(); }, FTimespan::FromSeconds(2.0))
			.UntilServer(TEXT("Server reconstructs sprint intent"), [](FArenaDuelPhase4NetworkState& State)
			{
				AArenaDuelCharacter* Pawn = ArenaDuelPhase4NetworkTests::FindServerClientPawn(State);
				return Pawn && Pawn->GetArenaDuelMovementComponent()->WantsSprintIntent();
			}, FTimespan::FromSeconds(5.0))
			.ThenClient(TEXT("Inject crouch slide intent"), 0, [](FArenaDuelPhase4NetworkState& State)
			{
				ArenaDuelPhase4NetworkTests::StartCrouch(State);
			})
			.UntilServer(TEXT("Server reconstructs native crouch"), [](FArenaDuelPhase4NetworkState& State)
			{
				AArenaDuelCharacter* Pawn = ArenaDuelPhase4NetworkTests::FindServerClientPawn(State);
				return Pawn && Pawn->IsCrouched();
			}, FTimespan::FromSeconds(5.0))
			.ThenClient(TEXT("Clean sprint and crouch input"), 0, [](FArenaDuelPhase4NetworkState& State) { ArenaDuelPhase4NetworkTests::StopAllInput(State); })
			.UntilServer(TEXT("Capture sprint and crouch authority"), [](FArenaDuelPhase4NetworkState& State) { return ArenaDuelPhase4NetworkTests::CaptureServerSnapshotAfterSettle(State); })
			.UntilClient(TEXT("Sprint and crouch converge"), 0, [](FArenaDuelPhase4NetworkState& State) { return ArenaDuelPhase4NetworkTests::HasConverged(State); }, FTimespan::FromSeconds(5.0));
	}

	TEST_METHOD(Slide)
	{
		Network
			.UntilServer(TEXT("Place slide fixture"), [](FArenaDuelPhase4NetworkState& State) { return ArenaDuelPhase4NetworkTests::PlaceServerPawn(State, FVector(-2500.0f, 0.0f, 100.0f)); }, FTimespan::FromSeconds(5.0))
			.UntilClient(TEXT("Prepare slide client"), 0, [](FArenaDuelPhase4NetworkState& State) { ArenaDuelPhase4NetworkTests::PrepareClient(State); return State.OwnedPawn && State.RemotePawn && ArenaDuelPhase4NetworkTests::HasInputMapping(State) && ArenaDuelPhase4NetworkTests::HasFixtureGeometry(State) && ArenaDuelPhase4NetworkTests::HasInitialReplication(State); }, FTimespan::FromSeconds(5.0))
			.ThenClient(TEXT("Build slide speed through Enhanced Input"), 0, [](FArenaDuelPhase4NetworkState& State) { ArenaDuelPhase4NetworkTests::StartInput(State); })
			.UntilClient(TEXT("Client reaches slide speed"), 0, [](FArenaDuelPhase4NetworkState& State) { return State.OwnedPawn && State.OwnedPawn->GetCharacterMovement()->IsMovingOnGround() && State.OwnedPawn->GetCharacterMovement()->Velocity.Size2D() >= State.OwnedPawn->GetArenaDuelMovementComponent()->SlideMinSpeed; }, FTimespan::FromSeconds(5.0))
			.ThenClient(TEXT("Trigger slide through dedicated slide input"), 0, [](FArenaDuelPhase4NetworkState& State) { ArenaDuelPhase4NetworkTests::StartSlide(State); })
			.UntilClient(TEXT("Client enters slide"), 0, [](FArenaDuelPhase4NetworkState& State) { return State.OwnedPawn && State.OwnedPawn->GetArenaDuelMovementComponent()->IsSliding(); }, FTimespan::FromSeconds(5.0))
			.UntilServer(TEXT("Server reconstructs slide"), [](FArenaDuelPhase4NetworkState& State) { AArenaDuelCharacter* Pawn = ArenaDuelPhase4NetworkTests::FindServerClientPawn(State); return Pawn && Pawn->GetArenaDuelMovementComponent()->IsSliding(); }, FTimespan::FromSeconds(5.0))
			.ThenClient(TEXT("Release slide input"), 0, [](FArenaDuelPhase4NetworkState& State) { ArenaDuelPhase4NetworkTests::StopAllInput(State); })
			.UntilServer(TEXT("Server exits slide"), [](FArenaDuelPhase4NetworkState& State) { AArenaDuelCharacter* Pawn = ArenaDuelPhase4NetworkTests::FindServerClientPawn(State); return Pawn && !Pawn->GetArenaDuelMovementComponent()->IsSliding(); }, FTimespan::FromSeconds(5.0))
			.UntilServer(TEXT("Capture slide authority"), [](FArenaDuelPhase4NetworkState& State) { return ArenaDuelPhase4NetworkTests::CaptureServerSnapshotAfterSettle(State); })
			.UntilClient(TEXT("Slide converges"), 0, [](FArenaDuelPhase4NetworkState& State) { return !State.OwnedPawn->GetArenaDuelMovementComponent()->IsSliding() && ArenaDuelPhase4NetworkTests::HasConverged(State); }, FTimespan::FromSeconds(5.0));
	}

	TEST_METHOD(SlideJump)
	{
		Network
			.UntilServer(TEXT("Place slide jump fixture"), [](FArenaDuelPhase4NetworkState& State) { return ArenaDuelPhase4NetworkTests::PlaceServerPawn(State, FVector(-1200.0f, 0.0f, 100.0f)); }, FTimespan::FromSeconds(5.0))
			.UntilClient(TEXT("Prepare slide jump client"), 0, [](FArenaDuelPhase4NetworkState& State) { ArenaDuelPhase4NetworkTests::PrepareClient(State); return State.OwnedPawn && State.RemotePawn && ArenaDuelPhase4NetworkTests::HasInputMapping(State) && ArenaDuelPhase4NetworkTests::HasFixtureGeometry(State) && ArenaDuelPhase4NetworkTests::HasInitialReplication(State); }, FTimespan::FromSeconds(5.0))
			.ThenClient(TEXT("Build slide jump speed through Enhanced Input"), 0, [](FArenaDuelPhase4NetworkState& State) { ArenaDuelPhase4NetworkTests::StartInput(State); })
			.UntilClient(TEXT("Client reaches slide speed"), 0, [](FArenaDuelPhase4NetworkState& State) { return State.OwnedPawn && State.OwnedPawn->GetCharacterMovement()->IsMovingOnGround() && State.OwnedPawn->GetCharacterMovement()->Velocity.Size2D() >= State.OwnedPawn->GetArenaDuelMovementComponent()->SlideMinSpeed; }, FTimespan::FromSeconds(5.0))
			.ThenClient(TEXT("Enter slide through dedicated slide input"), 0, [](FArenaDuelPhase4NetworkState& State) { ArenaDuelPhase4NetworkTests::StartSlide(State); })
			.UntilClient(TEXT("Client is actually sliding"), 0, [](FArenaDuelPhase4NetworkState& State) { return State.OwnedPawn && State.OwnedPawn->GetArenaDuelMovementComponent()->IsSliding(); }, FTimespan::FromSeconds(5.0))
			.UntilServer(TEXT("Server is actually sliding"), [](FArenaDuelPhase4NetworkState& State) { AArenaDuelCharacter* Pawn = ArenaDuelPhase4NetworkTests::FindServerClientPawn(State); return Pawn && Pawn->GetArenaDuelMovementComponent()->IsSliding(); }, FTimespan::FromSeconds(5.0))
			.UntilClient(TEXT("Client remains sliding at jump boundary"), 0, [](FArenaDuelPhase4NetworkState& State) { ArenaDuelPhase4NetworkTests::StartSlide(State); return State.OwnedPawn && State.OwnedPawn->GetArenaDuelMovementComponent()->IsSliding(); }, FTimespan::FromSeconds(2.0))
			.UntilClient(TEXT("Client predicts slide jump"), 0, [](FArenaDuelPhase4NetworkState& State) { if (State.OwnedPawn && State.OwnedPawn->GetArenaDuelMovementComponent()->IsSliding() && !State.bSlideJumpInjected) { State.bSlideJumpInjected = true; ArenaDuelPhase4NetworkTests::InjectJump(State); } return State.OwnedPawn && State.OwnedPawn->GetCharacterMovement()->IsFalling() && State.OwnedPawn->GetCharacterMovement()->Velocity.Z > 0.0f; }, FTimespan::FromSeconds(5.0))
			.UntilServer(TEXT("Server executes slide jump"), [](FArenaDuelPhase4NetworkState& State) { AArenaDuelCharacter* Pawn = ArenaDuelPhase4NetworkTests::FindServerClientPawn(State); return Pawn && Pawn->GetCharacterMovement()->IsFalling() && Pawn->GetCharacterMovement()->Velocity.Z > 0.0f && Pawn->GetCharacterMovement()->Velocity.Size2D() >= 500.0f; }, FTimespan::FromSeconds(5.0))
			.ThenClient(TEXT("Stop slide jump input"), 0, [](FArenaDuelPhase4NetworkState& State) { ArenaDuelPhase4NetworkTests::StopAllInput(State); })
			.UntilServer(TEXT("Capture slide jump authority"), [](FArenaDuelPhase4NetworkState& State) { return ArenaDuelPhase4NetworkTests::CaptureServerSnapshotAfterSettle(State); })
			.UntilClient(TEXT("Slide jump converges"), 0, [](FArenaDuelPhase4NetworkState& State) { return ArenaDuelPhase4NetworkTests::HasConverged(State); }, FTimespan::FromSeconds(5.0));
	}

	TEST_METHOD(WallRun)
	{
		Network
			.UntilServer(TEXT("Place wall run fixture"), [](FArenaDuelPhase4NetworkState& State) { return ArenaDuelPhase4NetworkTests::PlaceServerPawn(State, FVector(500.0f, 390.0f, 100.0f)); }, FTimespan::FromSeconds(5.0))
			.UntilClient(TEXT("Prepare wall run client"), 0, [](FArenaDuelPhase4NetworkState& State) { ArenaDuelPhase4NetworkTests::PrepareClient(State); return State.OwnedPawn && State.RemotePawn && ArenaDuelPhase4NetworkTests::HasInputMapping(State) && ArenaDuelPhase4NetworkTests::HasFixtureGeometry(State); }, FTimespan::FromSeconds(5.0))
			.ThenClient(TEXT("Approach wall under sprint input"), 0, [](FArenaDuelPhase4NetworkState& State) { ArenaDuelPhase4NetworkTests::StartInput(State); })
			.UntilClient(TEXT("Client reaches jumping speed"), 0, [](FArenaDuelPhase4NetworkState& State) { return State.OwnedPawn && State.OwnedPawn->GetCharacterMovement()->Velocity.Size2D() >= State.OwnedPawn->GetArenaDuelMovementComponent()->WallRunMinSpeed; }, FTimespan::FromSeconds(5.0))
			.ThenClient(TEXT("Enter falling through jump input"), 0, [](FArenaDuelPhase4NetworkState& State) { ArenaDuelPhase4NetworkTests::InjectJump(State); })
			.UntilClient(TEXT("Client enters wall run"), 0, [](FArenaDuelPhase4NetworkState& State) { return State.OwnedPawn && State.OwnedPawn->GetArenaDuelMovementComponent()->IsWallRunning(); }, FTimespan::FromSeconds(8.0))
			.UntilServer(TEXT("Server accepts wall run"), [](FArenaDuelPhase4NetworkState& State) { AArenaDuelCharacter* Pawn = ArenaDuelPhase4NetworkTests::FindServerClientPawn(State); return Pawn && Pawn->GetArenaDuelMovementComponent()->IsWallRunning(); }, FTimespan::FromSeconds(8.0))
			.ThenClient(TEXT("Stop wall run input"), 0, [](FArenaDuelPhase4NetworkState& State) { ArenaDuelPhase4NetworkTests::StopAllInput(State); })
			.UntilServer(TEXT("Capture wall run authority"), [](FArenaDuelPhase4NetworkState& State) { return ArenaDuelPhase4NetworkTests::CaptureServerSnapshotAfterSettle(State); })
			.UntilClient(TEXT("Wall run converges"), 0, [](FArenaDuelPhase4NetworkState& State) { return ArenaDuelPhase4NetworkTests::HasConverged(State); }, FTimespan::FromSeconds(5.0));
	}

	TEST_METHOD(WallJump)
	{
		Network
			.UntilServer(TEXT("Place wall jump fixture"), [](FArenaDuelPhase4NetworkState& State) { return ArenaDuelPhase4NetworkTests::PlaceServerPawn(State, FVector(500.0f, 390.0f, 100.0f)); }, FTimespan::FromSeconds(5.0))
			.UntilClient(TEXT("Prepare wall jump client"), 0, [](FArenaDuelPhase4NetworkState& State) { ArenaDuelPhase4NetworkTests::PrepareClient(State); return State.OwnedPawn && State.RemotePawn && ArenaDuelPhase4NetworkTests::HasInputMapping(State) && ArenaDuelPhase4NetworkTests::HasFixtureGeometry(State); }, FTimespan::FromSeconds(5.0))
			.ThenClient(TEXT("Approach wall for jump"), 0, [](FArenaDuelPhase4NetworkState& State) { ArenaDuelPhase4NetworkTests::StartInput(State); })
			.UntilClient(TEXT("Client reaches jumping speed"), 0, [](FArenaDuelPhase4NetworkState& State) { return State.OwnedPawn && State.OwnedPawn->GetCharacterMovement()->Velocity.Size2D() >= State.OwnedPawn->GetArenaDuelMovementComponent()->WallRunMinSpeed; }, FTimespan::FromSeconds(5.0))
			.ThenClient(TEXT("Enter wall run"), 0, [](FArenaDuelPhase4NetworkState& State) { ArenaDuelPhase4NetworkTests::InjectJump(State); })
			.UntilClient(TEXT("Client enters wall run before jump"), 0, [](FArenaDuelPhase4NetworkState& State) { return State.OwnedPawn && State.OwnedPawn->GetArenaDuelMovementComponent()->IsWallRunning(); }, FTimespan::FromSeconds(8.0))
			.UntilServer(TEXT("Server enters wall run before jump"), [](FArenaDuelPhase4NetworkState& State) { AArenaDuelCharacter* Pawn = ArenaDuelPhase4NetworkTests::FindServerClientPawn(State); return Pawn && Pawn->GetArenaDuelMovementComponent()->IsWallRunning(); }, FTimespan::FromSeconds(8.0))
			.ThenClient(TEXT("Trigger wall jump through input"), 0, [](FArenaDuelPhase4NetworkState& State) { ArenaDuelPhase4NetworkTests::InjectJump(State); })
			.UntilClient(TEXT("Client executes wall jump"), 0, [](FArenaDuelPhase4NetworkState& State) { return State.OwnedPawn && State.OwnedPawn->GetCharacterMovement()->IsFalling() && State.OwnedPawn->GetCharacterMovement()->Velocity.Z > 0.0f && State.OwnedPawn->GetCharacterMovement()->Velocity.Y < -50.0f; }, FTimespan::FromSeconds(5.0))
			.UntilServer(TEXT("Server executes wall jump"), [](FArenaDuelPhase4NetworkState& State) { AArenaDuelCharacter* Pawn = ArenaDuelPhase4NetworkTests::FindServerClientPawn(State); return Pawn && Pawn->GetCharacterMovement()->IsFalling() && Pawn->GetCharacterMovement()->Velocity.Z > 0.0f && Pawn->GetCharacterMovement()->Velocity.Y < -50.0f; }, FTimespan::FromSeconds(5.0))
			.ThenClient(TEXT("Stop wall jump input"), 0, [](FArenaDuelPhase4NetworkState& State) { ArenaDuelPhase4NetworkTests::StopAllInput(State); })
			.UntilServer(TEXT("Capture wall jump authority"), [](FArenaDuelPhase4NetworkState& State) { return ArenaDuelPhase4NetworkTests::CaptureServerSnapshotAfterSettle(State); })
			.UntilClient(TEXT("Wall jump converges"), 0, [](FArenaDuelPhase4NetworkState& State) { return ArenaDuelPhase4NetworkTests::HasConverged(State); }, FTimespan::FromSeconds(5.0));
	}

	TEST_METHOD(Stamina)
	{
		Network
			.UntilServer(TEXT("Place stamina fixture"), [](FArenaDuelPhase4NetworkState& State) { if (!ArenaDuelPhase4NetworkTests::PlaceServerPawn(State, FVector(500.0f, 390.0f, 100.0f))) return false; if (AArenaDuelCharacter* Pawn = ArenaDuelPhase4NetworkTests::FindServerClientPawn(State)) { Pawn->GetArenaDuelMovementComponent()->ConsumeStamina(Pawn->GetArenaDuelMovementComponent()->GetMaxStamina() - 5.0f); State.InitialServerStamina = Pawn->GetArenaDuelMovementComponent()->GetStamina(); State.LowestServerStamina = State.InitialServerStamina; return true; } return false; }, FTimespan::FromSeconds(5.0))
			.UntilClient(TEXT("Prepare stamina client"), 0, [](FArenaDuelPhase4NetworkState& State) { ArenaDuelPhase4NetworkTests::PrepareClient(State); return State.OwnedPawn && State.RemotePawn && ArenaDuelPhase4NetworkTests::HasInputMapping(State) && ArenaDuelPhase4NetworkTests::HasFixtureGeometry(State); }, FTimespan::FromSeconds(5.0))
			.UntilClient(TEXT("Client receives authoritative starting stamina"), 0, [](FArenaDuelPhase4NetworkState& State) { if (!State.OwnedPawn || State.OwnedPawn->GetArenaDuelMovementComponent()->GetStamina() > 60.0f) return false; if (State.InitialClientStamina <= 0.0f || State.InitialClientStamina > 60.0f) { State.InitialClientStamina = State.OwnedPawn->GetArenaDuelMovementComponent()->GetStamina(); State.LowestClientStamina = State.InitialClientStamina; } return true; }, FTimespan::FromSeconds(5.0))
			.ThenClient(TEXT("Start network stamina action through Enhanced Input"), 0, [](FArenaDuelPhase4NetworkState& State) { ArenaDuelPhase4NetworkTests::StartInput(State); })
			.UntilClient(TEXT("Build network stamina speed"), 0, [](FArenaDuelPhase4NetworkState& State) { return State.OwnedPawn && State.OwnedPawn->GetCharacterMovement()->Velocity.Size2D() >= State.OwnedPawn->GetArenaDuelMovementComponent()->WallRunMinSpeed; }, FTimespan::FromSeconds(5.0))
			.ThenClient(TEXT("Enter falling for network stamina action"), 0, [](FArenaDuelPhase4NetworkState& State) { ArenaDuelPhase4NetworkTests::InjectJump(State); })
			.UntilClient(TEXT("Client reaches wall run and drains stamina"), 0, [](FArenaDuelPhase4NetworkState& State) { if (!State.OwnedPawn) return false; State.LowestClientStamina = FMath::Min(State.LowestClientStamina, State.OwnedPawn->GetArenaDuelMovementComponent()->GetStamina()); return State.OwnedPawn->GetArenaDuelMovementComponent()->IsWallRunning() && State.LowestClientStamina < State.InitialClientStamina; }, FTimespan::FromSeconds(8.0))
			.UntilServer(TEXT("Server reaches wall run and drains stamina"), [](FArenaDuelPhase4NetworkState& State) { AArenaDuelCharacter* Pawn = ArenaDuelPhase4NetworkTests::FindServerClientPawn(State); if (!Pawn) return false; State.LowestServerStamina = FMath::Min(State.LowestServerStamina, Pawn->GetArenaDuelMovementComponent()->GetStamina()); ArenaDuelPhase4NetworkTests::LastServerSnapshot.bStaminaValid = true; ArenaDuelPhase4NetworkTests::LastServerSnapshot.Stamina = Pawn->GetArenaDuelMovementComponent()->GetStamina(); return Pawn->GetArenaDuelMovementComponent()->IsWallRunning() && State.LowestServerStamina < State.InitialServerStamina; }, FTimespan::FromSeconds(8.0))
			.UntilClient(TEXT("Client and server stamina remain coherent"), 0, [](FArenaDuelPhase4NetworkState& State) { return State.OwnedPawn && ArenaDuelPhase4NetworkTests::LastServerSnapshot.bStaminaValid && FMath::Abs(State.OwnedPawn->GetArenaDuelMovementComponent()->GetStamina() - ArenaDuelPhase4NetworkTests::LastServerSnapshot.Stamina) <= 35.0f; }, FTimespan::FromSeconds(5.0))
			.UntilServer(TEXT("Stamina exhaustion exits authoritative wall run"), [](FArenaDuelPhase4NetworkState& State) { AArenaDuelCharacter* Pawn = ArenaDuelPhase4NetworkTests::FindServerClientPawn(State); return Pawn && !Pawn->GetArenaDuelMovementComponent()->IsWallRunning() && Pawn->GetArenaDuelMovementComponent()->GetStamina() <= 0.0f; }, FTimespan::FromSeconds(8.0))
			.ThenClient(TEXT("Stop stamina input"), 0, [](FArenaDuelPhase4NetworkState& State) { ArenaDuelPhase4NetworkTests::StopAllInput(State); })
			.UntilClient(TEXT("Owning client leaves wall run"), 0, [](FArenaDuelPhase4NetworkState& State) { return State.OwnedPawn && !State.OwnedPawn->GetArenaDuelMovementComponent()->IsWallRunning(); }, FTimespan::FromSeconds(5.0))
			.UntilClient(TEXT("Stamina regeneration begins"), 0, [](FArenaDuelPhase4NetworkState& State) { return State.OwnedPawn && State.OwnedPawn->GetArenaDuelMovementComponent()->GetStamina() > State.LowestClientStamina; }, FTimespan::FromSeconds(5.0));
	}

	TEST_METHOD(Traversal)
	{
		Network
			.UntilServer(TEXT("Place vault fixture"), [](FArenaDuelPhase4NetworkState& State) { return ArenaDuelPhase4NetworkTests::PlaceServerPawn(State, FVector(820.0f, 0.0f, 100.0f)); }, FTimespan::FromSeconds(5.0))
			.UntilClient(TEXT("Prepare traversal client"), 0, [](FArenaDuelPhase4NetworkState& State) { ArenaDuelPhase4NetworkTests::PrepareClient(State); return State.OwnedPawn && State.RemotePawn && ArenaDuelPhase4NetworkTests::HasInputMapping(State) && ArenaDuelPhase4NetworkTests::HasFixtureGeometry(State); }, FTimespan::FromSeconds(5.0))
			.UntilClient(TEXT("Traversal client has a stable collision-free start"), 0, [](FArenaDuelPhase4NetworkState& State) { return State.OwnedPawn && ArenaDuelPhase4HardeningTests::IsFinite(State.OwnedPawn->GetActorLocation()) && !ArenaDuelPhase4HardeningTests::IsCapsuleOverlapping(State.OwnedPawn); }, FTimespan::FromSeconds(5.0))
			.ThenClient(TEXT("Approach vault with normal input"), 0, [](FArenaDuelPhase4NetworkState& State) { ArenaDuelPhase4NetworkTests::StartInput(State); })
			.UntilClient(TEXT("Reach traversal speed"), 0, [](FArenaDuelPhase4NetworkState& State) { return State.OwnedPawn && State.OwnedPawn->GetCharacterMovement()->Velocity.Size2D() >= State.OwnedPawn->GetArenaDuelMovementComponent()->WallRunMinSpeed; }, FTimespan::FromSeconds(5.0))
			.ThenClient(TEXT("Jump toward vault through input"), 0, [](FArenaDuelPhase4NetworkState& State) { ArenaDuelPhase4NetworkTests::InjectJump(State); })
			.UntilClient(TEXT("Client enters vault"), 0, [](FArenaDuelPhase4NetworkState& State) { return ArenaDuelPhase4NetworkTests::IsVault(State.OwnedPawn); }, FTimespan::FromSeconds(8.0))
			.UntilServer(TEXT("Server validates vault"), [](FArenaDuelPhase4NetworkState& State) { return ArenaDuelPhase4NetworkTests::IsVault(ArenaDuelPhase4NetworkTests::FindServerClientPawn(State)); }, FTimespan::FromSeconds(8.0))
			.UntilClient(TEXT("Vault completes in valid mode"), 0, [](FArenaDuelPhase4NetworkState& State) { return State.OwnedPawn && (State.OwnedPawn->GetCharacterMovement()->IsMovingOnGround() || State.OwnedPawn->GetCharacterMovement()->IsFalling()) && !ArenaDuelPhase4NetworkTests::IsVault(State.OwnedPawn) && State.OwnedPawn->GetActorLocation().X > 1050.0f && !ArenaDuelPhase4HardeningTests::IsCapsuleOverlapping(State.OwnedPawn); }, FTimespan::FromSeconds(5.0))
			.ThenClient(TEXT("Stop traversal input"), 0, [](FArenaDuelPhase4NetworkState& State) { ArenaDuelPhase4NetworkTests::StopAllInput(State); })
			.UntilServer(TEXT("Server vault completes collision-free behind obstacle"), [](FArenaDuelPhase4NetworkState& State) { return ArenaDuelPhase4NetworkTests::IsVaultComplete(ArenaDuelPhase4NetworkTests::FindServerClientPawn(State)); }, FTimespan::FromSeconds(8.0))
			.UntilServer(TEXT("Capture traversal authority"), [](FArenaDuelPhase4NetworkState& State) { return ArenaDuelPhase4NetworkTests::CaptureServerSnapshotAfterSettle(State); })
			.UntilClient(TEXT("Traversal converges"), 0, [](FArenaDuelPhase4NetworkState& State) { return ArenaDuelPhase4NetworkTests::HasConverged(State); }, FTimespan::FromSeconds(5.0));
	}

	TEST_METHOD(ClientServerConvergence)
	{
		Network
			.UntilServer(TEXT("Place convergence fixture"), [](FArenaDuelPhase4NetworkState& State) { return ArenaDuelPhase4NetworkTests::PlaceServerPawn(State, FVector(-2500.0f, 0.0f, 100.0f)); }, FTimespan::FromSeconds(5.0))
			.UntilClient(TEXT("Prepare convergence client"), 0, [](FArenaDuelPhase4NetworkState& State) { ArenaDuelPhase4NetworkTests::PrepareClient(State); return State.OwnedPawn && State.RemotePawn && ArenaDuelPhase4NetworkTests::HasInputMapping(State) && ArenaDuelPhase4NetworkTests::HasFixtureGeometry(State); }, FTimespan::FromSeconds(5.0))
			.ThenClient(TEXT("Drive representative movement through input"), 0, [](FArenaDuelPhase4NetworkState& State) { ArenaDuelPhase4NetworkTests::StartInput(State); })
			.UntilClient(TEXT("Client movement settles"), 0, [](FArenaDuelPhase4NetworkState& State) { return State.OwnedPawn && FVector::Dist2D(State.OwnedPawn->GetActorLocation(), State.ClientStart) > 100.0f; }, FTimespan::FromSeconds(5.0))
			.ThenClient(TEXT("Stop representative movement"), 0, [](FArenaDuelPhase4NetworkState& State) { ArenaDuelPhase4NetworkTests::StopAllInput(State); })
			.UntilServer(TEXT("Server remains finite and authoritative state settles"), [](FArenaDuelPhase4NetworkState& State) { AArenaDuelCharacter* Pawn = ArenaDuelPhase4NetworkTests::FindServerClientPawn(State); return Pawn && ArenaDuelPhase4NetworkTests::IsFinite(Pawn->GetActorLocation()) && ArenaDuelPhase4NetworkTests::IsFinite(Pawn->GetCharacterMovement()->Velocity) && ArenaDuelPhase4NetworkTests::CaptureServerSnapshotAfterSettle(State); }, FTimespan::FromSeconds(5.0))
			.UntilClient(TEXT("Client/server location converges"), 0, [](FArenaDuelPhase4NetworkState& State) { return ArenaDuelPhase4NetworkTests::HasConverged(State); }, FTimespan::FromSeconds(5.0));
	}

	TEST_METHOD(CrossControlIsolation)
	{
		Network
			.UntilServer(TEXT("Place cross-control fixture"), [](FArenaDuelPhase4NetworkState& State) { if (!ArenaDuelPhase4NetworkTests::PlaceServerPawn(State, FVector(-2500.0f, 0.0f, 100.0f))) return false; AArenaDuelCharacter* Owned = ArenaDuelPhase4NetworkTests::FindServerClientPawn(State); AArenaDuelCharacter* Remote = ArenaDuelPhase4NetworkTests::FindRemoteCharacter(State.World, Owned); if (!Remote) return false; Remote->SetActorLocation(FVector(-2500.0f, 300.0f, 100.0f), false, nullptr, ETeleportType::TeleportPhysics); Remote->GetArenaDuelMovementComponent()->Velocity = FVector::ZeroVector; Remote->GetArenaDuelMovementComponent()->SetMovementMode(MOVE_Walking); Remote->GetArenaDuelMovementComponent()->StopSprint(); Remote->GetArenaDuelMovementComponent()->StopSlide(); Remote->ForceNetUpdate(); State.RemoteStartMovementMode = MOVE_None; return true; }, FTimespan::FromSeconds(5.0))
			.UntilClient(TEXT("Prepare cross-control pawns"), 0, [](FArenaDuelPhase4NetworkState& State) { ArenaDuelPhase4NetworkTests::PrepareClient(State); return State.OwnedPawn && State.RemotePawn && ArenaDuelPhase4NetworkTests::HasInputMapping(State) && ArenaDuelPhase4NetworkTests::HasFixtureGeometry(State); }, FTimespan::FromSeconds(5.0))
			.UntilServer(TEXT("Stabilize remote pawn before advanced input"), [](FArenaDuelPhase4NetworkState& State) { AArenaDuelCharacter* Owned = ArenaDuelPhase4NetworkTests::FindServerClientPawn(State); AArenaDuelCharacter* Remote = ArenaDuelPhase4NetworkTests::FindRemoteCharacter(State.World, Owned); if (!Remote) return false; Remote->SetActorLocation(FVector(-2500.0f, 300.0f, 100.0f), false, nullptr, ETeleportType::TeleportPhysics); Remote->GetArenaDuelMovementComponent()->Velocity = FVector::ZeroVector; Remote->GetArenaDuelMovementComponent()->SetMovementMode(MOVE_Walking); Remote->GetArenaDuelMovementComponent()->StopSprint(); Remote->GetArenaDuelMovementComponent()->StopSlide(); Remote->ForceNetUpdate(); State.RemoteStartLocation = Remote->GetActorLocation(); State.RemoteStartVelocity = Remote->GetArenaDuelMovementComponent()->Velocity; State.RemoteStartMovementMode = Remote->GetArenaDuelMovementComponent()->MovementMode; State.RemoteStartCustomMovementMode = Remote->GetArenaDuelMovementComponent()->CustomMovementMode; State.RemoteStartSprintIntent = false; State.RemoteStartCrouchIntent = false; return true; }, FTimespan::FromSeconds(5.0))
			.ThenClient(TEXT("Build advanced movement speed on owning pawn"), 0, [](FArenaDuelPhase4NetworkState& State) { ArenaDuelPhase4NetworkTests::StartInput(State); })
			.UntilClient(TEXT("Owning pawn reaches slide speed"), 0, [](FArenaDuelPhase4NetworkState& State) { return State.OwnedPawn && State.OwnedPawn->GetCharacterMovement()->Velocity.Size2D() >= State.OwnedPawn->GetArenaDuelMovementComponent()->SlideMinSpeed; }, FTimespan::FromSeconds(5.0))
			.ThenClient(TEXT("Trigger owning pawn slide"), 0, [](FArenaDuelPhase4NetworkState& State) { ArenaDuelPhase4NetworkTests::StartSlide(State); })
			.UntilClient(TEXT("Owning pawn enters slide without remote movement"), 0, [](FArenaDuelPhase4NetworkState& State) { return State.OwnedPawn && State.OwnedPawn->GetArenaDuelMovementComponent()->IsSliding() && ArenaDuelPhase4NetworkTests::RemotePawnIsIsolated(State); }, FTimespan::FromSeconds(5.0))
			.UntilServer(TEXT("Server observes owning slide without remote movement"), [](FArenaDuelPhase4NetworkState& State) { AArenaDuelCharacter* Pawn = ArenaDuelPhase4NetworkTests::FindServerClientPawn(State); if (Pawn && Pawn->GetArenaDuelMovementComponent()->IsSliding() && ArenaDuelPhase4NetworkTests::RemotePawnIsIsolated(State)) State.bServerAdvancedMovementObserved = true; return Pawn && State.bServerAdvancedMovementObserved; }, FTimespan::FromSeconds(5.0))
			.ThenClient(TEXT("Trigger owning pawn slide jump"), 0, [](FArenaDuelPhase4NetworkState& State) { ArenaDuelPhase4NetworkTests::InjectJump(State); })
			.UntilClient(TEXT("Owning advanced movement does not affect remote pawn"), 0, [](FArenaDuelPhase4NetworkState& State) { return State.OwnedPawn && (State.OwnedPawn->GetArenaDuelMovementComponent()->IsSliding() || State.OwnedPawn->GetCharacterMovement()->IsFalling()) && ArenaDuelPhase4NetworkTests::RemotePawnIsIsolated(State); }, FTimespan::FromSeconds(5.0))
			.UntilServer(TEXT("Server preserves cross-control isolation during advanced movement"), [](FArenaDuelPhase4NetworkState& State) { AArenaDuelCharacter* Pawn = ArenaDuelPhase4NetworkTests::FindServerClientPawn(State); if (Pawn && (Pawn->GetCharacterMovement()->IsFalling() || Pawn->GetArenaDuelMovementComponent()->IsSliding())) State.bServerAdvancedMovementObserved = true; return Pawn && State.bServerAdvancedMovementObserved && ArenaDuelPhase4NetworkTests::RemotePawnIsIsolated(State); }, FTimespan::FromSeconds(5.0))
			.ThenClient(TEXT("Stop cross-control input"), 0, [](FArenaDuelPhase4NetworkState& State) { ArenaDuelPhase4NetworkTests::StopAllInput(State); })
			.UntilClient(TEXT("Remote pawn remains isolated after advanced movement"), 0, [](FArenaDuelPhase4NetworkState& State) { return ArenaDuelPhase4NetworkTests::RemotePawnIsIsolated(State); }, FTimespan::FromSeconds(5.0));
	}
};

namespace ArenaDuelGroundFeelTests
{
	// Drives the character with one input direction at a fixed 120 Hz step and returns the seconds until Done is true.
	template <typename TDone>
	static float SimulateUntil(AArenaDuelCharacter* Character, UArenaDuelCharacterMovementComponent* Move, const FVector& Input, TDone Done, float MaxSeconds = 2.0f)
	{
		constexpr float Step = 1.0f / 120.0f;
		float Elapsed = 0.0f;
		while (Elapsed < MaxSeconds && !Done())
		{
			if (!Input.IsNearlyZero()) Character->AddMovementInput(Input, 1.0f);
			Move->TickComponent(Step, LEVELTICK_All, nullptr);
			Elapsed += Step;
		}
		return Elapsed;
	}
}

ARENA_PHASE4_COMPONENT_TEST(FArenaDuelGroundFeelTuningTest, "ArenaDuel.GroundFeel.CentralTuning", {
	FAutomationEditorCommonUtils::LoadMap(ArenaDuelPhase4Tests::MovementMap);
	AArenaDuelCharacter* Character = ArenaDuelPhase4HardeningTests::SpawnCharacter(GEditor->GetEditorWorldContext().World(), FVector::ZeroVector);
	UArenaDuelCharacterMovementComponent* Move = ArenaDuelPhase4HardeningTests::Movement(Character);
	TestNotNull(TEXT("Ground feel movement"), Move);
	if (!Move) return true;
	Move->SetMovementMode(MOVE_Walking);
	TestEqual(TEXT("Walking uses the ground acceleration tunable"), Move->GetMaxAcceleration(), Move->GroundAcceleration);
	TestEqual(TEXT("Braking deceleration follows its tunable"), Move->BrakingDecelerationWalking, Move->GroundBrakingDeceleration);
	TestEqual(TEXT("Turn friction follows its tunable"), Move->GroundFriction, Move->GroundTurnFriction);
	Move->SetMovementMode(MOVE_Falling);
	TestEqual(TEXT("Air control keeps its own acceleration"), Move->GetMaxAcceleration(), Move->AirAcceleration);
	TestEqual(TEXT("Remote smoothing follows its tunable"), Move->NetworkSimulatedSmoothLocationTime, Move->RemoteSmoothLocationTime);
	return true;
})

ARENA_PHASE4_COMPONENT_TEST(FArenaDuelGroundFeelResponseTest, "ArenaDuel.GroundFeel.StartStopAndCounterStrafe", {
	FAutomationEditorCommonUtils::LoadMap(ArenaDuelPhase4Tests::MovementMap);
	AArenaDuelCharacter* Character = ArenaDuelPhase4HardeningTests::SpawnCharacter(GEditor->GetEditorWorldContext().World(), FVector::ZeroVector);
	UArenaDuelCharacterMovementComponent* Move = ArenaDuelPhase4HardeningTests::Movement(Character);
	TestNotNull(TEXT("Ground feel movement"), Move);
	if (!Move || !Character) return true;
	using namespace ArenaDuelGroundFeelTests;
	Move->bRunPhysicsWithNoController = true;
	Move->SetMovementMode(MOVE_Walking);
	const FVector Right = Character->GetActorRightVector();

	const float StartSeconds = SimulateUntil(Character, Move, Right, [Move]() { return Move->Velocity.Size2D() >= 0.95f * Move->WalkSpeed; });
	TestTrue(FString::Printf(TEXT("Strafe reaches 95 percent speed quickly (%.3f s)"), StartSeconds), StartSeconds <= 0.20f);

	SimulateUntil(Character, Move, Right, [Move]() { return Move->Velocity.Size2D() >= 0.99f * Move->WalkSpeed; });
	const FVector CounterStart = Character->GetActorLocation();
	const float CounterSeconds = SimulateUntil(Character, Move, -Right, [Move, Right]() { return FVector::DotProduct(Move->Velocity, Right) <= 0.0f; });
	const float CounterDistance = FVector::Dist2D(CounterStart, Character->GetActorLocation());
	TestTrue(FString::Printf(TEXT("Counter-strafe kills sideways speed almost at once (%.3f s, %.1f cm)"), CounterSeconds, CounterDistance), CounterSeconds <= 0.09f && CounterDistance <= 25.0f);

	const float ReverseSeconds = SimulateUntil(Character, Move, -Right, [Move, Right]() { return FVector::DotProduct(Move->Velocity, -Right) >= 0.95f * Move->WalkSpeed; });
	TestTrue(FString::Printf(TEXT("Full reversal reaches top speed the other way quickly (%.3f s after the stop)"), ReverseSeconds), ReverseSeconds <= 0.22f);

	const FVector ReleaseStart = Character->GetActorLocation();
	const float ReleaseSeconds = SimulateUntil(Character, Move, FVector::ZeroVector, [Move]() { return Move->Velocity.Size2D() <= 5.0f; });
	const float ReleaseDistance = FVector::Dist2D(ReleaseStart, Character->GetActorLocation());
	TestTrue(FString::Printf(TEXT("Releasing input stops without a long slide (%.3f s, %.1f cm)"), ReleaseSeconds, ReleaseDistance), ReleaseSeconds <= 0.16f && ReleaseDistance <= 45.0f);
	TestTrue(TEXT("Counter-strafing stops sooner than just letting go"), CounterSeconds < ReleaseSeconds);
	AddInfo(FString::Printf(TEXT("Ground feel: start %.3f s, counter-strafe %.3f s / %.1f cm, reversal %.3f s, release %.3f s / %.1f cm"), StartSeconds, CounterSeconds, CounterDistance, ReverseSeconds, ReleaseSeconds, ReleaseDistance));
	return true;
})
#endif
