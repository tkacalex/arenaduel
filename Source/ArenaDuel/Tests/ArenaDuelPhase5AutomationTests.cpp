#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "ArenaDuel/Weapons/ArenaDuelWeaponComponent.h"
#include "ArenaDuel/Weapons/ArenaDuelWeaponTarget.h"
#include "ArenaDuel/UI/ArenaDuelHUDWidget.h"
#include "../Characters/ArenaDuelCharacter.h"
#include "EnhancedInput/Public/InputMappingContext.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "GameFramework/PlayerStart.h"
#include "Components/BoxComponent.h"
#include "Components/CanvasPanel.h"
#include "Components/Overlay.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FArenaDuelNativeHUDWidgetTreeTest, "ArenaDuel.HUD.NativeWidgetTree", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FArenaDuelNativeHUDWidgetTreeTest::RunTest(const FString& Parameters)
{
	UArenaDuelHUDWidget* HUD = NewObject<UArenaDuelHUDWidget>(GetTransientPackage());
	TestNotNull(TEXT("Native HUD widget was created"), HUD);
	if (!HUD)
	{
		return false;
	}

	TestTrue(TEXT("Native HUD initializes successfully"), HUD->Initialize());
	UCanvasPanel* RootCanvas = Cast<UCanvasPanel>(HUD->GetRootWidget());
	TestNotNull(TEXT("Widget tree root is the native canvas"), RootCanvas);
	if (!RootCanvas)
	{
		return false;
	}
	TestEqual(TEXT("Root canvas contains six expected top-level widgets"), RootCanvas->GetChildrenCount(), 6);
	TestTrue(TEXT("Health panel is present"), Cast<UOverlay>(RootCanvas->GetChildAt(0)) != nullptr);
	TestTrue(TEXT("Weapon panel is present"), Cast<UOverlay>(RootCanvas->GetChildAt(1)) != nullptr);
	TestTrue(TEXT("Both ability slots are present"), Cast<UOverlay>(RootCanvas->GetChildAt(2)) && Cast<UOverlay>(RootCanvas->GetChildAt(3)));
	TestTrue(TEXT("Match header is present"), Cast<UOverlay>(RootCanvas->GetChildAt(4)) != nullptr);
	TestTrue(TEXT("Defeated layer is present"), RootCanvas->GetChildAt(5) != nullptr);

	TestFalse(TEXT("Repeated initialization does not rebuild the widget tree"), HUD->Initialize());
	TestEqual(TEXT("Root child count remains stable after repeated initialization"), RootCanvas->GetChildrenCount(), 6);
	return true;
}

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
	APlayerStart* PlayerStartA = nullptr;
	APlayerStart* PlayerStartB = nullptr;
	for (AActor* Actor : World->PersistentLevel->Actors)
	{
		if (!Actor) continue;
		Labels.Add(Actor->GetActorLabel());
		if (APlayerStart* Start = Cast<APlayerStart>(Actor))
		{
			if (Start->GetActorLabel() == TEXT("Phase5_PlayerStart_A")) PlayerStartA = Start;
			if (Start->GetActorLabel() == TEXT("Phase5_PlayerStart_B")) PlayerStartB = Start;
		}
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
	TestNotNull(TEXT("PlayerStart A resolves"), PlayerStartA);
	TestNotNull(TEXT("PlayerStart B resolves"), PlayerStartB);
	if (PlayerStartA && PlayerStartB)
	{
		const FVector AToB = (PlayerStartB->GetActorLocation() - PlayerStartA->GetActorLocation()).GetSafeNormal();
		const FVector BToA = -AToB;
		TestTrue(TEXT("PlayerStart A faces PlayerStart B"), FVector::DotProduct(PlayerStartA->GetActorForwardVector(), AToB) > 0.95f);
		TestTrue(TEXT("PlayerStart B faces PlayerStart A"), FVector::DotProduct(PlayerStartB->GetActorForwardVector(), BToA) > 0.95f);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FArenaDuelPhase5CharacterVisualsTest, "ArenaDuel.Phase5.CharacterVisuals", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FArenaDuelPhase5CharacterVisualsTest::RunTest(const FString& Parameters)
{
	UClass* CharacterClass = AArenaDuelCharacter::StaticClass();
	const AArenaDuelCharacter* Character = CharacterClass ? Cast<AArenaDuelCharacter>(CharacterClass->GetDefaultObject()) : nullptr;
	TestNotNull(TEXT("Native ArenaDuelCharacter CDO exists"), Character);
	if (!Character) return false;
	const FBoolProperty* OwnerNoSeeProperty = FindFProperty<FBoolProperty>(UPrimitiveComponent::StaticClass(), TEXT("bOwnerNoSee"));
	TestNotNull(TEXT("Development body visual exists"), Character->BodyVisual.Get());
	TestNotNull(TEXT("Development head visual exists"), Character->HeadVisual.Get());
	if (Character->BodyVisual)
	{
		TestEqual(TEXT("Body visual collision is disabled"), Character->BodyVisual->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
		TestTrue(TEXT("Body visual is hidden from owning player"), OwnerNoSeeProperty && OwnerNoSeeProperty->GetPropertyValue_InContainer(Character->BodyVisual));
	}
	if (Character->HeadVisual)
	{
		TestEqual(TEXT("Head visual collision is disabled"), Character->HeadVisual->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
		TestTrue(TEXT("Head visual is hidden from owning player"), OwnerNoSeeProperty && OwnerNoSeeProperty->GetPropertyValue_InContainer(Character->HeadVisual));
	}
	TestTrue(TEXT("Body hit zone covers lower standing body"), Character->BodyHitZone && Character->BodyHitZone->GetRelativeLocation().Z <= -15.0f && Character->BodyHitZone->GetScaledBoxExtent().Z >= 70.0f);
	TestTrue(TEXT("Head hit zone remains above body"), Character->HeadHitZone && Character->HeadHitZone->GetRelativeLocation().Z >= 68.0f);
	return true;
}

#endif
