// Copyright Epic Games, Inc. All Rights Reserved.

#include "ArenaDuelCharacterMovementComponent.h"

#include "ArenaDuelCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

class FSavedMove_ArenaDuel final : public FSavedMove_Character
{
public:
	using Super = FSavedMove_Character;

	uint8 bSavedWantsSprint : 1;
	uint8 bSavedWantsCrouchSlide : 1;
	uint8 bSavedAdvancedJump : 1;
	uint8 bSavedWallJump : 1;

	virtual void Clear() override
	{
		Super::Clear();
		bSavedWantsSprint = false;
		bSavedWantsCrouchSlide = false;
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
				bSavedWantsCrouchSlide = Movement->WantsCrouchSlideIntent();
				bSavedAdvancedJump = Movement->HasAdvancedJumpIntent();
				bSavedWallJump = Movement->WantsWallJumpIntent();
			}
		}
	}

	virtual bool CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* Character, float MaxDelta) const override
	{
		const FSavedMove_ArenaDuel* ArenaMove = static_cast<const FSavedMove_ArenaDuel*>(NewMove.Get());
		return ArenaMove && bSavedWantsSprint == ArenaMove->bSavedWantsSprint && bSavedWantsCrouchSlide == ArenaMove->bSavedWantsCrouchSlide && bSavedAdvancedJump == ArenaMove->bSavedAdvancedJump && bSavedWallJump == ArenaMove->bSavedWallJump && Super::CanCombineWith(NewMove, Character, MaxDelta);
	}

	virtual void PrepMoveFor(ACharacter* Character) override
	{
		Super::PrepMoveFor(Character);
		if (AArenaDuelCharacter* ArenaCharacter = Cast<AArenaDuelCharacter>(Character))
		{
			if (UArenaDuelCharacterMovementComponent* Movement = ArenaCharacter->GetArenaDuelMovementComponent())
			{
				Movement->SetSprintIntentFromNetwork(bSavedWantsSprint);
				Movement->SetCrouchSlideIntentFromNetwork(bSavedWantsCrouchSlide);
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
		if (bSavedWantsCrouchSlide)
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

void UArenaDuelCharacterMovementComponent::StartCrouchOrSlide()
{
	bWantsCrouchOrSlide = true;
	if (!IsFalling() && Velocity.Size2D() >= SlideMinSpeed)
	{
		bSlideQueued = false;
		SlideInputBufferRemaining = 0.0f;
		EnterSlide();
	}
	else if (!IsFalling() && bWantsSprint && Velocity.Size2D() >= SlideQueueMinSpeed)
	{
		bSlideQueued = true;
		SlideInputBufferRemaining = SlideInputBuffer;
	}
	else if (CharacterOwner)
	{
		bSlideQueued = false;
		SlideInputBufferRemaining = 0.0f;
		CharacterOwner->Crouch();
	}
}

void UArenaDuelCharacterMovementComponent::StopCrouchOrSlide()
{
	bWantsCrouchOrSlide = false;
	bSlideQueued = false;
	SlideInputBufferRemaining = 0.0f;
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
	SetCrouchSlideIntentFromNetwork((Flags & FSavedMove_Character::FLAG_Custom_1) != 0);
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

void UArenaDuelCharacterMovementComponent::SetCrouchSlideIntentFromNetwork(bool bWantsCrouchSlideIntent)
{
	bWantsCrouchOrSlide = bWantsCrouchSlideIntent;
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
	Super::UpdateCharacterStateBeforeMovement(DeltaSeconds);
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

	MaxWalkSpeed = WalkSpeed;
	MaxWalkSpeedCrouched = CrouchSpeed;
	AirControl = AirControlTuning;
	if (bSlideQueued)
	{
		SlideInputBufferRemaining = FMath::Max(0.0f, SlideInputBufferRemaining - DeltaSeconds);
		if (!bWantsCrouchOrSlide || IsFalling())
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
		if (!bWantsCrouchOrSlide || !IsMovingOnGround() || (SlideElapsed >= SlideMinDuration && Velocity.Size2D() < SlideEndSpeed))
		{
			ExitSlide();
		}
	}
	else if (bWantsCrouchOrSlide && (MovementMode == MOVE_Walking || MovementMode == MOVE_NavWalking) && Velocity.Size2D() >= SlideMinSpeed)
	{
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

	Velocity.Z = 0.0f;
	SlideElapsed += DeltaSeconds;
	Velocity *= FMath::Clamp(1.0f - SlideFriction * DeltaSeconds, 0.0f, 1.0f);
	if (!Acceleration.IsNearlyZero())
	{
		Velocity += Acceleration.GetSafeNormal2D() * SlideSteering * GetMaxAcceleration() * DeltaSeconds;
	}
	Velocity = Velocity.GetClampedToMaxSize2D(GlobalMomentumCap);
	MoveAlongFloor(Velocity, DeltaSeconds, nullptr);

	if ((SlideElapsed >= SlideMinDuration && Velocity.Size2D() < SlideEndSpeed) || !bWantsCrouchOrSlide)
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
		CharacterOwner->Crouch();
	}
	SlideElapsed = 0.0f;
	const FVector HorizontalDirection = Velocity.GetSafeNormal2D();
	const float TargetSpeed = FMath::Max(Velocity.Size2D() + SlideInitialBoost, SlideEntrySpeed);
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
	bool bHasWalkableFloor = false;
	if (UpdatedComponent)
	{
		FFindFloorResult FloorResult;
		FindFloor(UpdatedComponent->GetComponentLocation(), FloorResult, false);
		bHasWalkableFloor = FloorResult.IsWalkableFloor();
	}
	SetMovementMode(bHasWalkableFloor ? MOVE_Walking : MOVE_Falling);
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
	Stamina = FMath::Clamp(Stamina - Amount, 0.0f, MaxStamina);
	TimeSinceStaminaUse = 0.0f;
}
