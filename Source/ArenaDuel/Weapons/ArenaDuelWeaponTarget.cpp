// Copyright Epic Games, Inc. All Rights Reserved.

#include "ArenaDuelWeaponTarget.h"
#include "Components/BoxComponent.h"

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
}
