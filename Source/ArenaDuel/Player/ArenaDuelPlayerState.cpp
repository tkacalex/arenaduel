// Copyright Epic Games, Inc. All Rights Reserved.

#include "ArenaDuelPlayerState.h"
#include "AbilitySystemComponent.h"
#include "../Combat/ArenaDuelAttributeSet.h"
#include "../Abilities/ArenaDuelGameplayTags.h"
#include "../Abilities/ArenaDuelGA_ShadowStep.h"
#include "../Abilities/ArenaDuelGA_VeilWall.h"
#include "../Abilities/ArenaDuelGA_ArcBarrier.h"
#include "../Abilities/ArenaDuelGA_BurstLeap.h"
#include "../Abilities/ArenaDuelArcBarrier.h"
#include "../Abilities/ArenaDuelVeilWall.h"
#include "GameplayEffect.h"
#include "GameFramework/Pawn.h"
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
	GrantCharacterAbilities();
}

void AArenaDuelPlayerState::GrantCharacterAbilities()
{
	if (!HasAuthority() || !AbilitySystemComponent) return;
	GrantCurrentKit();
}

void AArenaDuelPlayerState::RemoveAllKitAbilitiesAndCooldowns()
{
	if (!AbilitySystemComponent) return;
	AbilitySystemComponent->CancelAllAbilities();
	const UClass* KitClasses[] = { UArenaDuelGA_ShadowStep::StaticClass(), UArenaDuelGA_VeilWall::StaticClass(), UArenaDuelGA_ArcBarrier::StaticClass(), UArenaDuelGA_BurstLeap::StaticClass() };
	TArray<FGameplayAbilitySpecHandle> HandlesToClear;
	for (const FGameplayAbilitySpec& Spec : AbilitySystemComponent->GetActivatableAbilities())
	{
		if (Spec.Ability)
		{
			for (const UClass* KitClass : KitClasses) if (Spec.Ability->GetClass() == KitClass) { HandlesToClear.Add(Spec.Handle); break; }
		}
	}
	for (const FGameplayAbilitySpecHandle& Handle : HandlesToClear) AbilitySystemComponent->ClearAbility(Handle);
	FGameplayTagContainer CooldownTags;
	CooldownTags.AddTag(TAG_Cooldown_Shadow_ShadowStep.GetTag());
	CooldownTags.AddTag(TAG_Cooldown_Shadow_VeilWall.GetTag());
	CooldownTags.AddTag(TAG_Cooldown_Warden_ArcBarrier.GetTag());
	CooldownTags.AddTag(TAG_Cooldown_Warden_BurstLeap.GetTag());
	AbilitySystemComponent->RemoveActiveEffectsWithGrantedTags(CooldownTags);
}

void AArenaDuelPlayerState::GrantCurrentKit()
{
	if (!HasAuthority() || !AbilitySystemComponent) return;
	TSubclassOf<UGameplayAbility> Primary = nullptr;
	TSubclassOf<UGameplayAbility> Secondary = nullptr;
	if (CharacterArchetype == EArenaDuelCharacterArchetype::Warden)
	{
		Primary = UArenaDuelGA_ArcBarrier::StaticClass();
		Secondary = UArenaDuelGA_BurstLeap::StaticClass();
	}
	else
	{
		Primary = UArenaDuelGA_ShadowStep::StaticClass();
		Secondary = UArenaDuelGA_VeilWall::StaticClass();
	}
	if (Primary && !AbilitySystemComponent->FindAbilitySpecFromClass(Primary)) AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(Primary, 1));
	if (Secondary && !AbilitySystemComponent->FindAbilitySpecFromClass(Secondary)) AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(Secondary, 1));
}

void AArenaDuelPlayerState::ResetAbilitiesForNewRound()
{
	if (!HasAuthority() || !AbilitySystemComponent) return;
	if (GetWorld())
	{
		AArenaDuelVeilWall::DestroyOwnedByPlayerState(GetWorld(), this);
		AArenaDuelArcBarrier::DestroyOwnedByPlayerState(GetWorld(), this);
	}
	RemoveAllKitAbilitiesAndCooldowns();
	GrantCurrentKit();
	if (APawn* CurrentPawn = GetPawn()) AbilitySystemComponent->InitAbilityActorInfo(this, CurrentPawn);
	ForceNetUpdate();
}

bool AArenaDuelPlayerState::SetCharacterArchetypeForDevelopment(EArenaDuelCharacterArchetype NewArchetype)
{
	return SetCharacterArchetypeAuthoritatively(NewArchetype);
}

void AArenaDuelPlayerState::SetCharacterReadyAuthoritatively(bool bReady)
{
	if (!HasAuthority()) return;
	bCharacterReady = bReady;
	ForceNetUpdate();
}

bool AArenaDuelPlayerState::SetCharacterArchetypeAuthoritatively(EArenaDuelCharacterArchetype NewArchetype)
{
	if (!HasAuthority() || (NewArchetype != EArenaDuelCharacterArchetype::Shadow && NewArchetype != EArenaDuelCharacterArchetype::Warden) || CharacterArchetype == NewArchetype) return false;
	if (GetWorld())
	{
		AArenaDuelVeilWall::DestroyOwnedByPlayerState(GetWorld(), this);
		AArenaDuelArcBarrier::DestroyOwnedByPlayerState(GetWorld(), this);
	}
	RemoveAllKitAbilitiesAndCooldowns();
	CharacterArchetype = NewArchetype;
	bCharacterReady = false;
	GrantCurrentKit();
	if (APawn* CurrentPawn = GetPawn()) AbilitySystemComponent->InitAbilityActorInfo(this, CurrentPawn);
	ForceNetUpdate();
	return true;
}

void AArenaDuelPlayerState::OnRep_CharacterArchetype() {}

FText AArenaDuelPlayerState::GetCharacterArchetypeDisplayName() const
{
	switch (CharacterArchetype)
	{
	case EArenaDuelCharacterArchetype::Warden: return FText::FromString(TEXT("WARDEN"));
	case EArenaDuelCharacterArchetype::Rift: return FText::FromString(TEXT("RIFT"));
	default: return FText::FromString(TEXT("SHADOW"));
	}
}

bool AArenaDuelPlayerState::TryActivatePrimaryAbility()
{
	if (!AbilitySystemComponent || CharacterArchetype == EArenaDuelCharacterArchetype::Rift) return false;
	return AbilitySystemComponent->TryActivateAbilityByClass(CharacterArchetype == EArenaDuelCharacterArchetype::Warden ? UArenaDuelGA_ArcBarrier::StaticClass() : UArenaDuelGA_ShadowStep::StaticClass());
}

bool AArenaDuelPlayerState::TryActivateSecondaryAbility()
{
	if (!AbilitySystemComponent || CharacterArchetype == EArenaDuelCharacterArchetype::Rift) return false;
	return AbilitySystemComponent->TryActivateAbilityByClass(CharacterArchetype == EArenaDuelCharacterArchetype::Warden ? UArenaDuelGA_BurstLeap::StaticClass() : UArenaDuelGA_VeilWall::StaticClass());
}

float AArenaDuelPlayerState::GetPrimaryAbilityCooldownRemaining() const
{
	return GetCooldownRemaining(CharacterArchetype == EArenaDuelCharacterArchetype::Warden ? TAG_Cooldown_Warden_ArcBarrier.GetTag() : TAG_Cooldown_Shadow_ShadowStep.GetTag());
}

float AArenaDuelPlayerState::GetSecondaryAbilityCooldownRemaining() const
{
	return GetCooldownRemaining(CharacterArchetype == EArenaDuelCharacterArchetype::Warden ? TAG_Cooldown_Warden_BurstLeap.GetTag() : TAG_Cooldown_Shadow_VeilWall.GetTag());
}

FText AArenaDuelPlayerState::GetPrimaryAbilityDisplayName() const
{
	return FText::FromString(CharacterArchetype == EArenaDuelCharacterArchetype::Warden ? TEXT("ARC BARRIER") : TEXT("SHADOW STEP"));
}

FText AArenaDuelPlayerState::GetSecondaryAbilityDisplayName() const
{
	return FText::FromString(CharacterArchetype == EArenaDuelCharacterArchetype::Warden ? TEXT("BURST LEAP") : TEXT("VEIL WALL"));
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
		// 255 denotes an unassigned/departing player, never either playable duel side.
		DuelSlot = NewDuelSlot == 255 ? 255 : FMath::Min<uint8>(NewDuelSlot, 1);
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
	DOREPLIFETIME(AArenaDuelPlayerState, CharacterArchetype);
	DOREPLIFETIME(AArenaDuelPlayerState, bCharacterReady);
	DOREPLIFETIME(AArenaDuelPlayerState, bAdminGodMode);
	DOREPLIFETIME(AArenaDuelPlayerState, bAdminInfiniteAmmo);
	DOREPLIFETIME(AArenaDuelPlayerState, bAdminInfiniteStamina);
}
