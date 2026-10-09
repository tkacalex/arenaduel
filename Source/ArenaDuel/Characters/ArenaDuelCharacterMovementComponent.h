// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "ArenaDuelCharacterMovementComponent.generated.h"

UENUM(BlueprintType)
enum class EArenaDuelCustomMovementMode : uint8
{
	None UMETA(DisplayName = "None"),
	Slide UMETA(DisplayName = "Slide"),
	WallRun UMETA(DisplayName = "Wall Run"),
	Vault UMETA(DisplayName = "Vault"),
	Mantle UMETA(DisplayName = "Mantle")
};

UCLASS(ClassGroup = (Movement), meta = (BlueprintSpawnableComponent))
class ARENADUEL_API UArenaDuelCharacterMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	UArenaDuelCharacterMovementComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	void StartSprint();
	void StopSprint();
	void StartSlide();
	void ReleaseSlideInput();
	void StopSlide();
	bool IsSprinting() const { return bWantsSprint && IsMovingOnGround() && !IsSliding(); }
	bool IsSliding() const { return MovementMode == MOVE_Custom && CustomMovementMode == static_cast<uint8>(EArenaDuelCustomMovementMode::Slide); }
	bool IsWallRunning() const { return MovementMode == MOVE_Custom && CustomMovementMode == static_cast<uint8>(EArenaDuelCustomMovementMode::WallRun); }
	float GetStamina() const { return Stamina; }
	float GetMaxStamina() const { return MaxStamina; }
	bool WantsSprintIntent() const { return bWantsSprint; }
	bool WantsSlideIntent() const { return bWantsSlide; }
	bool IsSlideQueued() const { return bSlideQueued; }
	/** True when a press of the slide key right now starts or queues a slide: on the ground and at running speed. Mirrors StartSlide. */
	bool WouldSlideOnPress() const { return IsMovingOnGround() && (Velocity.Size2D() >= SlideMinSpeed || (bWantsSprint && Velocity.Size2D() >= SlideQueueMinSpeed)); }
	bool IsSlideBoostReady() const { return TimeSinceSlideEnded >= SlideBoostCooldown; }
	void SetSprintIntentFromNetwork(bool bWantsSprintIntent);
	void SetSlideIntentFromNetwork(bool bWantsSlideIntent);
	void ConsumeStamina(float Amount);
	void RefillStaminaForDevelopment();
	void ResetMovementIntentForDevelopment();
	FString GetDevelopmentMovementState() const;
	bool IsMantling() const { return MovementMode == MOVE_Custom && CustomMovementMode == static_cast<uint8>(EArenaDuelCustomMovementMode::Mantle); }

	bool TrySlideJump();
	/** A slide is cancelled by a second press: the key was let go during the slide and is down again. */
	static bool ShouldCancelSlide(bool bSlideKeyDown, bool bReleasedDuringSlide, float Elapsed, float MinTime) { return bSlideKeyDown && bReleasedDuringSlide && Elapsed >= MinTime; }
	float GetSlideCancelMinTime() const { return SlideCancelMinTime; }
	float GetAimSprintSpeedScale() const { return AimSprintSpeedScale; }

	/**
	 * Source style air strafing for one step. Speed is only added along the wished direction, and only
	 * while the velocity along that direction is below WishSpeedCap. Holding forward at speed therefore
	 * adds nothing; steering sideways while turning adds a little each step. The result never exceeds
	 * MaxGainSpeed through gains, and speed that is already above it is left alone.
	 */
	static FVector ComputeAirStrafe(const FVector& InVelocity, const FVector& WishDirection, float WishSpeedCap, float AccelerationPerSecond, float MaxGainSpeed, float DeltaTime);
	/** Horizontal speed kept by a landing: the share of the speed above sprint speed given by Loss is removed. */
	static float ComputeLandingSpeed(float Speed, float InSprintSpeed, float Loss) { return Speed <= InSprintSpeed ? Speed : InSprintSpeed + (Speed - InSprintSpeed) * (1.0f - FMath::Clamp(Loss, 0.0f, 1.0f)); }
	bool IsAutoBhopEnabled() const { return bAutoBhop; }
	float GetBhopMaxSpeed() const { return BhopMaxSpeed; }
	float GetBhopLandingGrace() const { return BhopLandingGrace; }
	float GetLandingSpeedLoss() const { return LandingSpeedLoss; }
	float GetJumpBufferSeconds() const { return JumpBufferSeconds; }
	float GetAirStrafeWishSpeed() const { return AirStrafeWishSpeed; }
	float GetAirStrafeAccelerate() const { return AirStrafeAccelerate; }
	/** Lets one launch, such as a dash, exceed the momentum cap in the air until the next landing. */
	void AllowSpeedUntilLanding(float Speed) { SpeedAllowance = FMath::Max(SpeedAllowance, Speed); }
	bool TryWallJump();
	void QueueAdvancedJump(bool bWallJump);
	void ClearAdvancedJumpIntent();
	bool HasAdvancedJumpIntent() const { return bAdvancedJumpRequested; }
	bool WantsWallJumpIntent() const { return bAdvancedWallJump; }

	virtual float GetMaxSpeed() const override;
	virtual float GetMaxAcceleration() const override;
	virtual void CalcVelocity(float DeltaTime, float Friction, bool bFluid, float BrakingDeceleration) override;
	/** Copies the ground and smoothing tunables below onto the engine movement properties. */
	void ApplyGroundTuning();
	/** Counter-strafing step: returns InVelocity with the part that opposes InputDirection reduced for DeltaTime. Pure, so it can be tested directly. */
	FVector ApplyCounterStrafe(const FVector& InVelocity, const FVector& InputDirection, float DeltaTime) const;
	virtual bool CanCrouchInCurrentState() const override;
	virtual FNetworkPredictionData_Client* GetPredictionData_Client() const override;
	virtual void UpdateFromCompressedFlags(uint8 Flags) override;
	virtual void UpdateCharacterStateBeforeMovement(float DeltaSeconds) override;
	virtual void PhysCustom(float DeltaSeconds, int32 Iterations) override;
	virtual void PhysFalling(float DeltaSeconds, int32 Iterations) override;
	virtual void ProcessLanded(const FHitResult& Hit, float RemainingTime, int32 Iterations) override;
	virtual void OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode) override;
	virtual void TickComponent(float DeltaSeconds, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement|Ground")
	float WalkSpeed = 600.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement|Ground")
	float SprintSpeed = 900.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement|Ground")
	float CrouchSpeed = 350.0f;

	/** Acceleration on the ground while input is held. Higher reaches top speed sooner. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement|Ground", meta = (ClampMin = "0"))
	float GroundAcceleration = 4200.0f;

	/** Deceleration on the ground with no input. Higher stops sooner. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement|Ground", meta = (ClampMin = "0"))
	float GroundBrakingDeceleration = 6000.0f;

	/** How quickly velocity turns toward the input direction while moving. Higher removes sideways drift. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement|Ground", meta = (ClampMin = "0"))
	float GroundTurnFriction = 11.0f;

	/** Friction applied while stopping with no input. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement|Ground", meta = (ClampMin = "0"))
	float GroundBrakingFriction = 12.0f;

	/** Counter-strafing: extra deceleration applied to the part of the velocity that opposes the held input. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement|Ground", meta = (ClampMin = "0"))
	float CounterStrafeDeceleration = 5200.0f;

	/** Top speed multiplier while moving purely sideways. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement|Ground", meta = (ClampMin = "0.1", ClampMax = "1.5"))
	float StrafeSpeedScale = 1.0f;

	/** Top speed multiplier while moving purely backwards. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement|Ground", meta = (ClampMin = "0.1", ClampMax = "1.5"))
	float BackwardSpeedScale = 1.0f;

	/** Acceleration used for air control and every non-ground mode. Kept at the engine default so air tuning is unchanged. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement|Air", meta = (ClampMin = "0"))
	float AirAcceleration = 2048.0f;

	/** Seconds over which a remote player's position correction is blended. Lower shows fast strafes sooner, higher hides jitter. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement|Network", meta = (ClampMin = "0"))
	float RemoteSmoothLocationTime = 0.07f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement|Network", meta = (ClampMin = "0"))
	float RemoteSmoothRotationTime = 0.05f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement|Slide")
	float SlideMinSpeed = 700.0f;

	/** Speed added on slide entry while the boost is ready. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement|Slide")
	float SlideInitialBoost = 250.0f;

	/** Minimum speed right after a boosted slide entry. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement|Slide")
	float SlideEntrySpeed = 1200.0f;

	/** Exponential speed decay per second on flat ground. Low values keep the slide fast and smooth. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement|Slide")
	float SlideFriction = 0.18f;

	/** How fast the slide direction follows movement input, in degrees per second. Speed is preserved while turning. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement|Slide", meta = (ClampMin = "0"))
	float SlideTurnRate = 85.0f;

	/** Extra deceleration while holding backwards input during a slide. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement|Slide", meta = (ClampMin = "0"))
	float SlideBrakeDeceleration = 900.0f;

	/** Scales slope gravity: downhill slides accelerate, uphill slides bleed speed. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement|Slide", meta = (ClampMin = "0"))
	float SlideGravityScale = 1.0f;

	/** Time after a slide ends before the next slide receives the entry boost again. Prevents slide-spam speed stacking. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement|Slide", meta = (ClampMin = "0"))
	float SlideBoostCooldown = 0.75f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement|Slide")
	float SlideEndSpeed = 450.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement|Slide")
	float SlideMinDuration = 0.4f;

	/** Maximum slide time on flat ground or uphill. Time spent accelerating downhill does not count. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement|Slide")
	float SlideDuration = 1.5f;

	/** Holding jump jumps again on every landing. Off: the jump has to be pressed again around each landing. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement|Bhop")
	bool bAutoBhop = true;

	/** Air strafing cannot push the horizontal speed above this. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement|Bhop", meta = (ClampMin = "0"))
	float BhopMaxSpeed = 1300.0f;

	/** Air strafe strength: speed along the wished direction up to which air input still adds speed. Smaller needs more precise turning. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement|Bhop", meta = (ClampMin = "0"))
	float AirStrafeWishSpeed = 70.0f;

	/** Air strafe acceleration, as a multiple of WalkSpeed per second. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement|Bhop", meta = (ClampMin = "0"))
	float AirStrafeAccelerate = 2.5f;

	/** Below this speed the air is steered plainly and input may accelerate up to it, so a jump at walking pace handles like before. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement|Bhop", meta = (ClampMin = "0"))
	float AirLowSpeedControl = 600.0f;

	/** Seconds after a landing without ground braking. A jump inside this window keeps the speed. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement|Bhop", meta = (ClampMin = "0", ClampMax = "0.3"))
	float BhopLandingGrace = 0.05f;

	/** Share of the speed above sprint speed that every landing takes away. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement|Bhop", meta = (ClampMin = "0", ClampMax = "1"))
	float LandingSpeedLoss = 0.04f;

	/** A jump pressed this long before a landing still counts for that landing. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement|Bhop", meta = (ClampMin = "0"))
	float JumpBufferSeconds = 0.12f;

	/** Sprint speed multiplier while aiming down sights. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement|Ground", meta = (ClampMin = "0.1", ClampMax = "1"))
	float AimSprintSpeedScale = 0.9f;

	/** Slide cancel: pressing the slide key again ends the slide at once, once it has run this long. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement|Slide", meta = (ClampMin = "0"))
	float SlideCancelMinTime = 0.15f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement|Slide")
	float SlideInputBuffer = 0.3f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement|Slide")
	float SlideQueueMinSpeed = 560.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement|Slide")
	float SlideJumpHorizontalRetention = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement|Slide")
	float SlideJumpVerticalMultiplier = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement|Air")
	float AirControlTuning = 0.55f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement|Momentum")
	float GlobalMomentumCap = 1350.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement|WallRun")
	float WallRunMinSpeed = 650.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement|WallRun")
	float WallRunSpeed = 900.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement|WallRun")
	float WallRunGravityScale = 0.2f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement|WallRun")
	float WallRunMaxDuration = 3.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement|WallRun")
	float WallRunDrain = 20.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement|WallRun")
	float WallJumpAwayForce = 500.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement|WallRun")
	float WallJumpUpForce = 420.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement|WallRun")
	float WallJumpForwardRetention = 0.75f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement|Stamina")
	float MaxStamina = 100.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement|Stamina")
	float StaminaRegenDelay = 0.75f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement|Stamina")
	float StaminaRegenRate = 30.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement|Traversal")
	float VaultMaxHeight = 85.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement|Traversal")
	float MantleMaxHeight = 150.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement|Traversal")
	float TraversalReach = 100.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement|WallRun")
	float WallReattachCooldown = 0.25f;

protected:
	bool TryFindWall(FHitResult& OutHit, FVector& OutNormal) const;
	bool TryStartTraversal();
	void PhysSlide(float DeltaSeconds, int32 Iterations);
	FVector ComputeSlideSlopeAcceleration() const;
	void PhysWallRun(float DeltaSeconds, int32 Iterations);
	void PhysTraversal(float DeltaSeconds, int32 Iterations, bool bMantle);
	void EnterSlide();
	void ExitSlide();
	void EnterWallRun(const FVector& WallNormal);
	void ExitWallRun();

	bool bWantsSprint = false;
	bool bWantsSlide = false;
	bool bSlideConsumedUntilRelease = false;
	bool bSlideQueued = false;
	/** The slide key was let go since this slide began. Read from the replicated slide intent, so client and server agree. */
	bool bSlideReleasedDuringSlide = false;
	/** Seconds on the ground since the last landing, counted only inside the landing grace. */
	float TimeSinceLanded = 1000.0f;
	float SpeedAllowance = 0.0f;
	bool bDevHopWasOn = false;
	float DevStrafeSide = 1.0f;
	bool bDevFirstJumpDone = false;
	/** Owner only: jump at the start of the next step, set by a landing with jump held or buffered. */
	bool bLandingJumpPending = false;
	float SlideInputBufferRemaining = 0.0f;
	float SlideElapsed = 0.0f;
	float TimeSinceSlideEnded = 1000.0f;
	FVector WallNormal = FVector::ZeroVector;
	UPROPERTY(Replicated)
	float Stamina = 100.0f;
	float TimeSinceStaminaUse = 0.0f;
	float WallRunElapsed = 0.0f;
	bool bAdvancedJumpRequested = false;
	bool bAdvancedWallJump = false;
	float WallReattachTimeRemaining = 0.0f;
	FVector LastWallNormal = FVector::ZeroVector;
	FVector TraversalStart = FVector::ZeroVector;
	FVector TraversalTarget = FVector::ZeroVector;
	float TraversalElapsed = 0.0f;
	float TraversalDuration = 0.0f;
};
