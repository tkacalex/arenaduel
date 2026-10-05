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

#endif
