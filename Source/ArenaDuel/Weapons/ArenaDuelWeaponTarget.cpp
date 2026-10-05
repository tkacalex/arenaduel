// Copyright Epic Games, Inc. All Rights Reserved.

#include "ArenaDuelWeaponTarget.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"

AArenaDuelWeaponTarget::AArenaDuelWeaponTarget()
{
	PrimaryActorTick.bCanEverTick = false;
	BodyHitZone = CreateDefaultSubobject<UBoxComponent>(TEXT("BodyHitZone"));
	RootComponent = BodyHitZone;
	BodyHitZone->SetBoxExtent(FVector(35.0f, 35.0f, 75.0f));
	BodyHitZone->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	HeadHitZone = CreateDefaultSubobject<UBoxComponent>(TEXT("HeadHitZone"));
	HeadHitZone->SetupAttachment(BodyHitZone);
	HeadHitZone->SetRelativeLocation(FVector(0.0f, 0.0f, 105.0f));
	HeadHitZone->SetBoxExtent(FVector(28.0f, 28.0f, 28.0f));
	HeadHitZone->ComponentTags.Add(TEXT("HeadHitZone"));
	HeadHitZone->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	BodyVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyVisual"));
	BodyVisual->SetupAttachment(BodyHitZone);
	BodyVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BodyVisual->SetRelativeScale3D(FVector(0.7f, 0.7f, 1.5f));
	if (CubeMesh.Succeeded()) BodyVisual->SetStaticMesh(CubeMesh.Object);
	HeadVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HeadVisual"));
	HeadVisual->SetupAttachment(BodyHitZone);
	HeadVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HeadVisual->SetRelativeLocation(FVector(0.0f, 0.0f, 105.0f));
	HeadVisual->SetRelativeScale3D(FVector(0.65f));
	if (SphereMesh.Succeeded()) HeadVisual->SetStaticMesh(SphereMesh.Object);
}
