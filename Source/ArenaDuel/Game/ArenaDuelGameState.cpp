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
	ForceNetUpdate();
}

void AArenaDuelGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AArenaDuelGameState, RoundNumber);
	DOREPLIFETIME(AArenaDuelGameState, bRoundInProgress);
	DOREPLIFETIME(AArenaDuelGameState, LastRoundWinnerSlot);
}

void AArenaDuelGameState::OnRep_RoundInProgress()
{
	if (bRoundInProgress || !GetWorld()) return;
	for (TActorIterator<AArenaDuelCharacter> It(GetWorld()); It; ++It)
	{
		It->SetRoundInputLocked(true);
	}
}
