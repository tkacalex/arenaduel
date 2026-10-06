// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ArenaDuelHUDWidget.generated.h"

class UTextBlock;
class UProgressBar;
class UCanvasPanel;

UCLASS()
class ARENADUEL_API UArenaDuelHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UFUNCTION(BlueprintCallable) void SetMatchHeaderVisible(bool bVisible);
	UFUNCTION(BlueprintCallable) void SetPlayerNames(const FText& LeftName, const FText& RightName);
	UFUNCTION(BlueprintCallable) void SetScores(const FText& LeftScore, const FText& RightScore);
	UFUNCTION(BlueprintCallable) void SetRoundTimer(const FText& TimerText);
	UFUNCTION(BlueprintCallable) void SetRoundWins(int32 LeftWins, int32 RightWins);
	UFUNCTION(BlueprintCallable) void SetAbilitySlotState(int32 Index, const FText& Name, const FText& Key, float Cooldown, bool bReady);

protected:
	void BuildWidgetTree();
	void RefreshData();
	void SetText(UTextBlock* TextBlock, const FText& Text) const;

	UPROPERTY() TObjectPtr<UCanvasPanel> RootCanvas;
	UPROPERTY() TObjectPtr<UTextBlock> HealthValue;
	UPROPERTY() TObjectPtr<UTextBlock> HealthLabel;
	UPROPERTY() TObjectPtr<UTextBlock> StaminaLabel;
	UPROPERTY() TObjectPtr<UProgressBar> HealthBar;
	UPROPERTY() TObjectPtr<UProgressBar> StaminaBar;
	UPROPERTY() TObjectPtr<UTextBlock> WeaponName;
	UPROPERTY() TObjectPtr<UTextBlock> FireMode;
	UPROPERTY() TObjectPtr<UTextBlock> MagazineAmmo;
	UPROPERTY() TObjectPtr<UTextBlock> ReserveAmmo;
	UPROPERTY() TObjectPtr<UTextBlock> ReloadLabel;
	UPROPERTY() TObjectPtr<UTextBlock> DefeatedLabel;
	UPROPERTY() TObjectPtr<UTextBlock> PlayerLeft;
	UPROPERTY() TObjectPtr<UTextBlock> PlayerRight;
	UPROPERTY() TObjectPtr<UTextBlock> ScoreLeft;
	UPROPERTY() TObjectPtr<UTextBlock> ScoreRight;
	UPROPERTY() TObjectPtr<UTextBlock> TimerLabel;
	UPROPERTY() TArray<TObjectPtr<UTextBlock>> AbilityKeys;
	UPROPERTY() TArray<TObjectPtr<UTextBlock>> AbilityNames;
	FTimerHandle RefreshTimer;
};
