#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "ArenaDuel/Characters/ArenaDuelCharacter.h"
#include "ArenaDuel/Characters/ArenaDuelCharacterMovementComponent.h"
#include "Engine/World.h"
#include "GameMapsSettings.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Tests/AutomationEditorCommon.h"
#include "UObject/UnrealType.h"

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

#endif
