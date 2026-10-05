// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ArenaDuelWeaponTarget.generated.h"

class UBoxComponent;

UCLASS()
class ARENADUEL_API AArenaDuelWeaponTarget : public AActor
{
	GENERATED_BODY()

public:
	AArenaDuelWeaponTarget();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TObjectPtr<UBoxComponent> BodyHitZone;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TObjectPtr<UBoxComponent> HeadHitZone;
};
