#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "ArenaDuel/Characters/ArenaDuelCharacterMovementComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FArenaDuelBhopConfigTest, "ArenaDuel.Bhop.Config", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FArenaDuelBhopConfigTest::RunTest(const FString& Parameters)
{
	const UArenaDuelCharacterMovementComponent* Movement = GetDefault<UArenaDuelCharacterMovementComponent>();
	TestTrue(TEXT("Hopping can be faster than sprinting"), Movement->GetBhopMaxSpeed() > Movement->SprintSpeed);
	TestTrue(TEXT("Hopping stays inside the momentum cap"), Movement->GetBhopMaxSpeed() <= Movement->GlobalMomentumCap);
	TestTrue(TEXT("The landing grace is a few frames, not a slide"), Movement->GetBhopLandingGrace() > 0.0f && Movement->GetBhopLandingGrace() <= 0.1f);
	TestTrue(TEXT("A landing costs a small share of the extra speed"), Movement->GetLandingSpeedLoss() >= 0.0f && Movement->GetLandingSpeedLoss() < 0.5f);
	TestTrue(TEXT("The jump buffer is short"), Movement->GetJumpBufferSeconds() > 0.0f && Movement->GetJumpBufferSeconds() <= 0.25f);
	const float Loss = Movement->GetLandingSpeedLoss();
	TestEqual(TEXT("A landing at sprint speed costs nothing"), UArenaDuelCharacterMovementComponent::ComputeLandingSpeed(Movement->SprintSpeed, Movement->SprintSpeed, Loss), Movement->SprintSpeed);
	TestEqual(TEXT("A landing at walk speed costs nothing"), UArenaDuelCharacterMovementComponent::ComputeLandingSpeed(Movement->WalkSpeed, Movement->SprintSpeed, Loss), Movement->WalkSpeed);
	TestTrue(TEXT("A fast landing loses its share of the extra speed only"), FMath::IsNearlyEqual(UArenaDuelCharacterMovementComponent::ComputeLandingSpeed(Movement->SprintSpeed + 300.0f, Movement->SprintSpeed, Loss), Movement->SprintSpeed + 300.0f * (1.0f - Loss), 0.01f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FArenaDuelBhopAirStrafeTest, "ArenaDuel.Bhop.AirStrafeRule", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FArenaDuelBhopAirStrafeTest::RunTest(const FString& Parameters)
{
	const UArenaDuelCharacterMovementComponent* Movement = GetDefault<UArenaDuelCharacterMovementComponent>();
	const float Cap = Movement->GetAirStrafeWishSpeed();
	const float Accel = Movement->GetAirStrafeAccelerate() * Movement->WalkSpeed;
	const float Max = Movement->GetBhopMaxSpeed();
	const float Step = 1.0f / 60.0f;
	const auto Strafe = [&](const FVector& Velocity, const FVector& Wish) { return UArenaDuelCharacterMovementComponent::ComputeAirStrafe(Velocity, Wish, Cap, Accel, Max, Step); };

	const FVector Running(900.0, 0.0, 0.0);
	TestTrue(TEXT("Holding forward at speed adds nothing"), FMath::IsNearlyEqual(Strafe(Running, FVector::XAxisVector).Size(), 900.0, 0.01));
	TestTrue(TEXT("Holding back in the air does not brake to a stop in one step"), Strafe(Running, -FVector::XAxisVector).X > 800.0);
	const FVector Sideways = Strafe(Running, FVector::YAxisVector);
	TestTrue(TEXT("Strafing sideways adds speed"), Sideways.Size() > 900.0 && Sideways.Y > 0.0);
	TestTrue(TEXT("One step adds only a little"), Sideways.Size() < 910.0);
	TestTrue(TEXT("No input leaves the velocity alone"), Strafe(Running, FVector::ZeroVector).Equals(Running));

	// A perfect strafe: the wished direction is kept at a right angle to the velocity while turning.
	FVector Velocity = Running;
	for (int32 Index = 0; Index < 60 * 60; ++Index) Velocity = Strafe(Velocity, FVector(-Velocity.Y, Velocity.X, 0.0));
	TestTrue(TEXT("Even perfect strafing stops at the hop speed limit"), Velocity.Size() <= Max + 0.5 && Velocity.Size() > Max - 5.0);
	// Sloppy strafing: sideways held without turning the view.
	Velocity = Running;
	for (int32 Index = 0; Index < 60 * 5; ++Index) Velocity = Strafe(Velocity, FVector::YAxisVector);
	TestTrue(TEXT("Holding a strafe key without turning gains almost nothing"), Velocity.Size() < 910.0);
	// The gain per second must not depend on the frame rate: the same second at 30 and at 120 steps.
	FVector Slow = Running, Fast = Running;
	for (int32 Index = 0; Index < 30; ++Index) Slow = UArenaDuelCharacterMovementComponent::ComputeAirStrafe(Slow, FVector(-Slow.Y, Slow.X, 0.0), Cap, Accel, Max, 1.0f / 30.0f);
	for (int32 Index = 0; Index < 120; ++Index) Fast = UArenaDuelCharacterMovementComponent::ComputeAirStrafe(Fast, FVector(-Fast.Y, Fast.X, 0.0), Cap, Accel, Max, 1.0f / 120.0f);
	TestTrue(TEXT("A second of strafing gains about the same at 30 and at 120 steps"), FMath::Abs(Slow.Size() - Fast.Size()) < 3.0);
	// Speed from somewhere else, above the limit, is not cut by the strafe step.
	const FVector Dashing(3000.0, 0.0, 0.0);
	TestTrue(TEXT("Speed above the limit is not reduced by strafing"), Strafe(Dashing, FVector::YAxisVector).Size() >= 3000.0 - 0.01);
	TestTrue(TEXT("And not increased either"), Strafe(Dashing, FVector::YAxisVector).Size() <= 3000.0 + 0.01);
	return true;
}

#endif
