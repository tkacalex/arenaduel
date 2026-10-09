#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "ArenaDuel/Weapons/ArenaDuelFlashbang.h"
#include "ArenaDuel/Weapons/ArenaDuelWeaponComponent.h"
#include "ArenaDuel/Player/ArenaDuelPlayerState.h"

namespace ArenaDuelLoadoutTests
{
	// Protected loadout data is read through its reflected properties, so the component needs no test-only accessors.
	template <typename TValue>
	const TValue* Property(const UArenaDuelWeaponComponent* Component, const TCHAR* Name)
	{
		const FProperty* Found = UArenaDuelWeaponComponent::StaticClass()->FindPropertyByName(Name);
		return Found ? Found->ContainerPtrToValuePtr<TValue>(Component) : nullptr;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FArenaDuelLoadoutDefinitionTest, "ArenaDuel.Loadout.Definitions", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FArenaDuelLoadoutDefinitionTest::RunTest(const FString& Parameters)
{
	const UArenaDuelWeaponComponent* Weapons = GetDefault<UArenaDuelWeaponComponent>();
	const auto* Loadouts = ArenaDuelLoadoutTests::Property<TArray<FArenaDuelLoadoutDefinition>>(Weapons, TEXT("Loadouts"));
	if (!TestNotNull(TEXT("Loadouts are reflected data"), Loadouts)) return false;
	TestEqual(TEXT("One loadout per archetype"), Loadouts->Num(), 3);
	if (Loadouts->Num() != 3) return false;
	int32 TwoGunLoadouts = 0;
	for (int32 Archetype = 0; Archetype < Loadouts->Num(); ++Archetype)
	{
		const FArenaDuelLoadoutDefinition& Loadout = (*Loadouts)[Archetype];
		TestTrue(FString::Printf(TEXT("Archetype %d has a firearm"), Archetype), Loadout.Firearms.Num() >= 1);
		for (const uint8 Firearm : Loadout.Firearms) TestNotNull(FString::Printf(TEXT("Archetype %d firearm %d is a real weapon"), Archetype, Firearm), Weapons->GetWeaponDefinition(Firearm));
		TestTrue(FString::Printf(TEXT("Archetype %d carries a sane flashbang count"), Archetype), Loadout.Flashbangs >= 0 && Loadout.Flashbangs <= 5);
		if (Loadout.Firearms.Num() > 1) ++TwoGunLoadouts;
	}
	TestEqual(TEXT("Only the marksman carries two firearms"), TwoGunLoadouts, 1);
	const FArenaDuelLoadoutDefinition& Marksman = (*Loadouts)[static_cast<int32>(EArenaDuelCharacterArchetype::Rift)];
	TestTrue(TEXT("The marksman starts on the DMR"), Marksman.Firearms.Num() == 2 && Marksman.Firearms[0] == static_cast<uint8>(EArenaDuelWeaponId::RuneDMR));
	TestTrue(TEXT("A component without a player starts on its firearm"), Weapons->GetActiveSlot() == EArenaDuelLoadoutSlot::Primary);
	const auto* Knife = ArenaDuelLoadoutTests::Property<FArenaDuelKnifeDefinition>(Weapons, TEXT("Knife"));
	TestTrue(TEXT("Knife reach, damage and pace are positive"), Knife && Knife->Range > 0.0f && Knife->Damage > 0.0f && Knife->AttackInterval > 0.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FArenaDuelFlashbangStrengthTest, "ArenaDuel.Loadout.FlashbangStrength", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FArenaDuelFlashbangStrengthTest::RunTest(const FString& Parameters)
{
	const FVector Eye(0.0f, 0.0f, 0.0f);
	const FVector Forward(1.0f, 0.0f, 0.0f);
	constexpr float MaxDistance = 1800.0f;
	const float CloseFacing = AArenaDuelFlashbang::ComputeBlindStrength(FVector(200.0f, 0.0f, 0.0f), Eye, Forward, MaxDistance);
	const float CloseBehind = AArenaDuelFlashbang::ComputeBlindStrength(FVector(-200.0f, 0.0f, 0.0f), Eye, Forward, MaxDistance);
	const float FarFacing = AArenaDuelFlashbang::ComputeBlindStrength(FVector(1500.0f, 0.0f, 0.0f), Eye, Forward, MaxDistance);
	TestTrue(TEXT("A close burst in view is nearly full strength"), CloseFacing > 0.9f && CloseFacing <= 1.0f);
	TestTrue(TEXT("Looking away weakens it but does not cancel it"), CloseBehind > 0.1f && CloseBehind < 0.5f * CloseFacing);
	TestTrue(TEXT("Distance weakens it"), FarFacing > 0.0f && FarFacing < 0.5f * CloseFacing);
	TestEqual(TEXT("Beyond the maximum distance there is no effect"), AArenaDuelFlashbang::ComputeBlindStrength(FVector(1900.0f, 0.0f, 0.0f), Eye, Forward, MaxDistance), 0.0f);
	return true;
}

#endif
