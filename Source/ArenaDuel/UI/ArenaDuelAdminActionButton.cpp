// Copyright Epic Games, Inc. All Rights Reserved.

#include "ArenaDuelAdminActionButton.h"
#include "Components/TextBlock.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"

void UArenaDuelAdminActionButton::Configure(EArenaDuelAdminCommand InCommand, float InValue, UTextBlock* InLabelWidget, const FLinearColor& InColor, FOnArenaDuelAdminAction InAction)
{
	Command = InCommand;
	NumericValue = InValue;
	Action = MoveTemp(InAction);
	SetBackgroundColor(InColor);
	LabelWidget = InLabelWidget;
	if (!LabelWidget) return;
	LabelWidget->SetColorAndOpacity(FLinearColor(0.94f, 0.97f, 1.0f, 1.0f));
	LabelWidget->SetJustification(ETextJustify::Center);
	LabelWidget->SetFont(FSlateFontInfo(GEngine ? static_cast<const UObject*>(GEngine->GetLargeFont()) : nullptr, 14.0f));
	SetContent(LabelWidget);
	OnClicked.AddUniqueDynamic(this, &UArenaDuelAdminActionButton::HandleButtonClicked);
}

void UArenaDuelAdminActionButton::SetLabel(const FText& InLabel)
{
	if (LabelWidget && !LabelWidget->GetText().EqualTo(InLabel)) LabelWidget->SetText(InLabel);
}

void UArenaDuelAdminActionButton::HandleButtonClicked()
{
	Action.ExecuteIfBound(Command, NumericValue);
}
