// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameState.h"
#include "ArenaDuelGameState.generated.h"

class AArenaDuelCharacter;

UENUM(BlueprintType)
enum class EArenaDuelMatchPhase : uint8
{
	CharacterSelect,
	Countdown,
	InRound,
	RoundBreak,
	MatchResult
};

UCLASS()
class ARENADUEL_API AArenaDuelGameState : public AGameState
{
	GENERATED_BODY()

public:
	int32 GetRoundNumber() const { return RoundNumber; }
	bool IsRoundInProgress() const { return MatchPhase == EArenaDuelMatchPhase::InRound && bRoundInProgress; }
	bool CanLivingCharacterMove(const AArenaDuelCharacter* Character) const;
	EArenaDuelMatchPhase GetMatchPhase() const { return MatchPhase; }
	bool IsCharacterSelectVisible() const { return MatchPhase == EArenaDuelMatchPhase::CharacterSelect || MatchPhase == EArenaDuelMatchPhase::Countdown; }
	float GetCountdownEndServerTime() const { return CountdownEndServerTime; }
	float GetCharacterAutoReadyEndServerTime() const { return CharacterAutoReadyEndServerTime; }
	void SetMatchPhase(EArenaDuelMatchPhase NewPhase, float EndServerTime = 0.0f);
	void SetCharacterAutoReadyEndServerTime(float EndServerTime);
	int32 GetLastRoundWinnerSlot() const { return LastRoundWinnerSlot; }
	bool IsMatchComplete() const { return bMatchComplete; }
	int32 GetMatchWinnerSlot() const { return MatchWinnerSlot; }
	void SetRoundState(int32 NewRoundNumber, bool bNewRoundInProgress, int32 NewLastRoundWinnerSlot);
	void SetMatchComplete(bool bNewMatchComplete, int32 NewMatchWinnerSlot);
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	UPROPERTY(ReplicatedUsing=OnRep_MatchPhase, VisibleInstanceOnly, BlueprintReadOnly, Category="Match")
	EArenaDuelMatchPhase MatchPhase = EArenaDuelMatchPhase::CharacterSelect;

	UPROPERTY(Replicated)
	float CountdownEndServerTime = 0.0f;

	UPROPERTY(Replicated)
	float CharacterAutoReadyEndServerTime = 0.0f;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category="Match")
	int32 RoundNumber = 1;

	UPROPERTY(ReplicatedUsing=OnRep_RoundInProgress, VisibleInstanceOnly, BlueprintReadOnly, Category="Match")
	bool bRoundInProgress = false;

	UPROPERTY(ReplicatedUsing=OnRep_RoundState, VisibleInstanceOnly, BlueprintReadOnly, Category="Match")
	int32 LastRoundWinnerSlot = INDEX_NONE;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category="Match")
	bool bMatchComplete = false;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category="Match")
	int32 MatchWinnerSlot = INDEX_NONE;

	UFUNCTION()
	void OnRep_RoundInProgress();
	UFUNCTION() void OnRep_RoundState();
	UFUNCTION()
	void OnRep_MatchPhase();
};
