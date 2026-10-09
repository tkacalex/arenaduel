// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "../UI/ArenaDuelAdminTypes.h"
#include "ArenaDuelPlayerState.h"
#include "ArenaDuelLocalSettings.h"
#include "ArenaDuelPlayerController.generated.h"

class UArenaDuelAdminWidget;
class UArenaDuelCharacterSelectWidget;
class UArenaDuelPlayerMenuWidget;

UCLASS()
class ARENADUEL_API AArenaDuelPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	virtual void SetupInputComponent() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	bool IsAdminMenuOpen() const { return bAdminMenuOpen; }
	bool CanUseDevelopmentAdmin() const;
	void SubmitAdminCommand(EArenaDuelAdminCommand Command, uint8 TargetDuelSlot, float NumericValue);
	void CloseAdminMenu();
	void OpenPlayerMenu(bool bFromCharacterSelect = false);
	void ClosePlayerMenu();
	bool IsPlayerMenuOpen() const { return bPlayerMenuOpen; }
	const FArenaDuelLocalSettings& GetLocalSettings() const { return LocalSettings; }
	void ApplyLocalSettings(const FArenaDuelLocalSettings& NewSettings, bool bSave);
	void ApplyCurrentUserSettingsToPawn() { ApplySettingsToPawn(); }
	void RequestCharacterSelection(EArenaDuelCharacterArchetype Archetype);
	void ToggleCharacterReady();
	bool IsCharacterSelectOpen() const { return bCharacterSelectOpen; }
	UArenaDuelCharacterSelectWidget* GetCharacterSelectWidget() const { return CharacterSelectWidget; }
	UFUNCTION(Server, Reliable)
	void ServerRequestCharacterSelection(EArenaDuelCharacterArchetype Archetype);
	UFUNCTION(Server, Reliable)
	void ServerSetCharacterReady(bool bReady);

	// Zombie Survival.
	UFUNCTION(Server, Reliable)
	void ServerSurvivalPurchase(uint8 Item);
	UFUNCTION(Server, Reliable)
	void ServerSurvivalRestart();
	/** Leaves the current game for the main menu. For a host this ends the session. */
	void ReturnToMainMenu();

	UFUNCTION(Server, Reliable)
	void ServerExecuteAdminCommand(EArenaDuelAdminCommand Command, uint8 TargetDuelSlot, float NumericValue);

protected:
	void RefreshMatchPresentation();
	UPROPERTY(Transient)
	TObjectPtr<UArenaDuelCharacterSelectWidget> CharacterSelectWidget;
	FTimerHandle MatchPresentationTimer;
	bool bCharacterSelectOpen = false;
	void ToggleAdminMenu();
	void SurvivalBuyAmmo();
	void SurvivalBuyHeal();
	void SurvivalBuyDamage();
	void SurvivalRestartKey();
	void SurvivalMenuKey();
	void HandleEscape();
	void HandleAdminWidgetAction(EArenaDuelAdminCommand Command, float NumericValue);
	void ExecuteAdminCommandAuthoritatively(EArenaDuelAdminCommand Command, uint8 TargetDuelSlot, float NumericValue);

	UPROPERTY(Transient)
	TObjectPtr<UArenaDuelAdminWidget> AdminWidget;
	UPROPERTY(Transient) TObjectPtr<UArenaDuelPlayerMenuWidget> PlayerMenuWidget;

	bool bAdminMenuOpen = false;
	bool bPreviousMoveInputIgnored = false;
	bool bPreviousLookInputIgnored = false;
	bool bPlayerMenuOpen = false;
	bool bPlayerMenuFromCharacterSelect = false;
	FArenaDuelLocalSettings LocalSettings;
	void ApplySettingsToPawn();
};
