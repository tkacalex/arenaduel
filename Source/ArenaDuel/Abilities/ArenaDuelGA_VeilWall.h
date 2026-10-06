// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArenaDuelGameplayAbility.h"
#include "ArenaDuelGA_VeilWall.generated.h"

UCLASS()
class ARENADUEL_API UArenaDuelGA_VeilWall : public UArenaDuelGameplayAbility
{
	GENERATED_BODY()

public:
	UArenaDuelGA_VeilWall();
	virtual const FGameplayTagContainer* GetCooldownTags() const override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

private:
	FGameplayTagContainer CooldownTags;
};
