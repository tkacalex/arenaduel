#include "ArenaDuelGA_RiftGrapple.h"
#include "ArenaDuelGameplayTags.h"
#include "ArenaDuelShadowCooldownEffects.h"
#include "ArenaDuelRiftTargeting.h"
#include "../Characters/ArenaDuelCharacter.h"
#include "../Characters/ArenaDuelCharacterMovementComponent.h"
#include "Abilities/Tasks/AbilityTask_ApplyRootMotionMoveToForce.h"
#include "GameFramework/RootMotionSource.h"
#include "DrawDebugHelpers.h"

UArenaDuelGA_RiftGrapple::UArenaDuelGA_RiftGrapple()
{
	FGameplayTagContainer Tags;
	Tags.AddTag(TAG_Ability_Rift_RiftGrapple.GetTag());
	SetAssetTags(Tags);
	CooldownTags.AddTag(TAG_Cooldown_Rift_RiftGrapple.GetTag());
	CooldownGameplayEffectClass = UArenaDuelGE_RiftGrappleCooldown::StaticClass();
}

const FGameplayTagContainer* UArenaDuelGA_RiftGrapple::GetCooldownTags() const { return &CooldownTags; }

void UArenaDuelGA_RiftGrapple::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	AArenaDuelCharacter* Character = ActorInfo ? Cast<AArenaDuelCharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	FVector Destination, Anchor;
	if (!Character || !Character->GetArenaDuelMovementComponent() || !ArenaDuelRiftTargeting::FindGrappleDestination(*Character, Destination, Anchor)
		|| !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	UArenaDuelCharacterMovementComponent* Movement = Character->GetArenaDuelMovementComponent();
	const float SpeedCap = FMath::Max(1.0f, Movement->GlobalMomentumCap);
	// Distance-derived time respects the existing momentum cap, including long anchors.
	const float Duration = FMath::Max(0.25f, FVector::Dist(Character->GetActorLocation(), Destination) / SpeedCap);
	Movement->SetMovementMode(MOVE_Falling);
	UAbilityTask_ApplyRootMotionMoveToForce* Pull = UAbilityTask_ApplyRootMotionMoveToForce::ApplyRootMotionMoveToForce(
		this, TEXT("RiftPull"), Destination, Duration, true, MOVE_Flying, true, nullptr,
		ERootMotionFinishVelocityMode::ClampVelocity, FVector::ZeroVector, SpeedCap);
	Pull->OnTimedOut.AddDynamic(this, &ThisClass::FinishPull);
	Pull->OnTimedOutAndDestinationReached.AddDynamic(this, &ThisClass::FinishPull);
	Character->PlayRiftCameraImpulse();
	if (Character->IsLocallyControlled()) DrawDebugLine(Character->GetWorld(), Character->GetPawnViewLocation(), Anchor, FColor(175, 80, 255), false, Duration, 0, 2.0f);
	Pull->ReadyForActivation();
}

void UArenaDuelGA_RiftGrapple::FinishPull()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}
