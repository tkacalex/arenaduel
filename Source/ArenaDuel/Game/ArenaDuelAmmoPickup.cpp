#include "ArenaDuelAmmoPickup.h"
#include "../Characters/ArenaDuelCharacter.h"
#include "../Weapons/ArenaDuelWeaponComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Materials/MaterialInterface.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

AArenaDuelAmmoPickup::AArenaDuelAmmoPickup()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));

	Visual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Visual"));
	Visual->SetupAttachment(RootComponent);
	// Walked through, never bumped into, and no cover against bullets.
	Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Visual->SetCastShadow(false);
	Visual->SetCanEverAffectNavigation(false);
	Visual->SetRelativeScale3D(FVector(0.42f, 0.28f, 0.22f));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (Cube.Succeeded()) Visual->SetStaticMesh(Cube.Object);

	Glow = CreateDefaultSubobject<UPointLightComponent>(TEXT("Glow"));
	Glow->SetupAttachment(RootComponent);
	Glow->SetRelativeLocation(FVector(0.0f, 0.0f, 40.0f));
	Glow->SetIntensity(320.0f);
	Glow->SetAttenuationRadius(380.0f);
	Glow->SetLightColor(FLinearColor(1.0f, 0.66f, 0.18f));
	Glow->SetCastShadows(false);
}

void AArenaDuelAmmoPickup::BeginPlay()
{
	Super::BeginPlay();
	if (GetNetMode() != NM_DedicatedServer)
	{
		// The modelled box is 42 x 28 x 22 cm and brings its own materials; without it, a scaled cube in the bright material.
		if (UStaticMesh* Box = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/ArenaDuel/Props/SM_AmmoBox.SM_AmmoBox"), nullptr, LOAD_NoWarn | LOAD_Quiet))
		{
			Visual->SetStaticMesh(Box);
			Visual->SetRelativeScale3D(FVector(1.3f));
		}
		else if (UMaterialInterface* Bright = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/ArenaDuel/Characters/Common/M_SurvivalAmmo.M_SurvivalAmmo"), nullptr, LOAD_NoWarn | LOAD_Quiet)) Visual->SetMaterial(0, Bright);
	}
	if (HasAuthority()) GetWorldTimerManager().SetTimer(PickupTimer, this, &AArenaDuelAmmoPickup::CheckPickup, 0.1f, true);
}

bool AArenaDuelAmmoPickup::IsInReach(const FVector& Location) const
{
	const FVector Offset = Location - GetActorLocation();
	return Offset.Size2D() <= PickupRadius && FMath::Abs(Offset.Z) < 160.0;
}

void AArenaDuelAmmoPickup::CheckPickup()
{
	for (TActorIterator<AArenaDuelCharacter> It(GetWorld()); It; ++It)
	{
		if (It->IsDead() || !It->GetController() || !It->GetWeaponComponent() || !IsInReach(It->GetActorLocation())) continue;
		It->GetWeaponComponent()->AddReserveAmmoShare(AmmoShare);
		Destroy();
		return;
	}
}

void AArenaDuelAmmoPickup::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (GetNetMode() == NM_DedicatedServer) return;
	// Turning and bobbing makes a small box readable from across the arena.
	SpinSeconds += DeltaSeconds;
	Visual->SetRelativeLocationAndRotation(FVector(0.0f, 0.0f, 6.0f * FMath::Sin(SpinSeconds * 2.4f)), FRotator(0.0f, SpinSeconds * 70.0f, 0.0f));
}
