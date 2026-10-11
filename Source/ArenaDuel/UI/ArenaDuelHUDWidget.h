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
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	/** How a confirmed hit is shown at the crosshair, Age seconds after it: opacity 0 to 1 and size (1 is the plain marker). A head shot is larger, lasts longer and punches in. */
	static void ComputeHitMarker(bool bHead, float Age, float& OutOpacity, float& OutScale);
	static float GetHitMarkerSeconds(bool bHead) { return bHead ? 0.55f : 0.22f; }
	bool IsHitMarkerShown() const;
	bool IsHeadshotShown() const { return IsHitMarkerShown() && bHitMarkerHead; }
	/** Shows the marker as one confirmed hit does. A body hit while a head shot is still shown keeps the head shot's marker and word until its own time is over; its sound is its own. */
	void ShowHitMarker(bool bHead, bool bWithSound);
	/** Takes the confirmed hits waiting on this weapon and shows and plays them: each once, no more than one sound of a kind per call. */
	void ShowConfirmedHits(class UArenaDuelWeaponComponent* Weapon, bool bWithSound);
	/** The weapon whose hits are shown. A change of it (a new pawn after a respawn) hides what the old one left. */
	void SetHitFeedbackSource(class UArenaDuelWeaponComponent* Weapon);
	/** Tests: where the marker's ticks have their middle and where it turns and scales about, in the root's own units from its top left, and the root's size. */
	FVector2D GetHitMarkerTickCentre() const;
	FVector2D GetHitMarkerPivotPoint() const;
	FVector2D GetHitMarkerRootSize() const;
	float GetHeadshotLabelCentreY() const;
	int32 GetHitSoundsAsked(bool bHead) const { return bHead ? HeadSoundsAsked : BodySoundsAsked; }
	void SetHitMarkerAgeForTests(float Seconds) { HitMarkerStartSeconds = FPlatformTime::Seconds() - Seconds; }

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
	UPROPERTY() TObjectPtr<UTextBlock> OvertimeLabel;
	// Hit confirmation at the crosshair: four ticks, and a word for a head shot.
	UPROPERTY() TObjectPtr<class UCanvasPanel> HitMarkerRoot;
	UPROPERTY() TArray<TObjectPtr<UBorder>> HitMarkerTicks;
	UPROPERTY() TObjectPtr<UTextBlock> HeadshotLabel;
	TWeakObjectPtr<class UArenaDuelWeaponComponent> HitMarkerWeapon;
	int32 HeadSoundsAsked = 0;
	int32 BodySoundsAsked = 0;
	double HitMarkerStartSeconds = -100.0;
	bool bHitMarkerHead = false;
	FString LastRoundTimerText;
	bool bWardenAbilityPalette = false;
	bool bRiftAbilityPalette = false;
	int32 LastHealthBand = INDEX_NONE;
	FTimerHandle RefreshTimer;
	FTimerHandle MatchRefreshTimer;
};
