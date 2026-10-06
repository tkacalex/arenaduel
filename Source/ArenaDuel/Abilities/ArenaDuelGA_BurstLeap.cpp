// Copyright Epic Games, Inc. All Rights Reserved.

#include "ArenaDuelGA_BurstLeap.h"
#include "ArenaDuelGameplayTags.h"
#include "ArenaDuelShadowCooldownEffects.h"
#include "../Characters/ArenaDuelCharacter.h"
#include "../Characters/ArenaDuelCharacterMovementComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

UArenaDuelGA_BurstLeap::UArenaDuelGA_BurstLeap()
{
	FGameplayTagContainer Tags;
	Tags.AddTag(TAG_Ability_Warden_BurstLeap.GetTag());
	SetAssetTags(Tags);
	CooldownTags.AddTag(TAG_Cooldown_Warden_BurstLeap.GetTag());
	CooldownGameplayEffectClass = UArenaDuelGE_BurstLeapCooldown::StaticClass();
}

const FGameplayTagContainer* UArenaDuelGA_BurstLeap::GetCooldownTags() const { return &CooldownTags; }

void UArenaDuelGA_BurstLeap::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	AArenaDuelCharacter* Character = ActorInfo ? Cast<AArenaDuelCharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	if (!Character || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	FVector Forward = Character->GetController() ? Character->GetController()->GetControlRotation().Vector().GetSafeNormal2D() : FVector::ZeroVector;
	if (Forward.IsNearlyZero()) Forward = Character->GetActorForwardVector().GetSafeNormal2D();
	FVector LaunchVelocity = Forward * 950.0f + FVector::UpVector * 720.0f;
	if (const UArenaDuelCharacterMovementComponent* Movement = Character->GetArenaDuelMovementComponent())
	{
		const float Cap = FMath::Max(Movement->GlobalMomentumCap, 950.0f);
		if (LaunchVelocity.Size() > Cap) LaunchVelocity = LaunchVelocity.GetSafeNormal() * Cap;
	}
	Character->LaunchCharacter(LaunchVelocity, true, true);
	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}
