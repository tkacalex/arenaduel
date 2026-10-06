// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameState.h"
#include "ArenaDuelGameState.generated.h"

UCLASS()
class ARENADUEL_API AArenaDuelGameState : public AGameState
{
	GENERATED_BODY()

public:
	int32 GetRoundNumber() const { return RoundNumber; }
	bool IsRoundInProgress() const { return bRoundInProgress; }
	int32 GetLastRoundWinnerSlot() const { return LastRoundWinnerSlot; }
	bool IsMatchComplete() const { return bMatchComplete; }
	int32 GetMatchWinnerSlot() const { return MatchWinnerSlot; }
	void SetRoundState(int32 NewRoundNumber, bool bNewRoundInProgress, int32 NewLastRoundWinnerSlot);
	void SetMatchComplete(bool bNewMatchComplete, int32 NewMatchWinnerSlot);
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category="Match")
	int32 RoundNumber = 1;

	UPROPERTY(ReplicatedUsing=OnRep_RoundInProgress, VisibleInstanceOnly, BlueprintReadOnly, Category="Match")
	bool bRoundInProgress = true;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category="Match")
	int32 LastRoundWinnerSlot = INDEX_NONE;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category="Match")
	bool bMatchComplete = false;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category="Match")
	int32 MatchWinnerSlot = INDEX_NONE;

	UFUNCTION()
	void OnRep_RoundInProgress();
};
