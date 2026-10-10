#include "ArenaDuelZombieGameMode.h"
#include "ArenaDuelZombieGameState.h"
#include "../Characters/ArenaDuelCharacter.h"
#include "../Characters/ArenaDuelZombie.h"
#include "../Player/ArenaDuelPlayerState.h"
#include "../Weapons/ArenaDuelWeaponComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/TargetPoint.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Misc/App.h"
#include "Components/BrushComponent.h"
#include "NavMesh/NavMeshBoundsVolume.h"
#include "PhysicsEngine/BodySetup.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"
#include "GameFramework/PlayerStart.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogArenaDuelSurvival, Log, All);

#if !UE_BUILD_SHIPPING
namespace
{
	AArenaDuelZombieGameMode* SurvivalMode(UWorld* World) { return World ? World->GetAuthGameMode<AArenaDuelZombieGameMode>() : nullptr; }
	// Development helpers for fast-forwarding a run. They only do something on a machine that runs the survival game mode.
	FAutoConsoleCommandWithWorld CmdKillAll(TEXT("ArenaDuel.Zombie.KillAll"), TEXT("Kill every zombie that is alive, without points"),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World) { if (AArenaDuelZombieGameMode* Mode = SurvivalMode(World)) Mode->DevKillAllZombies(); }));
	FAutoConsoleCommandWithWorldAndArgs CmdTimeScale(TEXT("ArenaDuel.Zombie.TimeScale"), TEXT("Scale the countdown, intermission and spawn timers of the run, 0.02 to 1"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World) { if (AArenaDuelZombieGameMode* Mode = SurvivalMode(World); Mode && Args.Num() > 0) Mode->DevSetTimeScale(FCString::Atof(*Args[0])); }));
	FAutoConsoleCommandWithWorldAndArgs CmdDamage(TEXT("ArenaDuel.Zombie.Damage"), TEXT("Scale the damage of zombies spawned from now on; 0 makes them harmless"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World) { if (AArenaDuelZombieGameMode* Mode = SurvivalMode(World); Mode && Args.Num() > 0) Mode->DevSetZombieDamageScale(FCString::Atof(*Args[0])); }));
	FAutoConsoleCommandWithWorldAndArgs CmdAutoPlay(TEXT("ArenaDuel.Zombie.AutoPlay"), TEXT("1: zombies that reach a player are killed and credited, so waves play themselves"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World) { if (AArenaDuelZombieGameMode* Mode = SurvivalMode(World); Mode && Args.Num() > 0) Mode->DevSetAutoPlay(FCString::Atoi(*Args[0]) != 0); }));
	FAutoConsoleCommandWithWorld CmdStats(TEXT("ArenaDuel.Zombie.Stats"), TEXT("Log the state of the run and of the enemies alive"),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World) { if (const AArenaDuelZombieGameMode* Mode = SurvivalMode(World)) Mode->DevLogStats(); }));
	FAutoConsoleCommandWithWorld CmdRestart(TEXT("ArenaDuel.Zombie.Restart"), TEXT("Restart the survival run"),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World) { if (AArenaDuelZombieGameMode* Mode = SurvivalMode(World)) Mode->RestartSurvival(); }));
	FAutoConsoleCommandWithWorldAndArgs CmdPoints(TEXT("ArenaDuel.Zombie.Points"), TEXT("Give the first player this many points"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			const APlayerController* Controller = World ? World->GetFirstPlayerController() : nullptr;
			if (AArenaDuelPlayerState* Player = Controller ? Controller->GetPlayerState<AArenaDuelPlayerState>() : nullptr; Player && SurvivalMode(World) && Args.Num() > 0) Player->AddSurvivalPoints(FCString::Atoi(*Args[0]));
		}));
	FAutoConsoleCommandWithWorldAndArgs CmdBuy(TEXT("ArenaDuel.Zombie.Buy"), TEXT("Buy for the first player: 0 ammo, 1 heal, 2 damage"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			AArenaDuelZombieGameMode* Mode = SurvivalMode(World);
			if (!Mode || Args.Num() == 0) return;
			const bool bBought = Mode->TryPurchase(World->GetFirstPlayerController(), static_cast<EArenaDuelSurvivalPurchase>(FMath::Clamp(FCString::Atoi(*Args[0]), 0, 2)));
			UE_LOG(LogArenaDuelSurvival, Log, TEXT("Survival purchase %s: %s"), *Args[0], bBought ? TEXT("bought") : TEXT("refused"));
		}));
}
#endif

AArenaDuelZombieGameMode::AArenaDuelZombieGameMode()
{
	GameStateClass = AArenaDuelZombieGameState::StaticClass();
	// The input mapping, the input actions and the meshes are set on the character Blueprint. The duel gets it
	// from BP_ArenaDuelGameMode; this mode is used as a native class by its map and has to name it itself.
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawn(TEXT("/Game/ArenaDuel/Characters/BP_ArenaDuelCharacter"));
	if (PlayerPawn.Succeeded()) DefaultPawnClass = PlayerPawn.Class;
	WaveTable = DefaultWaveTable();

	TypeConfigs.SetNum(5);
	FArenaDuelZombieTypeConfig& Normal = TypeConfigs[static_cast<int32>(EArenaDuelZombieType::Normal)];
	Normal.DisplayName = TEXT("ZOMBIE"); Normal.Health = 100.0f; Normal.MoveSpeed = 340.0f; Normal.Damage = 12.0f; Normal.Points = 100;
	Normal.MaterialPath = TEXT("/Game/ArenaDuel/Characters/Common/M_SurvivalZombie.M_SurvivalZombie");

	FArenaDuelZombieTypeConfig& Fast = TypeConfigs[static_cast<int32>(EArenaDuelZombieType::Fast)];
	Fast.DisplayName = TEXT("RUNNER"); Fast.Health = 55.0f; Fast.MoveSpeed = 760.0f; Fast.Damage = 9.0f; Fast.AttackInterval = 0.8f; Fast.AttackWindup = 0.25f; Fast.Scale = 0.9f; Fast.Points = 150;
	Fast.MaterialPath = TEXT("/Game/ArenaDuel/Characters/Common/M_SurvivalRunner.M_SurvivalRunner");

	// Armour: the body shrugs off more than half of a hit, the head takes more than usual.
	FArenaDuelZombieTypeConfig& Armored = TypeConfigs[static_cast<int32>(EArenaDuelZombieType::Armored)];
	Armored.DisplayName = TEXT("ARMOURED"); Armored.Health = 260.0f; Armored.MoveSpeed = 230.0f; Armored.Damage = 20.0f; Armored.AttackInterval = 1.4f; Armored.Scale = 1.12f; Armored.BodyDamageFactor = 0.45f; Armored.HeadDamageFactor = 1.5f; Armored.Points = 250;
	Armored.MaterialPath = TEXT("/Game/ArenaDuel/Characters/Common/M_SurvivalArmoured.M_SurvivalArmoured");

	FArenaDuelZombieTypeConfig& MiniBoss = TypeConfigs[static_cast<int32>(EArenaDuelZombieType::MiniBoss)];
	MiniBoss.DisplayName = TEXT("BRUTE"); MiniBoss.Health = 1500.0f; MiniBoss.MoveSpeed = 380.0f; MiniBoss.Damage = 28.0f; MiniBoss.AttackInterval = 1.5f; MiniBoss.AttackRange = 190.0f; MiniBoss.AttackWindup = 0.45f; MiniBoss.Scale = 1.45f; MiniBoss.Points = 1000;
	MiniBoss.MaterialPath = TEXT("/Game/ArenaDuel/Characters/Common/M_SurvivalBrute.M_SurvivalBrute");

	FArenaDuelZombieTypeConfig& Boss = TypeConfigs[static_cast<int32>(EArenaDuelZombieType::Boss)];
	Boss.DisplayName = TEXT("ABOMINATION"); Boss.Health = 6000.0f; Boss.MoveSpeed = 360.0f; Boss.Damage = 38.0f; Boss.AttackInterval = 1.7f; Boss.AttackRange = 240.0f; Boss.AttackWindup = 0.55f; Boss.Scale = 2.0f; Boss.Points = 5000;
	Boss.MaterialPath = TEXT("/Game/ArenaDuel/Characters/Common/M_SurvivalAbomination.M_SurvivalAbomination");
}

TArray<FArenaDuelWaveDefinition> AArenaDuelZombieGameMode::DefaultWaveTable()
{
	const auto Wave = [](int32 Normal, int32 Fast, int32 Armored, int32 MiniBoss, int32 Boss)
	{
		FArenaDuelWaveDefinition Definition;
		Definition.Normal = Normal; Definition.Fast = Fast; Definition.Armored = Armored; Definition.MiniBoss = MiniBoss; Definition.Boss = Boss;
		return Definition;
	};
	return { Wave(10, 0, 0, 0, 0), Wave(15, 0, 0, 0, 0), Wave(20, 2, 0, 0, 0), Wave(25, 3, 0, 0, 0), Wave(30, 0, 0, 1, 0),
		Wave(35, 5, 0, 0, 0), Wave(40, 0, 3, 0, 0), Wave(45, 5, 3, 0, 0), Wave(50, 0, 0, 2, 0), Wave(55, 0, 0, 0, 1) };
}

FArenaDuelWaveDefinition AArenaDuelZombieGameMode::ComputeWave(int32 WaveNumber, const TArray<FArenaDuelWaveDefinition>& Table)
{
	if (Table.Num() == 0 || WaveNumber < 1) return FArenaDuelWaveDefinition();
	if (WaveNumber <= Table.Num()) return Table[WaveNumber - 1];
	// Past the table every wave has five more normal zombies, a growing mix of specials,
	// mini bosses on every other wave and a big boss on every fifth.
	const int32 Extra = WaveNumber - Table.Num();
	FArenaDuelWaveDefinition Wave;
	// Every count has a ceiling: a late wave is long and hard, but it stays a wave that can be finished.
	Wave.Normal = FMath::Min(Table.Last().Normal + 5 * Extra, 120);
	Wave.Fast = FMath::Min(4 + Extra, 30);
	Wave.Armored = FMath::Min(2 + Extra / 2, 20);
	Wave.MiniBoss = Extra % 2 == 0 ? FMath::Min(1 + Extra / 4, 4) : 0;
	Wave.Boss = WaveNumber % 5 == 0 ? FMath::Min(WaveNumber / 10, 3) : 0;
	return Wave;
}

float AArenaDuelZombieGameMode::ComputeHealthScale(int32 WaveNumber, int32 TableWaves)
{
	return WaveNumber <= TableWaves ? 1.0f : FMath::Min(1.0f + 0.08f * static_cast<float>(WaveNumber - TableWaves), 3.0f);
}

float AArenaDuelZombieGameMode::ComputeSpeedScale(int32 WaveNumber, int32 TableWaves)
{
	return WaveNumber <= TableWaves ? 1.0f : FMath::Min(1.0f + 0.015f * static_cast<float>(WaveNumber - TableWaves), 1.25f);
}

TArray<EArenaDuelZombieType> AArenaDuelZombieGameMode::BuildSpawnQueue(const FArenaDuelWaveDefinition& Wave, int32 Seed)
{
	TArray<EArenaDuelZombieType> Queue;
	Queue.Reserve(Wave.Total());
	for (int32 Index = 0; Index < Wave.Normal; ++Index) Queue.Add(EArenaDuelZombieType::Normal);
	for (int32 Index = 0; Index < Wave.Fast; ++Index) Queue.Add(EArenaDuelZombieType::Fast);
	for (int32 Index = 0; Index < Wave.Armored; ++Index) Queue.Add(EArenaDuelZombieType::Armored);
	// Specials are spread through the wave; the first few spawns stay as they are so a wave opens gently.
	FRandomStream Random(Seed);
	for (int32 Index = Queue.Num() - 1; Index > 0; --Index) Queue.Swap(Index, Random.RandRange(0, Index));
	// Bosses come once most of the wave is through.
	for (int32 Index = 0; Index < Wave.MiniBoss; ++Index) Queue.Insert(EArenaDuelZombieType::MiniBoss, FMath::Max(0, Queue.Num() - Queue.Num() / 4));
	for (int32 Index = 0; Index < Wave.Boss; ++Index) Queue.Add(EArenaDuelZombieType::Boss);
	return Queue;
}

AArenaDuelZombieGameState* AArenaDuelZombieGameMode::GetSurvivalState() const
{
	return GetGameState<AArenaDuelZombieGameState>();
}

void AArenaDuelZombieGameMode::EnterCharacterSelect()
{
	// Survival has no character select. The duel flow that would lead here is never started.
}

void AArenaDuelZombieGameMode::BeginPlay()
{
	// Deliberately not the duel BeginPlay, which opens character select.
	AGameMode::BeginPlay();
	EnsureNavigationBounds();
	BeginRun();
}

void AArenaDuelZombieGameMode::EnsureNavigationBounds()
{
	// The navigation mesh is only built inside the bounds of a volume. The arena's extent is known from
	// its spawn points and player starts, so a volume of that size is made here; the run does not depend
	// on a volume in the map having a usable brush.
	UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (!Navigation) return;
	FBox Area(ForceInit);
	for (TActorIterator<ATargetPoint> It(GetWorld()); It; ++It) if (It->ActorHasTag(TEXT("ZombieSpawn"))) Area += It->GetActorLocation();
	for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It) Area += It->GetActorLocation();
	if (!Area.IsValid) return;
	Area = Area.ExpandBy(FVector(600.0f, 600.0f, 0.0f));
	Area.Min.Z -= 300.0f;
	Area.Max.Z += 700.0f;
	ANavMeshBoundsVolume* Volume = GetWorld()->SpawnActor<ANavMeshBoundsVolume>(Area.GetCenter(), FRotator::ZeroRotator);
	UBrushComponent* Brush = Volume ? Volume->GetBrushComponent() : nullptr;
	if (!Brush) return;
	UBodySetup* Body = NewObject<UBodySetup>(Brush);
	const FVector Size = Area.GetSize();
	Body->AggGeom.BoxElems.Add(FKBoxElem(Size.X, Size.Y, Size.Z));
	Brush->BrushBodySetup = Body;
	Brush->UpdateBounds();
	Navigation->OnNavigationBoundsUpdated(Volume);
	UE_LOG(LogArenaDuelSurvival, Log, TEXT("Survival navigation bounds registered: %s"), *Area.ToString());
}

void AArenaDuelZombieGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);
	// A player who joins a run in progress simply spawns into it.
	if (NewPlayer && !NewPlayer->GetPawn() && !bRunOver) RestartPlayer(NewPlayer);
}

void AArenaDuelZombieGameMode::Logout(AController* Exiting)
{
	if (const APlayerController* Controller = Cast<APlayerController>(Exiting))
	{
		if (AArenaDuelPlayerState* Player = Controller->GetPlayerState<AArenaDuelPlayerState>()) Player->SetDuelSlot(255);
	}
	AGameMode::Logout(Exiting);
}

void AArenaDuelZombieGameMode::RestartPlayer(AController* NewPlayer)
{
	Super::RestartPlayer(NewPlayer);
	if (AArenaDuelCharacter* Character = NewPlayer ? Cast<AArenaDuelCharacter>(NewPlayer->GetPawn()) : nullptr)
	{
		Character->SetRoundInputLocked(false);
		// Survival is played with every firearm; flashbang and knife come with the loadout as usual.
		if (Character->GetWeaponComponent()) Character->GetWeaponComponent()->GrantAllWeaponsForDevelopment();
	}
}

void AArenaDuelZombieGameMode::BeginRun()
{
	if (!HasAuthority() || !GetWorld()) return;
	GetWorldTimerManager().ClearTimer(WaveTimer);
	GetWorldTimerManager().ClearTimer(SpawnTimer);
	for (TActorIterator<AArenaDuelZombie> It(GetWorld()); It; ++It) It->Destroy();
	SpawnQueue.Reset();
	CurrentWave = 0;
	TotalKills = 0;
	bWaveActive = false;
	bRunOver = false;
	StatRelocated = 0; StatFellOut = 0; StatRelaxedSpawns = 0;
	AArenaDuelZombieGameState* State = GetSurvivalState();
	if (!State) return;
	State->SetRunTimes(State->GetServerWorldTimeSeconds(), -1.0f);
	for (APlayerState* Player : State->PlayerArray)
	{
		if (AArenaDuelPlayerState* SurvivalPlayer = Cast<AArenaDuelPlayerState>(Player)) SurvivalPlayer->ResetSurvival();
	}
	// The duel fields are held at "round in progress" for the whole run; that is what unlocks input and weapons.
	State->SetMatchComplete(false, INDEX_NONE);
	State->SetMatchPhase(EArenaDuelMatchPhase::InRound);
	State->SetRoundState(1, true, INDEX_NONE);
	State->SetGameOver(false);
	State->SetTotalKills(0);
	State->SetBoss(-1.0f, FString());
	RestartDuelPlayers();
	GetWorldTimerManager().SetTimer(WatchTimer, this, &AArenaDuelZombieGameMode::WatchTick, 0.5f, true);
	BeginIntermission(FirstWaveDelay);
}

void AArenaDuelZombieGameMode::RestartSurvival()
{
	BeginRun();
}

void AArenaDuelZombieGameMode::BeginIntermission(float Seconds)
{
	AArenaDuelZombieGameState* State = GetSurvivalState();
	if (!State || bRunOver) return;
	bWaveActive = false;
	const float Wait = Scaled(Seconds);
	State->SetWaveState(CurrentWave, 0, true, State->GetServerWorldTimeSeconds() + Wait);
	GetWorldTimerManager().SetTimer(WaveTimer, this, &AArenaDuelZombieGameMode::StartWave, Wait, false);
}

void AArenaDuelZombieGameMode::StartWave()
{
	AArenaDuelZombieGameState* State = GetSurvivalState();
	// A wave can only start from an intermission. This is what keeps a wave from being skipped or started twice.
	if (!State || bRunOver || bWaveActive) return;
	++CurrentWave;
	const FArenaDuelWaveDefinition Wave = ComputeWave(CurrentWave, WaveTable);
	SpawnQueue = BuildSpawnQueue(Wave, CurrentWave * 7919);
	bWaveActive = true;
	WaveStartWorldTime = GetWorld()->GetTimeSeconds();
	LastSpawnWorldTime = WaveStartWorldTime;
	State->SetWaveState(CurrentWave, SpawnQueue.Num(), false, 0.0f);
	const TCHAR* Special = Wave.Boss > 0 ? TEXT(" - BOSS") : Wave.MiniBoss > 0 ? TEXT(" - MINI BOSS") : Wave.Armored > 0 && Wave.Fast > 0 ? TEXT(" - RUNNERS AND ARMOUR") : Wave.Armored > 0 ? TEXT(" - ARMOURED") : Wave.Fast > 0 ? TEXT(" - RUNNERS") : TEXT("");
	State->Announce(FString::Printf(TEXT("WAVE %d%s"), CurrentWave, Special));
	UE_LOG(LogArenaDuelSurvival, Log, TEXT("Survival wave %d starts: normal=%d fast=%d armoured=%d miniboss=%d boss=%d total=%d"), CurrentWave, Wave.Normal, Wave.Fast, Wave.Armored, Wave.MiniBoss, Wave.Boss, Wave.Total());
	GetWorldTimerManager().SetTimer(SpawnTimer, this, &AArenaDuelZombieGameMode::SpawnTick, Scaled(SpawnInterval), true, 0.0f);
}

int32 AArenaDuelZombieGameMode::CountAliveZombies() const
{
	int32 Alive = 0;
	for (TActorIterator<AArenaDuelZombie> It(GetWorld()); It; ++It) if (!It->IsDead()) ++Alive;
	return Alive;
}

bool AArenaDuelZombieGameMode::IsSeenByAnyPlayer(const FVector& Location) const
{
	for (TActorIterator<AArenaDuelCharacter> It(GetWorld()); It; ++It)
	{
		if (It->IsDead() || !It->GetController()) continue;
		FHitResult Wall;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(ArenaDuelZombieSeen), false, *It);
		if (!GetWorld()->LineTraceSingleByChannel(Wall, It->GetPawnViewLocation(), Location, ECC_Visibility, Params)) return true;
	}
	return false;
}

bool AArenaDuelZombieGameMode::PickSpawnLocation(FVector& OutLocation, FRotator& OutRotation, bool bRelaxed, bool bUnseenOnly)
{
	UWorld* World = GetWorld();
	TArray<const AActor*> Points;
	for (TActorIterator<ATargetPoint> It(World); It; ++It) if (It->ActorHasTag(TEXT("ZombieSpawn"))) Points.Add(*It);
	if (Points.Num() == 0) return false;
	TArray<const AArenaDuelCharacter*> Players;
	for (TActorIterator<AArenaDuelCharacter> It(World); It; ++It) if (!It->IsDead() && It->GetController()) Players.Add(*It);

	const AActor* Best = nullptr;
	int32 BestScore = -1;
	// Start at a moving cursor so consecutive spawns use different parts of the arena.
	for (int32 Offset = 0; Offset < Points.Num(); ++Offset)
	{
		const AActor* Point = Points[(SpawnPointCursor + Offset) % Points.Num()];
		const FVector Location = Point->GetActorLocation();
		bool bTooClose = false, bSeen = false, bOccupied = false;
		for (const AArenaDuelCharacter* Player : Players)
		{
			if (FVector::Dist(Player->GetActorLocation(), Location) < MinSpawnDistance * (bRelaxed ? 0.5f : 1.0f)) bTooClose = true;
			FHitResult Wall;
			FCollisionQueryParams Params(SCENE_QUERY_STAT(ArenaDuelSpawnSight), false, Player);
			if (!World->LineTraceSingleByChannel(Wall, Player->GetPawnViewLocation(), Location + FVector(0.0f, 0.0f, 90.0f), ECC_Visibility, Params)) bSeen = true;
		}
		if (bTooClose) continue;
		for (TActorIterator<AArenaDuelZombie> It(World); It; ++It) if (!It->IsDead() && FVector::Dist2D(It->GetActorLocation(), Location) < 110.0f) { bOccupied = true; break; }
		if (bOccupied || (bSeen && bUnseenOnly)) continue;
		// Out of sight beats in sight; among equals the first after the cursor wins.
		const int32 Score = bSeen ? 1 : 2;
		if (Score > BestScore) { BestScore = Score; Best = Point; if (Score == 2) { SpawnPointCursor = (SpawnPointCursor + Offset + 1) % Points.Num(); break; } }
	}
	if (!Best) return false;
	if (BestScore < 2) SpawnPointCursor = (SpawnPointCursor + 1) % Points.Num();
	OutLocation = Best->GetActorLocation();
	// Spawn points have to lie on the navigation mesh; snap to it where there is one.
	if (UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World))
	{
		FNavLocation Projected;
		if (Navigation->ProjectPointToNavigation(OutLocation, Projected, FVector(200.0f, 200.0f, 300.0f))) OutLocation = Projected.Location;
	}
	OutLocation.Z += 96.0f;
	OutRotation = Players.Num() > 0 ? (Players[0]->GetActorLocation() - OutLocation).GetSafeNormal2D().Rotation() : Best->GetActorRotation();
	return true;
}

void AArenaDuelZombieGameMode::SpawnTick()
{
	if (!bWaveActive || bRunOver || !GetWorld()) return;
	if (SpawnQueue.Num() == 0)
	{
		GetWorldTimerManager().ClearTimer(SpawnTimer);
		FinishWaveIfDone();
		return;
	}
	if (CountAliveZombies() >= MaxActiveZombies) return;
	FVector Location;
	FRotator Rotation;
	// A player standing among the spawn points must not be able to hold a wave back: after a while without
	// a spawn, points at half the usual distance are accepted, still preferring the ones out of sight.
	const bool bStalled = GetWorld()->GetTimeSeconds() - LastSpawnWorldTime > SpawnStallSeconds;
	if (!PickSpawnLocation(Location, Rotation, bStalled)) return;
	const EArenaDuelZombieType Type = SpawnQueue[0];
	const FArenaDuelZombieTypeConfig& Config = TypeConfigs.IsValidIndex(static_cast<int32>(Type)) ? TypeConfigs[static_cast<int32>(Type)] : TypeConfigs[0];
	FActorSpawnParameters Parameters;
	Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding;
	// A big enemy needs its own capsule clear of the floor.
	Location.Z += 96.0f * (Config.Scale - 1.0f);
	AArenaDuelZombie* Zombie = GetWorld()->SpawnActor<AArenaDuelZombie>(AArenaDuelZombie::StaticClass(), Location, Rotation, Parameters);
	// A blocked point costs nothing: the entry stays in the queue and the next tick tries another point.
	if (!Zombie) return;
	SpawnQueue.RemoveAt(0);
	if (bStalled) ++StatRelaxedSpawns;
	LastSpawnWorldTime = GetWorld()->GetTimeSeconds();
	Zombie->InitializeZombie(Type, Config, ComputeHealthScale(CurrentWave, WaveTable.Num()), ZombieDamageScale, ComputeSpeedScale(CurrentWave, WaveTable.Num()));
	Zombie->SpawnDefaultController();
	RefreshGameState();
}

void AArenaDuelZombieGameMode::NotifyZombieKilled(AArenaDuelZombie* Zombie, AController* Killer, bool bHeadshot)
{
	if (!HasAuthority() || !Zombie) return;
	if (Killer)
	{
		++TotalKills;
		if (AArenaDuelPlayerState* Player = Killer->GetPlayerState<AArenaDuelPlayerState>())
		{
			const int32 TypeIndex = static_cast<int32>(Zombie->GetZombieType());
			Player->AddSurvivalPoints((TypeConfigs.IsValidIndex(TypeIndex) ? TypeConfigs[TypeIndex].Points : 100) + (bHeadshot ? HeadshotBonusPoints : 0));
			Player->AddSurvivalKill();
		}
	}
	RefreshGameState();
	FinishWaveIfDone();
}

void AArenaDuelZombieGameMode::RefreshGameState()
{
	AArenaDuelZombieGameState* State = GetSurvivalState();
	if (!State) return;
	State->SetZombiesRemaining(bWaveActive ? SpawnQueue.Num() + CountAliveZombies() : 0);
	State->SetTotalKills(TotalKills);
	// The strongest boss alive drives the boss bar.
	const AArenaDuelZombie* Boss = nullptr;
	for (TActorIterator<AArenaDuelZombie> It(GetWorld()); It; ++It)
	{
		if (!It->IsDead() && It->IsBossType() && (!Boss || static_cast<uint8>(It->GetZombieType()) > static_cast<uint8>(Boss->GetZombieType()))) Boss = *It;
	}
	State->SetBoss(Boss ? Boss->GetHealthFraction() : -1.0f, Boss ? Boss->GetDisplayName() : FString());
}

void AArenaDuelZombieGameMode::FinishWaveIfDone()
{
	if (!bWaveActive || bRunOver || SpawnQueue.Num() > 0 || CountAliveZombies() > 0) return;
	bWaveActive = false;
	GetWorldTimerManager().ClearTimer(SpawnTimer);
	UE_LOG(LogArenaDuelSurvival, Log, TEXT("Survival wave %d cleared after %.1f s, kills so far %d, moved=%d fell=%d relaxedSpawns=%d"), CurrentWave, GetWorld()->GetTimeSeconds() - WaveStartWorldTime, TotalKills, StatRelocated, StatFellOut, StatRelaxedSpawns);
	// Part of every reserve comes back with a cleared wave; the shop sells the rest.
	if (WaveClearAmmoShare > 0.0f)
	{
		for (TActorIterator<AArenaDuelCharacter> It(GetWorld()); It; ++It)
		{
			if (!It->IsDead() && It->GetWeaponComponent()) It->GetWeaponComponent()->AddReserveAmmoShare(WaveClearAmmoShare);
		}
	}
	if (AArenaDuelZombieGameState* State = GetSurvivalState()) State->Announce(FString::Printf(TEXT("WAVE %d CLEARED"), CurrentWave));
	BeginIntermission(IntermissionSeconds);
}

void AArenaDuelZombieGameMode::WatchTick()
{
	if (bRunOver || !GetWorld()) return;
	for (TActorIterator<AArenaDuelZombie> It(GetWorld()); It; ++It)
	{
		if (It->IsDead()) continue;
		// Fallen out of the arena: gone, and the wave goes on without it.
		if (It->GetActorLocation().Z < -1000.0f) { ++StatFellOut; It->KillSilently(); continue; }
		if (It->GetSecondsWithoutProgress() > StuckSeconds)
		{
			// Last resort for one that cannot get anywhere: it starts again from a spawn point. Never in
			// front of a player: both where it stands and where it goes have to be out of sight, otherwise
			// it waits and keeps trying to free itself.
			FVector Location;
			FRotator Rotation;
			if (!IsSeenByAnyPlayer(It->GetActorLocation() + FVector(0.0f, 0.0f, 50.0f)) && PickSpawnLocation(Location, Rotation, false, true))
			{
				It->TeleportTo(Location, Rotation, false, true);
				It->ResetProgress();
				++StatRelocated;
			}
		}
	}
	// Between waves the living recover slowly; the shop heals at once.
	if (const AArenaDuelZombieGameState* State = GetSurvivalState(); State && State->IsIntermission() && CurrentWave > 0 && IntermissionRegenPerSecond > 0.0f)
	{
		for (TActorIterator<AArenaDuelCharacter> It(GetWorld()); It; ++It)
		{
			if (!It->IsDead() && It->GetHealth() < It->GetMaxHealth()) It->ApplyServerHeal(IntermissionRegenPerSecond * 0.5f);
		}
	}
	if (bDevAutoPlay) DevAutoPlayTick();
	RefreshGameState();
	FinishWaveIfDone();
}

void AArenaDuelZombieGameMode::HandlePlayerDeath(AArenaDuelCharacter* DeadCharacter)
{
	if (!HasAuthority() || bRunOver || !GetWorld()) return;
	// The run ends when nobody is left standing.
	for (TActorIterator<AArenaDuelCharacter> It(GetWorld()); It; ++It) if (!It->IsDead() && It->GetController()) return;
	bRunOver = true;
	bWaveActive = false;
	GetWorldTimerManager().ClearTimer(WaveTimer);
	GetWorldTimerManager().ClearTimer(SpawnTimer);
	GetWorldTimerManager().ClearTimer(WatchTimer);
	UE_LOG(LogArenaDuelSurvival, Log, TEXT("Survival run over in wave %d with %d kills"), CurrentWave, TotalKills);
	if (AArenaDuelZombieGameState* State = GetSurvivalState())
	{
		State->SetGameOver(true);
		State->SetRunTimes(State->GetRunStartServerTime(), State->GetServerWorldTimeSeconds());
		State->SetRoundState(1, false, INDEX_NONE);
		State->Announce(TEXT("GAME OVER"));
	}
}

int32 AArenaDuelZombieGameMode::GetPurchaseCost(const AController* Buyer, EArenaDuelSurvivalPurchase Item) const
{
	if (Item == EArenaDuelSurvivalPurchase::Ammo) return AmmoCost;
	if (Item == EArenaDuelSurvivalPurchase::Heal) return HealCost;
	const AArenaDuelPlayerState* Player = Buyer ? Buyer->GetPlayerState<AArenaDuelPlayerState>() : nullptr;
	return DamageUpgradeCost * ((Player ? Player->GetSurvivalDamageLevel() : 0) + 1);
}

float AArenaDuelZombieGameMode::GetPlayerDamageScale(const AController* Player) const
{
	const AArenaDuelPlayerState* State = Player ? Player->GetPlayerState<AArenaDuelPlayerState>() : nullptr;
	return WeaponDamageScale * (1.0f + DamagePerUpgradeLevel * static_cast<float>(State ? State->GetSurvivalDamageLevel() : 0));
}

bool AArenaDuelZombieGameMode::TryPurchase(AController* Buyer, EArenaDuelSurvivalPurchase Item)
{
	const AArenaDuelZombieGameState* State = GetSurvivalState();
	AArenaDuelPlayerState* Player = Buyer ? Buyer->GetPlayerState<AArenaDuelPlayerState>() : nullptr;
	AArenaDuelCharacter* Character = Buyer ? Cast<AArenaDuelCharacter>(Buyer->GetPawn()) : nullptr;
	// Shopping is for the break between waves, and only for the living.
	if (!HasAuthority() || !State || !Player || !Character || Character->IsDead() || bRunOver || !State->IsIntermission()) return false;
	const int32 Cost = GetPurchaseCost(Buyer, Item);
	if (Player->GetSurvivalPoints() < Cost) return false;
	// Nothing is charged for a purchase that would do nothing.
	if (Item == EArenaDuelSurvivalPurchase::Heal && Character->GetHealth() >= Character->GetMaxHealth()) return false;
	if (Item == EArenaDuelSurvivalPurchase::Damage && Player->GetSurvivalDamageLevel() >= MaxDamageUpgradeLevel) return false;
	// The points go first and in one step, on the server, so one request can never be paid for twice or delivered unpaid.
	Player->AddSurvivalPoints(-Cost);
	switch (Item)
	{
	case EArenaDuelSurvivalPurchase::Ammo:
		if (Character->GetWeaponComponent()) Character->GetWeaponComponent()->RefillAllAmmoForDevelopment();
		break;
	case EArenaDuelSurvivalPurchase::Heal:
		Character->ApplyServerHeal(Character->GetMaxHealth());
		break;
	case EArenaDuelSurvivalPurchase::Damage:
		Player->AddSurvivalDamageLevel();
		break;
	}
	return true;
}

void AArenaDuelZombieGameMode::DevKillAllZombies()
{
	for (TActorIterator<AArenaDuelZombie> It(GetWorld()); It; ++It) It->KillSilently();
	// Fast-forwarding also empties the queue down to what the alive limit lets through per tick, so the
	// queue itself is left alone: the wave still has to spawn every enemy before it can end.
}

void AArenaDuelZombieGameMode::DevAutoPlayTick()
{
	AController* Credit = GetWorld()->GetFirstPlayerController();
	int32 Budget = 6;
	for (TActorIterator<AArenaDuelZombie> It(GetWorld()); It && Budget > 0; ++It)
	{
		if (It->IsDead()) continue;
		for (TActorIterator<AArenaDuelCharacter> Player(GetWorld()); Player; ++Player)
		{
			if (Player->IsDead() || !Player->GetController() || FVector::Dist(Player->GetActorLocation(), It->GetActorLocation()) > 450.0f * FMath::Max(1.0f, It->GetActorScale3D().X)) continue;
			It->KillCredited(Credit);
			--Budget;
			break;
		}
	}
}

void AArenaDuelZombieGameMode::DevLogStats() const
{
	int32 Alive = 0, Direct = 0, Freed = 0;
	float Slowest = 0.0f;
	for (TActorIterator<AArenaDuelZombie> It(GetWorld()); It; ++It)
	{
		if (It->IsDead()) continue;
		++Alive;
		if (It->IsDirectChasing()) ++Direct;
		Freed += It->GetUnstickCount();
		Slowest = FMath::Max(Slowest, It->GetSecondsWithoutProgress());
	}
	UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	// One sample path from an enemy to the first player says whether navigation really works.
	if (Navigation)
	{
		const APlayerController* Controller = GetWorld()->GetFirstPlayerController();
		const APawn* Player = Controller ? Controller->GetPawn() : nullptr;
		FNavLocation OnMesh;
		const bool bPlayerOnMesh = Player && Navigation->ProjectPointToNavigation(Player->GetActorLocation(), OnMesh, FVector(200.0f, 200.0f, 300.0f));
		int32 Valid = 0, Partial = 0, Sampled = 0;
		double Length = 0.0;
		for (TActorIterator<AArenaDuelZombie> It(GetWorld()); It && Player; ++It)
		{
			if (It->IsDead()) continue;
			++Sampled;
			if (const UNavigationPath* Path = UNavigationSystemV1::FindPathToLocationSynchronously(GetWorld(), It->GetActorLocation(), Player->GetActorLocation(), *It))
			{
				if (Path->IsValid()) { ++Valid; Length += Path->GetPathLength(); }
				if (Path->IsPartial()) ++Partial;
			}
		}
		UE_LOG(LogArenaDuelSurvival, Log, TEXT("Survival navigation: bounds=%d playerOnMesh=%d paths valid=%d partial=%d of %d averageLength=%.0f"), Navigation->GetNavigationBounds().Num(), bPlayerOnMesh ? 1 : 0, Valid, Partial, Sampled, Valid > 0 ? Length / Valid : 0.0);
	}
	UE_LOG(LogArenaDuelSurvival, Log, TEXT("Survival stats: wave=%d active=%d queue=%d alive=%d straight=%d freed=%d longestNoProgress=%.1f moved=%d fell=%d relaxedSpawns=%d navData=%d frameMs=%.1f"),
		CurrentWave, bWaveActive ? 1 : 0, SpawnQueue.Num(), Alive, Direct, Freed, Slowest, StatRelocated, StatFellOut, StatRelaxedSpawns, Navigation && Navigation->GetDefaultNavDataInstance() ? 1 : 0, FApp::GetDeltaTime() * 1000.0);
}