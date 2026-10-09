#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Engine/NetSerialization.h"
#include "../Game/ArenaDuelZombieTypes.h"
#include "ArenaDuelZombie.generated.h"

class AArenaDuelCharacter;
class UAnimSequence;
class UPointLightComponent;
enum class EArenaDuelShotResult : uint8;

/** Locomotion for a zombie body: idle, walk or run by speed. Cosmetic only. */
UCLASS(Transient)
class ARENADUEL_API UArenaDuelZombieAnimInstance : public UAnimSingleNodeInstance
{
	GENERATED_BODY()
public:
	UArenaDuelZombieAnimInstance();
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;
private:
	UPROPERTY() TObjectPtr<UAnimSequence> Idle;
	UPROPERTY() TObjectPtr<UAnimSequence> Walk;
	UPROPERTY() TObjectPtr<UAnimSequence> Run;
};

/**
 * A survival enemy. The server runs its behaviour: find the nearest living player, path to them over
 * the navigation mesh (or steer straight when there is none), swing when in reach with a clear line,
 * and for the bosses a charge and an area slam. Weapons hit it on the same per-bone hit zones as a player.
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

	/** Server only, right after spawning. HealthScale grows with the wave number. */
	void InitializeZombie(EArenaDuelZombieType InType, const FArenaDuelZombieTypeConfig& InConfig, float HealthScale, float DamageScale);
	/** Same contract as AArenaDuelCharacter::TraceHitZones. */
	bool TraceHitZones(const FVector& Start, const FVector& End, FHitResult& OutHit) const;
	/** Server only. BaseDamage is the weapon's body damage; the zone and the type's factors are applied here. Returns the damage dealt. */
	float TakeWeaponHit(float BaseDamage, float WeaponZoneMultiplier, EArenaDuelShotResult Zone, AController* InstigatorController, const FVector& HitLocation, const FVector& Direction);
	/** Server only. Kills without awarding anything, for clean-up and tests. */
	void KillSilently();

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

protected:
	UFUNCTION() void OnRep_ZombieType();
	UFUNCTION() void OnRep_Dead();
	/** Shows the swing or slam on every machine: the warning light for its duration. */
	UFUNCTION(NetMulticast, Unreliable) void MulticastTelegraph(float Seconds, bool bSlam);

	void ServerThink(float DeltaSeconds);
	AArenaDuelCharacter* FindTarget() const;
	bool HasLineTo(const AArenaDuelCharacter* Target) const;
	void ApplyTypeVisual();
	void Die(AController* Killer, bool bHeadshot);
	void StartRagdoll();

	UPROPERTY(VisibleAnywhere) TObjectPtr<UPointLightComponent> WarningLight;

	UPROPERTY(ReplicatedUsing = OnRep_ZombieType) EArenaDuelZombieType ZombieType = EArenaDuelZombieType::Normal;
	UPROPERTY(Replicated) float Health = 100.0f;
	UPROPERTY(Replicated) float MaxHealth = 100.0f;
	UPROPERTY(Replicated) float VisualScale = 1.0f;
	UPROPERTY(Replicated) FString MaterialPath;
	UPROPERTY(ReplicatedUsing = OnRep_Dead) bool bDead = false;
	UPROPERTY(Replicated) FVector_NetQuantize DeathHitLocation;
	UPROPERTY(Replicated) FVector_NetQuantizeNormal DeathHitDirection;

	// Server only.
	FArenaDuelZombieTypeConfig Config;
	float DamageScale = 1.0f;
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
	float WarningLightOffTime = 0.0f;
	bool bVisualApplied = false;
};
