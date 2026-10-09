#include "ArenaDuelZombie.h"
#include "ArenaDuelCharacter.h"
#include "../Game/ArenaDuelZombieGameMode.h"
#include "../Weapons/ArenaDuelWeaponComponent.h"
#include "AIController.h"
#include "Animation/AnimSequence.h"
#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Materials/MaterialInterface.h"
#include "Navigation/PathFollowingComponent.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

UArenaDuelZombieAnimInstance::UArenaDuelZombieAnimInstance()
{
	static ConstructorHelpers::FObjectFinder<UAnimSequence> IdleAsset(TEXT("/Game/Characters/Mannequins/Anims/Rifle/MF_Rifle_Idle_ADS"));
	static ConstructorHelpers::FObjectFinder<UAnimSequence> WalkAsset(TEXT("/Game/Characters/Mannequins/Anims/Rifle/Walk/MF_Rifle_Walk_Fwd"));
	static ConstructorHelpers::FObjectFinder<UAnimSequence> RunAsset(TEXT("/Game/Characters/Mannequins/Anims/Rifle/Jog/MF_Rifle_Jog_Fwd"));
	Idle = IdleAsset.Object; Walk = WalkAsset.Object; Run = RunAsset.Object;
	SetRootMotionMode(ERootMotionMode::IgnoreRootMotion);
}

void UArenaDuelZombieAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	const AArenaDuelZombie* Zombie = Cast<AArenaDuelZombie>(GetOwningActor());
	// A dead zombie is a ragdoll; clip selection must not restart on it.
	if (!Zombie || Zombie->IsDead()) { SetPlaying(false); return; }
	Super::NativeUpdateAnimation(DeltaSeconds);
	const float Speed = Zombie->GetVelocity().Size2D();
	UAnimSequence* Desired = Speed < 15.0f ? Idle.Get() : Speed < 380.0f ? Walk.Get() : Run.Get();
	if (Desired && GetCurrentAsset() != Desired) SetAnimationAsset(Desired, true);
	SetPlaying(true);
	SetPlayRate(Desired == Idle ? 1.0f : FMath::Clamp(Speed / (Desired == Run ? 600.0f : 200.0f), 0.5f, 1.8f));
}

AArenaDuelZombie::AArenaDuelZombie()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicateMovement(true);
	AIControllerClass = AAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	bUseControllerRotationYaw = false;

	GetCapsuleComponent()->InitCapsuleSize(38.0f, 92.0f);
	// Weapon traces test the per-bone hit zones, not the capsule.
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);

	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->bOrientRotationToMovement = true;
	Movement->RotationRate = FRotator(0.0f, 540.0f, 0.0f);
	Movement->MaxWalkSpeed = 330.0f;
	Movement->MaxAcceleration = 1800.0f;
	Movement->bUseRVOAvoidance = true;
	Movement->AvoidanceConsiderationRadius = 160.0f;
	Movement->AvoidanceWeight = 0.5f;
	Movement->NavAgentProps.AgentRadius = 38.0f;
	Movement->NavAgentProps.AgentHeight = 184.0f;

	GetMesh()->SetRelativeLocation(FVector(0.0f, 0.0f, -92.0f));
	GetMesh()->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
	GetMesh()->SetAnimInstanceClass(UArenaDuelZombieAnimInstance::StaticClass());
	GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	WarningLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("WarningLight"));
	WarningLight->SetupAttachment(GetCapsuleComponent());
	WarningLight->SetRelativeLocation(FVector(40.0f, 0.0f, 50.0f));
	WarningLight->SetIntensity(2600.0f);
	WarningLight->SetAttenuationRadius(420.0f);
	WarningLight->SetLightColor(FLinearColor(1.0f, 0.12f, 0.05f));
	WarningLight->SetCastShadows(false);
	WarningLight->SetVisibility(false);
}

void AArenaDuelZombie::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AArenaDuelZombie, ZombieType);
	DOREPLIFETIME(AArenaDuelZombie, Health);
	DOREPLIFETIME(AArenaDuelZombie, MaxHealth);
	DOREPLIFETIME(AArenaDuelZombie, VisualScale);
	DOREPLIFETIME(AArenaDuelZombie, MaterialPath);
	DOREPLIFETIME(AArenaDuelZombie, bDead);
	DOREPLIFETIME(AArenaDuelZombie, DeathHitLocation);
	DOREPLIFETIME(AArenaDuelZombie, DeathHitDirection);
}

void AArenaDuelZombie::BeginPlay()
{
	Super::BeginPlay();
	// The body is the player's world mesh; the Blueprint is where its asset reference lives.
	if (!GetMesh()->GetSkeletalMeshAsset())
	{
		const UClass* PlayerClass = LoadClass<AArenaDuelCharacter>(nullptr, TEXT("/Game/ArenaDuel/Characters/BP_ArenaDuelCharacter.BP_ArenaDuelCharacter_C"));
		const AArenaDuelCharacter* PlayerDefaults = PlayerClass ? Cast<AArenaDuelCharacter>(PlayerClass->GetDefaultObject()) : nullptr;
		if (PlayerDefaults && PlayerDefaults->GetMesh()) GetMesh()->SetSkeletalMeshAsset(PlayerDefaults->GetMesh()->GetSkeletalMeshAsset());
	}
	if (HasAuthority())
	{
		// The physics asset bodies are the hit zones, as on a player: queried directly, colliding with nothing.
		GetMesh()->SetCollisionObjectType(ECC_Pawn);
		GetMesh()->SetCollisionResponseToAllChannels(ECR_Ignore);
		GetMesh()->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	}
	ApplyTypeVisual();
}

void AArenaDuelZombie::InitializeZombie(EArenaDuelZombieType InType, const FArenaDuelZombieTypeConfig& InConfig, float HealthScale, float InDamageScale)
{
	if (!HasAuthority()) return;
	ZombieType = InType;
	Config = InConfig;
	DamageScale = FMath::Max(InDamageScale, 0.0f);
	MaxHealth = FMath::Max(1.0f, Config.Health * FMath::Max(HealthScale, 0.1f));
	Health = MaxHealth;
	VisualScale = Config.Scale;
	MaterialPath = Config.MaterialPath;
	GetCharacterMovement()->MaxWalkSpeed = Config.MoveSpeed;
	// Each zombie aims a little beside the player until it is close, so a group fans out instead of queueing.
	ApproachOffset = FVector(FMath::FRandRange(-1.0f, 1.0f), FMath::FRandRange(-1.0f, 1.0f), 0.0f).GetSafeNormal() * FMath::FRandRange(60.0f, 220.0f);
	SpecialReadyTime = GetWorld() ? GetWorld()->GetTimeSeconds() + FMath::FRandRange(2.0f, 4.0f) : 0.0f;
	ApplyTypeVisual();
}

void AArenaDuelZombie::OnRep_ZombieType()
{
	ApplyTypeVisual();
}

void AArenaDuelZombie::ApplyTypeVisual()
{
	SetActorScale3D(FVector(VisualScale));
	if (GetNetMode() == NM_DedicatedServer || MaterialPath.IsEmpty()) return;
	if (UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, *MaterialPath))
	{
		for (int32 Index = 0; Index < GetMesh()->GetNumMaterials(); ++Index) GetMesh()->SetMaterial(Index, Material);
		bVisualApplied = true;
	}
}

bool AArenaDuelZombie::TraceHitZones(const FVector& Start, const FVector& End, FHitResult& OutHit) const
{
	const USkeletalMeshComponent* Body = GetMesh();
	if (bDead || !Body || !Body->GetPhysicsAsset()) return false;
	return const_cast<USkeletalMeshComponent*>(Body)->LineTraceComponent(OutHit, Start, End, FCollisionQueryParams(SCENE_QUERY_STAT(ArenaDuelZombieHitZones), false));
}

float AArenaDuelZombie::TakeWeaponHit(float BaseDamage, float WeaponZoneMultiplier, EArenaDuelShotResult Zone, AController* InstigatorController, const FVector& HitLocation, const FVector& Direction)
{
	if (!HasAuthority() || bDead || BaseDamage <= 0.0f) return 0.0f;
	const bool bHead = Zone == EArenaDuelShotResult::Head;
	float Damage = BaseDamage * WeaponZoneMultiplier * (bHead ? Config.HeadDamageFactor : Config.BodyDamageFactor);
	if (const AArenaDuelZombieGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AArenaDuelZombieGameMode>() : nullptr)
	{
		Damage *= GameMode->GetPlayerDamageScale(InstigatorController);
	}
	Health = FMath::Max(0.0f, Health - Damage);
	DeathHitLocation = HitLocation;
	DeathHitDirection = Direction.GetSafeNormal();
	if (Health <= 0.0f) Die(InstigatorController, bHead);
	return Damage;
}

void AArenaDuelZombie::KillSilently()
{
	if (HasAuthority() && !bDead) Die(nullptr, false);
}

void AArenaDuelZombie::Die(AController* Killer, bool bHeadshot)
{
	if (bDead) return;
	bDead = true;
	Health = 0.0f;
	if (AAIController* AI = Cast<AAIController>(GetController())) AI->StopMovement();
	GetCharacterMovement()->DisableMovement();
	GetCharacterMovement()->SetComponentTickEnabled(false);
	SetReplicateMovement(false);
	AArenaDuelZombieGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AArenaDuelZombieGameMode>() : nullptr;
	// Bodies are removed after a while so a long run does not pile up ragdolls.
	SetLifeSpan(GameMode ? GameMode->GetCorpseSeconds(IsBossType()) : 8.0f);
	OnRep_Dead();
	if (GameMode) GameMode->NotifyZombieKilled(this, Killer, bHeadshot);
	ForceNetUpdate();
}

void AArenaDuelZombie::OnRep_Dead()
{
	if (!bDead) return;
	WarningLight->SetVisibility(false);
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	StartRagdoll();
}

void AArenaDuelZombie::StartRagdoll()
{
	USkeletalMeshComponent* Body = GetMesh();
	if (!Body || !Body->GetPhysicsAsset() || GetNetMode() == NM_DedicatedServer) return;
	Body->SetCollisionProfileName(TEXT("Ragdoll"));
	Body->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	Body->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	Body->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
	Body->SetAllBodiesSimulatePhysics(true);
	Body->SetSimulatePhysics(true);
	Body->WakeAllRigidBodies();
	Body->bBlendPhysics = true;
	if (!FVector(DeathHitDirection).IsNearlyZero()) Body->AddImpulseAtLocation(FVector(DeathHitDirection) * 3500.0f, FVector(DeathHitLocation));
}

void AArenaDuelZombie::MulticastTelegraph_Implementation(float Seconds, bool bSlam)
{
	if (GetNetMode() == NM_DedicatedServer || !GetWorld()) return;
	WarningLight->SetIntensity(bSlam ? 9000.0f : 2600.0f);
	WarningLight->SetAttenuationRadius(bSlam ? 900.0f : 420.0f);
	WarningLight->SetVisibility(true);
	WarningLightOffTime = GetWorld()->GetTimeSeconds() + Seconds;
}

void AArenaDuelZombie::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bVisualApplied && !MaterialPath.IsEmpty()) ApplyTypeVisual();
	if (WarningLight->IsVisible() && GetWorld() && GetWorld()->GetTimeSeconds() >= WarningLightOffTime) WarningLight->SetVisibility(false);
	if (!HasAuthority() || bDead) return;
	// Thinking ten times a second is plenty for melee enemies and keeps a full wave cheap.
	ThinkAccumulator += DeltaSeconds;
	if (bDirectChase && !bWindingUp)
	{
		if (const AArenaDuelCharacter* Target = FindTarget()) AddMovementInput((Target->GetActorLocation() + ApproachOffset - GetActorLocation()).GetSafeNormal2D(), 1.0f);
	}
	if (ThinkAccumulator >= 0.1f)
	{
		ServerThink(ThinkAccumulator);
		ThinkAccumulator = 0.0f;
	}
}

AArenaDuelCharacter* AArenaDuelZombie::FindTarget() const
{
	AArenaDuelCharacter* Best = nullptr;
	double BestDistance = TNumericLimits<double>::Max();
	for (TActorIterator<AArenaDuelCharacter> It(GetWorld()); It; ++It)
	{
		if (It->IsDead() || !It->GetController()) continue;
		const double Distance = FVector::DistSquared(It->GetActorLocation(), GetActorLocation());
		if (Distance < BestDistance) { BestDistance = Distance; Best = *It; }
	}
	return Best;
}

bool AArenaDuelZombie::HasLineTo(const AArenaDuelCharacter* Target) const
{
	if (!Target || !GetWorld()) return false;
	FHitResult Wall;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(ArenaDuelZombieSight), false, this);
	Params.AddIgnoredActor(Target);
	const FVector Eye = GetActorLocation() + FVector(0.0f, 0.0f, 50.0f * VisualScale);
	return !GetWorld()->LineTraceSingleByChannel(Wall, Eye, Target->GetActorLocation() + FVector(0.0f, 0.0f, 40.0f), ECC_Visibility, Params);
}

void AArenaDuelZombie::ServerThink(float DeltaSeconds)
{
	UWorld* World = GetWorld();
	AArenaDuelCharacter* Target = FindTarget();
	AAIController* AI = Cast<AAIController>(GetController());
	if (!World || !Target)
	{
		if (AI) AI->StopMovement();
		return;
	}
	const double Now = World->GetTimeSeconds();
	const FVector ToTarget = Target->GetActorLocation() - GetActorLocation();
	const float Distance = static_cast<float>(ToTarget.Size2D());
	const float Reach = Config.AttackRange * VisualScale;
	const bool bSameLevel = FMath::Abs(ToTarget.Z) < 170.0f * FMath::Max(VisualScale, 1.0f);

	// Progress towards the player, for the stuck check of the wave manager.
	if (Distance < BestTargetDistance - 40.0f || Distance < Reach * 1.5f) { BestTargetDistance = Distance; SecondsWithoutProgress = 0.0f; }
	else SecondsWithoutProgress += DeltaSeconds;

	// A swing or slam in progress: it lands only if the player is still in reach with nothing in between.
	if (bWindingUp)
	{
		if (Now < WindupEndTime) return;
		bWindingUp = false;
		if (bSlamWindup)
		{
			constexpr float SlamRadius = 560.0f;
			for (TActorIterator<AArenaDuelCharacter> It(World); It; ++It)
			{
				if (!It->IsDead() && FVector::Dist(It->GetActorLocation(), GetActorLocation()) <= SlamRadius && HasLineTo(*It)) It->ApplyServerDamage(Config.Damage * 1.6f * DamageScale);
			}
		}
		else if (Distance <= Reach * 1.25f && bSameLevel && HasLineTo(Target))
		{
			Target->ApplyServerDamage(Config.Damage * DamageScale);
		}
		NextAttackTime = Now + Config.AttackInterval;
		return;
	}

	const bool bFinalBoss = ZombieType == EArenaDuelZombieType::Boss;
	const bool bEnraged = bFinalBoss && GetHealthFraction() < 0.5f;
	float Speed = Config.MoveSpeed * (bEnraged ? 1.25f : 1.0f);

	// Bosses: a charge to close distance, and for the big one an area slam up close.
	if (IsBossType() && Now >= SpecialReadyTime)
	{
		const bool bSight = HasLineTo(Target);
		if (bFinalBoss && Distance < 480.0f && bSight)
		{
			bWindingUp = true; bSlamWindup = true;
			WindupEndTime = Now + (bEnraged ? 0.7f : 0.95f);
			SpecialReadyTime = Now + (bEnraged ? 3.2f : 5.0f);
			if (AI) AI->StopMovement();
			MulticastTelegraph(static_cast<float>(WindupEndTime - Now), true);
			return;
		}
		if (Distance > 500.0f && Distance < 1800.0f && bSight)
		{
			ChargeEndTime = Now + 1.3f;
			SpecialReadyTime = Now + (bFinalBoss ? 5.0f : 6.5f);
			MulticastTelegraph(0.4f, false);
		}
	}
	if (Now < ChargeEndTime) Speed *= 2.1f;
	GetCharacterMovement()->MaxWalkSpeed = Speed;

	// Melee.
	if (Distance <= Reach && bSameLevel && Now >= NextAttackTime && HasLineTo(Target))
	{
		bWindingUp = true; bSlamWindup = false;
		WindupEndTime = Now + Config.AttackWindup;
		if (AI) AI->StopMovement();
		MulticastTelegraph(Config.AttackWindup + 0.1f, false);
		return;
	}

	// Chase. Until close the goal sits a little beside the player; in reach it is the player.
	const FVector Goal = Target->GetActorLocation() + (Distance > 450.0f ? ApproachOffset : FVector::ZeroVector);
	if (Distance <= Reach * 0.8f)
	{
		if (AI) AI->StopMovement();
		bDirectChase = false;
		return;
	}
	if (AI)
	{
		const EPathFollowingRequestResult::Type Result = AI->MoveToLocation(Goal, Reach * 0.6f, true, true, true, true);
		// Without a navigation mesh, or when no path exists, steer straight at the goal instead of standing still.
		bDirectChase = Result == EPathFollowingRequestResult::Failed;
	}
	else bDirectChase = true;
}
