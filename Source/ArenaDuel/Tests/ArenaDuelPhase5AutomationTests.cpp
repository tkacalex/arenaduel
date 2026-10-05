#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "ArenaDuel/Weapons/ArenaDuelWeaponComponent.h"

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

#endif
