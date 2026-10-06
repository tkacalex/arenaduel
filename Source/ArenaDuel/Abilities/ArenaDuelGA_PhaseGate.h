#pragma once

#include "ArenaDuelGameplayAbility.h"
#include "ArenaDuelGA_PhaseGate.generated.h"

class AArenaDuelPhaseGateVisual;

UCLASS()
class ARENADUEL_API UArenaDuelGA_PhaseGate : public UArenaDuelGameplayAbility
{
	GENERATED_BODY()
public:
	UArenaDuelGA_PhaseGate();
	virtual const FGameplayTagContainer* GetCooldownTags() const override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
private:
	UFUNCTION() void FinishPhase();
	FGameplayTagContainer CooldownTags;
	FVector Destination = FVector::ZeroVector;
	TArray<TWeakObjectPtr<AArenaDuelPhaseGateVisual>> CastVisuals;
};
