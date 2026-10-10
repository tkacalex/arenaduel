#include "ArenaDuelZombieGameState.h"
#include "Net/UnrealNetwork.h"

void AArenaDuelZombieGameState::SetWaveState(int32 NewWave, int32 NewRemaining, bool bNewIntermission, float NewNextWaveServerTime)
{
	Wave = FMath::Max(NewWave, 0);
	ZombiesRemaining = FMath::Max(NewRemaining, 0);
	bIntermission = bNewIntermission;
	NextWaveServerTime = NewNextWaveServerTime;
}

void AArenaDuelZombieGameState::Announce(const FString& Text)
{
	Announcement = Text;
	AnnouncementServerTime = GetServerWorldTimeSeconds();
}

void AArenaDuelZombieGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AArenaDuelZombieGameState, Wave);
	DOREPLIFETIME(AArenaDuelZombieGameState, ZombiesRemaining);
	DOREPLIFETIME(AArenaDuelZombieGameState, bIntermission);
	DOREPLIFETIME(AArenaDuelZombieGameState, bGameOver);
	DOREPLIFETIME(AArenaDuelZombieGameState, NextWaveServerTime);
	DOREPLIFETIME(AArenaDuelZombieGameState, TotalKills);
	DOREPLIFETIME(AArenaDuelZombieGameState, BossHealthFraction);
	DOREPLIFETIME(AArenaDuelZombieGameState, BossName);
	DOREPLIFETIME(AArenaDuelZombieGameState, Announcement);
	DOREPLIFETIME(AArenaDuelZombieGameState, AnnouncementServerTime);
	DOREPLIFETIME(AArenaDuelZombieGameState, RunStartServerTime);
	DOREPLIFETIME(AArenaDuelZombieGameState, RunEndServerTime);
}
