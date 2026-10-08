// Copyright Epic Games, Inc. All Rights Reserved.

#include "ArenaDuelGameState.h"
#include "../Characters/ArenaDuelCharacter.h"
#include "../Player/ArenaDuelPlayerState.h"
#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"

bool AArenaDuelGameState::CanLivingCharacterMove(const AArenaDuelCharacter* Character) const
{
	if (!Character || Character->IsDead()) return false;
	if (MatchPhase == EArenaDuelMatchPhase::InRound && bRoundInProgress) return true;
	if (MatchPhase != EArenaDuelMatchPhase::RoundBreak || bMatchComplete || LastRoundWinnerSlot < 0) return false;
	const AArenaDuelPlayerState* Player = Character->GetPlayerState<AArenaDuelPlayerState>();
	return Player && Player->GetDuelSlot() == LastRoundWinnerSlot;
}

void AArenaDuelGameState::SetRoundState(int32 NewRoundNumber, bool bNewRoundInProgress, int32 NewLastRoundWinnerSlot)
{
	if (!HasAuthority()) return;
	RoundNumber = FMath::Max(1, NewRoundNumber);
	bRoundInProgress = bNewRoundInProgress;
	LastRoundWinnerSlot = NewLastRoundWinnerSlot;
	SetMatchPhase(bNewRoundInProgress ? EArenaDuelMatchPhase::InRound : EArenaDuelMatchPhase::RoundBreak);
	OnRep_RoundInProgress();
	ForceNetUpdate();
}

void AArenaDuelGameState::SetMatchPhase(EArenaDuelMatchPhase NewPhase, float EndServerTime)
{
	if (!HasAuthority()) return;
	MatchPhase = NewPhase;
	bRoundInProgress = NewPhase == EArenaDuelMatchPhase::InRound;
	CountdownEndServerTime = NewPhase == EArenaDuelMatchPhase::Countdown ? EndServerTime : 0.0f;
	if (NewPhase != EArenaDuelMatchPhase::CharacterSelect) CharacterAutoReadyEndServerTime = 0.0f;
	OnRep_MatchPhase();
	ForceNetUpdate();
}

void AArenaDuelGameState::SetCharacterAutoReadyEndServerTime(float EndServerTime)
{
	if (!HasAuthority()) return;
	CharacterAutoReadyEndServerTime = FMath::Max(0.0f, EndServerTime);
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
	DOREPLIFETIME(AArenaDuelGameState, CharacterAutoReadyEndServerTime);
}

void AArenaDuelGameState::OnRep_RoundInProgress()
{
	if (!GetWorld()) return;
	for (TActorIterator<AArenaDuelCharacter> It(GetWorld()); It; ++It)
		It->SetRoundInputLocked(!CanLivingCharacterMove(*It));
}

void AArenaDuelGameState::OnRep_RoundState()
{
	OnRep_RoundInProgress();
}
