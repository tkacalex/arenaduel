#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ArenaDuelPhaseGateVisual.generated.h"

class AArenaDuelPlayerState;
class UStaticMeshComponent;

UCLASS()
class ARENADUEL_API AArenaDuelPhaseGateVisual : public AActor
{
	GENERATED_BODY()
public:
	AArenaDuelPhaseGateVisual();
	virtual void BeginPlay() override;
	static void DestroyAllForRound(UWorld* World);
	static void DestroyOwnedByPlayerState(UWorld* World, const AArenaDuelPlayerState* State);
private:
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Center;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Rim;
};
