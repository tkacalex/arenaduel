#pragma once

#include "CoreMinimal.h"
#include "ArenaDuelGameMode.h"
#include "ArenaDuelZombieTypes.h"
#include "ArenaDuelZombieGameMode.generated.h"

class AArenaDuelZombie;
class AArenaDuelZombieGameState;

/**
 * Zombie Survival. One or more players against waves of AI enemies; no duel rounds, no character select.
 *
 * This class is the wave manager: it owns the wave table, the queue of enemies still to spawn, the limit
 * of enemies alive at once, the choice of spawn points, the points and the purchases. A wave is over only
 * when its queue is empty and no zombie of it is alive, and the alive count is always taken from the
 * actors in the world, never from a running counter, so it cannot drift.
 */
UCLASS()
class ARENADUEL_API AArenaDuelZombieGameMode : public AArenaDuelGameMode
{
	GENERATED_BODY()

public:
	AArenaDuelZombieGameMode();
	virtual void BeginPlay() override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;
	virtual void RestartPlayer(AController* NewPlayer) override;
	virtual void HandlePlayerDeath(AArenaDuelCharacter* DeadCharacter) override;

	/** The ten authored waves. */
	static TArray<FArenaDuelWaveDefinition> DefaultWaveTable();
	/** The wave to play: from the table while it lasts, then scaled up from its last entry. Wave numbers start at 1. */
	static FArenaDuelWaveDefinition ComputeWave(int32 WaveNumber, const TArray<FArenaDuelWaveDefinition>& Table);
	/** Enemy health multiplier for a wave: 1 through the table, then rising. */
	static float ComputeHealthScale(int32 WaveNumber, int32 TableWaves);
	/** Enemy speed multiplier for a wave: 1 through the table, then rising slowly up to a limit. */
	static float ComputeSpeedScale(int32 WaveNumber, int32 TableWaves);
	/** The spawn order of a wave: specials spread through the normal zombies, bosses last. */
	static TArray<EArenaDuelZombieType> BuildSpawnQueue(const FArenaDuelWaveDefinition& Wave, int32 Seed);

	void NotifyZombieKilled(AArenaDuelZombie* Zombie, AController* Killer, bool bHeadshot);
	/** Buys one item for the player if the run is between waves and the points suffice. Points are taken exactly once. */
	bool TryPurchase(AController* Buyer, EArenaDuelSurvivalPurchase Item);
	int32 GetPurchaseCost(const AController* Buyer, EArenaDuelSurvivalPurchase Item) const;
	float GetPlayerDamageScale(const AController* Player) const;
	float GetCorpseSeconds(bool bBoss) const { return bBoss ? BossCorpseSeconds : CorpseSeconds; }
	void RestartSurvival();
	/** Server only. A normal enemy asks before it swings: hits on one player are spaced out, so being surrounded hurts but is not instant death. */
	bool ClaimAttackOn(const AActor* Target);
	int32 GetCurrentWave() const { return CurrentWave; }
	const TArray<FArenaDuelWaveDefinition>& GetWaveTable() const { return WaveTable; }
	const TArray<FArenaDuelZombieTypeConfig>& GetTypeConfigs() const { return TypeConfigs; }
	int32 GetMaxActiveZombies() const { return MaxActiveZombies; }
	int32 CountAliveZombies() const;
	/** Development: kill everything alive, so a run can be fast-forwarded. No points are given. */
	void DevKillAllZombies();
	/** Development: scale every timer of the run. */
	void DevSetTimeScale(float Scale) { DevTimeScale = FMath::Clamp(Scale, 0.02f, 1.0f); }
	/** Development: enemy damage for zombies spawned from now on. 0 lets a run be watched without dying. */
	void DevSetZombieDamageScale(float Scale) { ZombieDamageScale = FMath::Max(Scale, 0.0f); }
	float GetZombieDamageScale() const { return ZombieDamageScale; }
	/** Development: ends the running wave at once: nothing more spawns and everything alive dies. */
	void DevFinishWave();
	/** Development: ends the running wave and starts the next one without the break. */
	void DevNextWaveNow();
	/** Development: zombies that reach a player are killed and credited, so whole waves play themselves and prove that enemies arrive. */
	void DevSetAutoPlay(bool bEnabled) { bDevAutoPlay = bEnabled; }
	/** Development: one line about the run to the log: alive, pathing, steering straight, freed, moved, frame time. */
	void DevLogStats() const;

protected:
	virtual void EnterCharacterSelect() override;

	UPROPERTY(EditDefaultsOnly, Category = "Survival|Waves") TArray<FArenaDuelWaveDefinition> WaveTable;
	/** Indexed by EArenaDuelZombieType. */
	UPROPERTY(EditDefaultsOnly, Category = "Survival|Enemies") TArray<FArenaDuelZombieTypeConfig> TypeConfigs;
	/** Enemies alive at the same time. The rest of a wave waits in the queue. */
	UPROPERTY(EditDefaultsOnly, Category = "Survival|Waves", meta = (ClampMin = "1")) int32 MaxActiveZombies = 24;
	UPROPERTY(EditDefaultsOnly, Category = "Survival|Waves", meta = (ClampMin = "0.05")) float SpawnInterval = 0.35f;
	UPROPERTY(EditDefaultsOnly, Category = "Survival|Waves", meta = (ClampMin = "0")) float FirstWaveDelay = 5.0f;
	UPROPERTY(EditDefaultsOnly, Category = "Survival|Waves", meta = (ClampMin = "0")) float IntermissionSeconds = 12.0f;
	/** A spawn point closer than this to any player is not used. */
	UPROPERTY(EditDefaultsOnly, Category = "Survival|Spawning", meta = (ClampMin = "0")) float MinSpawnDistance = 1100.0f;
	/** A zombie that has not come closer to a player for this long is moved to a fresh spawn point. */
	UPROPERTY(EditDefaultsOnly, Category = "Survival|Spawning", meta = (ClampMin = "1")) float StuckSeconds = 14.0f;
	UPROPERTY(EditDefaultsOnly, Category = "Survival|Enemies", meta = (ClampMin = "0")) float CorpseSeconds = 8.0f;
	UPROPERTY(EditDefaultsOnly, Category = "Survival|Enemies", meta = (ClampMin = "0")) float BossCorpseSeconds = 25.0f;
	/** Enemy damage to players, all types. */
	UPROPERTY(EditDefaultsOnly, Category = "Survival|Balance", meta = (ClampMin = "0")) float ZombieDamageScale = 1.0f;
	/** Weapon damage against zombies, all weapons. The duel values themselves stay as they are. */
	UPROPERTY(EditDefaultsOnly, Category = "Survival|Balance", meta = (ClampMin = "0")) float WeaponDamageScale = 1.0f;
	/** Shortest time between two swings of normal enemies at the same player. Bosses ignore it. */
	UPROPERTY(EditDefaultsOnly, Category = "Survival|Balance", meta = (ClampMin = "0")) float SecondsBetweenSwingsAtPlayer = 0.45f;
	/** Health a living player gets back per second between waves. */
	UPROPERTY(EditDefaultsOnly, Category = "Survival|Balance", meta = (ClampMin = "0")) float IntermissionRegenPerSecond = 6.0f;
	/** Share of every weapon's reserve that a cleared wave gives back, so a run cannot end for lack of ammunition. */
	UPROPERTY(EditDefaultsOnly, Category = "Survival|Balance", meta = (ClampMin = "0", ClampMax = "1")) float WaveClearAmmoShare = 0.35f;
	/** Seconds between two ammunition boxes appearing somewhere on the floor; the actual gap varies by a quarter. */
	UPROPERTY(EditDefaultsOnly, Category = "Survival|Pickups", meta = (ClampMin = "1")) float AmmoPickupInterval = 22.0f;
	/** Boxes lying around at the same time. */
	UPROPERTY(EditDefaultsOnly, Category = "Survival|Pickups", meta = (ClampMin = "0")) int32 MaxAmmoPickups = 3;
	/** A box that nobody takes disappears after this long. */
	UPROPERTY(EditDefaultsOnly, Category = "Survival|Pickups", meta = (ClampMin = "1")) float AmmoPickupLifeSeconds = 60.0f;
	/** Share of every weapon's reserve one box gives. */
	UPROPERTY(EditDefaultsOnly, Category = "Survival|Pickups", meta = (ClampMin = "0", ClampMax = "1")) float AmmoPickupShare = 0.5f;
	/** With no spawn for this long although the wave has room, the distance rule is relaxed so a wave cannot stall. */
	UPROPERTY(EditDefaultsOnly, Category = "Survival|Spawning", meta = (ClampMin = "0.5")) float SpawnStallSeconds = 4.0f;
	UPROPERTY(EditDefaultsOnly, Category = "Survival|Rewards", meta = (ClampMin = "0")) int32 HeadshotBonusPoints = 50;
	UPROPERTY(EditDefaultsOnly, Category = "Survival|Rewards", meta = (ClampMin = "0")) int32 AmmoCost = 500;
	UPROPERTY(EditDefaultsOnly, Category = "Survival|Rewards", meta = (ClampMin = "0")) int32 HealCost = 750;
	/** Cost of the first damage level; each further level costs this much more. */
	UPROPERTY(EditDefaultsOnly, Category = "Survival|Rewards", meta = (ClampMin = "0")) int32 DamageUpgradeCost = 1500;
	UPROPERTY(EditDefaultsOnly, Category = "Survival|Rewards", meta = (ClampMin = "0")) float DamagePerUpgradeLevel = 0.25f;
	UPROPERTY(EditDefaultsOnly, Category = "Survival|Rewards", meta = (ClampMin = "0")) int32 MaxDamageUpgradeLevel = 8;

	void EnsureNavigationBounds();
	void BeginRun();
	void BeginIntermission(float Seconds);
	void StartWave();
	void SpawnTick();
	void WatchTick();
	void FinishWaveIfDone();
	/** bRelaxed halves the distance rule; bUnseenOnly refuses points a player can see. */
	bool PickSpawnLocation(FVector& OutLocation, FRotator& OutRotation, bool bRelaxed = false, bool bUnseenOnly = false);
	bool IsSeenByAnyPlayer(const FVector& Location) const;
	void DevAutoPlayTick();
	void SpawnAmmoPickupIfDue();
	void RefreshGameState();
	AArenaDuelZombieGameState* GetSurvivalState() const;
	float Scaled(float Seconds) const { return FMath::Max(0.02f, Seconds * DevTimeScale); }

	TArray<EArenaDuelZombieType> SpawnQueue;
	int32 CurrentWave = 0;
	bool bWaveActive = false;
	bool bRunOver = false;
	int32 SpawnPointCursor = 0;
	int32 TotalKills = 0;
	float DevTimeScale = 1.0f;
	bool bDevAutoPlay = false;
	TMap<TWeakObjectPtr<const AActor>, float> NextSwingAllowed;
	float LastSpawnWorldTime = 0.0f;
	float NextAmmoPickupTime = 0.0f;
	float WaveStartWorldTime = 0.0f;
	int32 StatRelocated = 0;
	int32 StatFellOut = 0;
	int32 StatRelaxedSpawns = 0;
	FTimerHandle WaveTimer;
	FTimerHandle SpawnTimer;
	FTimerHandle WatchTimer;
};
