// Copyright Epic Games, Inc. All Rights Reserved.

#include "ArenaDuelArcBarrier.h"
#include "../Player/ArenaDuelPlayerState.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

AArenaDuelArcBarrier::AArenaDuelArcBarrier()
{
	bReplicates = true;
	SetReplicateMovement(false);
	InitialLifeSpan = 5.0f;
	CollisionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("BarrierCollision"));
	RootComponent = CollisionBox;
	CollisionBox->SetBoxExtent(GetBarrierHalfExtents());
	CollisionBox->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	CollisionBox->SetCollisionObjectType(ECC_WorldDynamic);
	CollisionBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	CollisionBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	CollisionBox->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	CollisionBox->SetGenerateOverlapEvents(false);
	BarrierVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BarrierVisual"));
	BarrierVisual->SetupAttachment(RootComponent);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Material(TEXT("/Engine/BasicShapes/BasicShapeMaterial_Inst.BasicShapeMaterial_Inst"));
	if (Cube.Succeeded()) BarrierVisual->SetStaticMesh(Cube.Object);
	if (Material.Succeeded()) BarrierVisual->SetMaterial(0, Material.Object);
	BarrierVisual->SetRelativeScale3D(FVector(0.25f, 4.25f, 2.45f));
	BarrierVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BarrierVisual->SetCastShadow(false);
}

void AArenaDuelArcBarrier::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority())
	{
		Health = MaxHealth;
		SetLifeSpan(5.0f);
	}
	UpdateVisualForHealth();
}

void AArenaDuelArcBarrier::SetBarrierOwnerState(AArenaDuelPlayerState* InOwnerState)
{
	if (HasAuthority()) BarrierOwnerState = InOwnerState;
}

void AArenaDuelArcBarrier::ApplyBarrierDamage(float Damage)
{
	if (!HasAuthority() || Damage <= 0.0f || !FMath::IsFinite(Damage)) return;
	Health = FMath::Max(0.0f, Health - Damage);
	UpdateVisualForHealth();
	ForceNetUpdate();
	if (Health <= 0.0f) Destroy();
}

void AArenaDuelArcBarrier::OnRep_Health() { UpdateVisualForHealth(); }

void AArenaDuelArcBarrier::UpdateVisualForHealth()
{
	if (!BarrierVisual) return;
	UMaterialInstanceDynamic* DynamicMaterial = Cast<UMaterialInstanceDynamic>(BarrierVisual->GetMaterial(0));
	if (!DynamicMaterial) DynamicMaterial = BarrierVisual->CreateAndSetMaterialInstanceDynamic(0);
	if (!DynamicMaterial) return;
	const float Ratio = MaxHealth > 0.0f ? Health / MaxHealth : 0.0f;
	const FLinearColor Color = Ratio > 0.66f ? FLinearColor(0.04f, 0.45f, 0.70f, 0.95f) : Ratio > 0.33f ? FLinearColor(0.10f, 0.32f, 0.55f, 0.90f) : FLinearColor(0.25f, 0.60f, 0.72f, 0.82f);
	DynamicMaterial->SetVectorParameterValue(TEXT("Color"), Color);
}

bool AArenaDuelArcBarrier::HasBarrierForOwner(UWorld* World, const AArenaDuelPlayerState* OwnerState)
{
	if (!World || !OwnerState) return false;
	for (TActorIterator<AArenaDuelArcBarrier> It(World); It; ++It) if (It->BarrierOwnerState == OwnerState || It->GetOwner() == OwnerState) return true;
	return false;
}

bool AArenaDuelArcBarrier::IsPlacementClear(UWorld* World, const FVector& Location, const FRotator& Rotation, const AActor* IgnoredActor)
{
	if (!World || Location.ContainsNaN() || Rotation.ContainsNaN()) return false;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(ArenaDuelBarrierPlacement), false);
	if (IgnoredActor) Params.AddIgnoredActor(IgnoredActor);
	return !World->OverlapBlockingTestByChannel(Location, Rotation.Quaternion(), ECC_Pawn, FCollisionShape::MakeBox(FVector(12.5f, 212.5f, 122.5f)), Params);
}

void AArenaDuelArcBarrier::DestroyAllForRound(UWorld* World)
{
	if (!World || !World->GetAuthGameMode()) return;
	for (TActorIterator<AArenaDuelArcBarrier> It(World); It; ++It) It->Destroy();
}

void AArenaDuelArcBarrier::DestroyOwnedByPlayerState(UWorld* World, const AArenaDuelPlayerState* OwnerState)
{
	if (!World || !OwnerState || !World->GetAuthGameMode()) return;
	for (TActorIterator<AArenaDuelArcBarrier> It(World); It; ++It) if (It->BarrierOwnerState == OwnerState || It->GetOwner() == OwnerState) It->Destroy();
}

void AArenaDuelArcBarrier::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AArenaDuelArcBarrier, BarrierOwnerState);
	DOREPLIFETIME(AArenaDuelArcBarrier, Health);
}
