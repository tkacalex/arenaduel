#include "ArenaDuelZombie.h"
#include "ArenaDuelCharacter.h"
#include "../Game/ArenaDuelZombieGameMode.h"
#include "../Weapons/ArenaDuelWeaponComponent.h"
#include "AIController.h"
#include "Animation/AnimSequence.h"
#include "Components/CapsuleComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Navigation/PathFollowingComponent.h"
#include "Net/UnrealNetwork.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/Package.h"

namespace
{
	const TCHAR* ZombieGrowlSound = TEXT("/Game/ArenaDuel/Audio/S_ZombieGrowl.S_ZombieGrowl");
	const TCHAR* ZombieAttackSound = TEXT("/Game/ArenaDuel/Audio/S_ZombieAttack.S_ZombieAttack");
	const TCHAR* ZombieHitSound = TEXT("/Game/ArenaDuel/Audio/S_ZombieHit.S_ZombieHit");
	const TCHAR* ZombieDeathSound = TEXT("/Game/ArenaDuel/Audio/S_ZombieDeath.S_ZombieDeath");

	/** Each sound is looked up once; a missing asset is not searched for again on every hit. */
	USoundBase* FindZombieSound(const TCHAR* Path)
	{
		static TMap<FString, TWeakObjectPtr<USoundBase>> Cache;
		static TSet<FString> Missing;
		const FString Key(Path);
		if (const TWeakObjectPtr<USoundBase>* Found = Cache.Find(Key); Found && Found->IsValid()) return Found->Get();
		if (Missing.Contains(Key)) return nullptr;
		USoundBase* Sound = LoadObject<USoundBase>(nullptr, Path, nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (Sound) Cache.Add(Key, Sound);
		else Missing.Add(Key);
		return Sound;
	}

	/** Zombie sounds are heard from where the zombie is and fade out over about thirty metres. */
	USoundAttenuation* ZombieSoundAttenuation()
	{
		static USoundAttenuation* Attenuation = nullptr;
		if (!Attenuation)
		{
			Attenuation = NewObject<USoundAttenuation>(GetTransientPackage(), TEXT("ArenaDuelZombieAttenuation"));
			Attenuation->AddToRoot();
			Attenuation->Attenuation.bAttenuate = true;
			Attenuation->Attenuation.bSpatialize = true;
			Attenuation->Attenuation.AttenuationShapeExtents = FVector(350.0f);
			Attenuation->Attenuation.FalloffDistance = 3000.0f;
		}
		return Attenuation;
	}
}

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

int32 AArenaDuelHitBurst::AliveBursts = 0;

AArenaDuelHitBurst::AArenaDuelHitBurst()
{
	PrimaryActorTick.bCanEverTick = true;
	Drops = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Drops"));
	SetRootComponent(Drops);
	Drops->SetMobility(EComponentMobility::Movable);
	Drops->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Drops->SetCastShadow(false);
	Drops->SetGenerateOverlapEvents(false);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (Sphere.Succeeded()) Drops->SetStaticMesh(Sphere.Object);
}

void AArenaDuelHitBurst::Spawn(UWorld* World, const FVector& Location, const FVector& ShotDirection, bool bHeavy)
{
	// A long burst of automatic fire must not pile up effects.
	if (!World || World->GetNetMode() == NM_DedicatedServer || AliveBursts >= 18) return;
	FActorSpawnParameters Parameters;
	Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Parameters.ObjectFlags |= RF_Transient;
	if (AArenaDuelHitBurst* Burst = World->SpawnActor<AArenaDuelHitBurst>(AArenaDuelHitBurst::StaticClass(), Location, FRotator::ZeroRotator, Parameters))
	{
		++AliveBursts;
		Burst->Launch(ShotDirection, bHeavy);
	}
}

void AArenaDuelHitBurst::Launch(const FVector& ShotDirection, bool bHeavy)
{
	static TWeakObjectPtr<UMaterialInterface> Blood;
	static bool bBloodSearched = false;
	if (!bBloodSearched)
	{
		bBloodSearched = true;
		Blood = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/ArenaDuel/Characters/Common/M_SurvivalBlood.M_SurvivalBlood"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	}
	if (Blood.IsValid()) Drops->SetMaterial(0, Blood.Get());
	const int32 Count = bHeavy ? 14 : 8;
	DropScale = bHeavy ? 0.06f : 0.045f;
	const FVector Back = -ShotDirection.GetSafeNormal();
	for (int32 Index = 0; Index < Count; ++Index)
	{
		// Most of it sprays back out of the wound towards the shooter, the rest in every direction.
		const FVector Direction = (Back * 0.7f + FMath::VRand() * 0.8f).GetSafeNormal();
		Positions.Add(GetActorLocation());
		Velocities.Add(Direction * FMath::FRandRange(110.0f, bHeavy ? 460.0f : 330.0f) + FVector(0.0f, 0.0f, 110.0f));
		Drops->AddInstance(FTransform(FRotator::ZeroRotator, GetActorLocation(), FVector(DropScale)), true);
	}
	SetLifeSpan(0.6f);
}

void AArenaDuelHitBurst::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Age += DeltaSeconds;
	const float Shrink = FMath::Clamp(1.0f - (Age - 0.3f) / 0.28f, 0.05f, 1.0f);
	for (int32 Index = 0; Index < Positions.Num(); ++Index)
	{
		Velocities[Index].Z -= 980.0f * DeltaSeconds;
		Positions[Index] += Velocities[Index] * DeltaSeconds;
		Drops->UpdateInstanceTransform(Index, FTransform(FRotator::ZeroRotator, Positions[Index], FVector(DropScale * Shrink)), true, Index == Positions.Num() - 1, false);
	}
}

void AArenaDuelHitBurst::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (Positions.Num() > 0) AliveBursts = FMath::Max(0, AliveBursts - 1);
	Super::EndPlay(EndPlayReason);
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
	DOREPLIFETIME(AArenaDuelZombie, DeathImpulse);
	DOREPLIFETIME(AArenaDuelZombie, bDeathHeadshot);
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
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	NextGrowlTime = Now + FMath::FRandRange(1.0f, 10.0f);
	MoveSampleLocation = GetActorLocation();
	MoveSampleTime = Now;
	ApplyTypeVisual();
}

void AArenaDuelZombie::InitializeZombie(EArenaDuelZombieType InType, const FArenaDuelZombieTypeConfig& InConfig, float HealthScale, float InDamageScale, float SpeedScale)
{
	if (!HasAuthority()) return;
	ZombieType = InType;
	Config = InConfig;
	DamageScale = FMath::Max(InDamageScale, 0.0f);
	MaxHealth = FMath::Max(1.0f, Config.Health * FMath::Max(HealthScale, 0.1f));
	Health = MaxHealth;
	VisualScale = Config.Scale;
	MaterialPath = Config.MaterialPath;
	// No two walk exactly alike: a group stretches out instead of arriving as one block.
	const float Variance = InType == EArenaDuelZombieType::Normal ? FMath::FRandRange(0.86f, 1.12f) : FMath::FRandRange(0.95f, 1.05f);
	BaseMoveSpeed = Config.MoveSpeed * FMath::Max(SpeedScale, 0.1f) * Variance;
	GetCharacterMovement()->MaxWalkSpeed = BaseMoveSpeed;
	// Each zombie aims a little beside the player until it is close, so a group fans out instead of queueing.
	// Runners swing wide and come in from the side.
	const bool bFlanker = InType == EArenaDuelZombieType::Fast;
	ApproachOffset = FVector(FMath::FRandRange(-1.0f, 1.0f), FMath::FRandRange(-1.0f, 1.0f), 0.0f).GetSafeNormal() * (bFlanker ? FMath::FRandRange(280.0f, 520.0f) : FMath::FRandRange(60.0f, 220.0f));
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	SpecialReadyTime = Now + FMath::FRandRange(2.0f, 4.0f);
	NextOffsetRollTime = Now + FMath::FRandRange(1.5f, 2.5f);
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
	// One attempt per path: a missing asset must not be searched for again every frame.
	bVisualApplied = true;
	if (UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, *MaterialPath))
	{
		for (int32 Index = 0; Index < GetMesh()->GetNumMaterials(); ++Index) GetMesh()->SetMaterial(Index, Material);
	}
}

void AArenaDuelZombie::PlayZombieSound(const TCHAR* Path, float Volume, float Pitch) const
{
	if (GetNetMode() == NM_DedicatedServer) return;
	// A bigger body sounds deeper.
	if (USoundBase* Sound = FindZombieSound(Path)) UGameplayStatics::PlaySoundAtLocation(this, Sound, GetActorLocation(), Volume, Pitch / FMath::Sqrt(FMath::Max(VisualScale, 0.5f)), 0.0f, ZombieSoundAttenuation());
}

bool AArenaDuelZombie::TraceHitZones(const FVector& Start, const FVector& End, FHitResult& OutHit) const
{
	const USkeletalMeshComponent* Body = GetMesh();
	if (bDead || !Body || !Body->GetPhysicsAsset()) return false;
	return const_cast<USkeletalMeshComponent*>(Body)->LineTraceComponent(OutHit, Start, End, FCollisionQueryParams(SCENE_QUERY_STAT(ArenaDuelZombieHitZones), false));
}

float AArenaDuelZombie::ComputeStaggerSeconds(EArenaDuelZombieType Type, float DamageFraction, bool bHead)
{
	const float Share = FMath::Clamp(DamageFraction, 0.0f, 1.0f);
	switch (Type)
	{
	// The big boss never flinches; the brute only from a heavy hit; armour only gives at the head.
	case EArenaDuelZombieType::Boss: return 0.0f;
	case EArenaDuelZombieType::MiniBoss: return Share >= 0.04f ? 0.12f : 0.0f;
	case EArenaDuelZombieType::Armored: return bHead ? 0.25f : 0.0f;
	case EArenaDuelZombieType::Fast: return bHead ? 0.45f : 0.3f;
	default: return 0.25f + (bHead ? 0.2f : 0.0f) + 0.3f * Share;
	}
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
	// A stronger hit throws the body further.
	DeathImpulse = FMath::Clamp(Damage * 60.0f, 2200.0f, 11000.0f);
	bDeathHeadshot = bHead;
	const bool bHeavy = Damage >= 60.0f;
	MulticastHitReact(HitLocation, Direction.GetSafeNormal(), bHead, bHeavy);
	if (Health <= 0.0f)
	{
		Die(InstigatorController, bHead);
		return Damage;
	}
	if (const float Stagger = ComputeStaggerSeconds(ZombieType, Damage / MaxHealth, bHead); Stagger > 0.0f && GetWorld())
	{
		StaggerEndTime = FMath::Max(StaggerEndTime, GetWorld()->GetTimeSeconds() + Stagger);
	}
	return Damage;
}

void AArenaDuelZombie::KillSilently()
{
	if (HasAuthority() && !bDead) Die(nullptr, false);
}

void AArenaDuelZombie::KillCredited(AController* Killer)
{
	if (HasAuthority() && !bDead) Die(Killer, false);
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
	PlayZombieSound(ZombieDeathSound, 0.8f, FMath::FRandRange(0.85f, 1.15f));
	StartRagdoll();
}

void AArenaDuelZombie::StartRagdoll()
{
	USkeletalMeshComponent* Body = GetMesh();
	if (!Body || !Body->GetPhysicsAsset() || GetNetMode() == NM_DedicatedServer) return;
	// The body keeps the speed it had, so a runner tumbles forward and a standing one drops where it stood.
	const FVector Carried = GetVelocity();
	Body->SetCollisionProfileName(TEXT("Ragdoll"));
	Body->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	Body->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	Body->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
	Body->SetAllBodiesSimulatePhysics(true);
	Body->SetSimulatePhysics(true);
	Body->WakeAllRigidBodies();
	Body->bBlendPhysics = true;
	Body->SetAllPhysicsLinearVelocity(Carried);
	if (!FVector(DeathHitDirection).IsNearlyZero())
	{
		// A head shot snaps the head back and up; a body shot pushes where it landed.
		const FVector Push = (FVector(DeathHitDirection) + FVector(0.0f, 0.0f, bDeathHeadshot ? 0.45f : 0.12f)).GetSafeNormal();
		Body->AddImpulseAtLocation(Push * DeathImpulse, FVector(DeathHitLocation));
	}
}

void AArenaDuelZombie::MulticastTelegraph_Implementation(float Seconds, bool bSlam)
{
	if (GetNetMode() == NM_DedicatedServer || !GetWorld()) return;
	WarningLight->SetIntensity(bSlam ? 9000.0f : 2600.0f);
	WarningLight->SetAttenuationRadius(bSlam ? 900.0f : 420.0f);
	WarningLight->SetVisibility(true);
	const float Now = GetWorld()->GetTimeSeconds();
	WarningLightOffTime = Now + Seconds;
	AttackAnimStart = Now;
	AttackAnimEnd = Now + FMath::Max(Seconds, 0.05f);
	PlayZombieSound(ZombieAttackSound, bSlam ? 1.0f : 0.7f, FMath::FRandRange(0.9f, 1.1f));
}

void AArenaDuelZombie::MulticastHitReact_Implementation(FVector_NetQuantize Location, FVector_NetQuantizeNormal Direction, bool bHead, bool bHeavy)
{
	if (GetNetMode() == NM_DedicatedServer || !GetWorld()) return;
	// The body is knocked the way the shot travelled.
	const FVector Local = GetActorRotation().UnrotateVector(FVector(Direction));
	HitLeanYaw = FMath::RadiansToDegrees(FMath::Atan2(Local.Y, Local.X));
	const float Knock = bHead ? 20.0f : bHeavy ? 16.0f : 9.0f;
	HitLean = FMath::Max(HitLean, Knock * (ZombieType == EArenaDuelZombieType::Boss ? 0.2f : ZombieType == EArenaDuelZombieType::MiniBoss ? 0.4f : 1.0f));
	AArenaDuelHitBurst::Spawn(GetWorld(), FVector(Location), FVector(Direction), bHead || bHeavy);
	if (const float Now = GetWorld()->GetTimeSeconds(); Now >= NextHitSoundTime)
	{
		NextHitSoundTime = Now + 0.07f;
		PlayZombieSound(ZombieHitSound, bHead ? 0.9f : 0.6f, bHead ? 1.25f : FMath::FRandRange(0.9f, 1.1f));
	}
}

void AArenaDuelZombie::UpdateCosmeticPose(float DeltaSeconds)
{
	const float Now = GetWorld()->GetTimeSeconds();
	if (Now >= NextGrowlTime)
	{
		NextGrowlTime = Now + FMath::FRandRange(6.0f, 16.0f);
		PlayZombieSound(ZombieGrowlSound, 0.5f, FMath::FRandRange(0.8f, 1.2f));
	}
	// The swing: the body leans back while it winds up and snaps forward as the hit lands.
	if (Now < AttackAnimEnd && AttackAnimEnd > AttackAnimStart)
	{
		const float Phase = (Now - AttackAnimStart) / (AttackAnimEnd - AttackAnimStart);
		AttackLean = Phase < 0.7f ? -14.0f * (Phase / 0.7f) : FMath::Lerp(-14.0f, 30.0f, (Phase - 0.7f) / 0.3f);
	}
	else AttackLean = FMath::FInterpTo(AttackLean, 0.0f, DeltaSeconds, 7.0f);
	HitLean = FMath::FInterpTo(HitLean, 0.0f, DeltaSeconds, 8.0f);
	if (FMath::Abs(AttackLean) < 0.05f && FMath::Abs(HitLean) < 0.05f)
	{
		if (!bPoseDirty) return;
		AttackLean = 0.0f; HitLean = 0.0f; bPoseDirty = false;
	}
	else bPoseDirty = true;
	// The mesh pivots at the feet, so a tilt of the whole mesh reads as a lean of the body.
	const FQuat HitYaw = FRotator(0.0f, HitLeanYaw, 0.0f).Quaternion();
	const FQuat Hit = HitYaw * FRotator(-HitLean, 0.0f, 0.0f).Quaternion() * HitYaw.Inverse();
	const FQuat Pose = Hit * FRotator(-AttackLean, 0.0f, 0.0f).Quaternion() * FRotator(0.0f, -90.0f, 0.0f).Quaternion();
	// Client-side movement smoothing rebuilds the mesh rotation from this offset every frame.
	BaseRotationOffset = Pose;
	GetMesh()->SetRelativeRotation(Pose);
}

void AArenaDuelZombie::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	// The material path can arrive on a client a moment after the actor; apply it once it is there.
	if (!bVisualApplied && !MaterialPath.IsEmpty()) ApplyTypeVisual();
	if (WarningLight->IsVisible() && GetWorld() && GetWorld()->GetTimeSeconds() >= WarningLightOffTime) WarningLight->SetVisibility(false);
	if (!bDead && GetWorld() && GetNetMode() != NM_DedicatedServer) UpdateCosmeticPose(DeltaSeconds);
	if (!HasAuthority() || bDead) return;
	// Thinking ten times a second is plenty for melee enemies and keeps a full wave cheap.
	ThinkAccumulator += DeltaSeconds;
	if (GetWorld()->GetTimeSeconds() < SidestepEndTime)
	{
		AddMovementInput(SidestepDirection, 1.0f);
	}
	else if (bDirectChase && !bWindingUp)
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
		bDirectChase = false;
		return;
	}
	const float Now = World->GetTimeSeconds();
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
		MoveSampleLocation = GetActorLocation(); MoveSampleTime = Now;
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
	float Speed = BaseMoveSpeed * (bEnraged ? 1.25f : 1.0f);

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
			MulticastTelegraph(WindupEndTime - Now, true);
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
	// A hit slows it down for a moment.
	if (Now < StaggerEndTime) Speed *= 0.35f;
	GetCharacterMovement()->MaxWalkSpeed = Speed;

	// Melee.
	AArenaDuelZombieGameMode* Rules = World->GetAuthGameMode<AArenaDuelZombieGameMode>();
	if (Distance <= Reach && bSameLevel && Now >= NextAttackTime && Now >= StaggerEndTime && HasLineTo(Target) && (IsBossType() || !Rules || Rules->ClaimAttackOn(Target)))
	{
		bWindingUp = true; bSlamWindup = false;
		WindupEndTime = Now + Config.AttackWindup;
		if (AI) AI->StopMovement();
		bDirectChase = false;
		MulticastTelegraph(Config.AttackWindup + 0.1f, false);
		return;
	}

	// In reach and waiting for the next swing.
	if (Distance <= Reach * 0.8f && bSameLevel)
	{
		if (AI) AI->StopMovement();
		bDirectChase = false;
		MoveSampleLocation = GetActorLocation(); MoveSampleTime = Now;
		return;
	}

	// Standing still although the player is not in reach: step aside and look for another way.
	if (Now - MoveSampleTime >= 1.2f)
	{
		if (FVector::Dist2D(GetActorLocation(), MoveSampleLocation) < 25.0f && Now >= StaggerEndTime && Now >= SidestepEndTime)
		{
			++UnstickCount;
			const FVector Forward = ToTarget.GetSafeNormal2D();
			const FVector Side = FVector::CrossProduct(FVector::UpVector, Forward) * (FMath::RandBool() ? 1.0f : -1.0f);
			SidestepDirection = (Side + Forward * 0.3f).GetSafeNormal2D();
			SidestepEndTime = Now + 0.7f;
			ApproachOffset = FVector(FMath::FRandRange(-1.0f, 1.0f), FMath::FRandRange(-1.0f, 1.0f), 0.0f).GetSafeNormal() * FMath::FRandRange(40.0f, 160.0f);
			if (AI) AI->StopMovement();
			bDirectChase = false;
			NextRepathTime = SidestepEndTime;
		}
		MoveSampleLocation = GetActorLocation(); MoveSampleTime = Now;
	}
	if (Now < SidestepEndTime) return;

	// Runners change the side they come from every couple of seconds.
	if (ZombieType == EArenaDuelZombieType::Fast && Now >= NextOffsetRollTime)
	{
		NextOffsetRollTime = Now + FMath::FRandRange(1.5f, 2.5f);
		ApproachOffset = FVector(FMath::FRandRange(-1.0f, 1.0f), FMath::FRandRange(-1.0f, 1.0f), 0.0f).GetSafeNormal() * FMath::FRandRange(280.0f, 520.0f);
	}

	// Chase. Until close the goal sits a little beside the player; in reach it is the player.
	const bool bUseOffset = Distance > (ZombieType == EArenaDuelZombieType::Fast ? 600.0f : 450.0f);
	if (!AI) { bDirectChase = HasLineTo(Target); return; }
	// A new path a couple of times a second follows a moving player closely enough and keeps a full wave cheap.
	if (Now < NextRepathTime && (bDirectChase || AI->GetMoveStatus() != EPathFollowingStatus::Idle)) return;
	NextRepathTime = Now + FMath::FRandRange(0.35f, 0.55f);
	EPathFollowingRequestResult::Type Result = EPathFollowingRequestResult::Failed;
	if (bUseOffset) Result = AI->MoveToLocation(Target->GetActorLocation() + ApproachOffset, Reach * 0.6f, true, true, true, true);
	if (Result == EPathFollowingRequestResult::Failed) Result = AI->MoveToLocation(Target->GetActorLocation(), Reach * 0.6f, true, true, true, true);
	// No path, for instance while the player is in the air or where there is no navigation mesh: steer
	// straight, but only with a clear line, so a zombie never pushes against a wall towards the player.
	bDirectChase = Result == EPathFollowingRequestResult::Failed && HasLineTo(Target);
}
