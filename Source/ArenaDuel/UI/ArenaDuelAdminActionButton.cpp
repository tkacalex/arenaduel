// Copyright Epic Games, Inc. All Rights Reserved.

#include "ArenaDuelAdminActionButton.h"
#include "Components/TextBlock.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Brushes/SlateColorBrush.h"

void UArenaDuelAdminActionButton::Configure(EArenaDuelAdminCommand InCommand, float InValue, UTextBlock* InLabelWidget, const FLinearColor& InColor, FOnArenaDuelAdminAction InAction, ETextJustify::Type Justification)
{
	Command = InCommand;
	NumericValue = InValue;
	Action = MoveTemp(InAction);
	LabelWidget = InLabelWidget;
	if (!LabelWidget) return;
	SetVisualColor(InColor);
	LabelWidget->SetColorAndOpacity(FLinearColor(0.94f, 0.97f, 1.0f, 1.0f));
	LabelWidget->SetJustification(Justification);
	LabelWidget->SetAutoWrapText(true);
	LabelWidget->SetFont(FSlateFontInfo(GEngine ? static_cast<const UObject*>(GEngine->GetSmallFont()) : nullptr, 14.0f));
	SetContent(LabelWidget);
	InitIsFocusable(true);
	OnClicked.AddUniqueDynamic(this, &UArenaDuelAdminActionButton::HandleButtonClicked);
}

void UArenaDuelAdminActionButton::SetLabel(const FText& InLabel)
{
	if (LabelWidget && !LabelWidget->GetText().EqualTo(InLabel)) LabelWidget->SetText(InLabel);
}

void UArenaDuelAdminActionButton::SetVisualColor(const FLinearColor& InColor)
{
	if (BaseColor.Equals(InColor, 0.002f)) return;
	BaseColor = InColor;
	FButtonStyle Style = FButtonStyle::GetDefault();
	Style.SetNormal(FSlateColorBrush(InColor));
	Style.SetHovered(FSlateColorBrush(FLinearColor::LerpUsingHSV(InColor, FLinearColor(0.43f, 0.91f, 1.0f, InColor.A), 0.16f)));
	Style.SetPressed(FSlateColorBrush(InColor * 0.72f));
	Style.SetDisabled(FSlateColorBrush(FLinearColor(InColor.R, InColor.G, InColor.B, 0.35f)));
	Style.SetNormalPadding(FMargin(15.0f, 4.0f));
	Style.SetPressedPadding(FMargin(15.0f, 5.0f, 15.0f, 3.0f));
	Style.SetNormalForeground(FSlateColor(FLinearColor::White));
	Style.SetHoveredForeground(FSlateColor(FLinearColor::White));
	Style.SetPressedForeground(FSlateColor(FLinearColor::White));
	SetStyle(Style);
	SetColorAndOpacity(FLinearColor::White);
}

void UArenaDuelAdminActionButton::HandleButtonClicked()
{
	Action.ExecuteIfBound(Command, NumericValue);
}
