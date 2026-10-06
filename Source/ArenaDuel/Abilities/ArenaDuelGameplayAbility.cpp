// Copyright Epic Games, Inc. All Rights Reserved.

#include "ArenaDuelGameplayAbility.h"
#include "../Characters/ArenaDuelCharacter.h"
#include "../Game/ArenaDuelGameState.h"
#include "../Player/ArenaDuelPlayerController.h"
#include "../Player/ArenaDuelPlayerState.h"

UArenaDuelGameplayAbility::UArenaDuelGameplayAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
}

bool UArenaDuelGameplayAbility::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags) || !ActorInfo || !ActorInfo->AbilitySystemComponent.IsValid())
	{
		return false;
	}

	const AArenaDuelPlayerState* PlayerState = Cast<AArenaDuelPlayerState>(ActorInfo->OwnerActor.Get());
	const AArenaDuelCharacter* Character = Cast<AArenaDuelCharacter>(ActorInfo->AvatarActor.Get());
	const AArenaDuelGameState* GameState = Character && Character->GetWorld() ? Character->GetWorld()->GetGameState<AArenaDuelGameState>() : nullptr;
	if (!PlayerState || !Character || Character->IsDead() || !GameState || !GameState->IsRoundInProgress())
	{
		return false;
	}

	if (const AArenaDuelPlayerController* PlayerController = Cast<AArenaDuelPlayerController>(Character->GetController()); PlayerController && PlayerController->IsAdminMenuOpen())
	{
		return false;
	}

	return true;
}
