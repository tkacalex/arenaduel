#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Editor.h"
#include "ArenaDuel/Game/ArenaDuelHealPad.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "Tests/AutomationEditorCommon.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FArenaDuelDistrictMapTest, "ArenaDuel.Maps.ArenaDistrict", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FArenaDuelDistrictMapTest::RunTest(const FString& Parameters)
{
	FAutomationEditorCommonUtils::LoadMap(TEXT("/Game/ArenaDuel/Maps/L_ArenaDistrict"));
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!TestNotNull(TEXT("L_ArenaDistrict loads"), World)) return false;

	const APlayerStart* First = nullptr;
	const APlayerStart* Second = nullptr;
	for (TActorIterator<APlayerStart> It(World); It; ++It)
	{
		if (It->PlayerStartTag == FName(TEXT("ArenaCore_P1"))) First = *It;
		if (It->PlayerStartTag == FName(TEXT("ArenaCore_P2"))) Second = *It;
	}
	if (!TestNotNull(TEXT("Start for player one"), First) || !TestNotNull(TEXT("Start for player two"), Second)) return false;

	const FVector A = First->GetActorLocation(), B = Second->GetActorLocation();
	TestTrue(TEXT("The starts mirror each other across the centre"), FMath::IsNearlyEqual(A.X, -B.X, 1.0) && FMath::IsNearlyEqual(A.Y, B.Y, 1.0));
	TestTrue(TEXT("The starts face each other"), FMath::Abs(FRotator::NormalizeAxis(First->GetActorRotation().Yaw - Second->GetActorRotation().Yaw)) > 179.0f);

	// No shot from spawn to spawn: the weapon trace channel is blocked at standing and at crouched eye height.
	for (const float EyeHeight : { 64.0f, 20.0f })
	{
		FHitResult Hit;
		TestTrue(FString::Printf(TEXT("Spawn to spawn is blocked at eye offset %.0f"), EyeHeight),
			World->LineTraceSingleByChannel(Hit, A + FVector(0, 0, EyeHeight), B + FVector(0, 0, EyeHeight), ECC_Visibility));
	}
	// The floor is under both starts.
	for (const FVector& Start : { A, B })
	{
		FHitResult Floor;
		TestTrue(TEXT("A start stands on the floor"), World->LineTraceSingleByChannel(Floor, Start, Start - FVector(0, 0, 400), ECC_Visibility) && Floor.ImpactPoint.Z > -5.0 && Floor.ImpactPoint.Z < 5.0);
	}

	// One heal pad, small, on the centre line and in the open: both flank windows have a clear shot at whoever stands on it.
	const AArenaDuelHealPad* Pad = nullptr;
	int32 Pads = 0;
	for (TActorIterator<AArenaDuelHealPad> It(World); It; ++It) { Pad = *It; ++Pads; }
	TestEqual(TEXT("The map has one heal pad"), Pads, 1);
	if (Pad)
	{
		const FVector PadLocation = Pad->GetActorLocation();
		TestTrue(TEXT("The pad lies on the centre line, on the floor"), FMath::Abs(PadLocation.X) < 1.0 && PadLocation.Z > -5.0 && PadLocation.Z < 10.0);
		TestTrue(TEXT("The pad is small"), Pad->GetPadHalfSize() <= 75.0f);
		TestEqual(TEXT("Five health per second"), Pad->GetHealPerSecond(), 5.0f);
		TestEqual(TEXT("In steps of one"), Pad->GetHealStep(), 1.0f);
		TestTrue(TEXT("So a step every fifth of a second"), FMath::IsNearlyEqual(Pad->GetStepInterval(), 0.2f, 0.001f));
		TestTrue(TEXT("Standing on the pad counts"), Pad->IsOnPad(PadLocation));
		TestFalse(TEXT("Standing beside the pad does not"), Pad->IsOnPad(PadLocation + FVector(Pad->GetPadHalfSize() + 40.0f, 0.0f, 0.0f)));
		TestFalse(TEXT("Jumping over the pad does not"), Pad->IsOnPad(PadLocation + FVector(0.0f, 0.0f, 90.0f)));
		for (const float Side : { -1.0f, 1.0f })
		{
			FHitResult Blocked;
			TestFalse(TEXT("A flank window has a clear shot at the pad"), World->LineTraceSingleByChannel(Blocked, FVector(Side * 1300.0f, 885.0f, 170.0f), PadLocation + FVector(0.0f, 0.0f, 60.0f), ECC_Visibility));
		}
	}

	int32 Pieces = 0;
	for (TActorIterator<AStaticMeshActor> It(World); It; ++It)
	{
		if (It->GetActorLabel().StartsWith(TEXT("District_"))) ++Pieces;
	}
	TestTrue(TEXT("The map has its cover, windows and parkour pieces"), Pieces >= 150);
	return true;
}

#endif
