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
	MatchResult,
	/** Between two rounds: both players take a match upgrade. Nobody moves or fights. */
	UpgradeSelect
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

	// Upgrade selection between rounds.
	bool IsUpgradeSelect() const { return MatchPhase == EArenaDuelMatchPhase::UpgradeSelect; }
	float GetUpgradeSelectEndServerTime() const { return UpgradeSelectEndServerTime; }
	/** Seconds until the selection closes on its own. */
	float GetUpgradeSelectSecondsRemaining() const;
	/** Server only. Opens the selection after a decided round; it ends at EndServerTime at the latest. */
	void BeginUpgradeSelect(float EndServerTime);

	// Round clock. The server publishes when the regular round ends; every machine derives the remaining
	// time from that and the synchronised server time, so nothing local (a menu, a hitch) can stop it.
	/** True while a round with a time limit is running, overtime included. */
	bool HasRoundTimer() const { return IsRoundInProgress() && (RoundEndServerTime > 0.0f || bOvertime); }
	/** True once the limit has passed with both players alive: the next kill decides the round. */
	bool IsOvertime() const { return IsRoundInProgress() && bOvertime; }
	float GetRoundEndServerTime() const { return RoundEndServerTime; }
	float GetOvertimeStartServerTime() const { return OvertimeStartServerTime; }
	/** Seconds left of the regular round; zero in overtime and when no clock runs. */
	float GetRoundSecondsRemaining() const;
	/** Minutes and seconds, rounded up so the clock shows 0:01 until the limit is really reached. */
	static FString FormatRoundClock(float Seconds);
	/** Server only. Starts the clock of the running round. */
	void SetRoundClock(float EndServerTime);
	/** Server only. The limit has passed and nobody has won. */
	void BeginOvertime();
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

	/** Server time at which the regular round ends; zero when no clock runs. */
	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category="Match")
	float RoundEndServerTime = 0.0f;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category="Match")
	bool bOvertime = false;

	UPROPERTY(Replicated)
	float OvertimeStartServerTime = 0.0f;

	UPROPERTY(Replicated)
	float UpgradeSelectEndServerTime = 0.0f;

	void ClearRoundClock();

	UFUNCTION()
	void OnRep_RoundInProgress();
	UFUNCTION() void OnRep_RoundState();
	UFUNCTION()
	void OnRep_MatchPhase();
};
