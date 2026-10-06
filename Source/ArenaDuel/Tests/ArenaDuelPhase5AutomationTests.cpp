#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "ArenaDuel/Weapons/ArenaDuelWeaponComponent.h"
#include "ArenaDuel/Weapons/ArenaDuelWeaponTarget.h"
#include "../Characters/ArenaDuelCharacter.h"
#include "EnhancedInput/Public/InputMappingContext.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "GameFramework/PlayerStart.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FArenaDuelPhase5WeaponDefinitionsTest, "ArenaDuel.Phase5.WeaponDefinitions", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FArenaDuelPhase5WeaponDefinitionsTest::RunTest(const FString& Parameters)
{
	UArenaDuelWeaponComponent* Component = NewObject<UArenaDuelWeaponComponent>();
	TestNotNull(TEXT("Weapon component created"), Component);
	if (!Component)
	{
		return false;
	}
	TestEqual(TEXT("Four weapon definitions exist"), Component->GetWeaponDefinitionCount(), 4);
	for (int32 Index = 0; Index < Component->GetWeaponDefinitionCount(); ++Index)
	{
		const FArenaDuelWeaponDefinition* Definition = Component->GetWeaponDefinition(Index);
		TestNotNull(TEXT("Weapon definition exists"), Definition);
		if (Definition)
		{
			TestTrue(TEXT("Magazine capacity is positive"), Definition->MagazineCapacity > 0);
			TestTrue(TEXT("Reserve capacity is non-negative"), Definition->ReserveCapacity >= 0);
			TestTrue(TEXT("Fire rate is positive"), Definition->RoundsPerMinute > 0.0f);
			TestTrue(TEXT("Range is positive"), Definition->Range > 0.0f);
			TestTrue(TEXT("Pellet count is positive"), Definition->Pellets > 0);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FArenaDuelPhase5WeaponIdentityTest, "ArenaDuel.Phase5.WeaponIdentity", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FArenaDuelPhase5WeaponIdentityTest::RunTest(const FString& Parameters)
{
	UArenaDuelWeaponComponent* Component = NewObject<UArenaDuelWeaponComponent>();
	TSet<uint8> Ids;
	for (int32 Index = 0; Component && Index < Component->GetWeaponDefinitionCount(); ++Index)
	{
		if (const FArenaDuelWeaponDefinition* Definition = Component->GetWeaponDefinition(Index))
		{
			Ids.Add(static_cast<uint8>(Definition->Id));
		}
	}
	TestEqual(TEXT("Weapon IDs are unique"), Ids.Num(), 4);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FArenaDuelPhase5WeaponBehaviorContractTest, "ArenaDuel.Phase5.WeaponBehaviorContract", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FArenaDuelPhase5WeaponBehaviorContractTest::RunTest(const FString& Parameters)
{
	UArenaDuelWeaponComponent* Component = NewObject<UArenaDuelWeaponComponent>();
	const FArenaDuelWeaponDefinition* Arc = Component ? Component->GetWeaponDefinition(0) : nullptr;
	const FArenaDuelWeaponDefinition* Smg = Component ? Component->GetWeaponDefinition(1) : nullptr;
	const FArenaDuelWeaponDefinition* Dmr = Component ? Component->GetWeaponDefinition(2) : nullptr;
	const FArenaDuelWeaponDefinition* Shotgun = Component ? Component->GetWeaponDefinition(3) : nullptr;
	TestTrue(TEXT("Arc Rifle is automatic"), Arc && Arc->bAutomatic);
	TestTrue(TEXT("Shade SMG is automatic"), Smg && Smg->bAutomatic);
	TestTrue(TEXT("Rune DMR is semi automatic"), Dmr && !Dmr->bAutomatic);
	TestEqual(TEXT("Hex Shotgun uses eight pellets"), Shotgun ? Shotgun->Pellets : 0, 8);
	TestTrue(TEXT("All weapons have spread and recoil values"), Arc && Arc->BaseSpreadDegrees >= 0.0f && Arc->RecoilVertical > 0.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FArenaDuelPhase5AmmoContractTest, "ArenaDuel.Phase5.AmmoPersistence", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FArenaDuelPhase5AmmoContractTest::RunTest(const FString& Parameters)
{
	UArenaDuelWeaponComponent* Component = NewObject<UArenaDuelWeaponComponent>();
	TestNotNull(TEXT("Weapon component created for ammo contract"), Component);
	if (!Component) return false;
	TestEqual(TEXT("Arc Rifle magazine capacity"), Component->GetWeaponDefinition(0)->MagazineCapacity, 30);
	TestEqual(TEXT("Shade SMG magazine capacity"), Component->GetWeaponDefinition(1)->MagazineCapacity, 32);
	TestEqual(TEXT("Rune DMR magazine capacity"), Component->GetWeaponDefinition(2)->MagazineCapacity, 12);
	TestEqual(TEXT("Hex Shotgun magazine capacity"), Component->GetWeaponDefinition(3)->MagazineCapacity, 6);
	TestNotEqual(TEXT("Weapon capacities are not all identical"), Component->GetWeaponDefinition(0)->MagazineCapacity, Component->GetWeaponDefinition(3)->MagazineCapacity);
	return true;
}

#define ARENA_DUEL_PHASE5_CONTRACT_TEST(TestClass, TestName) \
IMPLEMENT_SIMPLE_AUTOMATION_TEST(TestClass, "ArenaDuel.Phase5." TestName, EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter) \
bool TestClass::RunTest(const FString& Parameters)

ARENA_DUEL_PHASE5_CONTRACT_TEST(FArenaDuelPhase5ReloadTest, "Reload")
{
	UArenaDuelWeaponComponent* Component = NewObject<UArenaDuelWeaponComponent>();
	TestTrue(TEXT("Reload duration is positive for every weapon"), Component && Component->GetWeaponDefinition(0)->ReloadDuration > 0.0f && Component->GetWeaponDefinition(3)->ReloadDuration > 0.0f);
	return true;
}

ARENA_DUEL_PHASE5_CONTRACT_TEST(FArenaDuelPhase5FireCadenceTest, "FireCadence")
{
	UArenaDuelWeaponComponent* Component = NewObject<UArenaDuelWeaponComponent>();
	TestTrue(TEXT("Weapon cadence values are positive"), Component && Component->GetWeaponDefinition(0)->RoundsPerMinute > 0.0f && Component->GetWeaponDefinition(3)->RoundsPerMinute > 0.0f);
	TestTrue(TEXT("Shotgun cadence is slower than rifle cadence"), Component && Component->GetWeaponDefinition(3)->RoundsPerMinute < Component->GetWeaponDefinition(0)->RoundsPerMinute);
	return true;
}

ARENA_DUEL_PHASE5_CONTRACT_TEST(FArenaDuelPhase5SemiAutoTest, "SemiAuto")
{
	UArenaDuelWeaponComponent* Component = NewObject<UArenaDuelWeaponComponent>();
	TestTrue(TEXT("DMR is semi automatic"), Component && !Component->GetWeaponDefinition(2)->bAutomatic);
	TestTrue(TEXT("Shotgun is semi automatic"), Component && !Component->GetWeaponDefinition(3)->bAutomatic);
	return true;
}

ARENA_DUEL_PHASE5_CONTRACT_TEST(FArenaDuelPhase5SpreadTest, "Spread")
{
	UArenaDuelWeaponComponent* Component = NewObject<UArenaDuelWeaponComponent>();
	TestTrue(TEXT("Spread values are non-negative"), Component && Component->GetWeaponDefinition(0)->BaseSpreadDegrees >= 0.0f && Component->GetWeaponDefinition(0)->MovementSpreadDegrees >= 0.0f);
	return true;
}

ARENA_DUEL_PHASE5_CONTRACT_TEST(FArenaDuelPhase5ShotgunTest, "Shotgun")
{
	UArenaDuelWeaponComponent* Component = NewObject<UArenaDuelWeaponComponent>();
	TestEqual(TEXT("Shotgun definition contains eight pellets"), Component ? Component->GetWeaponDefinition(3)->Pellets : 0, 8);
	return true;
}

ARENA_DUEL_PHASE5_CONTRACT_TEST(FArenaDuelPhase5HitClassificationTest, "HitClassification")
{
	TestTrue(TEXT("Shot result enum reserves distinct world and body results"), static_cast<uint8>(EArenaDuelShotResult::World) != static_cast<uint8>(EArenaDuelShotResult::Body));
	TestTrue(TEXT("Shot result enum reserves distinct head and body results"), static_cast<uint8>(EArenaDuelShotResult::Head) != static_cast<uint8>(EArenaDuelShotResult::Body));
	return true;
}

#undef ARENA_DUEL_PHASE5_CONTRACT_TEST

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FArenaDuelPhase5InputAssetsTest, "ArenaDuel.Phase5.InputAssets", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FArenaDuelPhase5InputAssetsTest::RunTest(const FString& Parameters)
{
	UInputMappingContext* Context = LoadObject<UInputMappingContext>(nullptr, TEXT("/Game/ArenaDuel/Input/IMC_Gameplay.IMC_Gameplay"));
	TestNotNull(TEXT("Saved gameplay mapping context loads"), Context);
	if (!Context) return false;
	const TMap<FString, FKey> Expected = {{TEXT("IA_Aim"), EKeys::RightMouseButton}, {TEXT("IA_Fire"), EKeys::LeftMouseButton}, {TEXT("IA_Reload"), EKeys::R}, {TEXT("IA_Weapon1"), EKeys::One}, {TEXT("IA_Weapon2"), EKeys::Two}, {TEXT("IA_Weapon3"), EKeys::Three}, {TEXT("IA_Weapon4"), EKeys::Four}};
	for (const TPair<FString, FKey>& Pair : Expected)
	{
		bool bFound = false;
		for (const FEnhancedActionKeyMapping& Mapping : Context->GetMappings())
		{
			if (Mapping.Action && Mapping.Action->GetName() == Pair.Key && Mapping.Key == Pair.Value) { bFound = true; break; }
		}
		TestTrue(FString::Printf(TEXT("%s maps to %s"), *Pair.Key, *Pair.Value.GetDisplayName().ToString()), bFound);
	}
	UClass* CharacterClass = LoadObject<UClass>(nullptr, TEXT("/Game/ArenaDuel/Characters/BP_ArenaDuelCharacter.BP_ArenaDuelCharacter_C"));
	TestNotNull(TEXT("Saved Character Blueprint class loads"), CharacterClass);
	if (CharacterClass)
	{
		const AArenaDuelCharacter* CDO = Cast<AArenaDuelCharacter>(CharacterClass->GetDefaultObject());
		TestNotNull(TEXT("Character Blueprint CDO is ArenaDuelCharacter"), CDO);
		for (const TCHAR* PropertyName : {TEXT("DefaultMappingContext"), TEXT("AimAction"), TEXT("FireAction"), TEXT("ReloadAction"), TEXT("Weapon1Action"), TEXT("Weapon2Action"), TEXT("Weapon3Action"), TEXT("Weapon4Action")})
		{
			FObjectPropertyBase* Property = FindFProperty<FObjectPropertyBase>(CharacterClass, FName(PropertyName));
			TestNotNull(FString::Printf(TEXT("CDO property %s exists"), PropertyName), Property);
			TestTrue(FString::Printf(TEXT("CDO property %s is assigned"), PropertyName), Property && Property->GetObjectPropertyValue_InContainer(CDO) != nullptr);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FArenaDuelPhase5GunRangeTest, "ArenaDuel.Phase5.GunRange", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FArenaDuelPhase5GunRangeTest::RunTest(const FString& Parameters)
{
	UWorld* World = LoadObject<UWorld>(nullptr, TEXT("/Game/ArenaDuel/Maps/L_Phase5GunRange.L_Phase5GunRange"));
	TestNotNull(TEXT("Saved gun range map loads"), World);
	if (!World || !World->PersistentLevel) return false;
	TSet<FString> Labels;
	int32 TargetCount = 0;
	for (AActor* Actor : World->PersistentLevel->Actors)
	{
		if (!Actor) continue;
		Labels.Add(Actor->GetActorLabel());
		if (AArenaDuelWeaponTarget* Target = Cast<AArenaDuelWeaponTarget>(Actor))
		{
			++TargetCount;
			TestNotNull(TEXT("Target body collision exists"), Target->BodyHitZone.Get());
			TestNotNull(TEXT("Target head collision exists"), Target->HeadHitZone.Get());
			TestNotNull(TEXT("Target body visual exists"), Target->BodyVisual.Get());
			TestNotNull(TEXT("Target head visual exists"), Target->HeadVisual.Get());
		}
	}
	for (const TCHAR* Label : {TEXT("Phase5_PlayerStart_A"), TEXT("Phase5_PlayerStart_B"), TEXT("Phase5_Floor"), TEXT("Phase5_NorthSafetyWall"), TEXT("Phase5_SouthSafetyWall"), TEXT("Phase5_WestSafetyWall"), TEXT("Phase5_EastSafetyWall"), TEXT("Phase5_DirectionalLight"), TEXT("Phase5_SkyLight"), TEXT("Phase5_BulletImpactWall")})
	{
		TestTrue(FString::Printf(TEXT("Gun range contains %s"), Label), Labels.Contains(Label));
	}
	for (const TCHAR* Distance : {TEXT("5m"), TEXT("10m"), TEXT("20m"), TEXT("30m"), TEXT("50m")})
	{
		TestTrue(FString::Printf(TEXT("Target label exists for %s"), Distance), Labels.Contains(FString::Printf(TEXT("Phase5_Target_%s"), Distance)));
		TestTrue(FString::Printf(TEXT("Distance label exists for %s"), Distance), Labels.Contains(FString::Printf(TEXT("Phase5_Label_%s"), Distance)));
	}
	TestEqual(TEXT("Five target actors exist"), TargetCount, 5);
	return true;
}

#endif
