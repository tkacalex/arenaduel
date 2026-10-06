// Copyright Epic Games, Inc. All Rights Reserved.

#include "ArenaDuelShadowCooldownEffects.h"
#include "ArenaDuelGameplayTags.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"

namespace
{
	UTargetTagsGameplayEffectComponent* ConfigureCooldown(UGameplayEffect& Effect, const FObjectInitializer& ObjectInitializer, float Duration)
	{
		Effect.DurationPolicy = EGameplayEffectDurationType::HasDuration;
		Effect.DurationMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(Duration));
		return ObjectInitializer.CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(&Effect, TEXT("CooldownGrantedTags"));
	}
}

UArenaDuelGE_ShadowStepCooldown::UArenaDuelGE_ShadowStepCooldown(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	UTargetTagsGameplayEffectComponent* TargetTags = ConfigureCooldown(*this, ObjectInitializer, 5.0f);
	GEComponents.Add(TargetTags);
	FInheritedTagContainer GrantedTags;
	GrantedTags.AddTag(TAG_Cooldown_Shadow_ShadowStep.GetTag());
	TargetTags->SetAndApplyTargetTagChanges(GrantedTags);
}

UArenaDuelGE_VeilWallCooldown::UArenaDuelGE_VeilWallCooldown(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	UTargetTagsGameplayEffectComponent* TargetTags = ConfigureCooldown(*this, ObjectInitializer, 12.0f);
	GEComponents.Add(TargetTags);
	FInheritedTagContainer GrantedTags;
	GrantedTags.AddTag(TAG_Cooldown_Shadow_VeilWall.GetTag());
	TargetTags->SetAndApplyTargetTagChanges(GrantedTags);
}
