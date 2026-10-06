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
	bool HasAdminGodMode() const { return bAdminGodMode; }
	bool HasAdminInfiniteAmmo() const { return bAdminInfiniteAmmo; }
	bool HasAdminInfiniteStamina() const { return bAdminInfiniteStamina; }
	void GrantShadowAbilities();
	void ResetShadowAbilitiesForNewRound();
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
	virtual void BeginPlay() override;
	float GetCooldownRemaining(const FGameplayTag& CooldownTag) const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, Category="Abilities")
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY()
	TObjectPtr<UArenaDuelAttributeSet> AttributeSet;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category="Match")
	uint8 DuelSlot = 0;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category="Match")
	int32 RoundWins = 0;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category="Development")
	bool bAdminGodMode = false;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category="Development")
	bool bAdminInfiniteAmmo = false;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category="Development")
	bool bAdminInfiniteStamina = false;
};
