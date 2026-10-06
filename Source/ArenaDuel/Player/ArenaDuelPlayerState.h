// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "AbilitySystemInterface.h"
#include "ArenaDuelPlayerState.generated.h"

class UAbilitySystemComponent;
class UArenaDuelAttributeSet;

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
	void SetDuelSlot(uint8 NewDuelSlot);
	void AwardRoundWin();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, Category="Abilities")
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY()
	TObjectPtr<UArenaDuelAttributeSet> AttributeSet;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category="Match")
	uint8 DuelSlot = 0;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category="Match")
	int32 RoundWins = 0;
};
