// Copyright Epic Games, Inc. All Rights Reserved.

#include "ArenaDuelGA_ArcBarrier.h"
#include "ArenaDuelGameplayTags.h"
#include "ArenaDuelShadowCooldownEffects.h"
#include "ArenaDuelArcBarrier.h"
#include "../Characters/ArenaDuelCharacter.h"
#include "../Player/ArenaDuelPlayerState.h"
#include "Engine/World.h"

UArenaDuelGA_ArcBarrier::UArenaDuelGA_ArcBarrier()
{
	FGameplayTagContainer Tags;
	Tags.AddTag(TAG_Ability_Warden_ArcBarrier.GetTag());
	SetAssetTags(Tags);
	CooldownTags.AddTag(TAG_Cooldown_Warden_ArcBarrier.GetTag());
	CooldownGameplayEffectClass = UArenaDuelGE_ArcBarrierCooldown::StaticClass();
}

const FGameplayTagContainer* UArenaDuelGA_ArcBarrier::GetCooldownTags() const { return &CooldownTags; }

void UArenaDuelGA_ArcBarrier::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	AArenaDuelCharacter* Character = ActorInfo ? Cast<AArenaDuelCharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	AArenaDuelPlayerState* PlayerState = ActorInfo ? Cast<AArenaDuelPlayerState>(ActorInfo->OwnerActor.Get()) : nullptr;
	UWorld* World = Character ? Character->GetWorld() : nullptr;
	FVector Forward = Character && Character->GetController() ? Character->GetController()->GetControlRotation().Vector().GetSafeNormal2D() : FVector::ZeroVector;
	if (Forward.IsNearlyZero() && Character) Forward = Character->GetActorForwardVector().GetSafeNormal2D();
	const FVector SpawnLocation = Character ? Character->GetActorLocation() + Forward * 270.0f + FVector::UpVector * 30.0f : FVector::ZeroVector;
	const FRotator SpawnRotation(0.0f, Forward.Rotation().Yaw, 0.0f);
	if (!Character || !PlayerState || !World || Forward.IsNearlyZero()
		|| (Character->HasAuthority() && AArenaDuelArcBarrier::HasBarrierForOwner(World, PlayerState))
		|| !AArenaDuelArcBarrier::IsPlacementClear(World, SpawnLocation, SpawnRotation, Character))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	AArenaDuelArcBarrier* Barrier = nullptr;
	if (Character->HasAuthority())
	{
		FActorSpawnParameters Params;
		Params.Owner = PlayerState;
		Params.Instigator = Character;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::DontSpawnIfColliding;
		Barrier = World->SpawnActor<AArenaDuelArcBarrier>(AArenaDuelArcBarrier::StaticClass(), SpawnLocation, SpawnRotation, Params);
	}
	if ((Character->HasAuthority() && !Barrier) || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		if (Barrier) Barrier->Destroy();
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	if (Barrier) Barrier->SetBarrierOwnerState(PlayerState);
	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}
