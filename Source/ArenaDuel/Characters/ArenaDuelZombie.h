#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Engine/NetSerialization.h"
#include "../Game/ArenaDuelZombieTypes.h"
#include "ArenaDuelZombie.generated.h"

class AArenaDuelCharacter;
class UAnimSequence;
class UInstancedStaticMeshComponent;
class UPointLightComponent;
enum class EArenaDuelShotResult : uint8;

/**
 * Locomotion for a zombie body: idle, walk or run by speed, with the arms taken off the clip. The clips
 * are rifle clips; a zombie holds nothing, so its arms reach forward and it strikes with them. Cosmetic only.
 */
UCLASS(Transient)
class ARENADUEL_API UArenaDuelZombieAnimInstance : public UAnimSingleNodeInstance
{
	GENERATED_BODY()
public:
	UArenaDuelZombieAnimInstance();
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;
	/** One-handed swings and, last, the two-handed one for slams and bosses. */
	const UAnimSequence* GetAttackClip(int32 Index) const { return AttackClips[FMath::Clamp(Index, 0, 2)]; }
protected:
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
private:
	UPROPERTY() TObjectPtr<UAnimSequence> Idle;
	UPROPERTY() TObjectPtr<UAnimSequence> Walk;
	UPROPERTY() TObjectPtr<UAnimSequence> Run;
	UPROPERTY() TObjectPtr<UAnimSequence> AttackClips[3];
};

/** A short spray of droplets where a shot lands on a zombie. Local and cosmetic; it removes itself. */
UCLASS(NotPlaceable, Transient)
class ARENADUEL_API AArenaDuelHitBurst : public AActor
{
	GENERATED_BODY()
public:
	AArenaDuelHitBurst();
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	/** Spawns a burst unless too many are already alive. ShotDirection is the direction the shot travelled. */
	static void Spawn(UWorld* World, const FVector& Location, const FVector& ShotDirection, bool bHeavy);
private:
	void Launch(const FVector& ShotDirection, bool bHeavy);
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Drops;
	TArray<FVector> Positions;
	TArray<FVector> Velocities;
	float Age = 0.0f;
	float DropScale = 0.05f;
	static int32 AliveBursts;
};

/**
 * A survival enemy. The server runs its behaviour: find the nearest living player, path to them over
 * the navigation mesh, swing when in reach with a clear line, and for the bosses a charge and an area slam.
 * Weapons hit it on the same per-bone hit zones as a player. Hits make it flinch and slow down for a moment;
 * the swing, the flinch and the fall are shown on every machine.
 */
UCLASS()
class ARENADUEL_API AArenaDuelZombie : public ACharacter
{
	GENERATED_BODY()

public:
	AArenaDuelZombie();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Server only, right after spawning. HealthScale and SpeedScale grow with the wave number. */
	void InitializeZombie(EArenaDuelZombieType InType, const FArenaDuelZombieTypeConfig& InConfig, float HealthScale, float DamageScale, float SpeedScale = 1.0f);
	/** Same contract as AArenaDuelCharacter::TraceHitZones. */
	bool TraceHitZones(const FVector& Start, const FVector& End, FHitResult& OutHit) const;
	/** Server only. BaseDamage is the weapon's body damage; the zone and the type's factors are applied here. Returns the damage dealt. */
	float TakeWeaponHit(float BaseDamage, float WeaponZoneMultiplier, EArenaDuelShotResult Zone, AController* InstigatorController, const FVector& HitLocation, const FVector& Direction);
	/** Server only. Kills without awarding anything, for clean-up and tests. */
	void KillSilently();
	/** Server only. Kills and credits the kill, for the development auto play. */
	void KillCredited(AController* Killer);

	bool IsDead() const { return bDead; }
	EArenaDuelZombieType GetZombieType() const { return ZombieType; }
	bool IsBossType() const { return ZombieType == EArenaDuelZombieType::Boss || ZombieType == EArenaDuelZombieType::MiniBoss; }
	float GetHealthFraction() const { return MaxHealth > 0.0f ? FMath::Clamp(Health / MaxHealth, 0.0f, 1.0f) : 0.0f; }
	float GetHealth() const { return Health; }
	float GetMaxHealth() const { return MaxHealth; }
	const FString& GetDisplayName() const { return Config.DisplayName; }
	/** Seconds since this zombie last got closer to its target or attacked. The wave manager moves the ones that are stuck. */
	float GetSecondsWithoutProgress() const { return SecondsWithoutProgress; }
	void ResetProgress() { SecondsWithoutProgress = 0.0f; BestTargetDistance = TNumericLimits<float>::Max(); }
	/** True while it steers straight at the player because no path was found. */
	bool IsDirectChasing() const { return bDirectChase; }
	/** Progress of the swing being shown, 0 to 1, or below zero when there is none. Cosmetic. */
	float GetAttackPhase() const;
	/** When the swing being shown began and how long it takes to land, in world seconds. Cosmetic. */
	float GetAttackStartTime() const { return AttackAnimStart; }
	float GetAttackDuration() const { return AttackAnimEnd - AttackAnimStart; }
	/** True when the swing being shown uses both arms: the slam, and everything a boss does. */
	bool IsTwoArmedAttack() const { return bAttackBothArms || IsBossType(); }
	/** How often this zombie had to free itself from standing still. */
	int32 GetUnstickCount() const { return UnstickCount; }
	/** How long a hit slows a zombie down, and to which share of its speed. Pure, for tests. */
	static float ComputeStaggerSeconds(EArenaDuelZombieType Type, float DamageFraction, bool bHead);

protected:
	UFUNCTION() void OnRep_ZombieType();
	UFUNCTION() void OnRep_Dead();
	/** Shows the swing or slam on every machine: the warning light and the body's lunge for its duration. */
	UFUNCTION(NetMulticast, Unreliable) void MulticastTelegraph(float Seconds, bool bSlam);
	/** Shows a hit on every machine: the flinch, the droplets and the sound. */
	UFUNCTION(NetMulticast, Unreliable) void MulticastHitReact(FVector_NetQuantize Location, FVector_NetQuantizeNormal Direction, bool bHead, bool bHeavy);

	void ServerThink(float DeltaSeconds);
	void UpdateCosmeticPose(float DeltaSeconds);
	AArenaDuelCharacter* FindTarget() const;
	bool HasLineTo(const AArenaDuelCharacter* Target) const;
	void ApplyTypeVisual();
	void Die(AController* Killer, bool bHeadshot);
	void StartRagdoll();
	void PlayZombieSound(const TCHAR* Path, float Volume, float Pitch) const;

	UPROPERTY(VisibleAnywhere) TObjectPtr<UPointLightComponent> WarningLight;

	UPROPERTY(ReplicatedUsing = OnRep_ZombieType) EArenaDuelZombieType ZombieType = EArenaDuelZombieType::Normal;
	UPROPERTY(Replicated) float Health = 100.0f;
	UPROPERTY(Replicated) float MaxHealth = 100.0f;
	UPROPERTY(Replicated) float VisualScale = 1.0f;
	UPROPERTY(Replicated) FString MaterialPath;
	UPROPERTY(Replicated) FString MeshPath;
	UPROPERTY(ReplicatedUsing = OnRep_Dead) bool bDead = false;
	UPROPERTY(Replicated) FVector_NetQuantize DeathHitLocation;
	UPROPERTY(Replicated) FVector_NetQuantizeNormal DeathHitDirection;
	/** How hard the killing hit throws the body. */
	UPROPERTY(Replicated) float DeathImpulse = 3500.0f;
	UPROPERTY(Replicated) bool bDeathHeadshot = false;

	// Server only.
	FArenaDuelZombieTypeConfig Config;
	float DamageScale = 1.0f;
	float BaseMoveSpeed = 330.0f;
	float ThinkAccumulator = 0.0f;
	float NextAttackTime = 0.0f;
	float WindupEndTime = 0.0f;
	bool bWindingUp = false;
	bool bSlamWindup = false;
	float SpecialReadyTime = 0.0f;
	float ChargeEndTime = 0.0f;
	float SecondsWithoutProgress = 0.0f;
	float BestTargetDistance = TNumericLimits<float>::Max();
	FVector ApproachOffset = FVector::ZeroVector;
	bool bDirectChase = false;
	float NextRepathTime = 0.0f;
	float NextOffsetRollTime = 0.0f;
	float StaggerEndTime = 0.0f;
	FVector MoveSampleLocation = FVector::ZeroVector;
	float MoveSampleTime = 0.0f;
	float SidestepEndTime = 0.0f;
	FVector SidestepDirection = FVector::ZeroVector;
	int32 UnstickCount = 0;
	float NextGrowlTime = 0.0f;

	// Cosmetic, every machine.
	float WarningLightOffTime = 0.0f;
	bool bVisualApplied = false;
	float AttackAnimStart = 0.0f;
	float AttackAnimEnd = 0.0f;
	float AttackLean = 0.0f;
	bool bAttackBothArms = false;
	float HitLean = 0.0f;
	float HitLeanYaw = 0.0f;
	bool bPoseDirty = false;
	float NextHitSoundTime = 0.0f;
};
