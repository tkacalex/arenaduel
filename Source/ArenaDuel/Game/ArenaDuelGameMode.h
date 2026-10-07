// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "TimerManager.h"
#include "ArenaDuelGameMode.generated.h"

class AArenaDuelCharacter;
class AArenaDuelGameState;
class AArenaDuelPlayerState;
enum class EArenaDuelCharacterArchetype : uint8;

UCLASS()
class ARENADUEL_API AArenaDuelGameMode : public AGameMode
{
	GENERATED_BODY()

public:
	AArenaDuelGameMode();
	virtual void BeginPlay() override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;
	virtual void RestartPlayer(AController* NewPlayer) override;
	virtual AActor* FindPlayerStart_Implementation(AController* Player, const FString& IncomingName = TEXT("")) override;
	void RequestCharacterSelection(APlayerController* Requester, EArenaDuelCharacterArchetype Archetype);
	void RequestCharacterReady(APlayerController* Requester, bool bReady);
	void HandlePlayerDeath(AArenaDuelCharacter* DeadCharacter);
	void AdminRestartRound();
	void AdminAdvanceRound();
	void AdminAwardRound(uint8 WinningDuelSlot);
	void AdminResetMatch();
	AArenaDuelPlayerState* FindPlayerStateByDuelSlot(uint8 DuelSlot) const;

protected:
	void StartNextRound();
	void RestartDuelPlayers();
	void ResetMatchAndRestartPlayers();
	void ClearPendingRoundAndMatchTimers();
	void AssignDuelSlot(AArenaDuelPlayerState* JoiningPlayerState);
	void EndRoundForDevelopment(AArenaDuelPlayerState* WinningPlayerState);
	void EnterCharacterSelect();
	void StartSelectedMatch();
	void CheckBothReady();
	FTimerHandle CharacterCountdownTimer;

	FTimerHandle RoundRestartTimer;
	FTimerHandle MatchResetTimer;
	bool bRoundRestartPending = false;
};
