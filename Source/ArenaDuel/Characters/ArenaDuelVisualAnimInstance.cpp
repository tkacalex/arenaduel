#include "ArenaDuelVisualAnimInstance.h"
#include "ArenaDuelCharacter.h"
#include "../Weapons/ArenaDuelWeaponComponent.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSingleNodeInstanceProxy.h"
#include "TwoBoneIK.h"
#include "Animation/AnimationPoseData.h"
#include "Animation/AttributesRuntime.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "ArenaDuelCharacterMovementComponent.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	// First-person support arm. The arm mesh is a forearm and a hand, so the forearm is placed directly:
	// the wrist sits on the gun's support grip and the elbow lies in this direction from it, in camera
	// space (X forward, Y right, Z up). Pointing it back and down keeps the open elbow end off screen.
	TAutoConsoleVariable<float> CVarElbowX(TEXT("ArenaDuel.Arms.LeftElbowX"), -0.45f, TEXT("Support arm, wrist to elbow direction: forward"));
	TAutoConsoleVariable<float> CVarElbowY(TEXT("ArenaDuel.Arms.LeftElbowY"), -0.45f, TEXT("Support arm, wrist to elbow direction: right"));
	TAutoConsoleVariable<float> CVarElbowZ(TEXT("ArenaDuel.Arms.LeftElbowZ"), -0.80f, TEXT("Support arm, wrist to elbow direction: up"));
	TAutoConsoleVariable<float> CVarForearmRoll(TEXT("ArenaDuel.Arms.LeftRoll"), 120.0f, TEXT("Support arm roll around the forearm, degrees"));
	TAutoConsoleVariable<float> CVarWristX(TEXT("ArenaDuel.Arms.LeftWristX"), 0.0f, TEXT("Support wrist offset from the grip point: forward"));
	TAutoConsoleVariable<float> CVarWristY(TEXT("ArenaDuel.Arms.LeftWristY"), -2.0f, TEXT("Support wrist offset from the grip point: right"));
	// Weapon arm: the hand stays on the grip, only the forearm direction is set.
	TAutoConsoleVariable<float> CVarRightElbowX(TEXT("ArenaDuel.Arms.RightElbowX"), -0.50f, TEXT("Weapon arm, wrist to elbow direction: forward"));
	TAutoConsoleVariable<float> CVarRightElbowY(TEXT("ArenaDuel.Arms.RightElbowY"), 0.25f, TEXT("Weapon arm, wrist to elbow direction: right"));
	TAutoConsoleVariable<float> CVarRightElbowZ(TEXT("ArenaDuel.Arms.RightElbowZ"), -0.83f, TEXT("Weapon arm, wrist to elbow direction: up"));
	// Free hand with knife or flashbang, position in camera space.
	TAutoConsoleVariable<float> CVarFreeX(TEXT("ArenaDuel.Arms.FreeHandX"), 28.0f, TEXT("Free hand position: forward of the camera"));
	TAutoConsoleVariable<float> CVarFreeY(TEXT("ArenaDuel.Arms.FreeHandY"), -15.0f, TEXT("Free hand position: right of the camera"));
	TAutoConsoleVariable<float> CVarFreeZ(TEXT("ArenaDuel.Arms.FreeHandZ"), -16.0f, TEXT("Free hand position: above the camera"));
	TAutoConsoleVariable<float> CVarFreeRoll(TEXT("ArenaDuel.Arms.FreeHandRoll"), -80.0f, TEXT("Free hand roll around the forearm, degrees"));
	TAutoConsoleVariable<float> CVarWristZ(TEXT("ArenaDuel.Arms.LeftWristZ"), -1.0f, TEXT("Support wrist offset from the grip point: up"));

	struct FArenaDuelVisualPoseProxy : FAnimSingleNodeInstanceProxy
	{
		using FAnimSingleNodeInstanceProxy::FAnimSingleNodeInstanceProxy;
		float CrouchBlend = 0;
		float SlideBlend = 0;
		float LeftHandIKBlend = 0;
		FVector LeftGripInRightHand = FVector::ZeroVector;
		bool bHasLeftGrip = false;
		bool bAlignHead = false;
		// First-person arms only: place the forearm directly instead of solving the whole arm.
		bool bRigidForearm = false;
		FVector ElbowDirection = FVector::ZeroVector;
		FVector RightElbowDirection = FVector::ZeroVector;
		// World body: the reload plays on the upper body only, on top of whatever the legs are doing.
		const UAnimSequence* ReloadOverlay = nullptr;
		float ReloadTime = 0;
		float ReloadWeight = 0;
		bool bWasReloading = false;
		// World body: aim pose, low ready and view pitch, so an opponent can read the stance.
		bool bWorldBodyPose = false;
		const UAnimSequence* AimOverlay = nullptr;
		float AimTime = 0;
		float AimWeight = 0;
		float LowReadyWeight = 0;
		float LookPitch = 0;
		static constexpr float LowReadyDegrees = 26.0f;
		float ForearmRoll = 0;
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
			bAlignHead = bThirdPersonBody && !Character->IsDead();
			const USkeletalMeshComponent* Arms = Character ? Character->GetFirstPersonArms() : nullptr;
			const UArenaDuelWeaponComponent* Weapon = Character ? Character->GetWeaponComponent() : nullptr;
			const USkeletalMeshComponent* Body = Instance->GetSkelMeshComponent();
			bRigidForearm = Body && Body == Arms && Character && !Character->IsDead();
			if (Character && !Character->IsDead() && Body && (bRigidForearm || bThirdPersonBody) && Weapon)
			{
				FVector GripWorld;
				const int32 RightHandIndex = Body->GetBoneIndex(TEXT("hand_r"));
				const UCameraComponent* Camera = Character->GetFirstPersonCamera();
				const FRotator View = Camera ? Camera->GetComponentRotation() : Character->GetViewRotation();
				const FTransform BodyTransform = Body->GetComponentTransform();
				const bool bOneHanded = Weapon->GetActiveSlot() != EArenaDuelLoadoutSlot::Primary;
				bool bGrip = RightHandIndex != INDEX_NONE && Weapon->GetLeftHandGripWorldLocation(GripWorld, bThirdPersonBody);
				if (!bGrip && bOneHanded && RightHandIndex != INDEX_NONE)
				{
					// Flashbang and knife are held in one hand. The free hand gets a place of its own,
					// apart from the weapon hand, instead of the rifle clip's two-handed pose.
					if (bRigidForearm && Camera) GripWorld = Camera->GetComponentLocation() + View.RotateVector(FVector(CVarFreeX.GetValueOnGameThread(), CVarFreeY.GetValueOnGameThread(), CVarFreeZ.GetValueOnGameThread()));
					// World body, mesh space: X is the character's left, Y forward, Z up from the feet.
					else GripWorld = BodyTransform.TransformPosition(FVector(30.0f, 16.0f, 98.0f));
					bGrip = true;
				}
				if (bRigidForearm)
				{
					if (bGrip && !bOneHanded) GripWorld += View.RotateVector(FVector(CVarWristX.GetValueOnGameThread(), CVarWristY.GetValueOnGameThread(), CVarWristZ.GetValueOnGameThread()));
					const FVector ElbowView(CVarElbowX.GetValueOnGameThread(), CVarElbowY.GetValueOnGameThread(), CVarElbowZ.GetValueOnGameThread());
					const FVector RightElbowView(CVarRightElbowX.GetValueOnGameThread(), CVarRightElbowY.GetValueOnGameThread(), CVarRightElbowZ.GetValueOnGameThread());
					ElbowDirection = BodyTransform.InverseTransformVectorNoScale(View.RotateVector(ElbowView)).GetSafeNormal();
					RightElbowDirection = BodyTransform.InverseTransformVectorNoScale(View.RotateVector(RightElbowView)).GetSafeNormal();
					ForearmRoll = bOneHanded ? CVarFreeRoll.GetValueOnGameThread() : CVarForearmRoll.GetValueOnGameThread();
				}
				if (bGrip)
				{
					LeftGripInRightHand = Body->GetBoneTransform(RightHandIndex).InverseTransformPosition(GripWorld);
					bHasLeftGrip = !LeftGripInRightHand.ContainsNaN();
				}
			}			const UArenaDuelVisualAnimInstance* Visual = Cast<UArenaDuelVisualAnimInstance>(Instance);
			ReloadOverlay = Visual ? Visual->GetReloadClip() : nullptr;
			const bool bReloadingBody = bThirdPersonBody && Weapon && Weapon->IsReloading() && !Character->IsDead() && ReloadOverlay;
			if (bReloadingBody)
			{
				// The clip is stretched to the reload time of the weapon, as the first-person arms do.
				const float Rate = ReloadOverlay->GetPlayLength() / FMath::Max(Weapon->GetCurrentDefinition().ReloadDuration, 0.1f);
				ReloadTime = bWasReloading ? FMath::Min(ReloadTime + FMath::Max(DeltaSeconds, 0.0f) * Rate, ReloadOverlay->GetPlayLength()) : 0.0f;
			}
			bWasReloading = bReloadingBody;
			// What the opponent is doing with the weapon and where they look, read from replicated state.
			bWorldBodyPose = bThirdPersonBody && Character && !Character->IsDead();
			AimOverlay = Visual ? Visual->GetIdleClip() : nullptr;
			const bool bAimingBody = bWorldBodyPose && Weapon && Weapon->IsAiming() && Weapon->GetActiveSlot() == EArenaDuelLoadoutSlot::Primary;
			AimWeight = (bAimingBody ? 1.0f : 0.0f) + (AimWeight - (bAimingBody ? 1.0f : 0.0f)) * Decay;
			if (AimOverlay && AimOverlay->GetPlayLength() > 0.0f) AimTime = FMath::Fmod(AimTime + FMath::Max(DeltaSeconds, 0.0f), AimOverlay->GetPlayLength());
			const float LowTarget = bWorldBodyPose && !bAimingBody && !bReloadingBody ? 1.0f : 0.0f;
			LowReadyWeight = LowTarget + (LowReadyWeight - LowTarget) * Decay;
			const float PitchTarget = bWorldBodyPose ? FMath::Clamp(FRotator::NormalizeAxis(Character->GetBaseAimRotation().Pitch), -80.0f, 80.0f) : 0.0f;
			LookPitch = PitchTarget + (LookPitch - PitchTarget) * FMath::Exp(-20.0f * FMath::Max(DeltaSeconds, 0.0f));
			ReloadWeight = (bReloadingBody ? 1.0f : 0.0f) + (ReloadWeight - (bReloadingBody ? 1.0f : 0.0f)) * Decay;
			const float TargetBlend = bHasLeftGrip && Weapon && !Weapon->IsReloading() ? 1.0f : 0.0f;
			LeftHandIKBlend = TargetBlend + (LeftHandIKBlend - TargetBlend) * Decay;
		}
		virtual bool Evaluate(FPoseContext& Output) override
		{
			const bool bResult = FAnimSingleNodeInstanceProxy::Evaluate(Output);
			if (bWorldBodyPose)
			{
				const FBoneContainer& OverlayBones = Output.Pose.GetBoneContainer();
				const auto FindBone = [&OverlayBones](const TCHAR* Name)
				{
					const int32 SkeletonIndex = OverlayBones.GetReferenceSkeleton().FindBoneIndex(Name);
					return SkeletonIndex == INDEX_NONE ? FCompactPoseBoneIndex(INDEX_NONE) : OverlayBones.MakeCompactPoseIndex(FMeshPoseBoneIndex(SkeletonIndex));
				};
				const FCompactPoseBoneIndex ChestIndex = FindBone(TEXT("spine_02"));
				// Plays a clip on everything from the chest up; hips and legs keep the locomotion clip.
				const auto LayerUpperBody = [&Output, &OverlayBones, ChestIndex](const UAnimSequence* Clip, float Time, float Weight)
				{
					if (!Clip || Weight <= KINDA_SMALL_NUMBER || ChestIndex.GetInt() == INDEX_NONE) return;
					FCompactPose ClipPose;
					ClipPose.SetBoneContainer(&OverlayBones);
					FBlendedCurve ClipCurve;
					ClipCurve.InitFrom(Output.Curve);
					UE::Anim::FStackAttributeContainer ClipAttributes;
					FAnimationPoseData ClipData(ClipPose, ClipCurve, ClipAttributes);
					Clip->GetAnimationPose(ClipData, FAnimExtractContext(static_cast<double>(Time), false));
					for (const FCompactPoseBoneIndex BoneIndex : Output.Pose.ForEachBoneIndex())
					{
						if (BoneIndex == ChestIndex || OverlayBones.BoneIsChildOf(BoneIndex, ChestIndex)) Output.Pose[BoneIndex].Blend(Output.Pose[BoneIndex], ClipPose[BoneIndex], Weight);
					}
				};
				// Aiming raises the rifle to the eye whatever the legs do; not aiming leaves the carry pose of the clip.
				LayerUpperBody(AimOverlay, AimTime, AimWeight);
				LayerUpperBody(ReloadOverlay, ReloadTime, ReloadWeight);
				// Mesh space: X is the character's left, Y forward, Z up. A turn about X by a positive angle tips forward up.
				const FCompactPoseBoneIndex LowSpineIndex = FindBone(TEXT("spine_03")), HighSpineIndex = FindBone(TEXT("spine_05")), WeaponArmIndex = FindBone(TEXT("upperarm_r"));
				const bool bLook = FMath::Abs(LookPitch) > 0.5f && LowSpineIndex.GetInt() != INDEX_NONE && HighSpineIndex.GetInt() != INDEX_NONE;
				const bool bLower = LowReadyWeight > KINDA_SMALL_NUMBER && WeaponArmIndex.GetInt() != INDEX_NONE;
				if (bLook || bLower)
				{
					FCSPose<FCompactPose> LookPose;
					LookPose.InitPose(Output.Pose);
					const auto Turn = [&LookPose](FCompactPoseBoneIndex BoneIndex, float Degrees)
					{
						FTransform Bone = LookPose.GetComponentSpaceTransform(BoneIndex);
						Bone.SetRotation(FQuat(FVector::XAxisVector, FMath::DegreesToRadians(Degrees)) * Bone.GetRotation());
						if (!Bone.ContainsNaN()) LookPose.SetComponentSpaceTransform(BoneIndex, Bone);
					};
					if (bLook)
					{
						// The view pitch is spread over two spine joints, so chest, arms, weapon and head all follow the look.
						Turn(LowSpineIndex, LookPitch * 0.5f);
						Turn(HighSpineIndex, LookPitch * 0.5f);
					}
					// Not aiming: the weapon arm drops to a low ready. The support hand follows through its grip target.
					if (bLower) Turn(WeaponArmIndex, -LowReadyDegrees * LowReadyWeight);
					FCSPose<FCompactPose>::ConvertComponentPosesToLocalPoses(MoveTemp(LookPose), Output.Pose);
				}
			}
			// Peek fairness: the rifle clips lean the head ahead of the capsule, while the camera sits on
			// the capsule axis. Sliding the world body back so the head stays over that axis means an
			// opponent can see and hit a head only where its owner's camera can already see out.
			if (bAlignHead)
			{
				const FBoneContainer& AlignBones = Output.Pose.GetBoneContainer();
				const int32 HeadSkeletonIndex = AlignBones.GetReferenceSkeleton().FindBoneIndex(TEXT("head"));
				const FCompactPoseBoneIndex HeadIndex = HeadSkeletonIndex == INDEX_NONE ? FCompactPoseBoneIndex(INDEX_NONE) : AlignBones.MakeCompactPoseIndex(FMeshPoseBoneIndex(HeadSkeletonIndex));
				if (HeadIndex.GetInt() != INDEX_NONE)
				{
					FCSPose<FCompactPose> AlignPose;
					AlignPose.InitPose(Output.Pose);
					const FVector Head = AlignPose.GetComponentSpaceTransform(HeadIndex).GetLocation();
					if (!Head.ContainsNaN()) Output.Pose[FCompactPoseBoneIndex(0)].AddToTranslation(FVector(-Head.X, -Head.Y, 0.0f));
				}
			}
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
			// The right hand drives the gun. The support target is stored in that
			// hand's space so both hands use the same evaluated frame without lag.
			const bool bSupportHand = bHasLeftGrip && LeftHandIKBlend > KINDA_SMALL_NUMBER;
			if (bSupportHand || bRigidForearm)
			{
				const FBoneContainer& Bones = Output.Pose.GetBoneContainer();
				const FReferenceSkeleton& RefSkeleton = Bones.GetReferenceSkeleton();
				const auto Find = [&Bones, &RefSkeleton](const TCHAR* Name)
				{
					const int32 SkeletonIndex = RefSkeleton.FindBoneIndex(Name);
					return SkeletonIndex == INDEX_NONE ? FCompactPoseBoneIndex(INDEX_NONE) : Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(SkeletonIndex));
				};
				const FCompactPoseBoneIndex UpperIndex = Find(TEXT("upperarm_l")), LowerIndex = Find(TEXT("lowerarm_l")), HandIndex = Find(TEXT("hand_l"));
				const FCompactPoseBoneIndex RightUpperIndex = Find(TEXT("upperarm_r")), RightLowerIndex = Find(TEXT("lowerarm_r")), RightIndex = Find(TEXT("hand_r"));
				FCSPose<FCompactPose> ComponentPose;
				ComponentPose.InitPose(Output.Pose);
				bool bChanged = false;
				// Moves an arm as one rigid piece so that its wrist is at Wrist and its elbow lies against ToHand.
				const auto PlaceArm = [&ComponentPose, &bChanged](FCompactPoseBoneIndex Upper, FCompactPoseBoneIndex Lower, FCompactPoseBoneIndex Hand, const FVector& Wrist, const FVector& ToHand, float RollDegrees, bool bTurnHand, float Blend)
				{
					const FTransform OldUpper = ComponentPose.GetComponentSpaceTransform(Upper);
					const FTransform OldLower = ComponentPose.GetComponentSpaceTransform(Lower);
					const FTransform OldHand = ComponentPose.GetComponentSpaceTransform(Hand);
					const FVector Forearm = OldHand.GetLocation() - OldLower.GetLocation();
					if (Forearm.Size() <= KINDA_SMALL_NUMBER || ToHand.IsNearlyZero() || Wrist.ContainsNaN()) return;
					const FQuat Swing = FQuat(ToHand, FMath::DegreesToRadians(RollDegrees)) * FQuat::FindBetweenNormals(Forearm.GetSafeNormal(), ToHand);
					FTransform NewUpper = OldUpper, NewLower = OldLower, NewHand = OldHand;
					NewLower.SetRotation(Swing * OldLower.GetRotation());
					NewLower.SetLocation(Wrist - ToHand * Forearm.Size());
					// The upper arm follows rigidly, so no skin is stretched between it and the moved forearm.
					NewUpper.SetRotation(Swing * OldUpper.GetRotation());
					NewUpper.SetLocation(NewLower.GetLocation() + Swing.RotateVector(OldUpper.GetLocation() - OldLower.GetLocation()));
					NewHand.SetLocation(Wrist);
					if (bTurnHand) NewHand.SetRotation(Swing * OldHand.GetRotation());
					FTransform BlendedUpper, BlendedLower, BlendedHand;
					BlendedUpper.Blend(OldUpper, NewUpper, Blend);
					BlendedLower.Blend(OldLower, NewLower, Blend);
					BlendedHand.Blend(OldHand, NewHand, Blend);
					if (BlendedUpper.ContainsNaN() || BlendedLower.ContainsNaN() || BlendedHand.ContainsNaN()) return;
					ComponentPose.SetComponentSpaceTransform(Upper, BlendedUpper);
					ComponentPose.SetComponentSpaceTransform(Lower, BlendedLower);
					ComponentPose.SetComponentSpaceTransform(Hand, BlendedHand);
					bChanged = true;
				};
				const bool bLeftValid = UpperIndex.GetInt() != INDEX_NONE && LowerIndex.GetInt() != INDEX_NONE && HandIndex.GetInt() != INDEX_NONE;
				const bool bRightValid = RightUpperIndex.GetInt() != INDEX_NONE && RightLowerIndex.GetInt() != INDEX_NONE && RightIndex.GetInt() != INDEX_NONE;
				// The support target is read before the right forearm is turned; the right hand itself stays put.
				const FVector Effector = RightIndex.GetInt() != INDEX_NONE ? ComponentPose.GetComponentSpaceTransform(RightIndex).TransformPosition(LeftGripInRightHand) : FVector::ZeroVector;
				if (bRigidForearm && bRightValid)
				{
					// The weapon hand keeps its place and grip; only the forearm behind it is turned.
					PlaceArm(RightUpperIndex, RightLowerIndex, RightIndex, ComponentPose.GetComponentSpaceTransform(RightIndex).GetLocation(), -RightElbowDirection, 0.0f, false, 1.0f);
				}
				if (bSupportHand && bLeftValid && RightIndex.GetInt() != INDEX_NONE && !Effector.ContainsNaN())
				{
					if (bRigidForearm)
					{
						PlaceArm(UpperIndex, LowerIndex, HandIndex, Effector, -ElbowDirection, ForearmRoll, true, LeftHandIKBlend);
					}
					else
					{
						FTransform Upper = ComponentPose.GetComponentSpaceTransform(UpperIndex);
						FTransform Lower = ComponentPose.GetComponentSpaceTransform(LowerIndex);
						FTransform Hand = ComponentPose.GetComponentSpaceTransform(HandIndex);
						const FTransform OriginalUpper = Upper, OriginalLower = Lower, OriginalHand = Hand;
						const FVector JointTarget = Lower.GetLocation() + (Lower.GetLocation() - Upper.GetLocation()).GetSafeNormal() * 30.0f;
						AnimationCore::SolveTwoBoneIK(Upper, Lower, Hand, JointTarget, Effector, false, 1.0, 1.0);
						if (!JointTarget.ContainsNaN() && !Upper.ContainsNaN() && !Lower.ContainsNaN() && !Hand.ContainsNaN())
						{
							FTransform BlendedUpper, BlendedLower, BlendedHand;
							BlendedUpper.Blend(OriginalUpper, Upper, LeftHandIKBlend);
							BlendedLower.Blend(OriginalLower, Lower, LeftHandIKBlend);
							BlendedHand.Blend(OriginalHand, Hand, LeftHandIKBlend);
							ComponentPose.SetComponentSpaceTransform(UpperIndex, BlendedUpper);
							ComponentPose.SetComponentSpaceTransform(LowerIndex, BlendedLower);
							ComponentPose.SetComponentSpaceTransform(HandIndex, BlendedHand);
							bChanged = true;
						}
					}
				}
				if (bChanged) FCSPose<FCompactPose>::ConvertComponentPosesToLocalPoses(MoveTemp(ComponentPose), Output.Pose);
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
	static ConstructorHelpers::FObjectFinder<UAnimSequence> WalkBwd(TEXT("/Game/Characters/Mannequins/Anims/Rifle/Walk/MF_Rifle_Walk_Bwd"));
	static ConstructorHelpers::FObjectFinder<UAnimSequence> WalkLeft(TEXT("/Game/Characters/Mannequins/Anims/Rifle/Walk/MF_Rifle_Walk_Left"));
	static ConstructorHelpers::FObjectFinder<UAnimSequence> WalkRight(TEXT("/Game/Characters/Mannequins/Anims/Rifle/Walk/MF_Rifle_Walk_Right"));
	static ConstructorHelpers::FObjectFinder<UAnimSequence> RunBwd(TEXT("/Game/Characters/Mannequins/Anims/Rifle/Jog/MF_Rifle_Jog_Bwd"));
	static ConstructorHelpers::FObjectFinder<UAnimSequence> RunLeft(TEXT("/Game/Characters/Mannequins/Anims/Rifle/Jog/MF_Rifle_Jog_Left"));
	static ConstructorHelpers::FObjectFinder<UAnimSequence> RunRight(TEXT("/Game/Characters/Mannequins/Anims/Rifle/Jog/MF_Rifle_Jog_Right"));
	WalkClips[0] = Walk; WalkClips[1] = WalkBwd.Object; WalkClips[2] = WalkLeft.Object; WalkClips[3] = WalkRight.Object;
	RunClips[0] = Run; RunClips[1] = RunBwd.Object; RunClips[2] = RunLeft.Object; RunClips[3] = RunRight.Object;
	// A missing directional clip falls back to the forward clip.
	for (int32 Direction = 1; Direction < 4; ++Direction)
	{
		if (!WalkClips[Direction]) WalkClips[Direction] = Walk;
		if (!RunClips[Direction]) RunClips[Direction] = Run;
	}
	SetRootMotionMode(ERootMotionMode::IgnoreRootMotion);
}

void UArenaDuelVisualAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	const auto* Character = Cast<AArenaDuelCharacter>(GetOwningActor());
	// Death hands the world body to ragdoll physics. Clip selection must never restart on a corpse.
	if (!Character || Character->IsDead()) { SetPlaying(false); return; }
	Super::NativeUpdateAnimation(DeltaSeconds);	const bool bFirstPerson = GetSkelMeshComponent() == Character->GetFirstPersonArms();
	const float Speed = Character->GetVelocity().Size2D();
	UAnimSequence* Desired = Idle;
	bool bLocomotion = false;
	bool bRunning = false;
	if (!bFirstPerson && Character->GetArenaDuelMovementComponent() && Character->GetArenaDuelMovementComponent()->IsSliding()) Desired = Idle;
	// Only the first-person arms switch to the reload clip. The world body keeps its locomotion clip
	// and gets the reload layered onto the upper body in the proxy.
	else if (bFirstPerson && Character->GetWeaponComponent() && Character->GetWeaponComponent()->IsReloading()) Desired = Reload;
	else if (!bFirstPerson)
	{
		// A hop touches the ground for a frame or two. The fall clip is kept until the feet have really settled,
		// so chained hops do not flick to the run clip and back.
		GroundedSeconds = Character->GetCharacterMovement()->IsFalling() ? 0.0f : GroundedSeconds + DeltaSeconds;
		if (GroundedSeconds < 0.1f) Desired = Fall;
		else if (Speed > 10.0f)
		{
			// Pick the clip that matches the travel direction relative to the facing. The current
			// direction gets a small bonus so diagonal movement does not flicker between two clips.
			const FVector Local = Character->GetActorRotation().UnrotateVector(Character->GetVelocity());
			const float Scores[4] = { static_cast<float>(Local.X), static_cast<float>(-Local.X), static_cast<float>(-Local.Y), static_cast<float>(Local.Y) };
			int32 Best = MoveDirection;
			for (int32 Direction = 0; Direction < 4; ++Direction)
			{
				if (Scores[Direction] > Scores[Best] + (Best == MoveDirection ? 0.15f * Speed : 0.0f)) Best = Direction;
			}
			MoveDirection = Best;
			bLocomotion = true;
			bRunning = Speed > 380.0f;
			Desired = bRunning ? RunClips[MoveDirection].Get() : WalkClips[MoveDirection].Get();
		}
	}
	if (Desired && GetCurrentAsset() != Desired)
	{
		// Switching between locomotion clips keeps the stride phase so a direction change does not restart the step.
		const UAnimSequenceBase* Previous = Cast<UAnimSequenceBase>(GetCurrentAsset());
		const bool bWasLocomotion = Previous && (WalkClips[0] == Previous || WalkClips[1] == Previous || WalkClips[2] == Previous || WalkClips[3] == Previous
			|| RunClips[0] == Previous || RunClips[1] == Previous || RunClips[2] == Previous || RunClips[3] == Previous);
		const float Phase = bWasLocomotion && Previous->GetPlayLength() > 0.0f ? FMath::Frac(GetCurrentTime() / Previous->GetPlayLength()) : 0.0f;
		SetAnimationAsset(Desired, Desired != Reload);
		if (bLocomotion && bWasLocomotion) SetPosition(Phase * Desired->GetPlayLength(), false);
	}
	SetPlaying(true);	// Slower stride for crouch, animation only. No movement tuning is changed.
	const float ReloadRate = Reload && Character->GetWeaponComponent() ? Reload->GetPlayLength() / FMath::Max(Character->GetWeaponComponent()->GetCurrentDefinition().ReloadDuration, 0.1f) : 1.0f;
	SetPlayRate(Desired == Reload ? ReloadRate : bLocomotion ? FMath::Clamp(Speed / (bRunning ? 600.0f : 200.0f), 0.4f, 1.8f) : 1.0f);
}
