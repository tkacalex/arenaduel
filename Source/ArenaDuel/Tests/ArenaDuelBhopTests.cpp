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
	TestTrue(TEXT("A push exactly sideways turns the velocity without adding speed"), FMath::IsNearlyEqual(Sideways.Size(), 900.0, 0.01) && Sideways.Y > 0.0);
	const FVector Diagonal = Strafe(Running, FVector(0.05, 1.0, 0.0));
	TestTrue(TEXT("A push slightly ahead of sideways adds a little speed"), Diagonal.Size() > 900.0 && Diagonal.Size() < 905.0);
	TestTrue(TEXT("No input leaves the velocity alone"), Strafe(Running, FVector::ZeroVector).Equals(Running));

	// A good strafe: the wish is kept just inside the window while the view turns with the velocity.
	const auto GoodWish = [Cap](const FVector& Current)
	{
		const double Along = Cap * 0.5 / Current.Size();
		const FVector Forward = Current.GetSafeNormal();
		return Forward * Along + FVector(-Forward.Y, Forward.X, 0.0) * FMath::Sqrt(1.0 - Along * Along);
	};
	FVector Velocity = Running;
	for (int32 Index = 0; Index < 60 * 60; ++Index) Velocity = Strafe(Velocity, GoodWish(Velocity));
	TestTrue(TEXT("Even a minute of good strafing stops at the hop speed limit"), Velocity.Size() <= Max + 0.5 && Velocity.Size() > Max - 5.0);
	// Sloppy strafing: sideways held without turning the view.
	Velocity = Running;
	for (int32 Index = 0; Index < 60 * 5; ++Index) Velocity = Strafe(Velocity, FVector::YAxisVector);
	TestTrue(TEXT("Holding a strafe key without turning gains nothing"), Velocity.Size() < 900.5);
	// The gain per second must hardly depend on the frame rate. The wish is held at the same angle to the
	// velocity, as a player turning with the strafe does; one second at 60 and at 240 steps.
	const auto StrafeSecond = [&](int32 Steps)
	{
		FVector Current = Running;
		for (int32 Index = 0; Index < Steps; ++Index) Current = UArenaDuelCharacterMovementComponent::ComputeAirStrafe(Current, GoodWish(Current), Cap, Accel, Max, 1.0f / Steps);
		return Current.Size();
	};
	const double At60 = StrafeSecond(60), At240 = StrafeSecond(240);
	const double At20 = StrafeSecond(20);
	TestTrue(FString::Printf(TEXT("A second of good strafing gains speed (%.1f at 20 steps, %.1f at 60, %.1f at 240)"), At20 - 900.0, At60 - 900.0, At240 - 900.0), At60 > 940.0 && At240 > 940.0);
	TestTrue(TEXT("And the same at 60 and at 240 steps"), FMath::Abs(At60 - At240) < 2.0);
	TestTrue(TEXT("A low frame rate never gains more"), At20 <= At60 + 0.5);
	// Speed from somewhere else, above the limit, is not cut by the strafe step.
	const FVector Dashing(3000.0, 0.0, 0.0);
	TestTrue(TEXT("Speed above the limit is not reduced by strafing"), Strafe(Dashing, FVector::YAxisVector).Size() >= 3000.0 - 0.01);
	TestTrue(TEXT("And not increased either"), Strafe(Dashing, FVector::YAxisVector).Size() <= 3000.0 + 0.01);
	return true;
}

#endif
