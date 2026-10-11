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
	virtual void HandlePlayerDeath(AArenaDuelCharacter* DeadCharacter);
	void AdminRestartRound();
	void AdminAdvanceRound();
	void AdminAwardRound(uint8 WinningDuelSlot);
	void AdminResetMatch();
	AArenaDuelPlayerState* FindPlayerStateByDuelSlot(uint8 DuelSlot) const;

	/** The duel offers match upgrades between rounds. Modes built on the duel classes that have none say no. */
	virtual bool UsesMatchUpgrades() const { return true; }
	float GetUpgradeSelectSeconds() const { return UpgradeSelectSeconds; }
	/** A player's choice in the selection with this serial (AArenaDuelGameState::GetUpgradeSelectSerial). Checked on the server: the player, the phase, the selection, the id, the level, one per selection. */
	void RequestUpgradeChoice(APlayerController* Requester, uint8 UpgradeId, int32 SelectionSerial);
	/** The duel is fought by the two players holding a duel slot; anyone else connected only watches. Modes for more players say no. */
	virtual bool IsLimitedToDuelPlayers() const { return true; }
	bool IsUpgradeSelectionPending() const;
	void SetUpgradeSelectSecondsForTests(float Seconds) { UpgradeSelectSeconds = FMath::Max(0.1f, Seconds); }
	/** A round that ends with nobody left standing, for tests. */
	void EndRoundWithoutWinnerForTests() { EndRoundForDevelopment(nullptr); }

	/** The duel has a round clock. Modes built on the duel classes that have no timed rounds say no. */
	virtual bool UsesRoundTimer() const { return true; }
	float GetRoundDurationSeconds() const { return RoundDurationSeconds; }
	int32 GetRoundWinsToWinMatch() const { return RoundWinsToWinMatch; }
	/** How many rounds have been decided since this game mode began. A round is decided once. */
	int32 GetRoundsEvaluated() const { return RoundsEvaluated; }
	/** True while the server's time limit for the running round is pending. */
	bool IsRoundTimeLimitPending() const;
	/** Shorter times for automated tests; the rules stay the same. */
	void SetRoundTimingForTests(float InRoundDurationSeconds, float InRoundBreakSeconds, float InMatchResultSeconds);
	/** What the time limit does when it fires, callable at a chosen moment. bStale pretends the timer belonged to an earlier round. */
	void ExpireRoundTimeForTests(bool bStale = false);
	/** Development: sets how much regular time the running round has left. Does nothing in overtime or without a clock. */
	void SetRoundTimeRemainingForDevelopment(float Seconds);
	/** Development: kills the player in a duel slot through the normal damage path. */
	void KillDuelSlotForDevelopment(uint8 DuelSlot);

protected:
	/** Length of a regular round. At the limit with both players alive, overtime begins and the next kill decides. */
	UPROPERTY(EditDefaultsOnly, Category="Round", meta=(ClampMin="0"))
	float RoundDurationSeconds = 90.0f;
	/** Pause between two rounds, in which only the winner may move. */
	UPROPERTY(EditDefaultsOnly, Category="Round", meta=(ClampMin="0.1"))
	float RoundBreakSeconds = 3.0f;
	/** How long the match result stays before the return to character selection. */
	UPROPERTY(EditDefaultsOnly, Category="Round", meta=(ClampMin="0.1"))
	float MatchResultSeconds = 5.0f;
	UPROPERTY(EditDefaultsOnly, Category="Round", meta=(ClampMin="1"))
	int32 RoundWinsToWinMatch = 5;
	/** How long both players have to take a match upgrade after a decided round. It replaces the round break. */
	UPROPERTY(EditDefaultsOnly, Category="Round", meta=(ClampMin="0.1"))
	float UpgradeSelectSeconds = 10.0f;

	void BeginUpgradeSelection();
	void FinishUpgradeSelection();
	FTimerHandle UpgradeSelectTimer;

	/** Starts round RoundNumber: round state, a fresh serial and, for a timed mode, the clock. */
	void BeginRound(int32 RoundNumber, int32 LastWinnerSlot);
	void HandleRoundTimeExpired(int32 ForRoundSerial);
	FTimerHandle RoundTimeLimitTimer;
	// Every started round gets a serial. A time limit and an evaluation only count for the serial they belong to.
	int32 RoundSerial = 0;
	int32 EvaluatedRoundSerial = INDEX_NONE;
	int32 RoundsEvaluated = 0;
	void StartNextRound();
	void RestartDuelPlayers();
	void ResetMatchAndRestartPlayers();
	void ClearPendingRoundAndMatchTimers();
	void AssignDuelSlot(AArenaDuelPlayerState* JoiningPlayerState);
	void EndRoundForDevelopment(AArenaDuelPlayerState* WinningPlayerState);
	virtual void EnterCharacterSelect();
	void AutoReadyCharacterSelectPlayers();
	void StartCharacterAutoReadyTimer();
	void StartSelectedMatch();
	void CheckBothReady();
	FTimerHandle CharacterAutoReadyTimer;
	FTimerHandle CharacterCountdownTimer;

	FTimerHandle RoundRestartTimer;
	FTimerHandle MatchResetTimer;
	bool bRoundRestartPending = false;
};
