// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "AbilitySystemInterface.h"
#include "GameplayTagContainer.h"
#include "ArenaDuelPlayerState.generated.h"

class UAbilitySystemComponent;
class UArenaDuelAttributeSet;
class UGameplayAbility;

UENUM(BlueprintType)
enum class EArenaDuelCharacterArchetype : uint8
{
	Shadow UMETA(DisplayName="Shadow"),
	Warden UMETA(DisplayName="Warden"),
	Rift UMETA(DisplayName="Rift")
};

UCLASS()
class ARENADUEL_API AArenaDuelPlayerState : public APlayerState, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AArenaDuelPlayerState();
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	UArenaDuelAttributeSet* GetArenaDuelAttributes() const { return AttributeSet; }
	uint8 GetDuelSlot() const { return DuelSlot; }
	int32 GetRoundWins() const { return RoundWins; }
	// Zombie Survival. Kept per player so a co-op run can score everyone separately.
	int32 GetSurvivalPoints() const { return SurvivalPoints; }
	int32 GetSurvivalKills() const { return SurvivalKills; }
	int32 GetSurvivalDamageLevel() const { return SurvivalDamageLevel; }
	void AddSurvivalPoints(int32 Amount) { if (HasAuthority()) SurvivalPoints = FMath::Max(0, SurvivalPoints + Amount); }
	void AddSurvivalKill() { if (HasAuthority()) ++SurvivalKills; }
	void AddSurvivalDamageLevel() { if (HasAuthority()) ++SurvivalDamageLevel; }
	void ResetSurvival() { if (HasAuthority()) { SurvivalPoints = 0; SurvivalKills = 0; SurvivalDamageLevel = 0; } }
	bool HasAdminGodMode() const { return bAdminGodMode; }
	bool HasAdminInfiniteAmmo() const { return bAdminInfiniteAmmo; }
	bool HasAdminInfiniteStamina() const { return bAdminInfiniteStamina; }
	EArenaDuelCharacterArchetype GetCharacterArchetype() const { return CharacterArchetype; }
	static bool IsImplementedArchetype(EArenaDuelCharacterArchetype Archetype);
	TSubclassOf<UGameplayAbility> GetPrimaryAbilityClass() const;
	TSubclassOf<UGameplayAbility> GetSecondaryAbilityClass() const;
	FGameplayTag GetPrimaryCooldownTag() const;
	FGameplayTag GetSecondaryCooldownTag() const;
	FText GetCharacterArchetypeDisplayName() const;
	bool SetCharacterArchetypeForDevelopment(EArenaDuelCharacterArchetype NewArchetype);
	// Authority-only kit replacement shared by public selection and development admin.
	bool SetCharacterArchetypeAuthoritatively(EArenaDuelCharacterArchetype NewArchetype);
	bool IsCharacterReady() const { return bCharacterReady; }
	void SetCharacterReadyAuthoritatively(bool bReady);
	void GrantCharacterAbilities();
	void ResetAbilitiesForNewRound();
	bool TryActivatePrimaryAbility();
	bool TryActivateSecondaryAbility();
	float GetPrimaryAbilityCooldownRemaining() const;
	float GetSecondaryAbilityCooldownRemaining() const;
	FText GetPrimaryAbilityDisplayName() const;
	FText GetSecondaryAbilityDisplayName() const;
	float GetShadowStepCooldownRemaining() const;
	float GetVeilWallCooldownRemaining() const;
	void SetDuelSlot(uint8 NewDuelSlot);
	void AwardRoundWin();
	void SetRoundWinsForDevelopment(int32 NewRoundWins);
	void ToggleAdminGodMode();
	void ToggleAdminInfiniteAmmo();
	void ToggleAdminInfiniteStamina();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category="Character")
	bool bCharacterReady = false;

	virtual void BeginPlay() override;
	UFUNCTION() void OnRep_CharacterArchetype();
	float GetCooldownRemaining(const FGameplayTag& CooldownTag) const;
	void RemoveAllKitAbilitiesAndCooldowns();
	void GrantCurrentKit();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, Category="Abilities")
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY()
	TObjectPtr<UArenaDuelAttributeSet> AttributeSet;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category="Match")
	uint8 DuelSlot = 0;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category="Match")
	int32 RoundWins = 0;

	UPROPERTY(Replicated) int32 SurvivalPoints = 0;
	UPROPERTY(Replicated) int32 SurvivalKills = 0;
	UPROPERTY(Replicated) int32 SurvivalDamageLevel = 0;

	UPROPERTY(ReplicatedUsing=OnRep_CharacterArchetype, VisibleInstanceOnly, BlueprintReadOnly, Category="Character")
	EArenaDuelCharacterArchetype CharacterArchetype = EArenaDuelCharacterArchetype::Shadow;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category="Development")
	bool bAdminGodMode = false;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category="Development")
	bool bAdminInfiniteAmmo = false;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category="Development")
	bool bAdminInfiniteStamina = false;
};
