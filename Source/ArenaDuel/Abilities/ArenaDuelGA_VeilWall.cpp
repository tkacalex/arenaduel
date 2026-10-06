// Copyright Epic Games, Inc. All Rights Reserved.

#include "ArenaDuelGA_VeilWall.h"
#include "ArenaDuelGameplayTags.h"
#include "ArenaDuelShadowCooldownEffects.h"
#include "ArenaDuelVeilWall.h"
#include "../Characters/ArenaDuelCharacter.h"
#include "../Player/ArenaDuelPlayerState.h"

UArenaDuelGA_VeilWall::UArenaDuelGA_VeilWall()
{
	FGameplayTagContainer AbilityAssetTags;
	AbilityAssetTags.AddTag(TAG_Ability_Shadow_VeilWall.GetTag());
	SetAssetTags(AbilityAssetTags);
	CooldownTags.AddTag(TAG_Cooldown_Shadow_VeilWall.GetTag());
	CooldownGameplayEffectClass = UArenaDuelGE_VeilWallCooldown::StaticClass();
}

const FGameplayTagContainer* UArenaDuelGA_VeilWall::GetCooldownTags() const
{
	return &CooldownTags;
}

void UArenaDuelGA_VeilWall::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	AArenaDuelCharacter* Character = ActorInfo ? Cast<AArenaDuelCharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	if (!Character || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (Character->HasAuthority())
	{
		FVector Facing = Character->GetActorForwardVector().GetSafeNormal2D();
		if (Character->GetController()) Facing = Character->GetController()->GetControlRotation().Vector().GetSafeNormal2D();
		if (Facing.IsNearlyZero()) Facing = Character->GetActorForwardVector().GetSafeNormal2D();
		const FVector SpawnLocation = Character->GetActorLocation() + Facing * 300.0f;
		const FRotator SpawnRotation(0.0f, Facing.Rotation().Yaw, 0.0f);
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Owner = Character->GetPlayerState<AArenaDuelPlayerState>();
		SpawnParameters.Instigator = Character;
		Character->GetWorld()->SpawnActor<AArenaDuelVeilWall>(AArenaDuelVeilWall::StaticClass(), SpawnLocation, SpawnRotation, SpawnParameters);
	}

	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}
