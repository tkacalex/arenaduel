#include "ArenaDuelVisualAnimInstance.h"
#include "ArenaDuelCharacter.h"
#include "../Weapons/ArenaDuelWeaponComponent.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSingleNodeInstanceProxy.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "ArenaDuelCharacterMovementComponent.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	struct FArenaDuelVisualPoseProxy : FAnimSingleNodeInstanceProxy
	{
		using FAnimSingleNodeInstanceProxy::FAnimSingleNodeInstanceProxy;
		float CrouchBlend = 0;
		float SlideBlend = 0;
		float LeftHandIKBlend = 0;
		FVector LeftGripInRightHand = FVector::ZeroVector;
		FVector LeftElbowToHand = FVector::ZeroVector;
		FVector RightElbowToHand = FVector::ZeroVector;
		bool bHasLeftGrip = false;
		bool bFirstPersonArms = false;
		virtual void PreUpdate(UAnimInstance* Instance, float DeltaSeconds) override
		{
			FAnimSingleNodeInstanceProxy::PreUpdate(Instance, DeltaSeconds);
			const auto* Character = Cast<AArenaDuelCharacter>(Instance->GetOwningActor());
			const bool bThirdPersonBody = Character && Instance->GetSkelMeshComponent() == Character->GetMesh();
			const bool bSlidingBody = bThirdPersonBody && Character->GetArenaDuelMovementComponent() && Character->GetArenaDuelMovementComponent()->IsSliding();
			const bool bCrouchingBody = bThirdPersonBody && (Character->bIsCrouched || bSlidingBody);
			const float Decay = FMath::Exp(-12.0f * FMath::Max(DeltaSeconds, 0.0f));
			CrouchBlend = (bCrouchingBody ? 1.0f : 0.0f) + (CrouchBlend - (bCrouchingBody ? 1.0f : 0.0f)) * Decay;
			SlideBlend = (bSlidingBody ? 1.0f : 0.0f) + (SlideBlend - (bSlidingBody ? 1.0f : 0.0f)) * Decay;
			bHasLeftGrip = false;
			const USkeletalMeshComponent* Arms = Character ? Character->GetFirstPersonArms() : nullptr;
			const UArenaDuelWeaponComponent* Weapon = Character ? Character->GetWeaponComponent() : nullptr;
			if (Character && !Character->IsDead() && Arms && Instance->GetSkelMeshComponent() == Arms && Weapon)
			{
				FVector GripWorld;
				const int32 RightHandIndex = Arms->GetBoneIndex(TEXT("hand_r"));
				if (RightHandIndex != INDEX_NONE && Weapon->GetLeftHandGripWorldLocation(GripWorld))
				{
					LeftGripInRightHand = Arms->GetBoneTransform(RightHandIndex).InverseTransformPosition(GripWorld);
					bHasLeftGrip = !LeftGripInRightHand.ContainsNaN();
				}
			}
			// The viewmodel only has forearms and hands, so their open elbow ends must stay below
			// the frame. Both forearms are aimed along fixed camera-space directions (X forward,
			// Y right, Z up) that put each elbow lower, wider and nearer the eye than its hand.
			bFirstPersonArms = false;
			if (Character && Arms && Instance->GetSkelMeshComponent() == Arms && Character->GetFirstPersonCamera())
			{
				const FTransform& View = Character->GetFirstPersonCamera()->GetComponentTransform();
				const FTransform& ArmsWorld = Arms->GetComponentTransform();
				LeftElbowToHand = ArmsWorld.InverseTransformVectorNoScale(View.TransformVectorNoScale(FVector(0.38f, 0.32f, 0.87f).GetSafeNormal()));
				RightElbowToHand = ArmsWorld.InverseTransformVectorNoScale(View.TransformVectorNoScale(FVector(0.38f, -0.28f, 0.88f).GetSafeNormal()));
				bFirstPersonArms = !LeftElbowToHand.ContainsNaN() && !RightElbowToHand.ContainsNaN();
			}
			const float TargetBlend = bHasLeftGrip && Weapon && !Weapon->IsReloading() ? 1.0f : 0.0f;
			LeftHandIKBlend = TargetBlend + (LeftHandIKBlend - TargetBlend) * Decay;
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
					const FCompactPoseBoneIndex Index = Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(SkeletonIndex));
					if (Index.GetInt() == INDEX_NONE) continue;
					auto& Transform = Output.Pose[Index];
					if (FName(Name) == TEXT("pelvis")) Transform.AddToTranslation(FVector(0,0,-25 * CrouchBlend - 8 * SlideBlend));
					else
					{
						const bool bThigh = FString(Name).StartsWith(TEXT("thigh"));
						float Angle = (bThigh ? -35.0f : 65.0f) * CrouchBlend;
						if (FName(Name) == TEXT("thigh_l")) Angle -= 22.0f * SlideBlend;
						else if (FName(Name) == TEXT("thigh_r")) Angle += 18.0f * SlideBlend;
						else if (FName(Name) == TEXT("calf_l")) Angle -= 28.0f * SlideBlend;
						else if (FName(Name) == TEXT("calf_r")) Angle += 8.0f * SlideBlend;
						Transform.SetRotation(Transform.GetRotation() * FQuat(FVector::RightVector, FMath::DegreesToRadians(Angle)));
					}
				}
			}
			// The right hand drives the gun and keeps its animated transform. The support hand
			// target is stored in that hand's space so both hands use the same evaluated frame.
			if (bFirstPersonArms)
			{
				const FBoneContainer& Bones = Output.Pose.GetBoneContainer();
				const FReferenceSkeleton& RefSkeleton = Bones.GetReferenceSkeleton();
				auto Compact = [&](const TCHAR* Name)
				{
					const int32 SkeletonIndex = RefSkeleton.FindBoneIndex(Name);
					return SkeletonIndex == INDEX_NONE ? FCompactPoseBoneIndex(INDEX_NONE) : Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(SkeletonIndex));
				};
				const FCompactPoseBoneIndex LeftLowerIndex = Compact(TEXT("lowerarm_l"));
				const FCompactPoseBoneIndex LeftHandIndex = Compact(TEXT("hand_l"));
				const FCompactPoseBoneIndex RightLowerIndex = Compact(TEXT("lowerarm_r"));
				const FCompactPoseBoneIndex RightHandIndex = Compact(TEXT("hand_r"));
				if (LeftLowerIndex.GetInt() != INDEX_NONE && LeftHandIndex.GetInt() != INDEX_NONE && RightLowerIndex.GetInt() != INDEX_NONE && RightHandIndex.GetInt() != INDEX_NONE)
				{
					FCSPose<FCompactPose> ComponentPose;
					ComponentPose.InitPose(Output.Pose);
					const FTransform RightHand = ComponentPose.GetComponentSpaceTransform(RightHandIndex);
					FTransform LeftLower = ComponentPose.GetComponentSpaceTransform(LeftLowerIndex);
					FTransform LeftHand = ComponentPose.GetComponentSpaceTransform(LeftHandIndex);
					FTransform RightLower = ComponentPose.GetComponentSpaceTransform(RightLowerIndex);
					const FVector AnimatedLeftHand = LeftHand.GetLocation();
					const FVector LeftTarget = bHasLeftGrip ? FMath::Lerp(AnimatedLeftHand, RightHand.TransformPosition(LeftGripInRightHand), LeftHandIKBlend) : AnimatedLeftHand;
					// Swings a forearm about its hand so it keeps its length and points along ElbowToHand.
					auto Redirect = [](FTransform& Lower, FTransform* Hand, const FVector& AnimatedHand, const FVector& HandTarget, const FVector& ElbowToHand)
					{
						const FVector Forearm = AnimatedHand - Lower.GetLocation();
						const double Length = Forearm.Size();
						if (Length < UE_KINDA_SMALL_NUMBER) return;
						const FQuat Delta = FQuat::FindBetweenNormals(Forearm / Length, ElbowToHand);
						Lower.SetRotation(Delta * Lower.GetRotation());
						Lower.SetLocation(HandTarget - ElbowToHand * Length);
						if (Hand)
						{
							Hand->SetRotation(Delta * Hand->GetRotation());
							Hand->SetLocation(HandTarget);
						}
					};
					Redirect(LeftLower, &LeftHand, AnimatedLeftHand, LeftTarget, LeftElbowToHand);
					Redirect(RightLower, nullptr, RightHand.GetLocation(), RightHand.GetLocation(), RightElbowToHand);
					if (!LeftLower.ContainsNaN() && !LeftHand.ContainsNaN() && !RightLower.ContainsNaN())
					{
						ComponentPose.SetComponentSpaceTransform(LeftLowerIndex, LeftLower);
						ComponentPose.SetComponentSpaceTransform(LeftHandIndex, LeftHand);
						ComponentPose.SetComponentSpaceTransform(RightLowerIndex, RightLower);
						ComponentPose.SetComponentSpaceTransform(RightHandIndex, RightHand);
						FCSPose<FCompactPose>::ConvertComponentPosesToLocalPoses(MoveTemp(ComponentPose), Output.Pose);
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
	const auto* Character = Cast<AArenaDuelCharacter>(GetOwningActor());
	// Death is a latched presentation state. Do not let the normal single-node
	// selection restart locomotion on a corpse between timer updates.
	if (!Character || Character->IsDead()) { SetPlaying(false); return; }
	Super::NativeUpdateAnimation(DeltaSeconds);
	const bool bFirstPerson = GetSkelMeshComponent() == Character->GetFirstPersonArms();
	const float Speed = Character->GetVelocity().Size2D();
	UAnimSequence* Desired = Idle;
	if (!bFirstPerson && Character->GetArenaDuelMovementComponent() && Character->GetArenaDuelMovementComponent()->IsSliding()) Desired = Idle;
	else if (Character->GetWeaponComponent() && Character->GetWeaponComponent()->IsReloading()) Desired = Reload;
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
