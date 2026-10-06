// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ArenaDuelAdminActionButton.h"
#include "TimerManager.h"
#include "ArenaDuelAdminWidget.generated.h"

class UBorder;
class UCanvasPanel;
class UEditableTextBox;
class UHorizontalBox;
class UTextBlock;
class UVerticalBox;
class AArenaDuelGameState;
class AArenaDuelPlayerController;
class AArenaDuelPlayerState;
class AArenaDuelCharacter;

UCLASS()
class ARENADUEL_API UArenaDuelAdminWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual bool Initialize() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	void SetActionHandler(FOnArenaDuelAdminAction InAction) { Action = MoveTemp(InAction); }
	uint8 GetSelectedDuelSlot() const { return SelectedDuelSlot; }
	int32 GetAdminSectionCount() const { return Pages.Num(); }

protected:
	void BuildWidgetTree();
	void RefreshAdminState();
	void HandleAction(EArenaDuelAdminCommand Command, float Value);
	void SelectPage(int32 PageIndex);
	UArenaDuelAdminActionButton* AddButton(UHorizontalBox* Row, const FText& Label, EArenaDuelAdminCommand Command, float Value, const FLinearColor& Color);
	void AddSectionLabel(UVerticalBox* Parent, const FText& Label);
	UVerticalBox* CreatePage(const FString& Name);

	UPROPERTY() TObjectPtr<UCanvasPanel> RootCanvas;
	UPROPERTY() TObjectPtr<UBorder> PanelRoot;
	UPROPERTY() TObjectPtr<UTextBlock> TargetReadout;
	UPROPERTY() TObjectPtr<UTextBlock> MovementReadout;
	UPROPERTY() TObjectPtr<UTextBlock> NetworkReadout;
	UPROPERTY() TObjectPtr<UTextBlock> StatusReadout;
	UPROPERTY() TObjectPtr<UEditableTextBox> HealthInput;
	UPROPERTY() TObjectPtr<UEditableTextBox> Player1WinsInput;
	UPROPERTY() TObjectPtr<UEditableTextBox> Player2WinsInput;
	UPROPERTY() TObjectPtr<UArenaDuelAdminActionButton> SetHealthButton;
	UPROPERTY() TObjectPtr<UArenaDuelAdminActionButton> SetPlayer1WinsButton;
	UPROPERTY() TObjectPtr<UArenaDuelAdminActionButton> SetPlayer2WinsButton;
	UPROPERTY() TObjectPtr<UArenaDuelAdminActionButton> ResetMatchButton;
	UPROPERTY() TArray<TObjectPtr<UVerticalBox>> Pages;
	UPROPERTY() TArray<TObjectPtr<UArenaDuelAdminActionButton>> NavButtons;
	UPROPERTY() TArray<TObjectPtr<UArenaDuelAdminActionButton>> TargetButtons;

	FOnArenaDuelAdminAction Action;
	TWeakObjectPtr<AArenaDuelGameState> CachedGameState;
	TWeakObjectPtr<AArenaDuelPlayerState> CachedTargetState;
	TWeakObjectPtr<AArenaDuelCharacter> CachedTargetPawn;
	FTimerHandle RefreshTimer;
	FTimerHandle ResetConfirmTimer;
	uint8 SelectedDuelSlot = 0;
	int32 SelectedPage = 0;
	bool bConfirmingReset = false;
};
