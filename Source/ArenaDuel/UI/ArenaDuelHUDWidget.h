// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ArenaDuelHUDWidget.generated.h"

class UTextBlock;
class UProgressBar;
class UCanvasPanel;
class UBorder;
class UWidget;
class UCanvasPanel;
class AArenaDuelGameState;
class AArenaDuelPlayerState;

UCLASS()
class ARENADUEL_API UArenaDuelHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual bool Initialize() override;
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
	void RefreshMatchData();
	void SetText(UTextBlock* TextBlock, const FText& Text) const;

	UPROPERTY() TObjectPtr<UCanvasPanel> RootCanvas;
	UPROPERTY() TObjectPtr<UCanvasPanel> HealthContentCanvas;
	UPROPERTY() TObjectPtr<UCanvasPanel> WeaponContentCanvas;
	UPROPERTY() TObjectPtr<UCanvasPanel> MatchHeaderContentCanvas;
	UPROPERTY() TObjectPtr<UWidget> HealthPanelRoot;
	UPROPERTY() TObjectPtr<UWidget> WeaponPanelRoot;
	UPROPERTY() TObjectPtr<UWidget> MatchHeaderRoot;
	UPROPERTY() TObjectPtr<UWidget> DefeatedRoot;
	UPROPERTY() TObjectPtr<UWidget> MatchResultRoot;
	UPROPERTY() TObjectPtr<UTextBlock> HealthValue;
	UPROPERTY() TObjectPtr<UTextBlock> HealthLabel;
	UPROPERTY() TObjectPtr<UTextBlock> StaminaLabel;
	UPROPERTY() TObjectPtr<UProgressBar> HealthBar;
	UPROPERTY() TObjectPtr<UProgressBar> StaminaBar;
	UPROPERTY() TObjectPtr<UTextBlock> WeaponName;
	UPROPERTY() TObjectPtr<UTextBlock> SlotOverview;
	UPROPERTY() TObjectPtr<UTextBlock> FireMode;
	UPROPERTY() TObjectPtr<UTextBlock> MagazineAmmo;
	UPROPERTY() TObjectPtr<UTextBlock> ReserveAmmo;
	UPROPERTY() TObjectPtr<UTextBlock> ReloadLabel;
	UPROPERTY() TObjectPtr<UTextBlock> DefeatedLabel;
	UPROPERTY() TObjectPtr<UTextBlock> MatchWinnerLabel;
	UPROPERTY() TObjectPtr<UTextBlock> MatchFinalScoreLabel;
	UPROPERTY() TObjectPtr<UTextBlock> MatchRestartLabel;
	UPROPERTY() TObjectPtr<UTextBlock> PlayerLeft;
	UPROPERTY() TObjectPtr<UTextBlock> PlayerRight;
	UPROPERTY() TObjectPtr<UTextBlock> ScoreLeft;
	UPROPERTY() TObjectPtr<UTextBlock> ScoreRight;
	UPROPERTY() TObjectPtr<UTextBlock> TimerLabel;
	UPROPERTY() TArray<TObjectPtr<UTextBlock>> AbilityKeys;
	UPROPERTY() TArray<TObjectPtr<UTextBlock>> AbilityNames;
	UPROPERTY() TArray<TObjectPtr<UTextBlock>> AbilityCooldowns;
	UPROPERTY() TArray<TObjectPtr<UWidget>> AbilityRoots;
	UPROPERTY() TArray<TObjectPtr<UBorder>> AbilityBackgrounds;
	UPROPERTY() TArray<TObjectPtr<UBorder>> AbilityAccents;
	UPROPERTY() TArray<TObjectPtr<UBorder>> LeftRoundIndicators;
	UPROPERTY() TArray<TObjectPtr<UBorder>> RightRoundIndicators;
	TWeakObjectPtr<AArenaDuelGameState> CachedGameState;
	TWeakObjectPtr<AArenaDuelPlayerState> CachedPlayerStates[2];
	int32 LastLeftWins = INDEX_NONE;
	int32 LastRightWins = INDEX_NONE;
	int32 LastRoundNumber = INDEX_NONE;
	bool bLastRoundInProgress = false;
	bool bLastMatchComplete = false;
	bool bWardenAbilityPalette = false;
	bool bRiftAbilityPalette = false;
	int32 LastHealthBand = INDEX_NONE;
	FTimerHandle RefreshTimer;
	FTimerHandle MatchRefreshTimer;
};
