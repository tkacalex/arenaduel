// Copyright Epic Games, Inc. All Rights Reserved.

#include "ArenaDuelGameState.h"
#include "../Characters/ArenaDuelCharacter.h"
#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"

void AArenaDuelGameState::SetRoundState(int32 NewRoundNumber, bool bNewRoundInProgress, int32 NewLastRoundWinnerSlot)
{
	if (!HasAuthority()) return;
	RoundNumber = FMath::Max(1, NewRoundNumber);
	bRoundInProgress = bNewRoundInProgress;
	LastRoundWinnerSlot = NewLastRoundWinnerSlot;
	SetMatchPhase(bNewRoundInProgress ? EArenaDuelMatchPhase::InRound : EArenaDuelMatchPhase::RoundBreak);
	ForceNetUpdate();
}

void AArenaDuelGameState::SetMatchPhase(EArenaDuelMatchPhase NewPhase, float EndServerTime)
{
	if (!HasAuthority()) return;
	MatchPhase = NewPhase;
	bRoundInProgress = NewPhase == EArenaDuelMatchPhase::InRound;
	CountdownEndServerTime = NewPhase == EArenaDuelMatchPhase::Countdown ? EndServerTime : 0.0f;
	OnRep_MatchPhase();
	ForceNetUpdate();
}

void AArenaDuelGameState::OnRep_MatchPhase()
{
	OnRep_RoundInProgress();
}

void AArenaDuelGameState::SetMatchComplete(bool bNewMatchComplete, int32 NewMatchWinnerSlot)
{
	if (!HasAuthority()) return;
	bMatchComplete = bNewMatchComplete;
	MatchWinnerSlot = bNewMatchComplete && NewMatchWinnerSlot >= 0 && NewMatchWinnerSlot <= 1 ? NewMatchWinnerSlot : INDEX_NONE;
	ForceNetUpdate();
}

void AArenaDuelGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AArenaDuelGameState, RoundNumber);
	DOREPLIFETIME(AArenaDuelGameState, bRoundInProgress);
	DOREPLIFETIME(AArenaDuelGameState, LastRoundWinnerSlot);
	DOREPLIFETIME(AArenaDuelGameState, bMatchComplete);
	DOREPLIFETIME(AArenaDuelGameState, MatchWinnerSlot);
	DOREPLIFETIME(AArenaDuelGameState, MatchPhase);
	DOREPLIFETIME(AArenaDuelGameState, CountdownEndServerTime);
}

void AArenaDuelGameState::OnRep_RoundInProgress()
{
	if (!GetWorld()) return;
	for (TActorIterator<AArenaDuelCharacter> It(GetWorld()); It; ++It)
	{
		It->SetRoundInputLocked(!IsRoundInProgress());
	}
}
