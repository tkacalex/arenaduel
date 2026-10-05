// Copyright Epic Games, Inc. All Rights Reserved.

#include "ArenaDuelCharacterMovementComponent.h"

#include "ArenaDuelCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"

namespace ArenaDuelMovement
{
	static constexpr uint8 SlideMode = static_cast<uint8>(EArenaDuelCustomMovementMode::Slide);
	static constexpr uint8 WallRunMode = static_cast<uint8>(EArenaDuelCustomMovementMode::WallRun);
	static constexpr uint8 VaultMode = static_cast<uint8>(EArenaDuelCustomMovementMode::Vault);
	static constexpr uint8 MantleMode = static_cast<uint8>(EArenaDuelCustomMovementMode::Mantle);
}

UArenaDuelCharacterMovementComponent::UArenaDuelCharacterMovementComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	MaxWalkSpeed = WalkSpeed;
	MaxWalkSpeedCrouched = CrouchSpeed;
	AirControl = AirControlTuning;
	bCanWalkOffLedgesWhenCrouching = true;
}

void UArenaDuelCharacterMovementComponent::StartSprint()
{
	bWantsSprint = true;
}

void UArenaDuelCharacterMovementComponent::StopSprint()
{
	bWantsSprint = false;
}

void UArenaDuelCharacterMovementComponent::StartCrouchOrSlide()
{
	bWantsCrouchOrSlide = true;
	if (!IsFalling() && Velocity.Size2D() >= SlideMinSpeed)
	{
		EnterSlide();
	}
	else if (CharacterOwner)
	{
		CharacterOwner->Crouch();
	}
}

void UArenaDuelCharacterMovementComponent::StopCrouchOrSlide()
{
	bWantsCrouchOrSlide = false;
	if (IsSliding())
	{
		ExitSlide();
	}
	else if (CharacterOwner)
	{
		CharacterOwner->UnCrouch();
	}
}

float UArenaDuelCharacterMovementComponent::GetMaxSpeed() const
{
	if (IsSliding())
	{
		return FMath::Max(SlideMinSpeed, Velocity.Size2D());
	}
	if (IsCrouching())
	{
		return CrouchSpeed;
	}
	if (bWantsSprint && IsMovingOnGround())
	{
		return SprintSpeed;
	}
	return WalkSpeed;
}

void UArenaDuelCharacterMovementComponent::UpdateCharacterStateBeforeMovement(float DeltaSeconds)
{
	Super::UpdateCharacterStateBeforeMovement(DeltaSeconds);

	MaxWalkSpeed = WalkSpeed;
	MaxWalkSpeedCrouched = CrouchSpeed;
	AirControl = AirControlTuning;

	if (IsSliding())
	{
		if (!bWantsCrouchOrSlide || !IsMovingOnGround() || Velocity.Size2D() < SlideEndSpeed)
		{
			ExitSlide();
		}
	}
	else if (bWantsCrouchOrSlide && (MovementMode == MOVE_Walking || MovementMode == MOVE_NavWalking) && Velocity.Size2D() >= SlideMinSpeed)
	{
		EnterSlide();
	}

	if (IsFalling() && bWantsSprint && Velocity.Size2D() >= WallRunMinSpeed && Stamina > 0.0f)
	{
		FHitResult WallHit;
		FVector CandidateNormal;
		if (TryFindWall(WallHit, CandidateNormal))
		{
			EnterWallRun(CandidateNormal);
		}
		else
		{
			TryStartTraversal();
		}
	}
}

void UArenaDuelCharacterMovementComponent::PhysCustom(float DeltaSeconds, int32 Iterations)
{
	if (CustomMovementMode == ArenaDuelMovement::SlideMode)
	{
		PhysSlide(DeltaSeconds, Iterations);
	}
	else if (CustomMovementMode == ArenaDuelMovement::WallRunMode)
	{
		PhysWallRun(DeltaSeconds, Iterations);
	}
	else if (CustomMovementMode == ArenaDuelMovement::VaultMode)
	{
		PhysTraversal(DeltaSeconds, Iterations, false);
	}
	else if (CustomMovementMode == ArenaDuelMovement::MantleMode)
	{
		PhysTraversal(DeltaSeconds, Iterations, true);
	}
	else
	{
		SetMovementMode(MOVE_Falling);
	}
}

void UArenaDuelCharacterMovementComponent::PhysSlide(float DeltaSeconds, int32 Iterations)
{
	if (!CharacterOwner || !UpdatedComponent)
	{
		return;
	}

	Velocity.Z = 0.0f;
	Velocity *= FMath::Clamp(1.0f - SlideFriction * DeltaSeconds, 0.0f, 1.0f);
	if (!Acceleration.IsNearlyZero())
	{
		Velocity += Acceleration.GetSafeNormal2D() * SlideSteering * GetMaxAcceleration() * DeltaSeconds;
	}
	Velocity = Velocity.GetClampedToMaxSize2D(GlobalMomentumCap);
	MoveAlongFloor(Velocity, DeltaSeconds, nullptr);

	if (Velocity.Size2D() < SlideEndSpeed || !bWantsCrouchOrSlide)
	{
		ExitSlide();
	}
}

void UArenaDuelCharacterMovementComponent::PhysWallRun(float DeltaSeconds, int32 Iterations)
{
	if (!CharacterOwner || !UpdatedComponent || Stamina <= 0.0f || WallRunElapsed >= WallRunMaxDuration)
	{
		ExitWallRun();
		return;
	}

	FHitResult WallHit;
	FVector CandidateNormal;
	if (!TryFindWall(WallHit, CandidateNormal))
	{
		ExitWallRun();
		return;
	}

	WallNormal = CandidateNormal;
	WallRunElapsed += DeltaSeconds;
	ConsumeStamina(WallRunDrain * DeltaSeconds);
	const FVector AlongWall = FVector::VectorPlaneProject(Velocity, WallNormal).GetSafeNormal();
	const FVector DesiredVelocity = AlongWall.IsNearlyZero() ? FVector::VectorPlaneProject(CharacterOwner->GetActorForwardVector(), WallNormal).GetSafeNormal() * WallRunSpeed : AlongWall * WallRunSpeed;
	Velocity = FVector(DesiredVelocity.X, DesiredVelocity.Y, FMath::Max(Velocity.Z, -120.0f));
	Velocity.Z += GetGravityZ() * WallRunGravityScale * DeltaSeconds;
	Velocity = Velocity.GetClampedToMaxSize2D(GlobalMomentumCap);
	SafeMoveUpdatedComponent(Velocity * DeltaSeconds, UpdatedComponent->GetComponentQuat(), true, WallHit);
}

void UArenaDuelCharacterMovementComponent::PhysFalling(float DeltaSeconds, int32 Iterations)
{
	if (IsWallRunning())
	{
		return;
	}
	Super::PhysFalling(DeltaSeconds, Iterations);
	Velocity = Velocity.GetClampedToMaxSize2D(GlobalMomentumCap);
}

void UArenaDuelCharacterMovementComponent::OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode)
{
	Super::OnMovementModeChanged(PreviousMovementMode, PreviousCustomMode);
	if (PreviousCustomMode == ArenaDuelMovement::WallRunMode && CustomMovementMode != ArenaDuelMovement::WallRunMode)
	{
		WallRunElapsed = 0.0f;
		WallNormal = FVector::ZeroVector;
	}
}

void UArenaDuelCharacterMovementComponent::TickComponent(float DeltaSeconds, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaSeconds, TickType, ThisTickFunction);
	if (!IsWallRunning())
	{
		TimeSinceStaminaUse += DeltaSeconds;
		if (TimeSinceStaminaUse >= StaminaRegenDelay)
		{
			Stamina = FMath::Min(MaxStamina, Stamina + StaminaRegenRate * DeltaSeconds);
		}
	}
}

bool UArenaDuelCharacterMovementComponent::TrySlideJump()
{
	if (!IsSliding())
	{
		return false;
	}
	Velocity = Velocity.GetClampedToMaxSize2D(GlobalMomentumCap) * SlideJumpHorizontalRetention;
	Velocity.Z = JumpZVelocity * SlideJumpVerticalMultiplier;
	ExitSlide();
	SetMovementMode(MOVE_Falling);
	return true;
}

bool UArenaDuelCharacterMovementComponent::TryWallJump()
{
	if (!IsWallRunning())
	{
		return false;
	}
	const FVector Forward = FVector::VectorPlaneProject(Velocity, FVector::UpVector).GetSafeNormal();
	Velocity = Forward * WallJumpForwardRetention * WallRunSpeed + WallNormal * WallJumpAwayForce;
	Velocity.Z = WallJumpUpForce;
	ExitWallRun();
	SetMovementMode(MOVE_Falling);
	return true;
}

bool UArenaDuelCharacterMovementComponent::TryFindWall(FHitResult& OutHit, FVector& OutNormal) const
{
	if (!CharacterOwner || !GetWorld())
	{
		return false;
	}
	const FVector Start = CharacterOwner->GetActorLocation() + FVector::UpVector * 45.0f;
	const float Radius = CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleRadius();
	const float Distance = Radius + 35.0f;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(ArenaDuelWall), false, CharacterOwner);
	for (const FVector& Side : { CharacterOwner->GetActorRightVector(), -CharacterOwner->GetActorRightVector() })
	{
		if (GetWorld()->SweepSingleByChannel(OutHit, Start, Start + Side * Distance, FQuat::Identity, ECC_Visibility, FCollisionShape::MakeCapsule(Radius, 45.0f), Params))
		{
			OutNormal = OutHit.ImpactNormal;
			if (FMath::Abs(OutNormal.Z) < 0.25f)
			{
				return true;
			}
		}
	}
	return false;
}

bool UArenaDuelCharacterMovementComponent::TryStartTraversal()
{
	if (!CharacterOwner || !GetWorld() || !IsFalling())
	{
		return false;
	}
	const FVector Start = CharacterOwner->GetActorLocation() + FVector::UpVector * 40.0f;
	const FVector Forward = CharacterOwner->GetActorForwardVector();
	FCollisionQueryParams Params(SCENE_QUERY_STAT(ArenaDuelTraversal), false, CharacterOwner);
	FHitResult FrontHit;
	if (!GetWorld()->SweepSingleByChannel(FrontHit, Start, Start + Forward * TraversalReach, FQuat::Identity, ECC_Visibility, FCollisionShape::MakeCapsule(34.0f, 40.0f), Params))
	{
		return false;
	}
	const float ObstacleHeight = FrontHit.Location.Z - CharacterOwner->GetActorLocation().Z;
	if (ObstacleHeight < 10.0f || ObstacleHeight > MantleMaxHeight)
	{
		return false;
	}
	const bool bMantle = ObstacleHeight > VaultMaxHeight;
	const FVector TopStart = FrontHit.ImpactPoint + FVector::UpVector * (MantleMaxHeight + 30.0f);
	FHitResult TopHit;
	if (!GetWorld()->LineTraceSingleByChannel(TopHit, TopStart, FrontHit.ImpactPoint + FVector::UpVector * 10.0f, ECC_Visibility, Params) || TopHit.ImpactNormal.Z < 0.7f)
	{
		return false;
	}
	TraversalStart = CharacterOwner->GetActorLocation();
	TraversalTarget = TopHit.Location + FVector::UpVector * (CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 2.0f) + Forward * 45.0f;
	TraversalElapsed = 0.0f;
	TraversalDuration = bMantle ? 0.28f : 0.18f;
	SetMovementMode(MOVE_Custom, bMantle ? ArenaDuelMovement::MantleMode : ArenaDuelMovement::VaultMode);
	return true;
}

void UArenaDuelCharacterMovementComponent::PhysTraversal(float DeltaSeconds, int32 Iterations, bool bMantle)
{
	TraversalElapsed += DeltaSeconds;
	const float Alpha = FMath::Clamp(TraversalElapsed / FMath::Max(TraversalDuration, KINDA_SMALL_NUMBER), 0.0f, 1.0f);
	const FVector Position = FMath::Lerp(TraversalStart, TraversalTarget, Alpha) + FVector::UpVector * (bMantle ? 35.0f * FMath::Sin(Alpha * PI) : 15.0f * FMath::Sin(Alpha * PI));
	FHitResult Hit;
	SafeMoveUpdatedComponent(Position - UpdatedComponent->GetComponentLocation(), UpdatedComponent->GetComponentQuat(), true, Hit);
	Velocity = (TraversalTarget - TraversalStart) / FMath::Max(TraversalDuration, KINDA_SMALL_NUMBER);
	if (Alpha >= 1.0f)
	{
		SetMovementMode(MOVE_Walking);
		Velocity = Velocity.GetClampedToMaxSize2D(GlobalMomentumCap);
	}
}

void UArenaDuelCharacterMovementComponent::EnterSlide()
{
	if (IsSliding())
	{
		return;
	}
	if (CharacterOwner)
	{
		CharacterOwner->Crouch();
	}
	Velocity = Velocity.GetClampedToMaxSize2D(GlobalMomentumCap);
	Velocity += Velocity.GetSafeNormal2D() * SlideInitialBoost;
	SetMovementMode(MOVE_Custom, ArenaDuelMovement::SlideMode);
}

void UArenaDuelCharacterMovementComponent::ExitSlide()
{
	if (!IsSliding())
	{
		return;
	}
	SetMovementMode(IsMovingOnGround() ? MOVE_Walking : MOVE_Falling);
	if (!bWantsCrouchOrSlide && CharacterOwner)
	{
		CharacterOwner->UnCrouch();
	}
}

void UArenaDuelCharacterMovementComponent::EnterWallRun(const FVector& InWallNormal)
{
	if (IsWallRunning())
	{
		return;
	}
	WallNormal = InWallNormal;
	WallRunElapsed = 0.0f;
	SetMovementMode(MOVE_Custom, ArenaDuelMovement::WallRunMode);
}

void UArenaDuelCharacterMovementComponent::ExitWallRun()
{
	if (IsWallRunning())
	{
		SetMovementMode(MOVE_Falling);
	}
}

void UArenaDuelCharacterMovementComponent::ConsumeStamina(float Amount)
{
	Stamina = FMath::Clamp(Stamina - Amount, 0.0f, MaxStamina);
	TimeSinceStaminaUse = 0.0f;
}
