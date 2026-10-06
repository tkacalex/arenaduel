#include "ArenaDuelGA_PhaseGate.h"
#include "ArenaDuelGameplayTags.h"
#include "ArenaDuelShadowCooldownEffects.h"
#include "ArenaDuelRiftTargeting.h"
#include "ArenaDuelPhaseGateVisual.h"
#include "../Characters/ArenaDuelCharacter.h"
#include "../Game/ArenaDuelGameState.h"
#include "../Player/ArenaDuelPlayerState.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Engine/World.h"

UArenaDuelGA_PhaseGate::UArenaDuelGA_PhaseGate()
{
	FGameplayTagContainer Tags;
	Tags.AddTag(TAG_Ability_Rift_PhaseGate.GetTag());
	SetAssetTags(Tags);
	// A gate cast replaces a pull; a surviving root-motion source must not pull a teleport back.
	CancelAbilitiesWithTag.AddTag(TAG_Ability_Rift_RiftGrapple.GetTag());
	BlockAbilitiesWithTag.AddTag(TAG_Ability_Rift_RiftGrapple.GetTag());
	CooldownTags.AddTag(TAG_Cooldown_Rift_PhaseGate.GetTag());
	CooldownGameplayEffectClass = UArenaDuelGE_PhaseGateCooldown::StaticClass();
}

const FGameplayTagContainer* UArenaDuelGA_PhaseGate::GetCooldownTags() const { return &CooldownTags; }

void UArenaDuelGA_PhaseGate::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	AArenaDuelCharacter* Character = ActorInfo ? Cast<AArenaDuelCharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	if (!Character) { EndAbility(Handle, ActorInfo, ActivationInfo, true, true); return; }
	CastVisuals.Reset();
	if (Character->HasAuthority())
	{
		if (!ArenaDuelRiftTargeting::FindGateDestination(*Character, Destination))
		{
			EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
			return;
		}
		FActorSpawnParameters Params;
		Params.Owner = ActorInfo->OwnerActor.Get();
		Params.Instigator = Character;
		for (const FVector& Point : { Character->GetActorLocation(), Destination })
			CastVisuals.Add(Character->GetWorld()->SpawnActor<AArenaDuelPhaseGateVisual>(Point, FRotator(0, Character->GetControlRotation().Yaw, 0), Params));
	}
	Character->PlayRiftCameraImpulse();
	UAbilityTask_WaitDelay* Windup = UAbilityTask_WaitDelay::WaitDelay(this, 0.2f);
	Windup->OnFinish.AddDynamic(this, &ThisClass::FinishPhase);
	Windup->ReadyForActivation();
}

void UArenaDuelGA_PhaseGate::FinishPhase()
{
	AArenaDuelCharacter* Character = CurrentActorInfo ? Cast<AArenaDuelCharacter>(CurrentActorInfo->AvatarActor.Get()) : nullptr;
	if (!Character) { EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true); return; }
	if (!Character->HasAuthority()) return; // Owning client only predicts cast feedback, never teleport or cooldown.
	const AArenaDuelGameState* State = Character->GetWorld()->GetGameState<AArenaDuelGameState>();
	if (Character->IsDead() || !State || !State->IsRoundInProgress()
		|| !CommitCheck(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo)
		|| !ArenaDuelRiftTargeting::IsGateDestinationSafe(*Character, Destination))
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}
	// Revalidated immediately before teleport; no client-provided destination and no through-wall adjustment.
	const bool bTeleported = Character->TeleportTo(Destination, Character->GetActorRotation(), false, true);
	if (bTeleported) CommitAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo);
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, !bTeleported);
}

void UArenaDuelGA_PhaseGate::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (bWasCancelled) for (auto& Visual : CastVisuals) if (Visual.IsValid() && Visual->HasAuthority()) Visual->Destroy();
	CastVisuals.Reset();
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
