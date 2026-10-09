#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ArenaDuelHealPad.generated.h"

class UStaticMeshComponent;

/**
 * A small pad on the floor that slowly heals whoever stands on it. The server decides; health reaches
 * clients through the ability system as usual. Healing comes in small steps so the bar climbs evenly.
 */
UCLASS()
class ARENADUEL_API AArenaDuelHealPad : public AActor
{
	GENERATED_BODY()

public:
	AArenaDuelHealPad();

	float GetHealPerSecond() const { return HealPerSecond; }
	float GetHealStep() const { return HealStep; }
	float GetPadHalfSize() const { return PadHalfSize; }
	/** Seconds between two steps: one step of HealStep adds up to HealPerSecond over a second. */
	float GetStepInterval() const { return HealStep / FMath::Max(HealPerSecond, 0.01f); }
	/** True when a character whose feet are at FeetLocation stands on the pad. */
	bool IsOnPad(const FVector& FeetLocation) const;
	/** One heal step for every living player on the pad. The server timer calls this every GetStepInterval(). */
	void HealStandingPlayers();

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Heal Pad") TObjectPtr<UStaticMeshComponent> Visual;

	UPROPERTY(EditAnywhere, Category = "Heal Pad", meta = (ClampMin = "0.1")) float HealPerSecond = 5.0f;
	/** Health added per step. */
	UPROPERTY(EditAnywhere, Category = "Heal Pad", meta = (ClampMin = "0.1")) float HealStep = 1.0f;
	/** Half the side length of the square pad, in centimetres. */
	UPROPERTY(EditAnywhere, Category = "Heal Pad", meta = (ClampMin = "10")) float PadHalfSize = 60.0f;

	FTimerHandle HealTimer;
};
