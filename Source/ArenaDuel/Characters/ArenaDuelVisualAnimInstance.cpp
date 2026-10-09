#include "ArenaDuelVisualAnimInstance.h"
#include "ArenaDuelCharacter.h"
#include "../Weapons/ArenaDuelWeaponComponent.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSingleNodeInstanceProxy.h"
#include "TwoBoneIK.h"
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
	TAutoConsoleVariable<float> CVarElbowX(TEXT("ArenaDuel.Arms.LeftElbowX"), -0.75f, TEXT("Support arm, wrist to elbow direction: forward"));
	TAutoConsoleVariable<float> CVarElbowY(TEXT("ArenaDuel.Arms.LeftElbowY"), -0.25f, TEXT("Support arm, wrist to elbow direction: right"));
	TAutoConsoleVariable<float> CVarElbowZ(TEXT("ArenaDuel.Arms.LeftElbowZ"), -0.60f, TEXT("Support arm, wrist to elbow direction: up"));
	TAutoConsoleVariable<float> CVarForearmRoll(TEXT("ArenaDuel.Arms.LeftRoll"), 0.0f, TEXT("Support arm roll around the forearm, degrees"));
	TAutoConsoleVariable<float> CVarWristX(TEXT("ArenaDuel.Arms.LeftWristX"), 0.0f, TEXT("Support wrist offset from the grip point: forward"));
	TAutoConsoleVariable<float> CVarWristY(TEXT("ArenaDuel.Arms.LeftWristY"), 0.0f, TEXT("Support wrist offset from the grip point: right"));
	TAutoConsoleVariable<float> CVarWristZ(TEXT("ArenaDuel.Arms.LeftWristZ"), 0.0f, TEXT("Support wrist offset from the grip point: up"));

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
			bRigidForearm = Body && Body == Arms;
			if (Character && !Character->IsDead() && Body && (bRigidForearm || bThirdPersonBody) && Weapon)
			{
				FVector GripWorld;
				const int32 RightHandIndex = Body->GetBoneIndex(TEXT("hand_r"));
				if (RightHandIndex != INDEX_NONE && Weapon->GetLeftHandGripWorldLocation(GripWorld, bThirdPersonBody))
				{
					if (bRigidForearm)
					{
						const FRotator View = Character->GetFirstPersonCamera() ? Character->GetFirstPersonCamera()->GetComponentRotation() : Character->GetViewRotation();
						GripWorld += View.RotateVector(FVector(CVarWristX.GetValueOnGameThread(), CVarWristY.GetValueOnGameThread(), CVarWristZ.GetValueOnGameThread()));
						const FVector ElbowView(CVarElbowX.GetValueOnGameThread(), CVarElbowY.GetValueOnGameThread(), CVarElbowZ.GetValueOnGameThread());
						ElbowDirection = Body->GetComponentTransform().InverseTransformVectorNoScale(View.RotateVector(ElbowView)).GetSafeNormal();
						ForearmRoll = CVarForearmRoll.GetValueOnGameThread();
					}
					LeftGripInRightHand = Body->GetBoneTransform(RightHandIndex).InverseTransformPosition(GripWorld);
					bHasLeftGrip = !LeftGripInRightHand.ContainsNaN();
				}
			}
			const float TargetBlend = bHasLeftGrip && Weapon && !Weapon->IsReloading() ? 1.0f : 0.0f;
			LeftHandIKBlend = TargetBlend + (LeftHandIKBlend - TargetBlend) * Decay;
		}
		virtual bool Evaluate(FPoseContext& Output) override
		{
			const bool bResult = FAnimSingleNodeInstanceProxy::Evaluate(Output);
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
			if (bHasLeftGrip && LeftHandIKBlend > KINDA_SMALL_NUMBER)
			{
				const FBoneContainer& Bones = Output.Pose.GetBoneContainer();
				const FReferenceSkeleton& RefSkeleton = Bones.GetReferenceSkeleton();
				const int32 UpperSkeleton = RefSkeleton.FindBoneIndex(TEXT("upperarm_l"));
				const int32 LowerSkeleton = RefSkeleton.FindBoneIndex(TEXT("lowerarm_l"));
				const int32 HandSkeleton = RefSkeleton.FindBoneIndex(TEXT("hand_l"));
				const int32 RightSkeleton = RefSkeleton.FindBoneIndex(TEXT("hand_r"));
				if (UpperSkeleton != INDEX_NONE && LowerSkeleton != INDEX_NONE && HandSkeleton != INDEX_NONE && RightSkeleton != INDEX_NONE)
				{
				const FCompactPoseBoneIndex UpperIndex = Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(UpperSkeleton));
				const FCompactPoseBoneIndex LowerIndex = Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(LowerSkeleton));
				const FCompactPoseBoneIndex HandIndex = Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(HandSkeleton));
				const FCompactPoseBoneIndex RightIndex = Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(RightSkeleton));
					if (UpperIndex.GetInt() != INDEX_NONE && LowerIndex.GetInt() != INDEX_NONE && HandIndex.GetInt() != INDEX_NONE && RightIndex.GetInt() != INDEX_NONE)
					{
						FCSPose<FCompactPose> ComponentPose;
						ComponentPose.InitPose(Output.Pose);
						const FVector Effector = ComponentPose.GetComponentSpaceTransform(RightIndex).TransformPosition(LeftGripInRightHand);
						FTransform Upper = ComponentPose.GetComponentSpaceTransform(UpperIndex);
						FTransform Lower = ComponentPose.GetComponentSpaceTransform(LowerIndex);
						FTransform Hand = ComponentPose.GetComponentSpaceTransform(HandIndex);
						const FTransform OriginalUpper = Upper;
						const FTransform OriginalLower = Lower;
						const FTransform OriginalHand = Hand;
						const FVector JointTarget = Lower.GetLocation() + (Lower.GetLocation() - Upper.GetLocation()).GetSafeNormal() * 30.0f;
						if (!Effector.ContainsNaN() && !JointTarget.ContainsNaN())
						{
							const FVector Forearm = OriginalHand.GetLocation() - OriginalLower.GetLocation();
							if (bRigidForearm && !ElbowDirection.IsNearlyZero() && Forearm.Size() > KINDA_SMALL_NUMBER)
							{
								// Forearm and hand move as one piece: wrist on the grip, elbow along the tuned direction.
								const FVector ToHand = -ElbowDirection;
								const FQuat Swing = FQuat(ToHand, FMath::DegreesToRadians(ForearmRoll)) * FQuat::FindBetweenNormals(Forearm.GetSafeNormal(), ToHand);
								Lower.SetRotation(Swing * OriginalLower.GetRotation());
								Lower.SetLocation(Effector - ToHand * Forearm.Size());
								Hand.SetRotation(Swing * OriginalHand.GetRotation());
								Hand.SetLocation(Effector);
							}
							else AnimationCore::SolveTwoBoneIK(Upper, Lower, Hand, JointTarget, Effector, false, 1.0, 1.0);
							if (!Upper.ContainsNaN() && !Lower.ContainsNaN() && !Hand.ContainsNaN())
							{
								FTransform BlendedUpper, BlendedLower, BlendedHand;
								BlendedUpper.Blend(OriginalUpper, Upper, LeftHandIKBlend);
								BlendedLower.Blend(OriginalLower, Lower, LeftHandIKBlend);
								BlendedHand.Blend(OriginalHand, Hand, LeftHandIKBlend);
								ComponentPose.SetComponentSpaceTransform(UpperIndex, BlendedUpper);
								ComponentPose.SetComponentSpaceTransform(LowerIndex, BlendedLower);
								ComponentPose.SetComponentSpaceTransform(HandIndex, BlendedHand);
								FCSPose<FCompactPose>::ConvertComponentPosesToLocalPoses(MoveTemp(ComponentPose), Output.Pose);
							}
						}
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
	else if (Character->GetWeaponComponent() && Character->GetWeaponComponent()->IsReloading()) Desired = Reload;
	else if (!bFirstPerson)
	{
		if (Character->GetCharacterMovement()->IsFalling()) Desired = Fall;
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
