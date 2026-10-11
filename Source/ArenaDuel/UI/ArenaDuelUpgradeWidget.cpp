// Copyright Epic Games, Inc. All Rights Reserved.

#include "ArenaDuelUpgradeWidget.h"
#include "../Game/ArenaDuelGameState.h"
#include "../Player/ArenaDuelPlayerController.h"
#include "../Player/ArenaDuelPlayerState.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"
#include "Styling/CoreStyle.h"
#include "TimerManager.h"

namespace
{
	const FLinearColor UpgradeBackdrop(0.012f, 0.024f, 0.045f, 0.90f);
	const FLinearColor UpgradeCardFill(0.027f, 0.063f, 0.106f, 0.96f);
	const FLinearColor UpgradeCardTaken(0.060f, 0.150f, 0.200f, 0.96f);
	const FLinearColor UpgradeCardClosed(0.030f, 0.036f, 0.048f, 0.90f);
	const FLinearColor UpgradePrimary(0.953f, 0.969f, 0.988f, 1.0f);
	const FLinearColor UpgradeSecondary(0.600f, 0.680f, 0.760f, 1.0f);
	const FLinearColor UpgradeAccent(0.475f, 0.914f, 1.0f, 1.0f);
	const FLinearColor UpgradeMuted(0.380f, 0.420f, 0.480f, 1.0f);

	UTextBlock* UpgradeText(UWidgetTree* Tree, UCanvasPanel* Parent, const FString& Value, int32 Size, const FLinearColor& Color, const FVector2D& Position, const FVector2D& Extent, const TCHAR* Typeface = TEXT("Regular"), ETextJustify::Type Justify = ETextJustify::Center)
	{
		UTextBlock* Block = Tree->ConstructWidget<UTextBlock>();
		Block->SetText(FText::FromString(Value));
		Block->SetFont(FCoreStyle::GetDefaultFontStyle(Typeface, Size));
		Block->SetColorAndOpacity(Color);
		Block->SetJustification(Justify);
		Block->SetAutoWrapText(true);
		UCanvasPanelSlot* Slot = Parent->AddChildToCanvas(Block);
		Slot->SetAnchors(FAnchors(0.5f, 0.5f));
		Slot->SetAlignment(FVector2D(0.5f, 0.5f));
		Slot->SetPosition(Position);
		Slot->SetSize(Extent);
		return Block;
	}

	void SetTextIfChanged(UTextBlock* Block, const FString& Value)
	{
		if (Block && Block->GetText().ToString() != Value) Block->SetText(FText::FromString(Value));
	}

	FString Percent(float Share)
	{
		return FString::Printf(TEXT("%d %%"), FMath::RoundToInt(Share * 100.0f));
	}
}

FString UArenaDuelUpgradeWidget::GetUpgradeName(EArenaDuelUpgrade Kind)
{
	switch (Kind)
	{
	case EArenaDuelUpgrade::Reserve: return TEXT("SUPPLY");
	case EArenaDuelUpgrade::Reload: return TEXT("RELOAD");
	case EArenaDuelUpgrade::Ability: return TEXT("ABILITIES");
	default: return TEXT("UPGRADE");
	}
}

FString UArenaDuelUpgradeWidget::DescribeEffect(EArenaDuelUpgrade Kind, float StepPerLevel)
{
	switch (Kind)
	{
	case EArenaDuelUpgrade::Reserve: return FString::Printf(TEXT("+%s reserve ammunition per level. Magazine size stays the same."), *Percent(StepPerLevel));
	case EArenaDuelUpgrade::Reload: return FString::Printf(TEXT("%s shorter reload per level."), *Percent(StepPerLevel));
	case EArenaDuelUpgrade::Ability: return FString::Printf(TEXT("%s shorter ability cooldowns per level."), *Percent(StepPerLevel));
	default: return FString();
	}
}

FString UArenaDuelUpgradeWidget::DescribeLevel(EArenaDuelUpgrade Kind, float StepPerLevel, int32 Level)
{
	if (Level <= 0) return TEXT("no bonus");
	const FString Total = Percent(StepPerLevel * static_cast<float>(Level));
	switch (Kind)
	{
	case EArenaDuelUpgrade::Reserve: return FString::Printf(TEXT("+%s reserve"), *Total);
	case EArenaDuelUpgrade::Reload: return FString::Printf(TEXT("-%s reload time"), *Total);
	case EArenaDuelUpgrade::Ability: return FString::Printf(TEXT("-%s cooldowns"), *Total);
	default: return FString();
	}
}

bool UArenaDuelUpgradeWidget::Initialize()
{
	if (!Super::Initialize()) return false;
	SetIsFocusable(true);
	if (!WidgetTree) WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"), RF_Transient);
	BuildTree();
	return HasExpectedTree();
}

bool UArenaDuelUpgradeWidget::HasExpectedTree() const
{
	if (!RootCanvas || !Title || !Status || !Timer || Cards.Num() != ArenaDuelUpgrades::KindCount) return false;
	for (const FArenaDuelUpgradeCard& Card : Cards) if (!Card.Frame || !Card.Name || !Card.Effect || !Card.Level || !Card.Next || !Card.Button || !Card.ButtonLabel) return false;
	return true;
}

void UArenaDuelUpgradeWidget::BuildTree()
{
	if (!WidgetTree || RootCanvas) return;
	RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>();
	WidgetTree->RootWidget = RootCanvas;
	UBorder* Backdrop = WidgetTree->ConstructWidget<UBorder>();
	Backdrop->SetBrushColor(UpgradeBackdrop);
	UCanvasPanelSlot* BackdropSlot = RootCanvas->AddChildToCanvas(Backdrop);
	BackdropSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
	BackdropSlot->SetOffsets(FMargin(0.0f));

	Title = UpgradeText(WidgetTree, RootCanvas, TEXT("CHOOSE AN UPGRADE"), 30, UpgradePrimary, FVector2D(0.0f, -250.0f), FVector2D(900.0f, 44.0f), TEXT("Bold"));
	Timer = UpgradeText(WidgetTree, RootCanvas, TEXT(""), 16, UpgradeAccent, FVector2D(0.0f, -208.0f), FVector2D(900.0f, 26.0f));
	Status = UpgradeText(WidgetTree, RootCanvas, TEXT(""), 18, UpgradeSecondary, FVector2D(0.0f, 236.0f), FVector2D(900.0f, 30.0f));

	const float CardWidth = 300.0f, CardHeight = 340.0f, Gap = 28.0f;
	Cards.SetNum(ArenaDuelUpgrades::KindCount);
	for (int32 Index = 0; Index < ArenaDuelUpgrades::KindCount; ++Index)
	{
		FArenaDuelUpgradeCard& Card = Cards[Index];
		const float X = (static_cast<float>(Index) - 1.0f) * (CardWidth + Gap);
		Card.Frame = WidgetTree->ConstructWidget<UBorder>();
		Card.Frame->SetBrushColor(UpgradeCardFill);
		UCanvasPanelSlot* FrameSlot = RootCanvas->AddChildToCanvas(Card.Frame);
		FrameSlot->SetAnchors(FAnchors(0.5f, 0.5f));
		FrameSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		FrameSlot->SetPosition(FVector2D(X, 10.0f));
		FrameSlot->SetSize(FVector2D(CardWidth, CardHeight));
		Card.Name = UpgradeText(WidgetTree, RootCanvas, TEXT(""), 24, UpgradeAccent, FVector2D(X, -128.0f), FVector2D(CardWidth - 30.0f, 34.0f), TEXT("Bold"));
		Card.Effect = UpgradeText(WidgetTree, RootCanvas, TEXT(""), 14, UpgradePrimary, FVector2D(X, -70.0f), FVector2D(CardWidth - 40.0f, 64.0f));
		Card.Level = UpgradeText(WidgetTree, RootCanvas, TEXT(""), 16, UpgradePrimary, FVector2D(X, -6.0f), FVector2D(CardWidth - 40.0f, 26.0f), TEXT("Bold"));
		Card.Next = UpgradeText(WidgetTree, RootCanvas, TEXT(""), 14, UpgradeSecondary, FVector2D(X, 44.0f), FVector2D(CardWidth - 40.0f, 60.0f));
		Card.Button = WidgetTree->ConstructWidget<UButton>();
		Card.Button->SetBackgroundColor(FLinearColor(0.10f, 0.30f, 0.40f, 1.0f));
		UCanvasPanelSlot* ButtonSlot = RootCanvas->AddChildToCanvas(Card.Button);
		ButtonSlot->SetAnchors(FAnchors(0.5f, 0.5f));
		ButtonSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		ButtonSlot->SetPosition(FVector2D(X, 132.0f));
		ButtonSlot->SetSize(FVector2D(CardWidth - 60.0f, 46.0f));
		Card.ButtonLabel = WidgetTree->ConstructWidget<UTextBlock>();
		Card.ButtonLabel->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 15));
		Card.ButtonLabel->SetColorAndOpacity(UpgradePrimary);
		Card.ButtonLabel->SetJustification(ETextJustify::Center);
		Card.Button->AddChild(Card.ButtonLabel);
	}
	Cards[0].Button->OnClicked.AddDynamic(this, &ThisClass::ChooseFirst);
	Cards[1].Button->OnClicked.AddDynamic(this, &ThisClass::ChooseSecond);
	Cards[2].Button->OnClicked.AddDynamic(this, &ThisClass::ChooseThird);
}

void UArenaDuelUpgradeWidget::NativeConstruct()
{
	Super::NativeConstruct();
	Refresh();
	if (UWorld* World = GetWorld()) World->GetTimerManager().SetTimer(RefreshTimer, this, &ThisClass::Refresh, 0.1f, true);
}

void UArenaDuelUpgradeWidget::NativeDestruct()
{
	if (UWorld* World = GetWorld()) World->GetTimerManager().ClearTimer(RefreshTimer);
	Super::NativeDestruct();
}

FReply UArenaDuelUpgradeWidget::NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
	const FKey Key = Event.GetKey();
	if (Key == EKeys::One || Key == EKeys::NumPadOne) { Choose(0); return FReply::Handled(); }
	if (Key == EKeys::Two || Key == EKeys::NumPadTwo) { Choose(1); return FReply::Handled(); }
	if (Key == EKeys::Three || Key == EKeys::NumPadThree) { Choose(2); return FReply::Handled(); }
	return Super::NativeOnPreviewKeyDown(Geometry, Event);
}

void UArenaDuelUpgradeWidget::ChooseFirst() { Choose(0); }
void UArenaDuelUpgradeWidget::ChooseSecond() { Choose(1); }
void UArenaDuelUpgradeWidget::ChooseThird() { Choose(2); }

void UArenaDuelUpgradeWidget::Choose(int32 Index)
{
	// The widget only asks. The server decides whether this player may have this upgrade now.
	if (AArenaDuelPlayerController* Controller = Cast<AArenaDuelPlayerController>(GetOwningPlayer())) Controller->ChooseUpgrade(static_cast<uint8>(Index));
}

FString UArenaDuelUpgradeWidget::GetStatusText() const
{
	return Status ? Status->GetText().ToString() : FString();
}

void UArenaDuelUpgradeWidget::Refresh()
{
	const APlayerController* Controller = GetOwningPlayer();
	const AArenaDuelPlayerState* Player = Controller ? Controller->GetPlayerState<AArenaDuelPlayerState>() : nullptr;
	const AArenaDuelGameState* State = GetWorld() ? GetWorld()->GetGameState<AArenaDuelGameState>() : nullptr;
	if (!Player || !State || !HasExpectedTree()) return;
	const bool bChosen = Player->HasChosenUpgrade();
	const int32 MaxLevel = Player->GetMaxUpgradeLevel();
	for (int32 Index = 0; Index < ArenaDuelUpgrades::KindCount; ++Index)
	{
		const EArenaDuelUpgrade Kind = static_cast<EArenaDuelUpgrade>(Index);
		FArenaDuelUpgradeCard& Card = Cards[Index];
		const int32 Level = Player->GetUpgradeLevel(Kind);
		const float Step = Player->GetUpgradeStep(Kind);
		const bool bMaxed = Level >= MaxLevel;
		SetTextIfChanged(Card.Name, GetUpgradeName(Kind));
		SetTextIfChanged(Card.Effect, DescribeEffect(Kind, Step));
		SetTextIfChanged(Card.Level, FString::Printf(TEXT("LEVEL %d / %d   -   %s"), Level, MaxLevel, *DescribeLevel(Kind, Step, Level)));
		SetTextIfChanged(Card.Next, bMaxed ? FString(TEXT("Fully upgraded. Nothing more to gain here.")) : FString::Printf(TEXT("Next level %d: %s"), Level + 1, *DescribeLevel(Kind, Step, Level + 1)));
		SetTextIfChanged(Card.ButtonLabel, bMaxed ? FString(TEXT("MAXED")) : bChosen ? FString(TEXT("LOCKED IN")) : FString::Printf(TEXT("CHOOSE  [%d]"), Index + 1));
		const bool bEnabled = !bMaxed && !bChosen;
		if (Card.Button->GetIsEnabled() != bEnabled) Card.Button->SetIsEnabled(bEnabled);
		const FLinearColor Fill = bMaxed ? UpgradeCardClosed : UpgradeCardFill;
		if (Card.Frame->GetBrushColor() != Fill) Card.Frame->SetBrushColor(Fill);
		const FLinearColor NameColor = bMaxed ? UpgradeMuted : UpgradeAccent;
		if (Card.Name->GetColorAndOpacity().GetSpecifiedColor() != NameColor) Card.Name->SetColorAndOpacity(NameColor);
	}
	SetTextIfChanged(Title, bChosen ? FString(TEXT("UPGRADE TAKEN")) : FString(TEXT("CHOOSE AN UPGRADE")));
	SetTextIfChanged(Timer, FString::Printf(TEXT("Next round in %d s at the latest"), FMath::CeilToInt(State->GetUpgradeSelectSecondsRemaining())));
	SetTextIfChanged(Status, bChosen ? FString(TEXT("Waiting for the other player...")) : FString(TEXT("Both players choose one. No choice in time: the lowest of your upgrades is raised.")));
}
