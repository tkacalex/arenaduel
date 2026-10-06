// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArenaDuelGameplayAbility.h"
#include "ArenaDuelGA_BurstLeap.generated.h"

UCLASS()
class ARENADUEL_API UArenaDuelGA_BurstLeap : public UArenaDuelGameplayAbility
{
	GENERATED_BODY()
public:
	UArenaDuelGA_BurstLeap();
	virtual const FGameplayTagContainer* GetCooldownTags() const override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
private:
	FGameplayTagContainer CooldownTags;
};
