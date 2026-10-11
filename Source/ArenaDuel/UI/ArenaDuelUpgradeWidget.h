// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "../Game/ArenaDuelUpgrades.h"
#include "ArenaDuelUpgradeWidget.generated.h"

class AArenaDuelPlayerState;
class UBorder;
class UButton;
class UCanvasPanel;
class UTextBlock;

USTRUCT()
struct FArenaDuelUpgradeCard
{
	GENERATED_BODY()
	UPROPERTY() TObjectPtr<UBorder> Frame;
	UPROPERTY() TObjectPtr<UTextBlock> Name;
	UPROPERTY() TObjectPtr<UTextBlock> Effect;
	UPROPERTY() TObjectPtr<UTextBlock> Level;
	UPROPERTY() TObjectPtr<UTextBlock> Next;
	UPROPERTY() TObjectPtr<UButton> Button;
	UPROPERTY() TObjectPtr<UTextBlock> ButtonLabel;
};

/**
 * The upgrade selection between two rounds of a duel: three cards, one per kind, each with its name, what a
 * level does, the player's level and what the next level brings. After a choice it shows a waiting state
 * until the other player is done or the time is up. It only displays replicated state and sends the choice.
 */
UCLASS(meta=(DisableNativeTick))
class ARENADUEL_API UArenaDuelUpgradeWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	virtual bool Initialize() override;
	bool HasExpectedTree() const;
	/** The texts of a card, shared with tests and documentation: what the kind is called and what it does. */
	static FString GetUpgradeName(EArenaDuelUpgrade Kind);
	static FString DescribeEffect(EArenaDuelUpgrade Kind, float StepPerLevel);
	static FString DescribeLevel(EArenaDuelUpgrade Kind, float StepPerLevel, int32 Level);
	const FArenaDuelUpgradeCard* GetCard(int32 Index) const { return Cards.IsValidIndex(Index) ? &Cards[Index] : nullptr; }
	FString GetStatusText() const;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;
	void BuildTree();
	void Refresh();
	void Choose(int32 Index);
	UFUNCTION() void ChooseFirst();
	UFUNCTION() void ChooseSecond();
	UFUNCTION() void ChooseThird();
	UPROPERTY(Transient) TObjectPtr<UCanvasPanel> RootCanvas;
	UPROPERTY(Transient) TArray<FArenaDuelUpgradeCard> Cards;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> Title;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> Status;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> Timer;
	FTimerHandle RefreshTimer;
};
