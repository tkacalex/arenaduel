// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "AbilitySystemInterface.h"
#include "GameplayTagContainer.h"
#include "ActiveGameplayEffectHandle.h"
#include "../Game/ArenaDuelUpgrades.h"
#include "ArenaDuelPlayerState.generated.h"

class UAbilitySystemComponent;
class UArenaDuelAttributeSet;
class UGameplayAbility;
struct FGameplayEffectSpec;

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
	/** Server: joined while both duel places were taken, and so only watches. */
	bool IsDuelSpectator() const { return bDuelSpectator; }
	void SetDuelSpectator(bool bSpectator) { bDuelSpectator = bSpectator; }
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

	// Match upgrades. Levels live here so they survive rounds and respawns; the server is the only writer.
	int32 GetUpgradeLevel(EArenaDuelUpgrade Kind) const { return ArenaDuelUpgrades::IsValidId(static_cast<uint8>(Kind)) ? UpgradeLevels[static_cast<int32>(Kind)] : 0; }
	int32 GetMaxUpgradeLevel() const { return MaxUpgradeLevel; }
	bool CanTakeUpgrade(EArenaDuelUpgrade Kind) const { return ArenaDuelUpgrades::IsValidId(static_cast<uint8>(Kind)) && GetUpgradeLevel(Kind) < MaxUpgradeLevel; }
	bool HasAnyUpgradeAvailable() const;
	/** True once this player has taken (or been given) the upgrade of the running selection. */
	bool HasChosenUpgrade() const { return bUpgradeChosen; }
	/** What one level of a kind changes, as a share of the unchanged base value. */
	float GetUpgradeStep(EArenaDuelUpgrade Kind) const;
	/** The effect of a level, relative to the base value: additive per level, never compounding. */
	float GetUpgradeScaleAtLevel(EArenaDuelUpgrade Kind, int32 Level) const;
	float GetReserveAmmoScale() const { return GetUpgradeScaleAtLevel(EArenaDuelUpgrade::Reserve, GetUpgradeLevel(EArenaDuelUpgrade::Reserve)); }
	float GetReloadTimeScale() const { return GetUpgradeScaleAtLevel(EArenaDuelUpgrade::Reload, GetUpgradeLevel(EArenaDuelUpgrade::Reload)); }
	float GetAbilityCooldownScale() const { return GetUpgradeScaleAtLevel(EArenaDuelUpgrade::Ability, GetUpgradeLevel(EArenaDuelUpgrade::Ability)); }
	/** The upgrade a player gets who did not choose in time: an available kind with the lowest own level, ties in enum order. INDEX_NONE if nothing is left. */
	int32 PickAutomaticUpgrade() const;
	/** Server only. Adds one level; false at the maximum or for an unknown kind. */
	bool AddUpgradeLevelAuthoritatively(EArenaDuelUpgrade Kind);
	void SetUpgradeChosenAuthoritatively(bool bChosen);
	/** Server only. Back to the base values: match reset, return to selection, abandoned match. */
	void ResetUpgradesAuthoritatively();
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

	bool bDuelSpectator = false;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category="Match")
	int32 RoundWins = 0;

	UPROPERTY(Replicated, VisibleInstanceOnly, Category="Upgrades")
	uint8 UpgradeLevels[ArenaDuelUpgrades::KindCount] = {};

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category="Upgrades")
	bool bUpgradeChosen = false;

	// Starting values for play tests.
	UPROPERTY(EditDefaultsOnly, Category="Upgrades", meta=(ClampMin="1", ClampMax="10"))
	int32 MaxUpgradeLevel = 3;
	/** Reserve ammunition gained per level, as a share of the base reserve. */
	UPROPERTY(EditDefaultsOnly, Category="Upgrades", meta=(ClampMin="0"))
	float ReserveAmmoPerLevel = 0.10f;
	/** Reload time removed per level, as a share of the base reload time. */
	UPROPERTY(EditDefaultsOnly, Category="Upgrades", meta=(ClampMin="0", ClampMax="0.3"))
	float ReloadTimePerLevel = 0.05f;
	/** Ability cooldown removed per level, as a share of the base cooldown. */
	UPROPERTY(EditDefaultsOnly, Category="Upgrades", meta=(ClampMin="0", ClampMax="0.3"))
	float AbilityCooldownPerLevel = 0.05f;

	/** Server: shortens a regular ability cooldown the moment it is applied, by this player's upgrade. */
	void HandleEffectApplied(UAbilitySystemComponent* Target, const FGameplayEffectSpec& Spec, FActiveGameplayEffectHandle Handle);

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
