#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "UObject/UnrealType.h"
#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"
#include "ArenaDuel/Characters/ArenaDuelCharacter.h"
#include "ArenaDuel/Weapons/ArenaDuelFlashbang.h"
#include "ArenaDuel/Weapons/ArenaDuelWeaponComponent.h"
#include "ArenaDuel/Player/ArenaDuelPlayerState.h"
#include "Camera/CameraComponent.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Tests/AutomationEditorCommon.h"

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
	TestTrue(TEXT("Heavy stab hits harder and recovers slower than the quick slash"), Knife && Knife->HeavyDamage > Knife->Damage && Knife->HeavyAttackInterval > Knife->AttackInterval);
	const auto* Flash = ArenaDuelLoadoutTests::Property<FArenaDuelFlashbangDefinition>(Weapons, TEXT("Flashbang"));
	TestTrue(TEXT("Short throw is slower than the long throw"), Flash && Flash->ShortThrowSpeed > 0.0f && Flash->ShortThrowSpeed < Flash->ThrowSpeed);
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FArenaDuelAimRulesTest, "ArenaDuel.Loadout.AimRules", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FArenaDuelAimRulesTest::RunTest(const FString& Parameters)
{
	const UArenaDuelWeaponComponent* Weapons = GetDefault<UArenaDuelWeaponComponent>();
	const float* Settle = ArenaDuelLoadoutTests::Property<float>(Weapons, TEXT("AimSettleSeconds"));
	TestTrue(TEXT("The aim bonus comes in over a short, non-zero time"), Settle && *Settle > 0.0f && *Settle <= 0.3f);
	TestEqual(TEXT("Not aiming gives no aim bonus"), Weapons->GetAimAccuracyAlpha(), 0.0f);
	TestFalse(TEXT("No aim key is held by default"), Weapons->IsAimHeld());
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FArenaDuelSniperHeadshotTest, "ArenaDuel.Loadout.SniperHeadshotKills", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FArenaDuelSniperHeadshotTest::RunTest(const FString& Parameters)
{
	// Read from the player Blueprint, not the native class, so a saved override in the asset cannot hide a wrong value.
	const UClass* CharacterClass = LoadClass<AArenaDuelCharacter>(nullptr, TEXT("/Game/ArenaDuel/Characters/BP_ArenaDuelCharacter.BP_ArenaDuelCharacter_C"));
	const AArenaDuelCharacter* Defaults = CharacterClass ? Cast<AArenaDuelCharacter>(CharacterClass->GetDefaultObject()) : nullptr;
	const UArenaDuelWeaponComponent* Weapons = Defaults ? Defaults->GetWeaponComponent() : nullptr;
	if (!TestNotNull(TEXT("Weapon component of the player Blueprint"), Weapons)) return false;
	const FArenaDuelWeaponDefinition* Sniper = Weapons->GetWeaponDefinition(static_cast<int32>(EArenaDuelWeaponId::RuneDMR));
	if (!TestNotNull(TEXT("Rune DMR definition"), Sniper)) return false;
	const float MaxHealth = 100.0f;
	TestTrue(FString::Printf(TEXT("A Rune DMR head shot kills from full health (%.0f damage)"), Sniper->BodyDamage * Sniper->HeadshotMultiplier), Sniper->BodyDamage * Sniper->HeadshotMultiplier >= MaxHealth);
	TestTrue(TEXT("A body shot does not"), Sniper->BodyDamage < MaxHealth);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FArenaDuelSniperScopeTest, "ArenaDuel.Loadout.SniperScope", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FArenaDuelSniperScopeTest::RunTest(const FString& Parameters)
{
	const UClass* CharacterClass = LoadClass<AArenaDuelCharacter>(nullptr, TEXT("/Game/ArenaDuel/Characters/BP_ArenaDuelCharacter.BP_ArenaDuelCharacter_C"));
	const AArenaDuelCharacter* Defaults = CharacterClass ? Cast<AArenaDuelCharacter>(CharacterClass->GetDefaultObject()) : nullptr;
	const UArenaDuelWeaponComponent* Weapons = Defaults ? Defaults->GetWeaponComponent() : nullptr;
	if (!TestNotNull(TEXT("Weapon component of the player Blueprint"), Weapons)) return false;
	for (int32 Index = 0; Index < 4; ++Index)
	{
		const FArenaDuelWeaponDefinition* Definition = Weapons->GetWeaponDefinition(Index);
		if (!TestNotNull(TEXT("Weapon definition"), Definition)) return false;
		TestEqual(FString::Printf(TEXT("Only the Rune DMR has a scope (weapon %d)"), Index), Definition->bHasScope, Definition->Id == EArenaDuelWeaponId::RuneDMR);
	}
	const FArenaDuelWeaponDefinition* Sniper = Weapons->GetWeaponDefinition(static_cast<int32>(EArenaDuelWeaponId::RuneDMR));
	TestTrue(TEXT("The second zoom is stronger than the first, and both are stronger than plain aiming"), Sniper->ScopeFOVSecond < Sniper->ScopeFOVFirst && Sniper->ScopeFOVFirst < Sniper->AimFOV);
	TestTrue(TEXT("Moving in the scope is slower but possible"), Sniper->ScopedMoveSpeedScale > 0.2f && Sniper->ScopedMoveSpeedScale < 1.0f);
	TestTrue(TEXT("The scope comes back quickly after a shot"), Sniper->ScopeRezoomSeconds >= 0.0f && Sniper->ScopeRezoomSeconds <= 0.5f);
	TestEqual(TEXT("A weapon starts outside the scope"), Weapons->GetScopeLevel(), 0);
	TestEqual(TEXT("And shows no scope picture"), Weapons->GetScopeOverlayAlpha(), 0.0f);
	TestEqual(TEXT("And moves at full speed"), Weapons->GetAimMoveSpeedScale(), 1.0f);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FArenaDuelCrouchedShotOriginTest, "ArenaDuel.Loadout.CrouchedShotOrigin", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FArenaDuelCrouchedShotOriginTest::RunTest(const FString& Parameters)
{
	FAutomationEditorCommonUtils::CreateNewMap();
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!TestNotNull(TEXT("Test world"), World)) return false;
	AArenaDuelCharacter* Character = World->SpawnActor<AArenaDuelCharacter>(AArenaDuelCharacter::StaticClass(), FVector(0.0f, 0.0f, 300.0f), FRotator::ZeroRotator);
	if (!TestNotNull(TEXT("Character"), Character) || !TestNotNull(TEXT("Camera"), Character->GetFirstPersonCamera())) return false;
	const float CameraHeight = Character->GetFirstPersonCamera()->GetRelativeLocation().Z;
	TestTrue(TEXT("Standing, shots start at the camera"), FMath::IsNearlyEqual(Character->GetPawnViewLocation().Z - Character->GetActorLocation().Z, CameraHeight, 0.1));
	// What a crouch does to the eye height in the engine; the camera does not follow it.
	Character->bIsCrouched = true;
	Character->RecalculateBaseEyeHeight();
	TestTrue(TEXT("The engine's eye height drops when crouched"), Character->BaseEyeHeight < CameraHeight - 10.0f);
	TestTrue(TEXT("Crouched, shots still start at the camera"), FMath::IsNearlyEqual(Character->GetPawnViewLocation().Z - Character->GetActorLocation().Z, CameraHeight, 0.1));
	return true;
}
namespace
{
	// The held item is replicated state the server owns; a test sets it the way replication would.
	void SetReplicatedByte(UObject* Object, const TCHAR* Name, uint8 Value)
	{
		if (const FByteProperty* Property = FindFProperty<FByteProperty>(Object->GetClass(), Name)) Property->SetPropertyValue_InContainer(Object, Value);
	}
	void SetReplicatedBool(UObject* Object, const TCHAR* Name, bool bValue)
	{
		if (const FBoolProperty* Property = FindFProperty<FBoolProperty>(Object->GetClass(), Name)) Property->SetPropertyValue_InContainer(Object, bValue);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FArenaDuelHeldItemPresentationTest, "ArenaDuel.Loadout.HeldItemPresentation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FArenaDuelHeldItemPresentationTest::RunTest(const FString& Parameters)
{
	FAutomationEditorCommonUtils::CreateNewMap();
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!TestNotNull(TEXT("Test world"), World)) return false;
	AArenaDuelCharacter* Character = World->SpawnActor<AArenaDuelCharacter>(AArenaDuelCharacter::StaticClass(), FVector(0.0f, 0.0f, 300.0f), FRotator::ZeroRotator);
	UArenaDuelWeaponComponent* Weapon = Character ? Character->GetWeaponComponent() : nullptr;
	if (!TestNotNull(TEXT("Weapon component"), Weapon) || !TestNotNull(TEXT("First person weapon mesh"), Weapon->GetFirstPersonWeaponMesh())) return false;
	const auto Hold = [Weapon](EArenaDuelLoadoutSlot Slot)
	{
		SetReplicatedByte(Weapon, TEXT("ActiveSlot"), static_cast<uint8>(Slot));
		Weapon->NotifyHeldItemChanged();
	};

	// Regression: after a throw the server puts the firearm back at once, and the throw used to play on with the firearm in hand.
	Hold(EArenaDuelLoadoutSlot::Flashbang);
	TestNotNull(TEXT("The grenade is in the hand before the throw"), Weapon->GetFirstPersonWeaponMesh()->GetStaticMesh().Get());
	Weapon->NotifyEquipmentThrownCosmetic(false);
	Hold(EArenaDuelLoadoutSlot::Primary);
	TestTrue(TEXT("The throw is shown"), Weapon->IsShowingThrow());
	TestTrue(TEXT("While it is shown the hands are not presented with the firearm"), Weapon->GetPresentedSlot() == EArenaDuelLoadoutSlot::Flashbang);
	TestNull(TEXT("While it is shown the hand is empty"), Weapon->GetFirstPersonWeaponMesh()->GetStaticMesh().Get());
	TestNull(TEXT("The world body's hand is empty too"), Weapon->GetThirdPersonWeaponMesh()->GetStaticMesh().Get());
	TestTrue(TEXT("The rule state is untouched: the firearm is the active slot"), Weapon->GetActiveSlot() == EArenaDuelLoadoutSlot::Primary);
	Weapon->FinishThrowPresentation();
	TestFalse(TEXT("The throw is over"), Weapon->IsShowingThrow());
	TestTrue(TEXT("The firearm is presented"), Weapon->GetPresentedSlot() == EArenaDuelLoadoutSlot::Primary);
	TestNotNull(TEXT("The firearm is in the hand"), Weapon->GetFirstPersonWeaponMesh()->GetStaticMesh().Get());
	// The other order of arrival: the slot first, the throw after it.
	Hold(EArenaDuelLoadoutSlot::Flashbang);
	Hold(EArenaDuelLoadoutSlot::Primary);
	Weapon->NotifyEquipmentThrownCosmetic(true);
	TestNull(TEXT("A throw that arrives after the slot change still empties the hand"), Weapon->GetFirstPersonWeaponMesh()->GetStaticMesh().Get());
	Weapon->FinishThrowPresentation();

	// Regression: a knife swing used to keep moving the viewmodel after a quick change to another item.
	Hold(EArenaDuelLoadoutSlot::Knife);
	Weapon->NotifyKnifeSwingCosmetic(false, true);
	TestTrue(TEXT("The swing is shown with the knife"), Weapon->IsShowingKnifeSwing());
	Hold(EArenaDuelLoadoutSlot::Primary);
	TestFalse(TEXT("The swing ends with the knife"), Weapon->IsShowingKnifeSwing());
	Weapon->NotifyKnifeSwingCosmetic(true, false);
	TestFalse(TEXT("A swing that arrives after the change is not shown"), Weapon->IsShowingKnifeSwing());
	Hold(EArenaDuelLoadoutSlot::Knife);
	TestFalse(TEXT("Coming back to the knife does not replay the old swing"), Weapon->IsShowingKnifeSwing());
	Hold(EArenaDuelLoadoutSlot::Primary);

	// Regression: on a weapon that is not the local player's the presentation switches itself off when idle,
	// and a reload that began later did not switch it on again, so the magazine and bolt stood still.
	Weapon->SetComponentTickEnabled(false);
	SetReplicatedBool(Weapon, TEXT("bReloading"), true);
	Weapon->OnRep_Reloading();
	TestTrue(TEXT("A reload wakes the presentation of an idle weapon"), Weapon->IsComponentTickEnabled());
	TestTrue(TEXT("The support hand goes with the magazine"), Weapon->DoesSupportHandFollowReload());
	SetReplicatedBool(Weapon, TEXT("bReloading"), false);
	return true;
}
#endif
