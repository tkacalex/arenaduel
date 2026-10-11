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
	/** How far the bolt (or the pump) travels back on a shot, in mesh units, and how long one cycle takes. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) float BoltTravel = 5.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) float BoltCycleSeconds = 0.09f;
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

	/** Telescopic sight. Right mouse then steps through two zoom levels and out again instead of aiming while held. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Scope")
	bool bHasScope = false;

	/** Field of view of the first and of the second zoom level, in degrees. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Scope", meta = (ClampMin = "2", ClampMax = "90"))
	float ScopeFOVFirst = 40.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Scope", meta = (ClampMin = "2", ClampMax = "90"))
	float ScopeFOVSecond = 15.0f;

	/** Mouse sensitivity in the scope follows the zoom, so the same hand movement covers the same distance on screen. This scales that. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Scope", meta = (ClampMin = "0.05", ClampMax = "4"))
	float ScopeSensitivityScale = 1.0f;

	/** Ground speed multiplier while looking through the scope. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Scope", meta = (ClampMin = "0.1", ClampMax = "1"))
	float ScopedMoveSpeedScale = 0.6f;

	/** After a shot the scope drops for this long and comes back to the same zoom level. 0 keeps it up. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Scope", meta = (ClampMin = "0"))
	float ScopeRezoomSeconds = 0.16f;

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

/** The three things a player can hold. A loadout may put more than one firearm into the primary slot. */
UENUM(BlueprintType)
enum class EArenaDuelLoadoutSlot : uint8
{
	Primary,
	Flashbang,
	Knife
};

/** What one character archetype carries each round. */
USTRUCT(BlueprintType)
struct FArenaDuelLoadoutDefinition
{
	GENERATED_BODY()

	/** Firearms in the primary slot as indices into WeaponDefinitions. The first one is in hand at round start. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TArray<uint8> Firearms;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (ClampMin = "0"))
	int32 Flashbangs = 1;
};

USTRUCT(BlueprintType)
struct FArenaDuelKnifeDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (ClampMin = "0")) float Range = 175.0f;
	/** Quick slash, left mouse. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (ClampMin = "0")) float Damage = 25.0f;
	/** Seconds until the next attack after a quick slash. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (ClampMin = "0.05")) float AttackInterval = 0.4f;
	/** Heavy stab, right mouse. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (ClampMin = "0")) float HeavyDamage = 70.0f;
	/** Seconds until the next attack after a heavy stab. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (ClampMin = "0.05")) float HeavyAttackInterval = 1.0f;
	/** Half width of the swing at full range. Several rays are fanned across it so a moving target is not missed by a hair. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (ClampMin = "0")) float SwingHalfWidth = 14.0f;
};

USTRUCT(BlueprintType)
struct FArenaDuelFlashbangDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (ClampMin = "0")) float ThrowSpeed = 1500.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) float ThrowUpSpeed = 220.0f;
	/** Short lob, right mouse. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (ClampMin = "0")) float ShortThrowSpeed = 650.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) float ShortThrowUpSpeed = 260.0f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (ClampMin = "0.1")) float FuseSeconds = 1.4f;
	/** Beyond this distance the flash has no effect. Large enough that a full-strength throw still blinds the thrower who watches it go off. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (ClampMin = "1")) float MaxBlindDistance = 3500.0f;
	/** Blind time for a point blank flash looked at directly. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (ClampMin = "0")) float MaxBlindSeconds = 3.2f;
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
	/** Switches to a slot. Selecting the primary slot again swaps between its firearms when there are two. */
	void SelectSlot(EArenaDuelLoadoutSlot Slot);
	void CyclePrimaryFirearm();
	/** Mouse wheel: steps through every usable item, both firearms of a two gun loadout included. */
	void CycleSlot(int32 Direction);
	EArenaDuelLoadoutSlot GetActiveSlot() const { return static_cast<EArenaDuelLoadoutSlot>(ActiveSlot); }
	int32 GetFlashbangsRemaining() const { return FlashbangsRemaining; }
	const TArray<uint8>& GetLoadoutFirearms() const { return LoadoutFirearms; }
	/** Authority only. Lifts the loadout restriction so admin tools and tests can equip any firearm. */
	void GrantAllWeaponsForDevelopment();
	void StartAim();
	void StopAim();
	/** True while the aim key is held, also while a reload or switch has paused the aim itself. */
	bool IsAimHeld() const { return bAimHeld; }
	/** 0 to 1: how far the aim accuracy bonus has come in since aiming began. */
	float GetAimAccuracyAlpha() const;
	void CancelCombatActions();
	void RefillAllAmmoForDevelopment();
	/** Server only. Adds this share of each weapon's reserve capacity to its reserve, up to the capacity. */
	void AddReserveAmmoShare(float Share);
	void RefreshWeaponVisual();
	void SetUserHipFOV(float NewFOV);
	void GetCurrentViewmodelBaseTransform(FVector& OutLocation, FRotator& OutRotation) const;
	/** The viewmodel root transform, relative to the camera, that puts the first person weapon on the line of sight. False without a weapon mesh. */
	bool SolveAimSightTransform(FVector& OutLocation, FQuat& OutRotation) const;
	UStaticMeshComponent* GetFirstPersonWeaponMesh() const { return FirstPersonWeaponMesh; }
	UStaticMeshComponent* GetThirdPersonWeaponMesh() const { return ThirdPersonWeaponMesh; }
	bool GetLeftHandGripWorldLocation(FVector& OutLocation, bool bThirdPerson = false) const;

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
	/** Cosmetic: which firearm is in hand and when the trigger was last pulled on this machine. */
	int32 GetEquippedWeaponIndex() const { return EquippedWeaponIndex; }
	float GetLastTriggerWorldTime() const { return PartShotWorldTime; }
	/** Cosmetic, on every machine: when the last knife swing and the last throw were shown, for the world body's arm. */
	float GetKnifeSwingWorldTime() const { return KnifeSwingStartWorldTime; }
	bool WasKnifeSwingHeavy() const { return bKnifeSwingHeavy; }
	float GetThrowWorldTime() const { return ThrowStartWorldTime; }
	bool WasThrowShort() const { return bThrowShort; }
	/** How the world weapon sits in the hand socket when it is not steered by the aim. */
	FRotator GetThirdPersonGripRotation() const { return WeaponVisualDefinitions.IsValidIndex(EquippedWeaponIndex) ? WeaponVisualDefinitions[EquippedWeaponIndex].ThirdPersonGripRotation : FRotator::ZeroRotator; }
	float GetAimSensitivityMultiplier() const;
	/** 0 none, 1 first zoom, 2 second zoom. Local to the owning player. */
	int32 GetScopeLevel() const { return ScopeLevel; }
	/** Field of view the current aim state settles at: the scope level's for a scoped weapon, else the weapon's aim FOV. */
	float GetActiveAimFOV() const;
	/** 0 to 1: how much of the scope picture, mask and reticle, is shown. Follows the weapon coming up. */
	float GetScopeOverlayAlpha() const;
	/** Ground speed multiplier from looking through a scope, 1 otherwise. */
	float GetAimMoveSpeedScale() const;
	/** The aim key was let go. A held aim stops; a scope stays, since it is stepped with presses. */
	void ReleaseAim();
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

	UFUNCTION(Server, Reliable)
	void ServerSelectSlot(uint8 Slot);

	UFUNCTION(Server, Reliable)
	void ServerCycleSlot(int8 Direction);

	/** Throws the flashbang or swings the knife, whichever is in hand. */
	UFUNCTION(Server, Reliable)
	void ServerUseEquipment(bool bAlternate);

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastKnifeSwing(bool bHit, bool bHeavy);
	/** The throw as everyone hears it and the thrower sees it. The grenade itself is the server's. */
	UFUNCTION(NetMulticast, Unreliable) void MulticastEquipmentThrown(bool bShort);

	void ApplyLoadoutFromArchetype();
	bool CanSwitchAuthoritative() const;
	void SetActiveSlotAuthoritative(EArenaDuelLoadoutSlot Slot);
	void ThrowFlashbangAuthoritative(bool bShort);
	void KnifeAttackAuthoritative(bool bHeavy);

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
	/** Local input intent. Not replicated; the server only ever sees the resulting aim state. */
	bool bAimHeld = false;
	float AimStartWorldTime = -1000.0f;
	float AimBlockedUntilWorldTime = 0.0f;
	double LastLandingWorldTime = -1000.0;
	uint8 ScopeLevel = 0;
	float ScopeFOVSmoothed = 0.0f;
	double ScopeSuppressedUntilWorldTime = 0.0;
	bool bViewmodelHiddenByScope = false;
	/** Applied on top of every weapon's own aim sensitivity. Below 1 the mouse turns slower while zoomed. */
	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Aim", meta = (ClampMin = "0.1", ClampMax = "2"))
	float AimSensitivityScale = 0.85f;
	/** Seconds until the aim accuracy bonus is fully in after aiming begins. */
	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Aim", meta = (ClampMin = "0"))
	float AimSettleSeconds = 0.12f;
	void SuspendAim();
	bool CanAimNow(bool bIgnoreSwitchBlock = false) const;
	void UpdateAimFromIntent();

	UPROPERTY(ReplicatedUsing=OnRep_EquippedWeapon, VisibleInstanceOnly, BlueprintReadOnly, Category="Loadout")
	uint8 ActiveSlot = 0;

	/** Firearms this player may equip this round. */
	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category="Loadout")
	TArray<uint8> LoadoutFirearms;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category="Loadout")
	uint8 FlashbangsRemaining = 0;

	/** One entry per character archetype, in enum order. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Loadout")
	TArray<FArenaDuelLoadoutDefinition> Loadouts;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Loadout")
	FArenaDuelKnifeDefinition Knife;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Loadout")
	FArenaDuelFlashbangDefinition Flashbang;

	/** Seconds after a switch before the new item can be used. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Loadout", meta = (ClampMin = "0"))
	float SwitchSeconds = 0.25f;

	int32 AppliedLoadoutArchetype = -1;
	bool bLoadoutUnrestricted = false;
	double EquipmentReadyServerTime = 0.0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Weapons")
	TObjectPtr<UStaticMeshComponent> FirstPersonWeaponMesh;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Weapons")
	TObjectPtr<UStaticMeshComponent> ThirdPersonWeaponMesh;
	/** Moving parts of the firearm in hand, in first person and on the world body: the bolt (the pump on the shotgun) and the magazine. Cosmetic. */
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> FirstPersonBoltMesh;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> FirstPersonMagMesh;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> ThirdPersonBoltMesh;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> ThirdPersonMagMesh;
	float PartShotWorldTime = -10.0f;
	float PartReloadStartWorldTime = -100.0f;
	bool bPartReloadSeen = false;
	bool bPartsMoved = false;
	void RefreshWeaponParts(const FArenaDuelWeaponVisualDefinition& Visual, bool bFirearm, UMaterialInterface* Accent);
	void TickWeaponParts();
	bool AreWeaponPartsBusy() const;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapons|Presentation") TArray<FArenaDuelWeaponVisualDefinition> WeaponVisualDefinitions;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapons|Presentation") FArenaDuelViewmodelFeel ViewmodelFeel;
	UPROPERTY() TObjectPtr<UMaterialInterface> WeaponBodyMaterial;
	UPROPERTY() TObjectPtr<UMaterialInterface> WeaponAccentCyan;
	UPROPERTY() TObjectPtr<UMaterialInterface> WeaponAccentViolet;
	UPROPERTY() TObjectPtr<class USoundBase> FireSound;

	// A tracer keeps its barrel end on the muzzle while it is visible, so it does not drift off a moving gun.
	struct FActiveTracer
	{
		TWeakObjectPtr<UStaticMeshComponent> Mesh;
		FVector End = FVector::ZeroVector;
		float ExpireWorldTime = 0.0f;
	};
	TArray<FActiveTracer> ActiveTracers;
	// Shot effects are created once and reused. Spawning and destroying render components per shot
	// at automatic fire rates caused frame time spikes.
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> TracerPool;
	UPROPERTY(Transient) TObjectPtr<class UPointLightComponent> MuzzleFlashLight;
	/** Knife or flashbang model in hand, built in code on first use. */
	UPROPERTY(Transient) TObjectPtr<UStaticMesh> ItemModel;
	UPROPERTY() TObjectPtr<UMaterialInterface> KnifeBladeMaterial;
	float MuzzleFlashOffWorldTime = 0.0f;
	UStaticMeshComponent* AcquireTracerMesh(UMaterialInterface* Material);
	FVector GetTracerStart() const;
	void UpdateTracers();

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
	// Cosmetic, local: when the last knife swing and the last throw began.
	float KnifeSwingStartWorldTime = -10.0f;
	bool bKnifeSwingHeavy = false;
	float ThrowStartWorldTime = -10.0f;
	bool bThrowShort = false;
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
