// Copyright Epic Games, Inc. All Rights Reserved.

#include "ArenaDuelPlayerState.h"
#include "AbilitySystemComponent.h"
#include "../Combat/ArenaDuelAttributeSet.h"
#include "../Abilities/ArenaDuelGameplayTags.h"
#include "../Abilities/ArenaDuelGA_ShadowStep.h"
#include "../Abilities/ArenaDuelGA_VeilWall.h"
#include "GameplayEffect.h"
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

void AArenaDuelPlayerState::BeginPlay()
{
	Super::BeginPlay();
	GrantShadowAbilities();
}

void AArenaDuelPlayerState::GrantShadowAbilities()
{
	if (!HasAuthority() || !AbilitySystemComponent) return;
	const TArray<TSubclassOf<UGameplayAbility>> ShadowAbilities = {
		UArenaDuelGA_ShadowStep::StaticClass(),
		UArenaDuelGA_VeilWall::StaticClass()
	};
	for (const TSubclassOf<UGameplayAbility>& AbilityClass : ShadowAbilities)
	{
		if (AbilityClass && !AbilitySystemComponent->FindAbilitySpecFromClass(AbilityClass))
		{
			AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(AbilityClass, 1));
		}
	}
}

void AArenaDuelPlayerState::ResetShadowAbilitiesForNewRound()
{
	if (!HasAuthority() || !AbilitySystemComponent) return;
	GrantShadowAbilities();
	AbilitySystemComponent->CancelAllAbilities();
	FGameplayTagContainer CooldownTags;
	CooldownTags.AddTag(TAG_Cooldown_Shadow_ShadowStep.GetTag());
	CooldownTags.AddTag(TAG_Cooldown_Shadow_VeilWall.GetTag());
	AbilitySystemComponent->RemoveActiveEffectsWithGrantedTags(CooldownTags);
}

float AArenaDuelPlayerState::GetCooldownRemaining(const FGameplayTag& CooldownTag) const
{
	if (!AbilitySystemComponent || !CooldownTag.IsValid()) return 0.0f;
	FGameplayTagContainer CooldownQueryTags;
	CooldownQueryTags.AddTag(CooldownTag);
	const TArray<float> RemainingTimes = AbilitySystemComponent->GetActiveEffectsTimeRemaining(FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(CooldownQueryTags));
	float Remaining = 0.0f;
	for (const float Time : RemainingTimes) Remaining = FMath::Max(Remaining, Time);
	return Remaining;
}

float AArenaDuelPlayerState::GetShadowStepCooldownRemaining() const
{
	return GetCooldownRemaining(TAG_Cooldown_Shadow_ShadowStep.GetTag());
}

float AArenaDuelPlayerState::GetVeilWallCooldownRemaining() const
{
	return GetCooldownRemaining(TAG_Cooldown_Shadow_VeilWall.GetTag());
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

void AArenaDuelPlayerState::SetRoundWinsForDevelopment(int32 NewRoundWins)
{
	if (!HasAuthority()) return;
	RoundWins = FMath::Clamp(NewRoundWins, 0, 5);
	ForceNetUpdate();
}

void AArenaDuelPlayerState::ToggleAdminGodMode()
{
	if (HasAuthority()) { bAdminGodMode = !bAdminGodMode; ForceNetUpdate(); }
}

void AArenaDuelPlayerState::ToggleAdminInfiniteAmmo()
{
	if (HasAuthority()) { bAdminInfiniteAmmo = !bAdminInfiniteAmmo; ForceNetUpdate(); }
}

void AArenaDuelPlayerState::ToggleAdminInfiniteStamina()
{
	if (HasAuthority()) { bAdminInfiniteStamina = !bAdminInfiniteStamina; ForceNetUpdate(); }
}

void AArenaDuelPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AArenaDuelPlayerState, AbilitySystemComponent);
	DOREPLIFETIME(AArenaDuelPlayerState, DuelSlot);
	DOREPLIFETIME(AArenaDuelPlayerState, RoundWins);
	DOREPLIFETIME(AArenaDuelPlayerState, bAdminGodMode);
	DOREPLIFETIME(AArenaDuelPlayerState, bAdminInfiniteAmmo);
	DOREPLIFETIME(AArenaDuelPlayerState, bAdminInfiniteStamina);
}
