// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "TimerManager.h"
#include "Engine/NetSerialization.h"
#include "ArenaDuelCharacter.generated.h"

class UCameraComponent;
class UBoxComponent;
class UStaticMeshComponent;
class USkeletalMeshComponent;
class USceneComponent;
class UMaterialInterface;
class UInputAction;
class UInputMappingContext;
class UArenaDuelCharacterMovementComponent;
class UArenaDuelWeaponComponent;
class UAbilitySystemComponent;
class UArenaDuelAttributeSet;
struct FInputActionValue;

UCLASS()
class ARENADUEL_API AArenaDuelCharacter : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AArenaDuelCharacter(const FObjectInitializer& ObjectInitializer);
	virtual void BeginPlay() override;
	void RefreshCharacterVisuals();
	USkeletalMeshComponent* GetFirstPersonArms() const { return FirstPersonArms; }
	USceneComponent* GetFirstPersonViewmodelRoot() const { return FirstPersonViewmodelRoot; }

	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_PlayerState() override;
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	UArenaDuelCharacterMovementComponent* GetArenaDuelMovementComponent() const;
	UCameraComponent* GetFirstPersonCamera() const { return FirstPersonCamera; }
	UArenaDuelWeaponComponent* GetWeaponComponent() const { return WeaponComponent; }
	bool IsCrouchInputHeld() const { return bCrouchInputHeld; }
	bool IsDead() const { return bDead; }
	void SetRoundInputLocked(bool bLocked);
	bool CanProcessGameplayInput() const;
	float GetHealth() const;
	float GetMaxHealth() const;
	void ApplyServerDamage(float DamageAmount);
	// Server only. Remembers where and from which direction the latest hit landed, for the death ragdoll push.
	void RecordServerHit(const FVector& WorldLocation, const FVector& Direction);
	// Line trace against the animated physics bodies of the world mesh. OutHit.BoneName names the body hit.
	bool TraceHitZones(const FVector& Start, const FVector& End, FHitResult& OutHit) const;
	void AdminSetHealth(float NewHealth);
	void AdminKill();
	void AdminResetPlayer();
	void PlayShadowStepCameraImpulse();
	void PlayRiftCameraImpulse();
	virtual void CalcCamera(float DeltaTime, FMinimalViewInfo& OutResult) override;
	virtual void OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;
	virtual void OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;
	float GetSlideCameraBlend() const { return SlideCameraBlend; }

	/** Extra field of view at full slide, in degrees. Local presentation only. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Camera|Movement")
	float SlideCameraFOVKick = 7.0f;

	/** Constant camera tilt while sliding, in degrees. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Camera|Movement")
	float SlideCameraBaseRoll = 2.0f;

	/** Additional tilt toward the steering direction while sliding, in degrees. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Camera|Movement")
	float SlideCameraSteerRoll = 3.5f;

	/** Additional camera drop below crouch height while sliding, in cm. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Camera|Movement")
	float SlideCameraExtraDrop = 10.0f;

	/** How quickly the eye height follows crouch and uncrouch. Higher is snappier. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Camera|Movement", meta = (ClampMin = "0.1"))
	float CrouchCameraInterpSpeed = 12.0f;

	/** How quickly slide FOV, tilt and drop blend in and out. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Camera|Movement", meta = (ClampMin = "0.1"))
	float SlideCameraInterpSpeed = 10.0f;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Combat")
	TObjectPtr<UBoxComponent> BodyHitZone;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Combat")
	TObjectPtr<UBoxComponent> HeadHitZone;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Development")
	TObjectPtr<UStaticMeshComponent> BodyVisual;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Development")
	TObjectPtr<UStaticMeshComponent> HeadVisual;

protected:
	virtual void PawnClientRestart() override;

	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);
	void JumpStarted();
	void JumpCompleted();
	void SprintStarted();
	void SprintCompleted();
	void CrouchStarted();
	void CrouchCompleted();
	void SlideStarted();
	void SlideCompleted();
	void WeaponFireStarted();
	void WeaponFireCompleted();
	void AimStarted();
	void AimCompleted();
	void WeaponReloadStarted();
	void Weapon1Started();
	void Weapon2Started();
	void Weapon3Started();
	void Weapon4Started();
	void PrimaryAbilityStarted();
	void SecondaryAbilityStarted();
	void InitializeAbilityActorInfo();
	void HandleDeath();
	void ApplyDevelopmentDeathPose();
	void StartLocalDeathCamera();
	void UpdateLocalDeathCamera();
	void UpdateShadowStepCameraImpulse();
	void StartDeathRagdoll();
	void ConfigureHitZoneCollision();
	void SettleDeathRagdoll();
	FRotator GetThirdPersonMeshBaseRotation() const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCameraComponent> FirstPersonCamera;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Visuals")
	TObjectPtr<USceneComponent> FirstPersonViewmodelRoot;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Visuals")
	TObjectPtr<USkeletalMeshComponent> FirstPersonArms;
	UPROPERTY() TArray<TObjectPtr<UMaterialInterface>> ArchetypeArmorMaterials;
	UPROPERTY() TObjectPtr<UMaterialInterface> CyanVisualMaterial;
	UPROPERTY() TObjectPtr<UMaterialInterface> VioletVisualMaterial;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> LeftShoulderArmor;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> RightShoulderArmor;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> LookAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> JumpAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> SprintAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> CrouchAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> SlideAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> FireAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> AimAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> ReloadAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> Weapon1Action;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> Weapon2Action;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> Weapon3Action;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> Weapon4Action;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapons", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UArenaDuelWeaponComponent> WeaponComponent;

	bool bCrouchInputHeld = false;

	UPROPERTY(ReplicatedUsing=OnRep_Dead, VisibleInstanceOnly, BlueprintReadOnly, Category="Combat")
	bool bDead = false;

	UFUNCTION() void OnRep_Dead();
	// Replicated with bDead so every machine pushes its local ragdoll the same way.
	UPROPERTY(Replicated) FVector_NetQuantize DeathHitLocation;
	UPROPERTY(Replicated) FVector_NetQuantizeNormal DeathHitDirection;
	void SetDeadState();

	FTimerHandle LocalDeathCameraTimer;
	FTimerHandle DeathPoseTimer;
	float DeathPoseStartTime = 0.0f;
	FRotator DeathPoseStartRotation = FRotator::ZeroRotator;
	FTimerHandle ShadowStepCameraTimer;
	FRotator ShadowStepCameraBaseRotation = FRotator::ZeroRotator;
	float ShadowStepCameraStartTime = 0.0f;
	FVector LocalDeathCameraStartLocation = FVector::ZeroVector;
	FRotator LocalDeathCameraStartRotation = FRotator::ZeroRotator;
	float LocalDeathCameraStartTime = 0.0f;
	bool bDeathPresentationLatched = false;
	void UpdateLocalMovementCamera(float DeltaSeconds);
	float CrouchEyeOffset = 0.0f;
	float SlideCameraBlend = 0.0f;
	float SlideCameraRoll = 0.0f;
	FVector CameraBaseRelativeLocation = FVector(0.0f, 0.0f, 64.0f);
	FVector LivingMeshRelativeLocation = FVector::ZeroVector;
	FRotator LivingMeshRelativeRotation = FRotator::ZeroRotator;
};
