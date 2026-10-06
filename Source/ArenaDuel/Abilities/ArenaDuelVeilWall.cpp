// Copyright Epic Games, Inc. All Rights Reserved.

#include "ArenaDuelVeilWall.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"
#include "../Player/ArenaDuelPlayerState.h"

AArenaDuelVeilWall::AArenaDuelVeilWall()
{
	bReplicates = true;
	SetReplicateMovement(false);
	InitialLifeSpan = 3.0f;
	WallVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WallVisual"));
	RootComponent = WallVisual;
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ShapeMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial_Inst.BasicShapeMaterial_Inst"));
	if (CubeMesh.Succeeded()) WallVisual->SetStaticMesh(CubeMesh.Object);
	if (ShapeMaterial.Succeeded()) WallVisual->SetMaterial(0, ShapeMaterial.Object);
	// The long local Y axis spans across the player's facing direction, creating a sight-blocking plane.
	WallVisual->SetRelativeScale3D(FVector(0.18f, 4.5f, 2.5f));
	WallVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WallVisual->SetGenerateOverlapEvents(false);
	WallVisual->SetCastShadow(false);
	WallVisual->SetRenderCustomDepth(true);
}

void AArenaDuelVeilWall::BeginPlay()
{
	Super::BeginPlay();
	SetLifeSpan(3.0f);
	if (UMaterialInstanceDynamic* WallMaterial = WallVisual ? WallVisual->CreateAndSetMaterialInstanceDynamic(0) : nullptr)
	{
		WallMaterial->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.10f, 0.025f, 0.22f, 0.86f));
	}
}

FVector AArenaDuelVeilWall::GetVisualScale() const
{
	return WallVisual ? WallVisual->GetRelativeScale3D() : FVector::ZeroVector;
}

void AArenaDuelVeilWall::DestroyAllForRound(UWorld* World)
{
	if (!World || !World->GetAuthGameMode()) return;
	for (TActorIterator<AArenaDuelVeilWall> It(World); It; ++It) It->Destroy();
}

void AArenaDuelVeilWall::DestroyOwnedByPlayerState(UWorld* World, const AArenaDuelPlayerState* OwnerState)
{
	if (!World || !OwnerState || !World->GetAuthGameMode()) return;
	for (TActorIterator<AArenaDuelVeilWall> It(World); It; ++It) if (It->GetOwner() == OwnerState) It->Destroy();
}

void AArenaDuelVeilWall::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
}
