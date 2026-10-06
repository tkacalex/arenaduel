// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "TimerManager.h"
#include "ArenaDuelGameMode.generated.h"

class AArenaDuelCharacter;
class AArenaDuelGameState;
class AArenaDuelPlayerState;

UCLASS()
class ARENADUEL_API AArenaDuelGameMode : public AGameMode
{
	GENERATED_BODY()

public:
	AArenaDuelGameMode();
	virtual void BeginPlay() override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
	void HandlePlayerDeath(AArenaDuelCharacter* DeadCharacter);

protected:
	void StartNextRound();
	void RestartDuelPlayers();
	void AssignDuelSlot(AArenaDuelPlayerState* JoiningPlayerState);

	FTimerHandle RoundRestartTimer;
	bool bRoundRestartPending = false;
};
