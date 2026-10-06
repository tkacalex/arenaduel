// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ArenaDuelAdminActionButton.h"
#include "TimerManager.h"
#include "ArenaDuelAdminWidget.generated.h"

class UBorder;
class UOverlay;
class UGridPanel;
class UEditableTextBox;
class UHorizontalBox;
class UScrollBox;
class UTextBlock;
class UVerticalBox;
class AArenaDuelGameState;
class AArenaDuelPlayerController;
class AArenaDuelPlayerState;
class AArenaDuelCharacter;
struct FKeyEvent;

UCLASS()
class ARENADUEL_API UArenaDuelAdminWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UArenaDuelAdminWidget(const FObjectInitializer& ObjectInitializer);
	virtual bool Initialize() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	void SetActionHandler(FOnArenaDuelAdminAction InAction) { Action = MoveTemp(InAction); }
	uint8 GetSelectedDuelSlot() const { return SelectedDuelSlot; }
	int32 GetAdminSectionCount() const { return Pages.Num(); }
	bool HasPlayerSelector() const { return TargetButtons.Num() == 2; }
	bool HasImplementedArchetypeSelector() const { return ArchetypeButtons.Num() == 3; }

protected:
	void BuildWidgetTree();
	void RefreshAdminState();
	void HandleAction(EArenaDuelAdminCommand Command, float Value);
	void SelectPage(int32 PageIndex);
	UArenaDuelAdminActionButton* AddButton(UHorizontalBox* Row, const FText& Label, EArenaDuelAdminCommand Command, float Value, const FLinearColor& Color, float Width = 188.0f, ETextJustify::Type Justification = ETextJustify::Center);
	UArenaDuelAdminActionButton* AddGridButton(UGridPanel* Grid, int32 Column, int32 Row, const FText& Label, EArenaDuelAdminCommand Command, float Value, const FLinearColor& Color, float Width = 250.0f);
	void AddSectionLabel(UVerticalBox* Parent, const FText& Label);
	void AddHelperText(UVerticalBox* Parent, const FText& Text);
	void SetActionStatus(const FString& Message, const FLinearColor& Color);
	UVerticalBox* AddCard(UVerticalBox* Parent, const FText& Title, const FText& Description = FText());
	UEditableTextBox* AddNumericInput(UHorizontalBox* Parent, const FText& InitialValue, const FText& Hint, float Width = 140.0f);
	UTextBlock* AddStatusCard(UHorizontalBox* Parent, const FText& Label, const FLinearColor& Accent);
	UVerticalBox* CreatePage(const FString& Name, const FString& Description);

	UPROPERTY() TObjectPtr<UOverlay> RootOverlay;
	UPROPERTY() TObjectPtr<UBorder> Backdrop;
	UPROPERTY() TObjectPtr<UVerticalBox> ContentHost;
	UPROPERTY() TObjectPtr<UScrollBox> PageScrollBox;
	UPROPERTY() TObjectPtr<UTextBlock> PageTitle;
	UPROPERTY() TObjectPtr<UTextBlock> PageDescription;
	UPROPERTY() TArray<TObjectPtr<UTextBlock>> TargetStatusValues;
	UPROPERTY() TArray<TObjectPtr<UTextBlock>> MovementStatusValues;
	UPROPERTY() TArray<TObjectPtr<UTextBlock>> NetworkStatusValues;
	UPROPERTY() TObjectPtr<UTextBlock> StatusReadout;
	UPROPERTY() TObjectPtr<UEditableTextBox> HealthInput;
	UPROPERTY() TObjectPtr<UEditableTextBox> Player1WinsInput;
	UPROPERTY() TObjectPtr<UEditableTextBox> Player2WinsInput;
	UPROPERTY() TObjectPtr<UArenaDuelAdminActionButton> GodModeButton;
	UPROPERTY() TObjectPtr<UArenaDuelAdminActionButton> InfiniteAmmoButton;
	UPROPERTY() TObjectPtr<UArenaDuelAdminActionButton> InfiniteStaminaButton;
	UPROPERTY() TObjectPtr<UArenaDuelAdminActionButton> DebugOverlayButton;
	UPROPERTY() TObjectPtr<UArenaDuelAdminActionButton> HitZonesButton;
	UPROPERTY() TObjectPtr<UArenaDuelAdminActionButton> SetHealthButton;
	UPROPERTY() TObjectPtr<UArenaDuelAdminActionButton> SetPlayer1WinsButton;
	UPROPERTY() TObjectPtr<UArenaDuelAdminActionButton> SetPlayer2WinsButton;
	UPROPERTY() TObjectPtr<UArenaDuelAdminActionButton> ResetMatchButton;
	UPROPERTY() TArray<TObjectPtr<UArenaDuelAdminActionButton>> WeaponButtons;
	UPROPERTY() TArray<TObjectPtr<UArenaDuelAdminActionButton>> ArchetypeButtons;
	UPROPERTY() TArray<TObjectPtr<UVerticalBox>> Pages;
	TArray<FString> PageNames;
	TArray<FString> PageDescriptions;
	UPROPERTY() TArray<TObjectPtr<UArenaDuelAdminActionButton>> NavButtons;
	UPROPERTY() TArray<TObjectPtr<UBorder>> NavAccents;
	UPROPERTY() TArray<TObjectPtr<UArenaDuelAdminActionButton>> TargetButtons;
	UPROPERTY() TArray<TObjectPtr<UTextBlock>> TargetReadoutValues;

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
