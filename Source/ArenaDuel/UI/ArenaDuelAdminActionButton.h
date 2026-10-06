// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/Button.h"
#include "ArenaDuelAdminTypes.h"
#include "ArenaDuelAdminActionButton.generated.h"

class UTextBlock;

DECLARE_DELEGATE_TwoParams(FOnArenaDuelAdminAction, EArenaDuelAdminCommand, float);

UCLASS()
class ARENADUEL_API UArenaDuelAdminActionButton : public UButton
{
	GENERATED_BODY()

public:
	void Configure(EArenaDuelAdminCommand InCommand, float InValue, UTextBlock* InLabelWidget, const FLinearColor& InColor, FOnArenaDuelAdminAction InAction, ETextJustify::Type Justification = ETextJustify::Center);
	void SetLabel(const FText& InLabel);
	void SetVisualColor(const FLinearColor& InColor);

protected:
	UFUNCTION()
	void HandleButtonClicked();

	EArenaDuelAdminCommand Command = EArenaDuelAdminCommand::FullHeal;
	float NumericValue = 0.0f;
	TObjectPtr<class UTextBlock> LabelWidget;
	FOnArenaDuelAdminAction Action;
	FLinearColor BaseColor = FLinearColor::Transparent;
};
