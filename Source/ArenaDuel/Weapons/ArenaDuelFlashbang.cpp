#include "ArenaDuelFlashbang.h"
#include "../Characters/ArenaDuelCharacter.h"
#include "ArenaDuelItemMeshes.h"
#include "../Characters/ArenaDuelZombie.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Materials/MaterialInterface.h"
#include "Components/PointLightComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Controller.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

AArenaDuelFlashbang::AArenaDuelFlashbang()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicateMovement(true);
	bAlwaysRelevant = true;

	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	Collision->InitSphereRadius(6.0f);
	// Bounces off the arena, passes through players and never blocks a weapon trace.
	Collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Collision->SetCollisionObjectType(ECC_WorldDynamic);
	Collision->SetCollisionResponseToAllChannels(ECR_Ignore);
	Collision->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	Collision->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
	RootComponent = Collision;

	Visual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Visual"));
	Visual->SetupAttachment(Collision);
	Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	Movement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("Movement"));
	Movement->SetUpdatedComponent(Collision);
	Movement->bShouldBounce = true;
	Movement->Bounciness = 0.42f;
	Movement->Friction = 0.35f;
	Movement->ProjectileGravityScale = 1.0f;
	Movement->bRotationFollowsVelocity = false;
	Movement->InitialSpeed = 0.0f;
	Movement->MaxSpeed = 4000.0f;
}

void AArenaDuelFlashbang::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority()) Movement->OnProjectileBounce.AddDynamic(this, &AArenaDuelFlashbang::HandleBounce);
	if (GetNetMode() == NM_DedicatedServer) return;
	// The same model as the one held in the hand. Slots: 0 body, 1 bands and fuse, 2 lever and pin.
	UStaticMesh* Model = ArenaDuelItemMeshes::Flashbang();
	Visual->SetStaticMesh(Model);
	if (Model && !Model->GetMaterial(0))
	{
		// Code-built fallback without materials of its own.
		Visual->SetMaterial(0, LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial")));
		Visual->SetMaterial(1, LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/ArenaDuel/Characters/Common/M_ArcaneMetal")));
		Visual->SetMaterial(2, LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/ArenaDuel/Characters/Common/M_ArcaneCyan")));
	}
}

void AArenaDuelFlashbang::HandleBounce(const FHitResult& ImpactResult, const FVector& ImpactVelocity)
{
	// Rolling to a stop is a string of tiny bounces; only real ones are heard, and not more than a few a second.
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	const float Speed = static_cast<float>(ImpactVelocity.Size());
	if (Speed < 120.0f || Now - LastBounceWorldTime < 0.12f) return;
	LastBounceWorldTime = Now;
	MulticastBounce(GetActorLocation(), FMath::Clamp(Speed / 900.0f, 0.25f, 1.0f));
}

void AArenaDuelFlashbang::MulticastBounce_Implementation(FVector_NetQuantize Location, float Strength)
{
	if (GetNetMode() == NM_DedicatedServer) return;
	if (USoundBase* Clink = ArenaDuelItemMeshes::Sound(TEXT("S_FlashBounce"))) UGameplayStatics::PlaySoundAtLocation(this, Clink, FVector(Location), Strength, FMath::FRandRange(0.92f, 1.08f));
}

void AArenaDuelFlashbang::Launch(const FVector& Velocity, float FuseSeconds, float InMaxBlindDistance, float InMaxBlindSeconds)
{
	if (!HasAuthority()) return;
	MaxBlindDistance = FMath::Max(InMaxBlindDistance, 1.0f);
	MaxBlindSeconds = FMath::Max(InMaxBlindSeconds, 0.0f);
	// The thrower's own capsule is already ignored through the Pawn channel.
	Movement->Velocity = Velocity;
	Movement->UpdateComponentVelocity();
	GetWorldTimerManager().SetTimer(FuseTimer, this, &AArenaDuelFlashbang::Detonate, FMath::Max(FuseSeconds, 0.1f), false);
}

float AArenaDuelFlashbang::ComputeBlindStrength(const FVector& BurstLocation, const FVector& Eye, const FVector& ViewDirection, float MaxDistance)
{
	const FVector ToBurst = BurstLocation - Eye;
	const float Distance = ToBurst.Size();
	if (Distance >= MaxDistance || MaxDistance <= 0.0f) return 0.0f;
	const float DistanceFactor = 1.0f - FMath::Square(Distance / MaxDistance);
	// Looking straight at the burst is the full effect. Looking away still leaves a quarter of it.
	const float Facing = Distance > 1.0f ? FVector::DotProduct(ViewDirection.GetSafeNormal(), ToBurst / Distance) : 1.0f;
	const float FacingFactor = FMath::Lerp(0.25f, 1.0f, FMath::Clamp((Facing + 0.2f) / 1.2f, 0.0f, 1.0f));
	return FMath::Clamp(DistanceFactor * FacingFactor, 0.0f, 1.0f);
}

void AArenaDuelFlashbang::Detonate()
{
	if (!HasAuthority() || !GetWorld()) return;
	const FVector Burst = GetActorLocation();
	for (TActorIterator<AArenaDuelCharacter> It(GetWorld()); It; ++It)
	{
		AArenaDuelCharacter* Character = *It;
		if (!Character || Character->IsDead()) continue;
		const FVector Eye = Character->GetPawnViewLocation();
		const FVector View = Character->GetController() ? Character->GetControlRotation().Vector() : Character->GetActorForwardVector();
		const float Strength = ComputeBlindStrength(Burst, Eye, View, MaxBlindDistance);
		if (Strength <= 0.05f) continue;
		// A wall between the burst and the eye shields completely. Players do not block this trace.
		FHitResult Wall;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(ArenaDuelFlashSight), false, this);
		Params.AddIgnoredActor(Character);
		if (GetWorld()->LineTraceSingleByChannel(Wall, Burst, Eye, ECC_Visibility, Params)) continue;
		Character->ClientApplyFlash(Strength, MaxBlindSeconds * Strength);
	}
	MulticastDetonate(Burst);
	Movement->StopMovementImmediately();
	Visual->SetVisibility(false);
	// Stay alive just long enough for the burst to reach every client.
	SetLifeSpan(0.5f);
}

void AArenaDuelFlashbang::MulticastDetonate_Implementation(FVector_NetQuantize Location)
{
	UWorld* World = GetWorld();
	if (Visual) Visual->SetVisibility(false);
	if (!World || World->GetNetMode() == NM_DedicatedServer) return;
	if (USoundBase* Bang = ArenaDuelItemMeshes::Sound(TEXT("S_FlashBang"))) UGameplayStatics::PlaySoundAtLocation(World, Bang, FVector(Location), 1.0f);
	AArenaDuelHitBurst::SpawnSparks(World, FVector(Location));
	// Cosmetic burst on a short timer of its own, so it outlives this actor's cleanup order.
	UPointLightComponent* Burst = NewObject<UPointLightComponent>(World->GetWorldSettings());
	Burst->SetWorldLocation(FVector(Location));
	Burst->SetIntensity(400000.0f);
	Burst->SetAttenuationRadius(2600.0f);
	Burst->SetLightColor(FLinearColor::White);
	// No shadow pass: a freshly spawned shadow-casting light costs a visible hitch for a 0.12 second blink.
	Burst->SetCastShadows(false);
	Burst->SetIndirectLightingIntensity(0.0f);
	Burst->SetVolumetricScatteringIntensity(0.0f);
	Burst->RegisterComponentWithWorld(World);
	FTimerHandle Handle;
	World->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(Burst, [Burst]() { Burst->DestroyComponent(); }), 0.12f, false);
}
