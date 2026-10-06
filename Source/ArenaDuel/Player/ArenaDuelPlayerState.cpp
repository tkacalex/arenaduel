// Copyright Epic Games, Inc. All Rights Reserved.

#include "ArenaDuelPlayerState.h"
#include "../Characters/ArenaDuelCharacter.h"
#include "AbilitySystemComponent.h"
#include "../Combat/ArenaDuelAttributeSet.h"
#include "../Abilities/ArenaDuelGameplayTags.h"
#include "../Abilities/ArenaDuelGA_ShadowStep.h"
#include "../Abilities/ArenaDuelGA_VeilWall.h"
#include "../Abilities/ArenaDuelGA_ArcBarrier.h"
#include "../Abilities/ArenaDuelGA_BurstLeap.h"
#include "../Abilities/ArenaDuelGA_RiftGrapple.h"
#include "../Abilities/ArenaDuelGA_PhaseGate.h"
#include "../Abilities/ArenaDuelPhaseGateVisual.h"
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
	const UClass* KitClasses[] = { UArenaDuelGA_ShadowStep::StaticClass(), UArenaDuelGA_VeilWall::StaticClass(), UArenaDuelGA_ArcBarrier::StaticClass(), UArenaDuelGA_BurstLeap::StaticClass(), UArenaDuelGA_RiftGrapple::StaticClass(), UArenaDuelGA_PhaseGate::StaticClass() };
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
	CooldownTags.AddTag(TAG_Cooldown_Rift_RiftGrapple.GetTag());
	CooldownTags.AddTag(TAG_Cooldown_Rift_PhaseGate.GetTag());
	AbilitySystemComponent->RemoveActiveEffectsWithGrantedTags(CooldownTags);
}

void AArenaDuelPlayerState::GrantCurrentKit()
{
	if (!HasAuthority() || !AbilitySystemComponent) return;
	const TSubclassOf<UGameplayAbility> Primary = GetPrimaryAbilityClass();
	const TSubclassOf<UGameplayAbility> Secondary = GetSecondaryAbilityClass();
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
		AArenaDuelPhaseGateVisual::DestroyOwnedByPlayerState(GetWorld(), this);
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
	if (!HasAuthority() || !IsImplementedArchetype(NewArchetype) || CharacterArchetype == NewArchetype) return false;
	if (GetWorld())
	{
		AArenaDuelVeilWall::DestroyOwnedByPlayerState(GetWorld(), this);
		AArenaDuelArcBarrier::DestroyOwnedByPlayerState(GetWorld(), this);
		AArenaDuelPhaseGateVisual::DestroyOwnedByPlayerState(GetWorld(), this);
	}
	RemoveAllKitAbilitiesAndCooldowns();
	CharacterArchetype = NewArchetype;
	OnRep_CharacterArchetype();
	bCharacterReady = false;
	GrantCurrentKit();
	if (APawn* CurrentPawn = GetPawn()) AbilitySystemComponent->InitAbilityActorInfo(this, CurrentPawn);
	ForceNetUpdate();
	return true;
}

void AArenaDuelPlayerState::OnRep_CharacterArchetype()
{
	if (auto* Character = Cast<AArenaDuelCharacter>(GetPawn())) Character->RefreshCharacterVisuals();
}

FText AArenaDuelPlayerState::GetCharacterArchetypeDisplayName() const
{
	switch (CharacterArchetype)
	{
	case EArenaDuelCharacterArchetype::Warden: return FText::FromString(TEXT("WARDEN"));
	case EArenaDuelCharacterArchetype::Rift: return FText::FromString(TEXT("RIFT"));
	case EArenaDuelCharacterArchetype::Shadow: return FText::FromString(TEXT("SHADOW"));
	default: return FText::GetEmpty();
	}
}

bool AArenaDuelPlayerState::TryActivatePrimaryAbility()
{
	return AbilitySystemComponent && GetPrimaryAbilityClass() && AbilitySystemComponent->TryActivateAbilityByClass(GetPrimaryAbilityClass());
}

bool AArenaDuelPlayerState::TryActivateSecondaryAbility()
{
	return AbilitySystemComponent && GetSecondaryAbilityClass() && AbilitySystemComponent->TryActivateAbilityByClass(GetSecondaryAbilityClass());
}

float AArenaDuelPlayerState::GetPrimaryAbilityCooldownRemaining() const
{
	return GetCooldownRemaining(GetPrimaryCooldownTag());
}

float AArenaDuelPlayerState::GetSecondaryAbilityCooldownRemaining() const
{
	return GetCooldownRemaining(GetSecondaryCooldownTag());
}

FText AArenaDuelPlayerState::GetPrimaryAbilityDisplayName() const
{
	switch (CharacterArchetype)
	{
	case EArenaDuelCharacterArchetype::Shadow: return FText::FromString(TEXT("SHADOW STEP"));
	case EArenaDuelCharacterArchetype::Warden: return FText::FromString(TEXT("ARC BARRIER"));
	case EArenaDuelCharacterArchetype::Rift: return FText::FromString(TEXT("RIFT GRAPPLE"));
	default: return FText::GetEmpty();
	}
}

FText AArenaDuelPlayerState::GetSecondaryAbilityDisplayName() const
{
	switch (CharacterArchetype)
	{
	case EArenaDuelCharacterArchetype::Shadow: return FText::FromString(TEXT("VEIL WALL"));
	case EArenaDuelCharacterArchetype::Warden: return FText::FromString(TEXT("BURST LEAP"));
	case EArenaDuelCharacterArchetype::Rift: return FText::FromString(TEXT("PHASE GATE"));
	default: return FText::GetEmpty();
	}
}

bool AArenaDuelPlayerState::IsImplementedArchetype(EArenaDuelCharacterArchetype Archetype)
{
	return Archetype == EArenaDuelCharacterArchetype::Shadow || Archetype == EArenaDuelCharacterArchetype::Warden || Archetype == EArenaDuelCharacterArchetype::Rift;
}

TSubclassOf<UGameplayAbility> AArenaDuelPlayerState::GetPrimaryAbilityClass() const
{
	switch (CharacterArchetype)
	{
	case EArenaDuelCharacterArchetype::Shadow: return UArenaDuelGA_ShadowStep::StaticClass();
	case EArenaDuelCharacterArchetype::Warden: return UArenaDuelGA_ArcBarrier::StaticClass();
	case EArenaDuelCharacterArchetype::Rift: return UArenaDuelGA_RiftGrapple::StaticClass();
	default: return nullptr;
	}
}

TSubclassOf<UGameplayAbility> AArenaDuelPlayerState::GetSecondaryAbilityClass() const
{
	switch (CharacterArchetype)
	{
	case EArenaDuelCharacterArchetype::Shadow: return UArenaDuelGA_VeilWall::StaticClass();
	case EArenaDuelCharacterArchetype::Warden: return UArenaDuelGA_BurstLeap::StaticClass();
	case EArenaDuelCharacterArchetype::Rift: return UArenaDuelGA_PhaseGate::StaticClass();
	default: return nullptr;
	}
}

FGameplayTag AArenaDuelPlayerState::GetPrimaryCooldownTag() const
{
	switch (CharacterArchetype)
	{
	case EArenaDuelCharacterArchetype::Shadow: return TAG_Cooldown_Shadow_ShadowStep;
	case EArenaDuelCharacterArchetype::Warden: return TAG_Cooldown_Warden_ArcBarrier;
	case EArenaDuelCharacterArchetype::Rift: return TAG_Cooldown_Rift_RiftGrapple;
	default: return FGameplayTag();
	}
}

FGameplayTag AArenaDuelPlayerState::GetSecondaryCooldownTag() const
{
	switch (CharacterArchetype)
	{
	case EArenaDuelCharacterArchetype::Shadow: return TAG_Cooldown_Shadow_VeilWall;
	case EArenaDuelCharacterArchetype::Warden: return TAG_Cooldown_Warden_BurstLeap;
	case EArenaDuelCharacterArchetype::Rift: return TAG_Cooldown_Rift_PhaseGate;
	default: return FGameplayTag();
	}
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
