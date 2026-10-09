// Copyright Epic Games, Inc. All Rights Reserved.

#include "ArenaDuelCharacterMovementComponent.h"

#include "ArenaDuelCharacter.h"
#include "../Player/ArenaDuelPlayerState.h"
#include "../Game/ArenaDuelGameState.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

class FSavedMove_ArenaDuel final : public FSavedMove_Character
{
public:
	using Super = FSavedMove_Character;

	uint8 bSavedWantsSprint : 1;
	uint8 bSavedWantsSlide : 1;
	uint8 bSavedAdvancedJump : 1;
	uint8 bSavedWallJump : 1;

	virtual void Clear() override
	{
		Super::Clear();
		bSavedWantsSprint = false;
		bSavedWantsSlide = false;
		bSavedAdvancedJump = false;
		bSavedWallJump = false;
	}

	virtual void SetMoveFor(ACharacter* Character, float InDeltaTime, FVector const& NewAccel, FNetworkPredictionData_Client_Character& ClientData) override
	{
		Super::SetMoveFor(Character, InDeltaTime, NewAccel, ClientData);
		if (const AArenaDuelCharacter* ArenaCharacter = Cast<AArenaDuelCharacter>(Character))
		{
			if (const UArenaDuelCharacterMovementComponent* Movement = ArenaCharacter->GetArenaDuelMovementComponent())
			{
				bSavedWantsSprint = Movement->WantsSprintIntent();
				bSavedWantsSlide = Movement->WantsSlideIntent();
				bSavedAdvancedJump = Movement->HasAdvancedJumpIntent();
				bSavedWallJump = Movement->WantsWallJumpIntent();
			}
		}
	}

	virtual bool CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* Character, float MaxDelta) const override
	{
		const FSavedMove_ArenaDuel* ArenaMove = static_cast<const FSavedMove_ArenaDuel*>(NewMove.Get());
		return ArenaMove && bSavedWantsSprint == ArenaMove->bSavedWantsSprint && bSavedWantsSlide == ArenaMove->bSavedWantsSlide && bSavedAdvancedJump == ArenaMove->bSavedAdvancedJump && bSavedWallJump == ArenaMove->bSavedWallJump && Super::CanCombineWith(NewMove, Character, MaxDelta);
	}

	virtual void PrepMoveFor(ACharacter* Character) override
	{
		Super::PrepMoveFor(Character);
		if (AArenaDuelCharacter* ArenaCharacter = Cast<AArenaDuelCharacter>(Character))
		{
			if (UArenaDuelCharacterMovementComponent* Movement = ArenaCharacter->GetArenaDuelMovementComponent())
			{
				Movement->SetSprintIntentFromNetwork(bSavedWantsSprint);
				Movement->SetSlideIntentFromNetwork(bSavedWantsSlide);
				Movement->QueueAdvancedJump(bSavedWallJump);
				if (!bSavedAdvancedJump)
				{
					Movement->ClearAdvancedJumpIntent();
				}
			}
		}
	}

	virtual uint8 GetCompressedFlags() const override
	{
		uint8 Result = Super::GetCompressedFlags();
		if (bSavedWantsSprint)
		{
			Result |= FLAG_Custom_0;
		}
		if (bSavedWantsSlide)
		{
			Result |= FLAG_Custom_1;
		}
		if (bSavedAdvancedJump)
		{
			Result |= FLAG_Custom_2;
		}
		if (bSavedWallJump)
		{
			Result |= FLAG_Custom_3;
		}
		return Result;
	}
};

class FNetworkPredictionData_Client_ArenaDuel final : public FNetworkPredictionData_Client_Character
{
public:
	explicit FNetworkPredictionData_Client_ArenaDuel(const UCharacterMovementComponent& ClientMovement)
		: FNetworkPredictionData_Client_Character(ClientMovement)
	{
	}

	virtual FSavedMovePtr AllocateNewMove() override
	{
		return FSavedMovePtr(new FSavedMove_ArenaDuel());
	}
};

DEFINE_LOG_CATEGORY_STATIC(LogArenaDuelSlide, Log, All);

namespace ArenaDuelMovement
{
	static constexpr uint8 SlideMode = static_cast<uint8>(EArenaDuelCustomMovementMode::Slide);
	static constexpr uint8 WallRunMode = static_cast<uint8>(EArenaDuelCustomMovementMode::WallRun);
	static constexpr uint8 VaultMode = static_cast<uint8>(EArenaDuelCustomMovementMode::Vault);
	static constexpr uint8 MantleMode = static_cast<uint8>(EArenaDuelCustomMovementMode::Mantle);
	// Minimum time after a slide ends before a held slide key may start a new slide. Prevents crouch/stand flicker.
	static constexpr float SlideReentryCooldown = 0.35f;
}

UArenaDuelCharacterMovementComponent::UArenaDuelCharacterMovementComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	MaxWalkSpeed = WalkSpeed;
	MaxWalkSpeedCrouched = CrouchSpeed;
	AirControl = AirControlTuning;
	ApplyGroundTuning();
	bCanWalkOffLedgesWhenCrouching = true;
	NavAgentProps.bCanCrouch = true;
	SetIsReplicatedByDefault(true);
}

void UArenaDuelCharacterMovementComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UArenaDuelCharacterMovementComponent, Stamina);
}

void UArenaDuelCharacterMovementComponent::StartSprint()
{
	bWantsSprint = true;
}

void UArenaDuelCharacterMovementComponent::StopSprint()
{
	bWantsSprint = false;
}

void UArenaDuelCharacterMovementComponent::StartSlide()
{
	// This method is called for a new Started input event, never every held frame.
	bSlideConsumedUntilRelease = false;
	bWantsSlide = true;
	if (IsMovingOnGround() && Velocity.Size2D() >= SlideMinSpeed)
	{
		bSlideQueued = false;
		SlideInputBufferRemaining = 0.0f;
		EnterSlide();
	}
	else if (IsMovingOnGround() && bWantsSprint && Velocity.Size2D() >= SlideQueueMinSpeed)
	{
		bSlideQueued = true;
		SlideInputBufferRemaining = SlideInputBuffer;
	}
	else
	{
		bSlideQueued = false;
		SlideInputBufferRemaining = 0.0f;
	}
}

void UArenaDuelCharacterMovementComponent::ReleaseSlideInput()
{
	bWantsSlide = false;
	bSlideConsumedUntilRelease = false;
	bSlideQueued = false;
	SlideInputBufferRemaining = 0.0f;
}

void UArenaDuelCharacterMovementComponent::StopSlide()
{
	ReleaseSlideInput();
	if (IsSliding())
	{
		ExitSlide();
	}
}

void UArenaDuelCharacterMovementComponent::ApplyGroundTuning()
{
	BrakingDecelerationWalking = GroundBrakingDeceleration;
	GroundFriction = GroundTurnFriction;
	bUseSeparateBrakingFriction = true;
	BrakingFriction = GroundBrakingFriction;
	BrakingFrictionFactor = 1.0f;
	NetworkSimulatedSmoothLocationTime = RemoteSmoothLocationTime;
	NetworkSimulatedSmoothRotationTime = RemoteSmoothRotationTime;
	ListenServerNetworkSimulatedSmoothLocationTime = RemoteSmoothLocationTime;
	ListenServerNetworkSimulatedSmoothRotationTime = RemoteSmoothRotationTime;
}

float UArenaDuelCharacterMovementComponent::GetMaxAcceleration() const
{
	// Only walking gets the snappy ground value. Air control and custom modes keep their tuned feel.
	return MovementMode == MOVE_Walking ? GroundAcceleration : AirAcceleration;
}

FVector UArenaDuelCharacterMovementComponent::ApplyCounterStrafe(const FVector& InVelocity, const FVector& InputDirection, float DeltaTime) const
{
	const FVector Direction = InputDirection.GetSafeNormal2D();
	const float OpposingSpeed = -FVector::DotProduct(FVector(InVelocity.X, InVelocity.Y, 0.0f), Direction);
	if (Direction.IsNearlyZero() || OpposingSpeed <= 0.0f || DeltaTime <= 0.0f) return InVelocity;
	return InVelocity + Direction * FMath::Min(OpposingSpeed, CounterStrafeDeceleration * DeltaTime);
}

void UArenaDuelCharacterMovementComponent::CalcVelocity(float DeltaTime, float Friction, bool bFluid, float BrakingDeceleration)
{
	// Counter-strafing: velocity that runs against the held input is removed much faster than normal
	// braking, so tapping the opposite key stops the player almost at once. It depends only on the
	// velocity and acceleration of the move being simulated, so prediction and server replay agree.
	if (MovementMode == MOVE_Walking && !Acceleration.IsNearlyZero())
	{
		Velocity = ApplyCounterStrafe(Velocity, Acceleration, DeltaTime);
	}
	Super::CalcVelocity(DeltaTime, Friction, bFluid, BrakingDeceleration);
}
float UArenaDuelCharacterMovementComponent::GetMaxSpeed() const
{
	if (IsSliding())
	{
		return FMath::Max(SlideMinSpeed, Velocity.Size2D());
	}
	// Strafe and backpedal scales follow the held input, which is part of every saved move.
	float DirectionScale = 1.0f;
	if (UpdatedComponent && !Acceleration.IsNearlyZero() && IsMovingOnGround())
	{
		const FVector LocalInput = UpdatedComponent->GetComponentQuat().UnrotateVector(Acceleration.GetSafeNormal2D());
		DirectionScale = 1.0f + FMath::Abs(LocalInput.Y) * (StrafeSpeedScale - 1.0f) + FMath::Max(-LocalInput.X, 0.0f) * (BackwardSpeedScale - 1.0f);
	}
	if (IsCrouching())
	{
		return CrouchSpeed * DirectionScale;
	}
	if (bWantsSprint && IsMovingOnGround())
	{
		return SprintSpeed * DirectionScale;
	}
	return WalkSpeed * DirectionScale;
}

bool UArenaDuelCharacterMovementComponent::CanCrouchInCurrentState() const
{
	return Super::CanCrouchInCurrentState() || (IsSliding() && CanEverCrouch() && UpdatedComponent && !UpdatedComponent->IsSimulatingPhysics());
}

FNetworkPredictionData_Client* UArenaDuelCharacterMovementComponent::GetPredictionData_Client() const
{
	if (ClientPredictionData == nullptr)
	{
		UArenaDuelCharacterMovementComponent* MutableThis = const_cast<UArenaDuelCharacterMovementComponent*>(this);
		MutableThis->ClientPredictionData = new FNetworkPredictionData_Client_ArenaDuel(*this);
	}
	return ClientPredictionData;
}

void UArenaDuelCharacterMovementComponent::UpdateFromCompressedFlags(uint8 Flags)
{
	Super::UpdateFromCompressedFlags(Flags);
	SetSprintIntentFromNetwork((Flags & FSavedMove_Character::FLAG_Custom_0) != 0);
	SetSlideIntentFromNetwork((Flags & FSavedMove_Character::FLAG_Custom_1) != 0);
	if ((Flags & FSavedMove_Character::FLAG_Custom_2) != 0)
	{
		QueueAdvancedJump((Flags & FSavedMove_Character::FLAG_Custom_3) != 0);
	}
	else
	{
		ClearAdvancedJumpIntent();
	}
}

void UArenaDuelCharacterMovementComponent::SetSprintIntentFromNetwork(bool bWantsSprintIntent)
{
	bWantsSprint = bWantsSprintIntent;
}

void UArenaDuelCharacterMovementComponent::SetSlideIntentFromNetwork(bool bWantsSlideIntent)
{
	bWantsSlide = bWantsSlideIntent;
	if (!bWantsSlide)
	{
		bSlideConsumedUntilRelease = false;
	}
}

void UArenaDuelCharacterMovementComponent::QueueAdvancedJump(bool bWallJump)
{
	bAdvancedJumpRequested = true;
	bAdvancedWallJump = bWallJump;
}

void UArenaDuelCharacterMovementComponent::ClearAdvancedJumpIntent()
{
	bAdvancedJumpRequested = false;
	bAdvancedWallJump = false;
}

void UArenaDuelCharacterMovementComponent::UpdateCharacterStateBeforeMovement(float DeltaSeconds)
{
	if (!CharacterOwner || !UpdatedComponent)
	{
		return;
	}
	if (IsSliding())
	{
		bWantsToCrouch = true;
	}
	Super::UpdateCharacterStateBeforeMovement(DeltaSeconds);
	// Compressed client movement intent cannot unlock a server-owned lobby or round break.
	if (const AArenaDuelGameState* State = GetWorld() ? GetWorld()->GetGameState<AArenaDuelGameState>() : nullptr; State && !State->CanLivingCharacterMove(Cast<AArenaDuelCharacter>(CharacterOwner)))
	{
		ClearAdvancedJumpIntent();
		StopMovementImmediately();
		DisableMovement();
		return;
	}
	if (bAdvancedJumpRequested)
	{
		const bool bExecuted = bAdvancedWallJump ? TryWallJump() : TrySlideJump();
		ClearAdvancedJumpIntent();
		if (bExecuted)
		{
			return;
		}
	}
	WallReattachTimeRemaining = FMath::Max(0.0f, WallReattachTimeRemaining - DeltaSeconds);
	if (!IsSliding())
	{
		TimeSinceSlideEnded = FMath::Min(TimeSinceSlideEnded + DeltaSeconds, 1000.0f);
	}

	MaxWalkSpeed = WalkSpeed;
	MaxWalkSpeedCrouched = CrouchSpeed;
	AirControl = AirControlTuning;
	ApplyGroundTuning();
	if (bSlideQueued)
	{
		SlideInputBufferRemaining = FMath::Max(0.0f, SlideInputBufferRemaining - DeltaSeconds);
		if (!bWantsSlide || IsFalling())
		{
			bSlideQueued = false;
		}
		else if (Velocity.Size2D() >= SlideMinSpeed)
		{
			bSlideQueued = false;
			SlideInputBufferRemaining = 0.0f;
			EnterSlide();
		}
		else if (SlideInputBufferRemaining <= 0.0f)
		{
			bSlideQueued = false;
			CharacterOwner->Crouch();
		}
	}

	if (IsSliding())
	{
		if (!bWantsSlide) bSlideReleasedDuringSlide = true;
		if (SlideElapsed >= SlideDuration || (SlideElapsed >= SlideMinDuration && Velocity.Size2D() < SlideEndSpeed))
		{
			UE_LOG(LogArenaDuelSlide, Verbose, TEXT("Slide end (time/speed) elapsed=%.2f speed=%.0f"), SlideElapsed, Velocity.Size2D());
			bSlideConsumedUntilRelease = bWantsSlide;
			ExitSlide();
		}
		else if (ShouldCancelSlide(bWantsSlide, bSlideReleasedDuringSlide, SlideElapsed, SlideCancelMinTime))
		{
			// Slide cancel. The player stands up with the speed they have; the held key must be let go
			// before it can start another slide, and the entry boost keeps its own cooldown.
			UE_LOG(LogArenaDuelSlide, Verbose, TEXT("Slide cancel elapsed=%.2f speed=%.0f"), SlideElapsed, Velocity.Size2D());
			bSlideConsumedUntilRelease = true;
			ExitSlide();
		}
	}
	else if (bWantsSlide && !bSlideConsumedUntilRelease && TimeSinceSlideEnded >= ArenaDuelMovement::SlideReentryCooldown && (MovementMode == MOVE_Walking || MovementMode == MOVE_NavWalking) && Velocity.Size2D() >= SlideMinSpeed)
	{
		UE_LOG(LogArenaDuelSlide, Verbose, TEXT("Auto slide entry (held input) role=%d speed=%.0f"), static_cast<int32>(CharacterOwner->GetLocalRole()), Velocity.Size2D());
		EnterSlide();
	}

	if (IsFalling() && Velocity.Size2D() >= WallRunMinSpeed)
	{
		FHitResult WallHit;
		FVector CandidateNormal;
		if (bWantsSprint && Stamina > 0.0f && TryFindWall(WallHit, CandidateNormal))
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
	// Switching from walking to a custom mode clears CurrentFloor in the engine.
	// MoveAlongFloor does nothing until the slide establishes its own floor contact.
	if (GetWorld() && GetWorld()->IsGameWorld())
	{
		FindFloor(UpdatedComponent->GetComponentLocation(), CurrentFloor, false);
		if (!CurrentFloor.IsWalkableFloor())
		{
			// Leaving a ledge: keep momentum and fall. Holding the key must not re-enter until landing + cooldown.
			UE_LOG(LogArenaDuelSlide, Verbose, TEXT("Slide end (no floor) dist=%.1f blocking=%d elapsed=%.2f"), CurrentFloor.FloorDist, CurrentFloor.bBlockingHit ? 1 : 0, SlideElapsed);
			bSlideConsumedUntilRelease = bWantsSlide;
			ExitSlide();
			return;
		}
	}

	const float Dt = FMath::Max(DeltaSeconds, 0.0f);
	FVector Horizontal(Velocity.X, Velocity.Y, 0.0f);

	// Slope gravity: downhill slides gain speed, uphill slides bleed it.
	const FVector SlopeAcceleration = ComputeSlideSlopeAcceleration();
	const bool bAcceleratingDownhill = !SlopeAcceleration.IsNearlyZero(1.0f) && FVector::DotProduct(SlopeAcceleration, Horizontal.GetSafeNormal()) > 50.0f;
	Horizontal += SlopeAcceleration * Dt;

	// Smooth exponential glide on flat ground; no friction while gravity pulls the slide downhill.
	if (!bAcceleratingDownhill)
	{
		Horizontal *= FMath::Exp(-SlideFriction * Dt);
	}

	// Steering rotates the slide direction without adding speed; backwards input brakes.
	const FVector InputDirection = FVector(Acceleration.X, Acceleration.Y, 0.0f).GetSafeNormal();
	const float Speed = Horizontal.Size();
	if (!InputDirection.IsNearlyZero() && Speed > KINDA_SMALL_NUMBER)
	{
		const FVector Forward = Horizontal / Speed;
		const float Alignment = FVector::DotProduct(Forward, InputDirection);
		if (Alignment < -0.5f)
		{
			Horizontal = Forward * FMath::Max(0.0f, Speed - SlideBrakeDeceleration * Dt);
		}
		else
		{
			const float CurrentYaw = FMath::RadiansToDegrees(FMath::Atan2(Forward.Y, Forward.X));
			const float TargetYaw = FMath::RadiansToDegrees(FMath::Atan2(InputDirection.Y, InputDirection.X));
			const float YawDelta = FMath::Clamp(FRotator::NormalizeAxis(TargetYaw - CurrentYaw), -SlideTurnRate * Dt, SlideTurnRate * Dt);
			Horizontal = FRotator(0.0f, YawDelta, 0.0f).RotateVector(Forward) * Speed;
		}
	}

	Velocity = Horizontal.GetClampedToMaxSize2D(GlobalMomentumCap);
	// Downhill time is free: long ramps keep the slide alive until speed or ground runs out.
	if (!bAcceleratingDownhill)
	{
		SlideElapsed += Dt;
	}
	MoveAlongFloor(Velocity, Dt, nullptr);

	if (IsSliding() && (SlideElapsed >= SlideDuration || (SlideElapsed >= SlideMinDuration && Velocity.Size2D() < SlideEndSpeed)))
	{
		UE_LOG(LogArenaDuelSlide, Verbose, TEXT("Slide end (time/speed) elapsed=%.2f speed=%.0f"), SlideElapsed, Velocity.Size2D());
		bSlideConsumedUntilRelease = bWantsSlide;
		ExitSlide();
	}
}

FVector UArenaDuelCharacterMovementComponent::ComputeSlideSlopeAcceleration() const
{
	if (SlideGravityScale <= 0.0f || !CurrentFloor.IsWalkableFloor())
	{
		return FVector::ZeroVector;
	}
	const FVector Normal = CurrentFloor.HitResult.ImpactNormal;
	if (Normal.IsNearlyZero() || Normal.Z >= 0.999f || Normal.Z <= 0.0f)
	{
		return FVector::ZeroVector;
	}
	// Horizontal part of gravity projected onto the floor plane: |g| * sin(theta) * cos(theta) along the downhill direction.
	return FVector(Normal.X, Normal.Y, 0.0f) * (-GetGravityZ() * SlideGravityScale * Normal.Z);
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
	const AArenaDuelCharacter* ArenaCharacter = Cast<AArenaDuelCharacter>(CharacterOwner);
	const AArenaDuelPlayerState* PlayerState = ArenaCharacter ? ArenaCharacter->GetPlayerState<AArenaDuelPlayerState>() : nullptr;
	if (PlayerState && PlayerState->HasAdminInfiniteStamina())
	{
		Stamina = MaxStamina;
		TimeSinceStaminaUse = StaminaRegenDelay;
		return;
	}
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
	ExitSlide();
	Velocity.Z = JumpZVelocity * SlideJumpVerticalMultiplier;
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
	const FVector TopStart = FrontHit.ImpactPoint + FVector::UpVector * (MantleMaxHeight + 30.0f);
	FHitResult TopHit;
	if (!GetWorld()->LineTraceSingleByChannel(TopHit, TopStart, FrontHit.ImpactPoint + FVector::UpVector * 10.0f, ECC_Visibility, Params) || TopHit.ImpactNormal.Z < 0.7f)
	{
		return false;
	}
	const float ObstacleHeight = TopHit.ImpactPoint.Z - CharacterOwner->GetActorLocation().Z;
	if (ObstacleHeight < 10.0f || ObstacleHeight > MantleMaxHeight)
	{
		return false;
	}
	const bool bMantle = ObstacleHeight > VaultMaxHeight;
	TraversalStart = CharacterOwner->GetActorLocation();
	const UCapsuleComponent* Capsule = CharacterOwner->GetCapsuleComponent();
	TraversalTarget = TopHit.Location + FVector::UpVector * (Capsule->GetScaledCapsuleHalfHeight() + 2.0f) + Forward * (Capsule->GetScaledCapsuleRadius() + 45.0f);
	const FCollisionShape CapsuleShape = FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(), Capsule->GetScaledCapsuleHalfHeight());
	if (GetWorld()->OverlapBlockingTestByChannel(TraversalTarget, CharacterOwner->GetActorQuat(), ECC_Pawn, CapsuleShape, Params))
	{
		return false;
	}
	TraversalElapsed = 0.0f;
	TraversalDuration = bMantle ? 0.28f : 0.18f;
	SetMovementMode(MOVE_Custom, bMantle ? ArenaDuelMovement::MantleMode : ArenaDuelMovement::VaultMode);
	return true;
}

void UArenaDuelCharacterMovementComponent::PhysTraversal(float DeltaSeconds, int32 Iterations, bool bMantle)
{
	TraversalElapsed += DeltaSeconds;
	const float Alpha = FMath::Clamp(TraversalElapsed / FMath::Max(TraversalDuration, KINDA_SMALL_NUMBER), 0.0f, 1.0f);
	const float ArcHeight = bMantle ? 35.0f : 120.0f;
	const FVector Position = FMath::Lerp(TraversalStart, TraversalTarget, Alpha) + FVector::UpVector * (ArcHeight * FMath::Sin(Alpha * PI));
	FHitResult Hit;
	if (SafeMoveUpdatedComponent(Position - UpdatedComponent->GetComponentLocation(), UpdatedComponent->GetComponentQuat(), true, Hit) && Hit.IsValidBlockingHit())
	{
		SetMovementMode(MOVE_Falling);
		Velocity = FVector::ZeroVector;
		return;
	}
	Velocity = (TraversalTarget - TraversalStart) / FMath::Max(TraversalDuration, KINDA_SMALL_NUMBER);
	if (Alpha >= 1.0f)
	{
		FFindFloorResult FloorResult;
		FindFloor(UpdatedComponent->GetComponentLocation(), FloorResult, false);
		SetMovementMode(FloorResult.IsWalkableFloor() ? MOVE_Walking : MOVE_Falling);
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
		bWantsToCrouch = true;
		// Shrink the capsule while still in walking mode. Walking keeps the capsule base on the floor;
		// crouching after switching to the custom mode would shrink around the center, lift the feet
		// off the floor, lose the floor and bounce between slide and stand every few frames.
		if (!IsCrouching() && IsMovingOnGround())
		{
			Crouch(false);
		}
	}
	UE_LOG(LogArenaDuelSlide, Verbose, TEXT("Slide enter role=%d speed=%.0f crouched=%d boost=%d"), CharacterOwner ? static_cast<int32>(CharacterOwner->GetLocalRole()) : -1, Velocity.Size2D(), IsCrouching() ? 1 : 0, IsSlideBoostReady() ? 1 : 0);
	SlideElapsed = 0.0f;
	bSlideReleasedDuringSlide = false;
	const FVector HorizontalDirection = Velocity.GetSafeNormal2D();
	const float CurrentSpeed = Velocity.Size2D();
	// The entry boost only applies when it has recharged, so slide spam cannot stack speed up to the cap.
	const float TargetSpeed = IsSlideBoostReady() ? FMath::Max(CurrentSpeed + SlideInitialBoost, SlideEntrySpeed) : CurrentSpeed;
	TimeSinceSlideEnded = 0.0f;
	Velocity = (HorizontalDirection * TargetSpeed).GetClampedToMaxSize2D(GlobalMomentumCap);
	SetMovementMode(MOVE_Custom, ArenaDuelMovement::SlideMode);
}

void UArenaDuelCharacterMovementComponent::ExitSlide()
{
	if (!IsSliding())
	{
		return;
	}
	SlideElapsed = 0.0f;
	bSlideReleasedDuringSlide = false;
	TimeSinceSlideEnded = 0.0f;
	bool bHasWalkableFloor = false;
	if (UpdatedComponent)
	{
		FFindFloorResult FloorResult;
		FindFloor(UpdatedComponent->GetComponentLocation(), FloorResult, false);
		bHasWalkableFloor = FloorResult.IsWalkableFloor();
	}
	SetMovementMode(bHasWalkableFloor ? MOVE_Walking : MOVE_Falling);
	const AArenaDuelCharacter* ArenaCharacter = Cast<AArenaDuelCharacter>(CharacterOwner);
	if (CharacterOwner && (!ArenaCharacter || !ArenaCharacter->IsCrouchInputHeld()))
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
	if (WallReattachTimeRemaining > 0.0f && FVector::DotProduct(InWallNormal, LastWallNormal) > 0.95f)
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
		LastWallNormal = WallNormal;
		WallReattachTimeRemaining = WallReattachCooldown;
		SetMovementMode(MOVE_Falling);
	}
}

void UArenaDuelCharacterMovementComponent::ConsumeStamina(float Amount)
{
	const AArenaDuelCharacter* ArenaCharacter = Cast<AArenaDuelCharacter>(CharacterOwner);
	const AArenaDuelPlayerState* PlayerState = ArenaCharacter ? ArenaCharacter->GetPlayerState<AArenaDuelPlayerState>() : nullptr;
	if (PlayerState && PlayerState->HasAdminInfiniteStamina())
	{
		Stamina = MaxStamina;
		TimeSinceStaminaUse = StaminaRegenDelay;
		return;
	}
	Stamina = FMath::Clamp(Stamina - Amount, 0.0f, MaxStamina);
	TimeSinceStaminaUse = 0.0f;
}

void UArenaDuelCharacterMovementComponent::RefillStaminaForDevelopment()
{
	if (CharacterOwner && CharacterOwner->HasAuthority())
	{
		Stamina = MaxStamina;
		TimeSinceStaminaUse = StaminaRegenDelay;
		CharacterOwner->ForceNetUpdate();
	}
}

void UArenaDuelCharacterMovementComponent::ResetMovementIntentForDevelopment()
{
	if (!CharacterOwner || !CharacterOwner->HasAuthority()) return;
	StopSprint();
	StopSlide();
	bSlideQueued = false;
	SlideInputBufferRemaining = 0.0f;
	TimeSinceSlideEnded = SlideBoostCooldown;
	ClearAdvancedJumpIntent();
	CharacterOwner->UnCrouch();
	CharacterOwner->ForceNetUpdate();
}

FString UArenaDuelCharacterMovementComponent::GetDevelopmentMovementState() const
{
	if (IsWallRunning()) return TEXT("WALL RUN");
	if (IsMantling()) return TEXT("MANTLE");
	if (MovementMode == MOVE_Custom && CustomMovementMode == static_cast<uint8>(EArenaDuelCustomMovementMode::Vault)) return TEXT("VAULT");
	if (IsSliding()) return TEXT("SLIDE");
	if (IsCrouching()) return TEXT("CROUCH");
	if (IsFalling()) return TEXT("AIR");
	if (IsSprinting()) return TEXT("SPRINT");
	return TEXT("WALK");
}
