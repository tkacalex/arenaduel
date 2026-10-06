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
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, Category="Abilities")
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY()
	TObjectPtr<UArenaDuelAttributeSet> AttributeSet;
};
