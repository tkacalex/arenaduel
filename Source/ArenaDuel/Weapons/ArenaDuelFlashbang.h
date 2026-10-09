#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/NetSerialization.h"
#include "ArenaDuelFlashbang.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UProjectileMovementComponent;

/**
 * Thrown flash grenade. The server simulates the flight and decides who is blinded; clients see the
 * replicated movement and a cosmetic burst.
 */
UCLASS()
class ARENADUEL_API AArenaDuelFlashbang : public AActor
{
	GENERATED_BODY()

public:
	AArenaDuelFlashbang();

	/** Server only. Starts the flight and the fuse. */
	void Launch(const FVector& Velocity, float FuseSeconds, float InMaxBlindDistance, float InMaxBlindSeconds);

	/**
	 * How strongly a viewer at Eye looking along ViewDirection is blinded by a burst at BurstLocation, 0 to 1.
	 * Distance and facing only; line of sight is checked separately.
	 */
	static float ComputeBlindStrength(const FVector& BurstLocation, const FVector& Eye, const FVector& ViewDirection, float MaxDistance);

protected:
	virtual void BeginPlay() override;
	void Detonate();

	UFUNCTION(NetMulticast, Reliable)
	void MulticastDetonate(FVector_NetQuantize Location);

	UPROPERTY(VisibleAnywhere) TObjectPtr<USphereComponent> Collision;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Visual;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UProjectileMovementComponent> Movement;

	FTimerHandle FuseTimer;
	float MaxBlindDistance = 3500.0f;
	float MaxBlindSeconds = 3.2f;
};
