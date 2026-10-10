#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Editor.h"
#include "ArenaDuel/Characters/ArenaDuelZombie.h"
#include "ArenaDuel/Game/ArenaDuelZombieGameMode.h"
#include "ArenaDuel/UI/ArenaDuelMainMenuWidget.h"
#include "ArenaDuel/Weapons/ArenaDuelWeaponComponent.h"
#include "Engine/TargetPoint.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/WorldSettings.h"
#include "NavMesh/NavMeshBoundsVolume.h"
#include "Tests/AutomationEditorCommon.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FArenaDuelSurvivalWaveTableTest, "ArenaDuel.Survival.WaveTable", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FArenaDuelSurvivalWaveTableTest::RunTest(const FString& Parameters)
{
	const AArenaDuelZombieGameMode* Rules = GetDefault<AArenaDuelZombieGameMode>();
	const TArray<FArenaDuelWaveDefinition>& Table = Rules->GetWaveTable();
	if (!TestEqual(TEXT("Ten authored waves"), Table.Num(), 10)) return false;
	// normal, fast, armoured, mini boss, boss
	const int32 Expected[10][5] = { {10,0,0,0,0}, {15,0,0,0,0}, {20,2,0,0,0}, {25,3,0,0,0}, {30,0,0,1,0}, {35,5,0,0,0}, {40,0,3,0,0}, {45,5,3,0,0}, {50,0,0,2,0}, {55,0,0,0,1} };
	for (int32 Index = 0; Index < 10; ++Index)
	{
		const FArenaDuelWaveDefinition Wave = AArenaDuelZombieGameMode::ComputeWave(Index + 1, Table);
		TestTrue(FString::Printf(TEXT("Wave %d matches the design table"), Index + 1), Wave.Normal == Expected[Index][0] && Wave.Fast == Expected[Index][1] && Wave.Armored == Expected[Index][2] && Wave.MiniBoss == Expected[Index][3] && Wave.Boss == Expected[Index][4]);
		const TArray<EArenaDuelZombieType> Queue = AArenaDuelZombieGameMode::BuildSpawnQueue(Wave, Index + 1);
		TestEqual(FString::Printf(TEXT("Wave %d queues every enemy exactly once"), Index + 1), Queue.Num(), Wave.Total());
		for (const EArenaDuelZombieType Type : { EArenaDuelZombieType::Normal, EArenaDuelZombieType::Fast, EArenaDuelZombieType::Armored, EArenaDuelZombieType::MiniBoss, EArenaDuelZombieType::Boss })
		{
			int32 Queued = 0;
			for (const EArenaDuelZombieType Entry : Queue) if (Entry == Type) ++Queued;
			TestEqual(FString::Printf(TEXT("Wave %d type %d count"), Index + 1, static_cast<int32>(Type)), Queued, Wave.Count(Type));
		}
		if (Wave.Boss > 0) TestTrue(TEXT("The big boss comes last"), Queue.Last() == EArenaDuelZombieType::Boss);
	}
	// Past the table every wave is bigger than the one before and nothing is ever negative.
	int32 Previous = AArenaDuelZombieGameMode::ComputeWave(10, Table).Total();
	for (int32 Number = 11; Number <= 60; ++Number)
	{
		const FArenaDuelWaveDefinition Wave = AArenaDuelZombieGameMode::ComputeWave(Number, Table);
		TestTrue(FString::Printf(TEXT("Wave %d has no negative count"), Number), Wave.Normal > 0 && Wave.Fast >= 0 && Wave.Armored >= 0 && Wave.MiniBoss >= 0 && Wave.Boss >= 0);
		// Rising until the limits are reached, never falling, and never past the limits.
		const bool bEarly = Number <= 20;
		const int32 Before = AArenaDuelZombieGameMode::ComputeWave(Number - 1, Table).Normal;
		TestTrue(FString::Printf(TEXT("Wave %d has at least as many normal zombies as wave %d"), Number, Number - 1), bEarly ? Wave.Normal > Before : Wave.Normal >= Before);
		const float Health = AArenaDuelZombieGameMode::ComputeHealthScale(Number, Table.Num()), HealthBefore = AArenaDuelZombieGameMode::ComputeHealthScale(Number - 1, Table.Num());
		TestTrue(FString::Printf(TEXT("Wave %d health scale does not fall"), Number), bEarly ? Health > HealthBefore : Health >= HealthBefore);
		const float Speed = AArenaDuelZombieGameMode::ComputeSpeedScale(Number, Table.Num());
		TestTrue(FString::Printf(TEXT("Wave %d stays within the limits"), Number), Wave.Normal <= 120 && Wave.Fast <= 30 && Wave.Armored <= 20 && Wave.MiniBoss <= 4 && Wave.Boss <= 3 && Health <= 3.0f && Speed >= 1.0f && Speed <= 1.25f);
		Previous = Wave.Total();
	}
	TestEqual(TEXT("Speed is unscaled through the table"), AArenaDuelZombieGameMode::ComputeSpeedScale(10, Table.Num()), 1.0f);
	TestTrue(TEXT("Late waves are faster"), AArenaDuelZombieGameMode::ComputeSpeedScale(20, Table.Num()) > 1.0f);
	// Hit reactions: a normal zombie flinches, more from a head shot; the big boss never does.
	TestTrue(TEXT("A hit slows a normal zombie"), AArenaDuelZombie::ComputeStaggerSeconds(EArenaDuelZombieType::Normal, 0.2f, false) > 0.0f);
	TestTrue(TEXT("A head shot slows it for longer"), AArenaDuelZombie::ComputeStaggerSeconds(EArenaDuelZombieType::Normal, 0.2f, true) > AArenaDuelZombie::ComputeStaggerSeconds(EArenaDuelZombieType::Normal, 0.2f, false));
	TestEqual(TEXT("The big boss does not flinch"), AArenaDuelZombie::ComputeStaggerSeconds(EArenaDuelZombieType::Boss, 1.0f, true), 0.0f);
	TestEqual(TEXT("Armour does not flinch from a body shot"), AArenaDuelZombie::ComputeStaggerSeconds(EArenaDuelZombieType::Armored, 0.5f, false), 0.0f);
	TestTrue(TEXT("No hit slows a zombie for more than a second"), AArenaDuelZombie::ComputeStaggerSeconds(EArenaDuelZombieType::Normal, 1.0f, true) < 1.0f);
	TestTrue(TEXT("Every fifth late wave has a big boss"), AArenaDuelZombieGameMode::ComputeWave(15, Table).Boss >= 1 && AArenaDuelZombieGameMode::ComputeWave(20, Table).Boss >= 1 && AArenaDuelZombieGameMode::ComputeWave(16, Table).Boss == 0);
	TestEqual(TEXT("Health is unscaled through the table"), AArenaDuelZombieGameMode::ComputeHealthScale(10, Table.Num()), 1.0f);
	TestEqual(TEXT("Wave zero is empty"), AArenaDuelZombieGameMode::ComputeWave(0, Table).Total(), 0);
	TestTrue(TEXT("The alive limit protects performance"), Rules->GetMaxActiveZombies() >= 8 && Rules->GetMaxActiveZombies() <= 64);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FArenaDuelSurvivalEnemyTypesTest, "ArenaDuel.Survival.EnemyTypes", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FArenaDuelSurvivalEnemyTypesTest::RunTest(const FString& Parameters)
{
	const TArray<FArenaDuelZombieTypeConfig>& Types = GetDefault<AArenaDuelZombieGameMode>()->GetTypeConfigs();
	if (!TestEqual(TEXT("Five enemy types"), Types.Num(), 5)) return false;
	const FArenaDuelZombieTypeConfig& Normal = Types[static_cast<int32>(EArenaDuelZombieType::Normal)];
	const FArenaDuelZombieTypeConfig& Fast = Types[static_cast<int32>(EArenaDuelZombieType::Fast)];
	const FArenaDuelZombieTypeConfig& Armored = Types[static_cast<int32>(EArenaDuelZombieType::Armored)];
	const FArenaDuelZombieTypeConfig& MiniBoss = Types[static_cast<int32>(EArenaDuelZombieType::MiniBoss)];
	const FArenaDuelZombieTypeConfig& Boss = Types[static_cast<int32>(EArenaDuelZombieType::Boss)];
	TestTrue(TEXT("Runners are much faster and have less health"), Fast.MoveSpeed > Normal.MoveSpeed * 1.8f && Fast.Health < Normal.Health);
	TestTrue(TEXT("Armoured zombies are slower, tougher, and weak in the head"), Armored.MoveSpeed < Normal.MoveSpeed && Armored.Health > Normal.Health && Armored.BodyDamageFactor < 1.0f && Armored.HeadDamageFactor > 1.0f);
	TestTrue(TEXT("Bosses have far more health and are bigger"), MiniBoss.Health > Normal.Health * 10.0f && Boss.Health > MiniBoss.Health * 2.0f && Boss.Scale > MiniBoss.Scale && MiniBoss.Scale > Normal.Scale);
	TestTrue(TEXT("Bosses pay more"), MiniBoss.Points > Normal.Points && Boss.Points > MiniBoss.Points);
	for (const FArenaDuelZombieTypeConfig& Type : Types) TestTrue(FString::Printf(TEXT("%s can be dodged: the swing takes time"), *Type.DisplayName), Type.AttackWindup > 0.1f && Type.AttackInterval > Type.AttackWindup);

	// Damage against one of each, straight through the same entry point the weapons use.
	FAutomationEditorCommonUtils::CreateNewMap();
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!TestNotNull(TEXT("Test world"), World)) return false;
	const auto Spawn = [World](EArenaDuelZombieType Type, const FArenaDuelZombieTypeConfig& Config)
	{
		AArenaDuelZombie* Zombie = World->SpawnActor<AArenaDuelZombie>(AArenaDuelZombie::StaticClass(), FVector(0.0f, 0.0f, 500.0f), FRotator::ZeroRotator);
		if (Zombie) Zombie->InitializeZombie(Type, Config, 1.0f, 1.0f);
		return Zombie;
	};
	AArenaDuelZombie* Walker = Spawn(EArenaDuelZombieType::Normal, Normal);
	AArenaDuelZombie* Tank = Spawn(EArenaDuelZombieType::Armored, Armored);
	if (!TestNotNull(TEXT("Normal zombie"), Walker) || !TestNotNull(TEXT("Armoured zombie"), Tank)) return false;
	TestEqual(TEXT("A zombie starts at full health"), Walker->GetHealth(), Normal.Health);
	TestEqual(TEXT("A body shot does the weapon's damage"), Walker->TakeWeaponHit(20.0f, 1.0f, EArenaDuelShotResult::Body, nullptr, FVector::ZeroVector, FVector::ForwardVector), 20.0f);
	TestEqual(TEXT("A head shot does the weapon's head damage"), Walker->TakeWeaponHit(20.0f, 1.4f, EArenaDuelShotResult::Head, nullptr, FVector::ZeroVector, FVector::ForwardVector), 28.0f);
	TestTrue(TEXT("Armour takes the edge off a body shot"), FMath::IsNearlyEqual(Tank->TakeWeaponHit(20.0f, 1.0f, EArenaDuelShotResult::Body, nullptr, FVector::ZeroVector, FVector::ForwardVector), 20.0f * Armored.BodyDamageFactor, 0.01f));
	TestTrue(TEXT("The armoured head is the weak spot"), FMath::IsNearlyEqual(Tank->TakeWeaponHit(20.0f, 1.4f, EArenaDuelShotResult::Head, nullptr, FVector::ZeroVector, FVector::ForwardVector), 28.0f * Armored.HeadDamageFactor, 0.01f));
	TestFalse(TEXT("Still alive after two hits"), Walker->IsDead());
	Walker->TakeWeaponHit(500.0f, 1.0f, EArenaDuelShotResult::Body, nullptr, FVector::ZeroVector, FVector::ForwardVector);
	TestTrue(TEXT("Dead once the health is gone"), Walker->IsDead() && Walker->GetHealth() <= 0.0f);
	TestEqual(TEXT("A dead zombie takes no more damage"), Walker->TakeWeaponHit(20.0f, 1.0f, EArenaDuelShotResult::Body, nullptr, FVector::ZeroVector, FVector::ForwardVector), 0.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FArenaDuelSurvivalMapTest, "ArenaDuel.Survival.ArenaMap", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FArenaDuelSurvivalMapTest::RunTest(const FString& Parameters)
{
	FAutomationEditorCommonUtils::LoadMap(TEXT("/Game/ArenaDuel/Maps/L_ZombieArena"));
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!TestNotNull(TEXT("L_ZombieArena loads"), World)) return false;
	TestTrue(TEXT("The map runs the survival game mode"), World->GetWorldSettings() && World->GetWorldSettings()->DefaultGameMode == AArenaDuelZombieGameMode::StaticClass());
	const APlayerStart* Start = nullptr;
	for (TActorIterator<APlayerStart> It(World); It; ++It) if (It->PlayerStartTag == FName(TEXT("ArenaCore_P1"))) Start = *It;
	if (!TestNotNull(TEXT("Player start"), Start)) return false;
	int32 Volumes = 0;
	for (TActorIterator<ANavMeshBoundsVolume> It(World); It; ++It) ++Volumes;
	TestTrue(TEXT("The map has a navigation bounds volume"), Volumes >= 1);
	int32 Points = 0, Hidden = 0;
	for (TActorIterator<ATargetPoint> It(World); It; ++It)
	{
		if (!It->ActorHasTag(TEXT("ZombieSpawn"))) continue;
		++Points;
		const FVector Location = It->GetActorLocation();
		TestTrue(TEXT("A spawn point keeps its distance from the player start"), FVector::Dist(Location, Start->GetActorLocation()) > 1500.0);
		FHitResult Floor;
		TestTrue(TEXT("A spawn point stands on the floor"), World->LineTraceSingleByChannel(Floor, Location + FVector(0, 0, 200), Location - FVector(0, 0, 400), ECC_Visibility) && FMath::Abs(Floor.ImpactPoint.Z) < 5.0);
		FHitResult Wall;
		if (World->LineTraceSingleByChannel(Wall, Start->GetActorLocation() + FVector(0, 0, 64), Location + FVector(0, 0, 90), ECC_Visibility)) ++Hidden;
	}
	TestTrue(TEXT("Enough spawn points to spread a wave"), Points >= 12);
	TestTrue(FString::Printf(TEXT("Most spawn points are out of sight of the start (%d of %d)"), Hidden, Points), Hidden * 2 > Points);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FArenaDuelMenuAddressTest, "ArenaDuel.Menu.JoinAddress", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FArenaDuelMenuAddressTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("A bare address gets the game port"), UArenaDuelMainMenuWidget::NormalizeJoinAddress(TEXT(" 192.168.0.12 ")), FString(TEXT("192.168.0.12:7777")));
	TestEqual(TEXT("An address with a port is kept"), UArenaDuelMainMenuWidget::NormalizeJoinAddress(TEXT("192.168.0.12:7778")), FString(TEXT("192.168.0.12:7778")));
	TestEqual(TEXT("A host name works"), UArenaDuelMainMenuWidget::NormalizeJoinAddress(TEXT("my-pc.local")), FString(TEXT("my-pc.local:7777")));
	for (const TCHAR* Bad : { TEXT(""), TEXT("   "), TEXT("192.168.0.12:abc"), TEXT("192.168.0.12:0"), TEXT("192.168.0.12:70000"), TEXT(":7777"), TEXT("host name"), TEXT("host?game=Other"), TEXT("/Game/Maps/Other"), TEXT("host#x") })
	{
		TestTrue(FString::Printf(TEXT("\"%s\" is refused"), Bad), UArenaDuelMainMenuWidget::NormalizeJoinAddress(Bad).IsEmpty());
	}
	return true;
}

#endif
