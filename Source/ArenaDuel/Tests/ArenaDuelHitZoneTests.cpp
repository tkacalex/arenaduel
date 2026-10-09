#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "ArenaDuel/Characters/ArenaDuelCharacter.h"
#include "ArenaDuel/Weapons/ArenaDuelWeaponComponent.h"

namespace ArenaDuelHitZoneTests
{
	// Fires a ray along the character's facing axis through one bone and reports the zone it scores as.
	bool ZoneThroughBone(AArenaDuelCharacter* Character, FName Bone, EArenaDuelShotResult& OutZone, FName& OutBone)
	{
		const FVector Target = Character->GetMesh()->GetBoneLocation(Bone);
		const FVector Forward = Character->GetActorForwardVector();
		FHitResult Hit;
		if (!Character->TraceHitZones(Target + Forward * 300.0f, Target - Forward * 300.0f, Hit)) return false;
		OutBone = Hit.BoneName;
		OutZone = UArenaDuelWeaponComponent::ClassifyHitBone(Hit.BoneName);
		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FArenaDuelHitZoneBoneMapTest, "ArenaDuel.HitZones.BoneClassification", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FArenaDuelHitZoneBoneMapTest::RunTest(const FString& Parameters)
{
	using EZone = EArenaDuelShotResult;
	TestEqual(TEXT("Head scores as head"), UArenaDuelWeaponComponent::ClassifyHitBone(TEXT("head")), EZone::Head);
	for (const TCHAR* Bone : { TEXT("pelvis"), TEXT("spine_02"), TEXT("spine_05"), TEXT("clavicle_l"), TEXT("neck_01") })
		TestEqual(FString::Printf(TEXT("%s scores as torso"), Bone), UArenaDuelWeaponComponent::ClassifyHitBone(Bone), EZone::Body);
	for (const TCHAR* Bone : { TEXT("upperarm_l"), TEXT("lowerarm_r"), TEXT("hand_l"), TEXT("thigh_r"), TEXT("calf_l"), TEXT("foot_r"), TEXT("ball_l") })
		TestEqual(FString::Printf(TEXT("%s scores as limb"), Bone), UArenaDuelWeaponComponent::ClassifyHitBone(Bone), EZone::Limb);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FArenaDuelHitZoneTraceTest, "ArenaDuel.HitZones.TraceMatchesBody", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FArenaDuelHitZoneTraceTest::RunTest(const FString& Parameters)
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!TestNotNull(TEXT("Editor world"), World)) return false;
	FActorSpawnParameters Spawn;
	Spawn.ObjectFlags |= RF_Transient;
	Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AArenaDuelCharacter* Character = World->SpawnActor<AArenaDuelCharacter>(AArenaDuelCharacter::StaticClass(), FVector(0.0f, 0.0f, 50000.0f), FRotator::ZeroRotator, Spawn);
	if (!TestNotNull(TEXT("Character"), Character)) return false;
	Character->RefreshCharacterVisuals();
	USkeletalMeshComponent* Mesh = Character->GetMesh();
	Mesh->RefreshBoneTransforms();
	Mesh->UpdateKinematicBonesToAnim(Mesh->GetComponentSpaceTransforms(), ETeleportType::TeleportPhysics, false);

	TestTrue(TEXT("World mesh answers queries only"), Mesh->GetCollisionEnabled() == ECollisionEnabled::QueryOnly);
	TestTrue(TEXT("World mesh blocks no collision channel"), Mesh->GetCollisionResponseToChannel(ECC_Visibility) == ECR_Ignore && Mesh->GetCollisionResponseToChannel(ECC_Pawn) == ECR_Ignore);
	TestTrue(TEXT("Legacy boxes no longer collide"), Character->BodyHitZone->GetCollisionEnabled() == ECollisionEnabled::NoCollision && Character->HeadHitZone->GetCollisionEnabled() == ECollisionEnabled::NoCollision);
	TestTrue(TEXT("Capsule stays out of weapon traces"), Character->GetCapsuleComponent()->GetCollisionResponseToChannel(ECC_Visibility) == ECR_Ignore);
	TestTrue(TEXT("Hit zones exist"), Mesh->Bodies.Num() > 8);

	struct FExpectation { FName Bone; EArenaDuelShotResult Zone; };
	const FExpectation Expectations[] = {
		{ TEXT("head"), EArenaDuelShotResult::Head }, { TEXT("spine_03"), EArenaDuelShotResult::Body }, { TEXT("pelvis"), EArenaDuelShotResult::Body },
		{ TEXT("lowerarm_l"), EArenaDuelShotResult::Limb }, { TEXT("hand_r"), EArenaDuelShotResult::Limb },
		{ TEXT("calf_l"), EArenaDuelShotResult::Limb }, { TEXT("foot_r"), EArenaDuelShotResult::Limb } };
	for (const FExpectation& Expected : Expectations)
	{
		EArenaDuelShotResult Zone = EArenaDuelShotResult::Miss;
		FName HitBone;
		const bool bHit = ArenaDuelHitZoneTests::ZoneThroughBone(Character, Expected.Bone, Zone, HitBone);
		TestTrue(FString::Printf(TEXT("Ray through %s hits a body"), *Expected.Bone.ToString()), bHit);
		if (bHit) TestEqual(FString::Printf(TEXT("Ray through %s scores its zone (hit %s)"), *Expected.Bone.ToString(), *HitBone.ToString()), Zone, Expected.Zone);
	}

	// Space the old boxes covered but the body does not occupy must be a miss.
	const FVector Center = Character->GetActorLocation();
	const FVector Forward = Character->GetActorForwardVector();
	const FVector Right = Character->GetActorRightVector();
	FHitResult Hit;
	const FVector BesideThigh = Center + Right * 34.0f + FVector(0, 0, -45.0f);
	TestFalse(TEXT("Beside the thigh inside the old body box is a miss"), Character->TraceHitZones(BesideThigh + Forward * 300.0f, BesideThigh - Forward * 300.0f, Hit));
	TestFalse(TEXT("Above the head is a miss"), Character->TraceHitZones(Center + FVector(0, 0, 110.0f) + Forward * 300.0f, Center + FVector(0, 0, 110.0f) - Forward * 300.0f, Hit));
	TestFalse(TEXT("Between the feet is a miss"), Character->TraceHitZones(Center + FVector(0, 0, -80.0f) + Forward * 300.0f, Center + FVector(0, 0, -80.0f) - Forward * 300.0f, Hit));
	Character->Destroy();
	return true;
}

#endif
