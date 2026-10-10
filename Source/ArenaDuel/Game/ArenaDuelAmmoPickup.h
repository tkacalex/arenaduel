#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ArenaDuelAmmoPickup.generated.h"

class UPointLightComponent;
class UStaticMeshComponent;

/**
 * An ammunition box on the floor. The first living player to walk over it gets a share of every weapon's
 * reserve back and the box is gone. The server decides; the box only turns and glows on the other machines.
 */
UCLASS()
class ARENADUEL_API AArenaDuelAmmoPickup : public AActor
{
	GENERATED_BODY()

public:
	AArenaDuelAmmoPickup();
	virtual void Tick(float DeltaSeconds) override;

	float GetAmmoShare() const { return AmmoShare; }
	void SetAmmoShare(float Share) { AmmoShare = FMath::Clamp(Share, 0.0f, 1.0f); }
	/** True when a character standing at Location is close enough to take the box. */
	bool IsInReach(const FVector& Location) const;

protected:
	virtual void BeginPlay() override;
	void CheckPickup();

	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Visual;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UPointLightComponent> Glow;

	/** Share of each weapon's reserve capacity the box gives. */
	UPROPERTY(EditAnywhere, Category = "Ammo Pickup", meta = (ClampMin = "0", ClampMax = "1")) float AmmoShare = 0.5f;
	UPROPERTY(EditAnywhere, Category = "Ammo Pickup", meta = (ClampMin = "10")) float PickupRadius = 110.0f;

	FTimerHandle PickupTimer;
	float SpinSeconds = 0.0f;
};
