#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "ArenaDuel/Abilities/ArenaDuelGA_ShadowStep.h"
#include "ArenaDuel/Characters/ArenaDuelCharacterMovementComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FArenaDuelShadowStepStrengthTest, "ArenaDuel.Shadow.StepStrength", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FArenaDuelShadowStepStrengthTest::RunTest(const FString& Parameters)
{
	const UArenaDuelGA_ShadowStep* Step = GetDefault<UArenaDuelGA_ShadowStep>();
	const UArenaDuelCharacterMovementComponent* Movement = GetDefault<UArenaDuelCharacterMovementComponent>();
	TestTrue(TEXT("The dash is clearly faster than a sprint"), Step->GetDashSpeed() >= Movement->SprintSpeed * 2.0f);
	TestTrue(TEXT("The dash is not held back by the movement momentum cap"), Step->GetDashSpeed() > Movement->GlobalMomentumCap);
	TestTrue(TEXT("The dash leaves the ground with a small hop, not a jump"), Step->GetDashLift() > 0.0f && Step->GetDashLift() < 420.0f);
	return true;
}

#endif
