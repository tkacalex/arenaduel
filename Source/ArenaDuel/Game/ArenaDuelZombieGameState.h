#pragma once

#include "CoreMinimal.h"
#include "ArenaDuelGameState.h"
#include "ArenaDuelZombieGameState.generated.h"

/**
 * Replicated state of a survival run: wave, enemies left, timers, boss health and the last announcement.
 * It extends the duel game state so characters and weapons, which ask that class whether play is live,
 * work unchanged; the duel round fields are simply held at "in round" for the whole run.
 */
UCLASS()
class ARENADUEL_API AArenaDuelZombieGameState : public AArenaDuelGameState
{
	GENERATED_BODY()

public:
	int32 GetWave() const { return Wave; }
	int32 GetZombiesRemaining() const { return ZombiesRemaining; }
	bool IsIntermission() const { return bIntermission; }
	bool IsGameOver() const { return bGameOver; }
	float GetNextWaveServerTime() const { return NextWaveServerTime; }
	int32 GetTotalKills() const { return TotalKills; }
	/** Below zero when no boss is alive. */
	float GetBossHealthFraction() const { return BossHealthFraction; }
	const FString& GetBossName() const { return BossName; }
	const FString& GetAnnouncement() const { return Announcement; }
	float GetAnnouncementServerTime() const { return AnnouncementServerTime; }

	void SetWaveState(int32 NewWave, int32 NewRemaining, bool bNewIntermission, float NewNextWaveServerTime);
	void SetZombiesRemaining(int32 NewRemaining) { ZombiesRemaining = FMath::Max(NewRemaining, 0); }
	void SetGameOver(bool bNewGameOver) { bGameOver = bNewGameOver; }
	void SetTotalKills(int32 NewKills) { TotalKills = FMath::Max(NewKills, 0); }
	void SetBoss(float HealthFraction, const FString& Name) { BossHealthFraction = HealthFraction; BossName = Name; }
	void Announce(const FString& Text);
	/** Start and end of the run in server time; the end is below zero while it is running. */
	float GetRunStartServerTime() const { return RunStartServerTime; }
	float GetRunEndServerTime() const { return RunEndServerTime; }
	void SetRunTimes(float Start, float End) { RunStartServerTime = Start; RunEndServerTime = End; }
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	UPROPERTY(Replicated) int32 Wave = 0;
	UPROPERTY(Replicated) int32 ZombiesRemaining = 0;
	UPROPERTY(Replicated) bool bIntermission = true;
	UPROPERTY(Replicated) bool bGameOver = false;
	UPROPERTY(Replicated) float NextWaveServerTime = 0.0f;
	UPROPERTY(Replicated) int32 TotalKills = 0;
	UPROPERTY(Replicated) float BossHealthFraction = -1.0f;
	UPROPERTY(Replicated) FString BossName;
	UPROPERTY(Replicated) FString Announcement;
	UPROPERTY(Replicated) float AnnouncementServerTime = -1000.0f;
	UPROPERTY(Replicated) float RunStartServerTime = 0.0f;
	UPROPERTY(Replicated) float RunEndServerTime = -1.0f;
};
