// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TimerManager.h"
#include "Engine/NetSerialization.h"
#include "ArenaDuelWeaponComponent.generated.h"

class UInputAction;
class UStaticMeshComponent;
class USceneComponent;
class UStaticMesh;
class UMaterialInterface;

USTRUCT(BlueprintType)
struct FArenaDuelWeaponVisualDefinition
{
	GENERATED_BODY()
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) TSoftObjectPtr<UStaticMesh> Mesh;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) FVector HipViewmodelLocation = FVector(35, 18, -34);
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) FRotator HipViewmodelRotation = FRotator::ZeroRotator;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) FVector FirstPersonGripLocation = FVector::ZeroVector;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) FRotator FirstPersonGripRotation = FRotator::ZeroRotator;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) FVector FirstPersonScale = FVector(1);
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) FVector LeftHandGripLocation = FVector::ZeroVector;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) FVector ThirdPersonGripLocation = FVector::ZeroVector;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) FRotator ThirdPersonGripRotation = FRotator::ZeroRotator;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) FVector ThirdPersonScale = FVector(0.75f);
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) float VisualRecoilKick = 1.0f;
};

// Cosmetic values only. Shot direction and server-side weapon balance never read this struct.
USTRUCT(BlueprintType)
struct FArenaDuelViewmodelFeel
{
	GENERATED_BODY()
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) float AimResponse = 14.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) float MotionResponse = 12.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) float RecoilResponse = 19.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) float SwayDegreesPerDegreePerSecond = 0.004f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) float MaxSwayDegrees = 2.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) float MovementBob = 0.65f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) float SprintBob = 1.2f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) float IdleBreathing = 0.12f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) float AdsMotionMultiplier = 0.22f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) float SprintLowering = 10.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) float LandingKick = 2.5f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) float WallPushback = 22.0f;
};

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
	Head,
	Limb
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

	// Arms, hands, legs and feet.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	float LimbDamageMultiplier = 0.8f;

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
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	void StartFire();
	void StopFire();
	void Reload();
	void EquipWeapon(int32 Index);
	void StartAim();
	void StopAim();
	void CancelCombatActions();
	void RefillAllAmmoForDevelopment();
	void RefreshWeaponVisual();
	void SetUserHipFOV(float NewFOV);
	void GetCurrentViewmodelBaseTransform(FVector& OutLocation, FRotator& OutRotation) const;
	UStaticMeshComponent* GetFirstPersonWeaponMesh() const { return FirstPersonWeaponMesh; }
	UStaticMeshComponent* GetThirdPersonWeaponMesh() const { return ThirdPersonWeaponMesh; }
	bool GetLeftHandGripWorldLocation(FVector& OutLocation) const;

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
	// Maps a physics asset bone to the zone it scores as: head, torso or limb.
	static EArenaDuelShotResult ClassifyHitBone(FName BoneName);
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
	bool HasInfiniteAmmoForDevelopment() const;
	bool IsLocalAdminMenuOpen() const;
	void CancelLocalAndServerFire(bool bClearLocalRecoil = false);
	void UpdateAimVisual();
	void TickLocalPresentation(float DeltaSeconds);
	void ApplyLocalRecoil();
	void LocalCosmeticShot();
	void RecoverCosmeticKick();
	void CompleteReload();
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
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Weapons")
	TObjectPtr<UStaticMeshComponent> ThirdPersonWeaponMesh;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapons|Presentation") TArray<FArenaDuelWeaponVisualDefinition> WeaponVisualDefinitions;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapons|Presentation") FArenaDuelViewmodelFeel ViewmodelFeel;
	UPROPERTY() TObjectPtr<UMaterialInterface> WeaponBodyMaterial;
	UPROPERTY() TObjectPtr<UMaterialInterface> WeaponAccentCyan;
	UPROPERTY() TObjectPtr<UMaterialInterface> WeaponAccentViolet;
	UPROPERTY() TObjectPtr<class USoundBase> FireSound;

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
	float LocalRecoilRecoveryTimeRemaining = 0.0f;
	float LastCosmeticShotWorldTime = -1.0f;
	float HipFOV = 90.0f;
	float AimBlend = 0.0f;
	float SprintBlend = 0.0f;
	float BobBlend = 0.0f;
	float BobPhase = 0.0f;
	float IdlePhase = 0.0f;
	float SwayYaw = 0.0f;
	float SwayYawVelocity = 0.0f;
	float SwayPitch = 0.0f;
	float SwayPitchVelocity = 0.0f;
	float RecoilVelocity = 0.0f;
	float VisualYawKick = 0.0f;
	float VisualYawVelocity = 0.0f;
	float LandingOffset = 0.0f;
	float LandingVelocity = 0.0f;
	float WallBlend = 0.0f;
	float WallTraceTime = 0.0f;
	float WallTarget = 0.0f;
	float EquipDrop = 0.0f;
	FRotator PreviousControlRotation = FRotator::ZeroRotator;
	float PreviousVerticalVelocity = 0.0f;
	bool bHadControlRotation = false;
	bool bWasFalling = false;
	bool bPresentationInitialized = false;

	UFUNCTION()
	void OnRep_EquippedWeapon();

	// Cosmetic only: lets every machine draw the muzzle flash and tracers of a server-validated shot.
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastShotFired(const TArray<FVector_NetQuantize>& TraceEnds);

	UFUNCTION(Client, Unreliable)
	void ClientShotConfirmation(int32 Sequence, EArenaDuelShotResult Result, float Distance, int32 PelletsHit, int32 HeadPellets);
};
