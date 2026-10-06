// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ArenaDuelArcBarrier.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class AArenaDuelPlayerState;

UCLASS()
class ARENADUEL_API AArenaDuelArcBarrier : public AActor
{
	GENERATED_BODY()
public:
	AArenaDuelArcBarrier();
	void SetBarrierOwnerState(AArenaDuelPlayerState* InOwnerState);
	void ApplyBarrierDamage(float Damage);
	static bool HasBarrierForOwner(UWorld* World, const AArenaDuelPlayerState* OwnerState);
	static bool IsPlacementClear(UWorld* World, const FVector& Location, const FRotator& Rotation, const AActor* IgnoredActor);
	static void DestroyAllForRound(UWorld* World);
	static void DestroyOwnedByPlayerState(UWorld* World, const AArenaDuelPlayerState* OwnerState);
	float GetBarrierHealth() const { return Health; }
	FVector GetBarrierHalfExtents() const { return FVector(12.5f, 212.5f, 122.5f); }
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
protected:
	virtual void BeginPlay() override;
	UFUNCTION() void OnRep_Health();
	void UpdateVisualForHealth();
	UPROPERTY(VisibleAnywhere) TObjectPtr<UBoxComponent> CollisionBox;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> BarrierVisual;
	UPROPERTY(Replicated) TObjectPtr<AArenaDuelPlayerState> BarrierOwnerState;
	UPROPERTY(ReplicatedUsing=OnRep_Health) float Health = 350.0f;
	UPROPERTY(EditDefaultsOnly, Category="Barrier") float MaxHealth = 350.0f;
};
