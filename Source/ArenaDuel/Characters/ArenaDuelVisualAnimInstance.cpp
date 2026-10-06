#include "ArenaDuelVisualAnimInstance.h"
#include "ArenaDuelCharacter.h"
#include "../Weapons/ArenaDuelWeaponComponent.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSingleNodeInstanceProxy.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	struct FArenaDuelVisualPoseProxy : FAnimSingleNodeInstanceProxy
	{
		using FAnimSingleNodeInstanceProxy::FAnimSingleNodeInstanceProxy;
		float CrouchBlend = 0;
		virtual void PreUpdate(UAnimInstance* Instance, float DeltaSeconds) override
		{
			FAnimSingleNodeInstanceProxy::PreUpdate(Instance, DeltaSeconds);
			const auto* Character = Cast<AArenaDuelCharacter>(Instance->GetOwningActor());
			const bool bCrouchingBody = Character && Character->bIsCrouched && Instance->GetSkelMeshComponent() == Character->GetMesh();
			CrouchBlend = FMath::FInterpTo(CrouchBlend, bCrouchingBody ? 1.0f : 0.0f, DeltaSeconds, 12.0f);
		}
		virtual bool Evaluate(FPoseContext& Output) override
		{
			const bool bResult = FAnimSingleNodeInstanceProxy::Evaluate(Output);
			// Small visual crouch overlay, since the installed pack has no crouch clips.
			// This never moves the capsule, changes hitboxes, or produces root motion.
			if (CrouchBlend > KINDA_SMALL_NUMBER)
			{
				const auto& Bones = Output.Pose.GetBoneContainer();
				for (const TCHAR* Name : {TEXT("pelvis"),TEXT("thigh_l"),TEXT("thigh_r"),TEXT("calf_l"),TEXT("calf_r")})
				{
					const int32 SkeletonIndex = Bones.GetReferenceSkeleton().FindBoneIndex(Name);
					if (SkeletonIndex == INDEX_NONE) continue;
					const FCompactPoseBoneIndex Index = Bones.GetCompactPoseIndexFromSkeletonIndex(SkeletonIndex);
					if (Index.GetInt() == INDEX_NONE) continue;
					auto& Transform = Output.Pose[Index];
					if (FName(Name) == TEXT("pelvis")) Transform.AddToTranslation(FVector(0,0,-25 * CrouchBlend));
					else
					{
						const bool bThigh = FString(Name).StartsWith(TEXT("thigh"));
						Transform.SetRotation(Transform.GetRotation() * FQuat(FVector::RightVector, FMath::DegreesToRadians((bThigh ? -35.0f : 65.0f) * CrouchBlend)));
					}
				}
			}
			return bResult;
		}
	};
}

FAnimInstanceProxy* UArenaDuelVisualAnimInstance::CreateAnimInstanceProxy()
{
	return new FArenaDuelVisualPoseProxy(this);
}

UArenaDuelVisualAnimInstance::UArenaDuelVisualAnimInstance()
{
	static ConstructorHelpers::FObjectFinder<UAnimSequence> IdleAsset(TEXT("/Game/Characters/Mannequins/Anims/Rifle/MF_Rifle_Idle_ADS"));
	static ConstructorHelpers::FObjectFinder<UAnimSequence> WalkAsset(TEXT("/Game/Characters/Mannequins/Anims/Rifle/Walk/MF_Rifle_Walk_Fwd"));
	static ConstructorHelpers::FObjectFinder<UAnimSequence> RunAsset(TEXT("/Game/Characters/Mannequins/Anims/Rifle/Jog/MF_Rifle_Jog_Fwd"));
	static ConstructorHelpers::FObjectFinder<UAnimSequence> FallAsset(TEXT("/Game/Characters/Mannequins/Anims/Rifle/Jump/MM_Rifle_Jump_Fall_Loop"));
	static ConstructorHelpers::FObjectFinder<UAnimSequence> ReloadAsset(TEXT("/Game/Characters/Mannequins/Anims/Rifle/MM_Rifle_Reload"));
	Idle = IdleAsset.Object; Walk = WalkAsset.Object; Run = RunAsset.Object; Fall = FallAsset.Object; Reload = ReloadAsset.Object;
	SetRootMotionMode(ERootMotionMode::IgnoreRootMotion);
}

void UArenaDuelVisualAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);
	const auto* Character = Cast<AArenaDuelCharacter>(GetOwningActor());
	if (!Character || Character->IsDead()) { SetPlaying(false); return; }
	const bool bFirstPerson = GetSkelMeshComponent() == Character->GetFirstPersonArms();
	const float Speed = Character->GetVelocity().Size2D();
	UAnimSequence* Desired = Idle;
	if (Character->GetWeaponComponent() && Character->GetWeaponComponent()->IsReloading()) Desired = Reload;
	else if (!bFirstPerson)
	{
		if (Character->GetCharacterMovement()->IsFalling()) Desired = Fall;
		else if (Speed > 380.0f) Desired = Run;
		else if (Speed > 10.0f) Desired = Walk;
	}
	if (Desired && GetCurrentAsset() != Desired) SetAnimationAsset(Desired, Desired != Reload);
	SetPlaying(true);
	// Slower stride for crouch, animation only. No movement tuning is changed.
	const float ReloadRate = Reload && Character->GetWeaponComponent() ? Reload->GetPlayLength() / FMath::Max(Character->GetWeaponComponent()->GetCurrentDefinition().ReloadDuration, 0.1f) : 1.0f;
	SetPlayRate(Desired == Reload ? ReloadRate : Desired == Walk || Desired == Run ? FMath::Clamp(Speed / (Desired == Run ? 600.0f : 200.0f), 0.4f, 1.8f) : 1.0f);
}
