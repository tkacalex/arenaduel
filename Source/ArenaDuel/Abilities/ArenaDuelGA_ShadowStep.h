// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArenaDuelGameplayAbility.h"
#include "ArenaDuelGA_ShadowStep.generated.h"

UCLASS()
class ARENADUEL_API UArenaDuelGA_ShadowStep : public UArenaDuelGameplayAbility
{
	GENERATED_BODY()

public:
	UArenaDuelGA_ShadowStep();
	virtual const FGameplayTagContainer* GetCooldownTags() const override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	float GetDashSpeed() const { return DashSpeed; }
	float GetDashLift() const { return DashLift; }

protected:
	/** Horizontal speed of the dash. It is its own value and not limited by the movement momentum cap. */
	UPROPERTY(EditDefaultsOnly, Category = "Shadow Step", meta = (ClampMin = "0"))
	float DashSpeed = 6750.0f;

	/** Upward speed added when the dash starts on the ground. A small hop keeps ground braking from eating the dash. */
	UPROPERTY(EditDefaultsOnly, Category = "Shadow Step", meta = (ClampMin = "0"))
	float DashLift = 200.0f;

private:
	FGameplayTagContainer CooldownTags;
};
