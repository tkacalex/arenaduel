// Copyright Epic Games, Inc. All Rights Reserved.

#include "ArenaDuelPlayerState.h"
#include "AbilitySystemComponent.h"
#include "../Combat/ArenaDuelAttributeSet.h"
#include "Net/UnrealNetwork.h"

AArenaDuelPlayerState::AArenaDuelPlayerState()
{
	SetNetUpdateFrequency(100.0f);
	SetMinNetUpdateFrequency(30.0f);
	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);
	AttributeSet = CreateDefaultSubobject<UArenaDuelAttributeSet>(TEXT("AttributeSet"));
}

UAbilitySystemComponent* AArenaDuelPlayerState::GetAbilitySystemComponent() const { return AbilitySystemComponent; }

void AArenaDuelPlayerState::SetDuelSlot(uint8 NewDuelSlot)
{
	if (HasAuthority())
	{
		DuelSlot = FMath::Min<uint8>(NewDuelSlot, 1);
		ForceNetUpdate();
	}
}

void AArenaDuelPlayerState::AwardRoundWin()
{
	if (HasAuthority() && RoundWins < 5)
	{
		++RoundWins;
		ForceNetUpdate();
	}
}

void AArenaDuelPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AArenaDuelPlayerState, AbilitySystemComponent);
	DOREPLIFETIME(AArenaDuelPlayerState, DuelSlot);
	DOREPLIFETIME(AArenaDuelPlayerState, RoundWins);
}
