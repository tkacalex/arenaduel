// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "TimerManager.h"
#include "ArenaDuelCharacter.generated.h"

class UCameraComponent;
class UBoxComponent;
class UStaticMeshComponent;
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
	void AdminSetHealth(float NewHealth);
	void AdminKill();
	void AdminResetPlayer();
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
	void InitializeAbilityActorInfo();
	void HandleDeath();
	void ApplyDevelopmentDeathPose();
	void StartLocalDeathCamera();
	void UpdateLocalDeathCamera();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCameraComponent> FirstPersonCamera;

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
	void SetDeadState();

	FTimerHandle LocalDeathCameraTimer;
	FVector LocalDeathCameraStartLocation = FVector::ZeroVector;
	FRotator LocalDeathCameraStartRotation = FRotator::ZeroRotator;
	float LocalDeathCameraStartTime = 0.0f;
};
