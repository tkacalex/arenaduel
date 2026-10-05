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
	void StartCrouchOrSlide();
	void StopCrouchOrSlide();
	bool IsSprinting() const { return bWantsSprint && IsMovingOnGround() && !IsSliding(); }
	bool IsSliding() const { return MovementMode == MOVE_Custom && CustomMovementMode == static_cast<uint8>(EArenaDuelCustomMovementMode::Slide); }
	bool IsWallRunning() const { return MovementMode == MOVE_Custom && CustomMovementMode == static_cast<uint8>(EArenaDuelCustomMovementMode::WallRun); }
	float GetStamina() const { return Stamina; }
	float GetMaxStamina() const { return MaxStamina; }
	bool WantsSprintIntent() const { return bWantsSprint; }
	bool WantsCrouchSlideIntent() const { return bWantsCrouchOrSlide; }
	bool IsSlideQueued() const { return bSlideQueued; }
	void SetSprintIntentFromNetwork(bool bWantsSprintIntent);
	void SetCrouchSlideIntentFromNetwork(bool bWantsCrouchSlideIntent);
	void ConsumeStamina(float Amount);
	bool IsMantling() const { return MovementMode == MOVE_Custom && CustomMovementMode == static_cast<uint8>(EArenaDuelCustomMovementMode::Mantle); }

	bool TrySlideJump();
	bool TryWallJump();
	void QueueAdvancedJump(bool bWallJump);
	void ClearAdvancedJumpIntent();
	bool HasAdvancedJumpIntent() const { return bAdvancedJumpRequested; }
	bool WantsWallJumpIntent() const { return bAdvancedWallJump; }

	virtual float GetMaxSpeed() const override;
	virtual FNetworkPredictionData_Client* GetPredictionData_Client() const override;
	virtual void UpdateFromCompressedFlags(uint8 Flags) override;
	virtual void UpdateCharacterStateBeforeMovement(float DeltaSeconds) override;
	virtual void PhysCustom(float DeltaSeconds, int32 Iterations) override;
	virtual void PhysFalling(float DeltaSeconds, int32 Iterations) override;
	virtual void OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode) override;
	virtual void TickComponent(float DeltaSeconds, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement|Ground")
	float WalkSpeed = 600.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement|Ground")
	float SprintSpeed = 900.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement|Ground")
	float CrouchSpeed = 350.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement|Slide")
	float SlideMinSpeed = 700.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement|Slide")
	float SlideInitialBoost = 150.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement|Slide")
	float SlideEntrySpeed = 1025.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement|Slide")
	float SlideFriction = 0.72f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement|Slide")
	float SlideSteering = 0.25f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement|Slide")
	float SlideEndSpeed = 400.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement|Slide")
	float SlideMinDuration = 0.4f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement|Slide")
	float SlideInputBuffer = 0.3f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement|Slide")
	float SlideQueueMinSpeed = 560.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement|Slide")
	float SlideJumpHorizontalRetention = 0.9f;

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
	void PhysWallRun(float DeltaSeconds, int32 Iterations);
	void PhysTraversal(float DeltaSeconds, int32 Iterations, bool bMantle);
	void EnterSlide();
	void ExitSlide();
	void EnterWallRun(const FVector& WallNormal);
	void ExitWallRun();

	bool bWantsSprint = false;
	bool bWantsCrouchOrSlide = false;
	bool bSlideQueued = false;
	float SlideInputBufferRemaining = 0.0f;
	float SlideElapsed = 0.0f;
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
