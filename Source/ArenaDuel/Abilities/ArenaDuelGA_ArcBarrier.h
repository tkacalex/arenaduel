// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArenaDuelGameplayAbility.h"
#include "ArenaDuelGA_ArcBarrier.generated.h"

UCLASS()
class ARENADUEL_API UArenaDuelGA_ArcBarrier : public UArenaDuelGameplayAbility
{
	GENERATED_BODY()
public:
	UArenaDuelGA_ArcBarrier();
	virtual const FGameplayTagContainer* GetCooldownTags() const override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
private:
	FGameplayTagContainer CooldownTags;
};
