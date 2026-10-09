// Copyright Epic Games, Inc. All Rights Reserved.

#include "ArenaDuelGA_ShadowStep.h"
#include "ArenaDuelGameplayTags.h"
#include "ArenaDuelShadowCooldownEffects.h"
#include "../Characters/ArenaDuelCharacter.h"
#include "../Characters/ArenaDuelCharacterMovementComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

UArenaDuelGA_ShadowStep::UArenaDuelGA_ShadowStep()
{
	FGameplayTagContainer AbilityAssetTags;
	AbilityAssetTags.AddTag(TAG_Ability_Shadow_ShadowStep.GetTag());
	SetAssetTags(AbilityAssetTags);
	CooldownTags.AddTag(TAG_Cooldown_Shadow_ShadowStep.GetTag());
	CooldownGameplayEffectClass = UArenaDuelGE_ShadowStepCooldown::StaticClass();
}

const FGameplayTagContainer* UArenaDuelGA_ShadowStep::GetCooldownTags() const
{
	return &CooldownTags;
}

void UArenaDuelGA_ShadowStep::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	AArenaDuelCharacter* Character = ActorInfo ? Cast<AArenaDuelCharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	if (!Character || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	FVector Direction = Character->GetLastMovementInputVector().GetSafeNormal2D();
	if (Direction.IsNearlyZero() && Character->GetCharacterMovement())	Direction = Character->GetCharacterMovement()->GetCurrentAcceleration().GetSafeNormal2D();
	if (Direction.IsNearlyZero() && Character->GetController())	Direction = Character->GetController()->GetControlRotation().Vector().GetSafeNormal2D();
	if (Direction.IsNearlyZero())	Direction = Character->GetActorForwardVector().GetSafeNormal2D();

	const UArenaDuelCharacterMovementComponent* Movement = Character->GetArenaDuelMovementComponent();
	// From the ground the dash lifts off a little; in the air it keeps the vertical speed it has.
	const float CurrentZ = Character->GetVelocity().Z;
	const float LaunchZ = Movement && Movement->IsMovingOnGround() ? DashLift : CurrentZ;
	Character->LaunchCharacter(Direction * DashSpeed + FVector(0.0f, 0.0f, LaunchZ), true, true);
	Character->PlayShadowStepCameraImpulse();
	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}
