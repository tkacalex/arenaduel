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
	void AdminRestartRound();
	void AdminAdvanceRound();
	void AdminAwardRound(uint8 WinningDuelSlot);
	void AdminResetMatch();
	AArenaDuelPlayerState* FindPlayerStateByDuelSlot(uint8 DuelSlot) const;

protected:
	void StartNextRound();
	void RestartDuelPlayers();
	void AssignDuelSlot(AArenaDuelPlayerState* JoiningPlayerState);
	void EndRoundForDevelopment(AArenaDuelPlayerState* WinningPlayerState);

	FTimerHandle RoundRestartTimer;
	bool bRoundRestartPending = false;
};
