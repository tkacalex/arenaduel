// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ArenaDuelVeilWall.generated.h"

class UStaticMeshComponent;

UCLASS()
class ARENADUEL_API AArenaDuelVeilWall : public AActor
{
	GENERATED_BODY()

public:
	AArenaDuelVeilWall();
	static void DestroyAllForRound(UWorld* World);
	virtual void BeginPlay() override;
	FVector GetVisualScale() const;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> WallVisual;
};
