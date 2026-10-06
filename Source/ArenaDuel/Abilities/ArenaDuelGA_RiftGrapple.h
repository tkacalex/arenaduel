#pragma once

#include "ArenaDuelGameplayAbility.h"
#include "ArenaDuelGA_RiftGrapple.generated.h"

UCLASS()
class ARENADUEL_API UArenaDuelGA_RiftGrapple : public UArenaDuelGameplayAbility
{
	GENERATED_BODY()
public:
	UArenaDuelGA_RiftGrapple();
	virtual const FGameplayTagContainer* GetCooldownTags() const override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
private:
	UFUNCTION() void FinishPull();
	FGameplayTagContainer CooldownTags;
};
