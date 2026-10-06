// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TimerManager.h"
#include "ArenaDuelWeaponComponent.generated.h"

class UInputAction;
class UStaticMeshComponent;

UENUM(BlueprintType)
enum class EArenaDuelWeaponId : uint8
{
	ArcRifle,
	ShadeSMG,
	RuneDMR,
	HexShotgun
};

UENUM(BlueprintType)
enum class EArenaDuelShotResult : uint8
{
	Miss,
	World,
	Body,
	Head
};

USTRUCT(BlueprintType)
struct ARENADUEL_API FArenaDuelWeaponRuntimeState
{
	GENERATED_BODY()

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly)
	int32 MagazineAmmo = 0;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly)
	int32 ReserveAmmo = 0;
};

USTRUCT(BlueprintType)
struct ARENADUEL_API FArenaDuelWeaponDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	EArenaDuelWeaponId Id = EArenaDuelWeaponId::ArcRifle;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FName DisplayName = TEXT("Arc Rifle");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	int32 MagazineCapacity = 30;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	int32 ReserveCapacity = 120;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	float RoundsPerMinute = 650.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	float Range = 10000.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	float BaseSpreadDegrees = 0.35f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	float MovementSpreadDegrees = 1.25f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	float RecoilVertical = 0.35f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	float RecoilHorizontal = 0.08f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	float AimFOV = 78.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	float AimSensitivityMultiplier = 0.8f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	float BodyDamage = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	float HeadshotMultiplier = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	float AimSpreadMultiplier = 0.65f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FVector AimViewmodelLocation = FVector(55.0f, 2.0f, -12.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FRotator AimViewmodelRotation = FRotator::ZeroRotator;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	float ReloadDuration = 1.8f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	int32 Pellets = 1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	bool bAutomatic = true;
};

UCLASS(ClassGroup=(Weapons), meta=(BlueprintSpawnableComponent))
class ARENADUEL_API UArenaDuelWeaponComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UArenaDuelWeaponComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	void StartFire();
	void StopFire();
	void Reload();
	void EquipWeapon(int32 Index);
	void StartAim();
	void StopAim();
	void CancelCombatActions();

	const FArenaDuelWeaponDefinition& GetCurrentDefinition() const;
	int32 GetWeaponDefinitionCount() const { return WeaponDefinitions.Num(); }
	const FArenaDuelWeaponDefinition* GetWeaponDefinition(int32 Index) const { return WeaponDefinitions.IsValidIndex(Index) ? &WeaponDefinitions[Index] : nullptr; }
	EArenaDuelWeaponId GetCurrentWeaponId() const;
	FName GetCurrentWeaponName() const;
	int32 GetCurrentMagazineAmmo() const;
	int32 GetReserveAmmo() const;
	bool IsReloading() const { return bReloading; }
	bool IsFireHeld() const { return bFireHeld; }
	bool IsAiming() const { return bAiming; }
	float GetAimSensitivityMultiplier() const { return GetCurrentDefinition().AimSensitivityMultiplier; }
	EArenaDuelShotResult GetLastShotResult() const { return LastShotResult; }
	float GetLastShotDistance() const { return LastShotDistance; }
	float GetLastShotAge() const;
	int32 GetLastShotSequence() const { return LastShotSequence; }
	int32 GetLastPelletsHit() const { return LastPelletsHit; }
	int32 GetLastHeadPellets() const { return LastHeadPellets; }
	float GetCurrentSpreadDegrees() const;
	float GetCrosshairKick() const;

protected:
	UFUNCTION(Server, Reliable)
	void ServerSetFireHeld(bool bHeld);

	UFUNCTION(Server, Reliable)
	void ServerSetAiming(bool bAimingState);

	UFUNCTION(Server, Reliable)
	void ServerRequestReload();

	UFUNCTION(Server, Reliable)
	void ServerRequestEquip(int32 Index);

	void FireAuthoritative();
	void StartAuthoritativeFire();
	void StopAuthoritativeFire();
	bool CanBeginAuthoritativeFire() const;
	bool IsRoundInProgress() const;
	void CancelLocalAndServerFire();
	void UpdateAimVisual();
	void ApplyLocalRecoil();
	void LocalCosmeticShot();
	void RecoverCosmeticKick();
	void CompleteReload();
	void RefreshWeaponVisual();
	void GetCurrentViewmodelBaseTransform(FVector& OutLocation, FRotator& OutRotation) const;
	void SetLastShot(EArenaDuelShotResult Result, float Distance, AActor* Target);
	void InitializeRuntimeAmmo();
	FArenaDuelWeaponRuntimeState* GetMutableCurrentRuntimeState();
	const FArenaDuelWeaponRuntimeState* GetCurrentRuntimeState() const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapons")
	TArray<FArenaDuelWeaponDefinition> WeaponDefinitions;

	UPROPERTY(ReplicatedUsing=OnRep_EquippedWeapon, VisibleInstanceOnly, BlueprintReadOnly, Category="Weapons")
	uint8 EquippedWeaponIndex = 0;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category="Weapons")
	TArray<FArenaDuelWeaponRuntimeState> RuntimeAmmo;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category="Weapons")
	bool bReloading = false;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category="Weapons")
	bool bAiming = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Weapons")
	TObjectPtr<UStaticMeshComponent> FirstPersonWeaponMesh;

	FTimerHandle ReloadTimerHandle;

	EArenaDuelShotResult LastShotResult = EArenaDuelShotResult::Miss;
	float LastShotDistance = 0.0f;
	TWeakObjectPtr<AActor> LastShotTarget;
	int32 LastShotSequence = 0;
	int32 LastPelletsHit = 0;
	int32 LastHeadPellets = 0;
	float LastShotWorldTime = -1.0f;
	bool bFireHeld = false;
	bool bServerFireHeld = false;
	float FireCooldownRemaining = 0.0f;
	FTimerHandle AutomaticFireTimerHandle;
	FTimerHandle LocalCosmeticFireTimerHandle;
	FTimerHandle LocalCosmeticRecoveryTimerHandle;
	FTimerHandle AimVisualTimerHandle;
	TArray<double> NextAllowedFireServerTimes;
	float LocalWeaponKick = 0.0f;
	float LocalRecoilPitchRemaining = 0.0f;
	float LocalRecoilYawRemaining = 0.0f;
	float LastCosmeticShotWorldTime = -1.0f;
	float HipFOV = 90.0f;

	UFUNCTION()
	void OnRep_EquippedWeapon();

	UFUNCTION(Client, Unreliable)
	void ClientShotConfirmation(int32 Sequence, EArenaDuelShotResult Result, float Distance, int32 PelletsHit, int32 HeadPellets);
};
