// Copyright Epic Games, Inc. All Rights Reserved.

#include "ArenaDuelHUDWidget.h"
#include "../Characters/ArenaDuelCharacter.h"
#include "../Characters/ArenaDuelCharacterMovementComponent.h"
#include "../Weapons/ArenaDuelWeaponComponent.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "GameFramework/PlayerController.h"

namespace
{
	const FLinearColor Background(0.027f, 0.063f, 0.106f, 0.82f);
	const FLinearColor Cyan(0.475f, 0.914f, 1.0f, 1.0f);
	const FLinearColor Violet(0.725f, 0.439f, 1.0f, 1.0f);
	const FLinearColor Primary(0.953f, 0.969f, 0.988f, 1.0f);
	const FLinearColor Secondary(0.608f, 0.667f, 0.737f, 1.0f);
	const FLinearColor Danger(0.882f, 0.357f, 0.412f, 1.0f);

	UCanvasPanelSlot* Place(UWidget* Widget, const FAnchors& Anchors, const FMargin& Offsets, const FVector2D& Alignment)
	{
		UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(Widget->Slot);
		if (Slot)
		{
			Slot->SetAnchors(Anchors);
			Slot->SetOffsets(Offsets);
			Slot->SetAlignment(Alignment);
		}
		return Slot;
	}

	UTextBlock* Text(UWidgetTree* Tree, const FString& Value, int32 Size, const FLinearColor& Color)
	{
		UTextBlock* Block = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		Block->SetText(FText::FromString(Value));
		Block->SetColorAndOpacity(Color);
		Block->SetFont(FSlateFontInfo(GEngine ? static_cast<const UObject*>(GEngine->GetLargeFont()) : nullptr, static_cast<float>(Size)));
		return Block;
	}

	UProgressBar* Bar(UWidgetTree* Tree, const FLinearColor& Fill)
	{
		UProgressBar* Progress = Tree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass());
		Progress->SetPercent(1.0f);
		Progress->SetFillColorAndOpacity(Fill);
		return Progress;
	}
}

void UArenaDuelHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();
	BuildWidgetTree();
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().SetTimer(RefreshTimer, this, &UArenaDuelHUDWidget::RefreshData, 0.05f, true);
	}
	RefreshData();
}

void UArenaDuelHUDWidget::NativeDestruct()
{
	if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(RefreshTimer);
	Super::NativeDestruct();
}

void UArenaDuelHUDWidget::BuildWidgetTree()
{
	if (!WidgetTree || RootCanvas) return;
	RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("ArenaDuelHUDCanvas"));
	WidgetTree->RootWidget = RootCanvas;

	UOverlay* HealthPanel = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	RootCanvas->AddChild(HealthPanel);
	Place(HealthPanel, FAnchors(0.0f, 1.0f), FMargin(45.0f, -205.0f, 540.0f, -38.0f), FVector2D(0.0f, 1.0f));
	UBorder* HealthBackground = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	HealthBackground->SetBrushColor(Background);
	HealthPanel->AddChild(HealthBackground);
	UOverlay* HealthContent = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	HealthPanel->AddChild(HealthContent);
	HealthValue = Text(WidgetTree, TEXT("100"), 78, Primary); HealthContent->AddChild(HealthValue); Place(HealthValue, FAnchors(0,0), FMargin(20,8,190,94), FVector2D(0,0));
	HealthLabel = Text(WidgetTree, TEXT("HP"), 22, Secondary); HealthContent->AddChild(HealthLabel); Place(HealthLabel, FAnchors(0,0), FMargin(204,22,0,0), FVector2D(0,0));
	HealthBar = Bar(WidgetTree, Primary); HealthContent->AddChild(HealthBar); Place(HealthBar, FAnchors(0,0), FMargin(205,50,500,66), FVector2D(0,0));
	StaminaLabel = Text(WidgetTree, TEXT("STAMINA"), 16, Cyan); HealthContent->AddChild(StaminaLabel); Place(StaminaLabel, FAnchors(0,0), FMargin(205,80,0,0), FVector2D(0,0));
	StaminaBar = Bar(WidgetTree, Cyan); HealthContent->AddChild(StaminaBar); Place(StaminaBar, FAnchors(0,0), FMargin(205,104,500,116), FVector2D(0,0));

	UOverlay* WeaponPanel = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	RootCanvas->AddChild(WeaponPanel); Place(WeaponPanel, FAnchors(1.0f,1.0f), FMargin(-600,-225,-45,-38), FVector2D(1,1));
	UBorder* WeaponBackground = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass()); WeaponBackground->SetBrushColor(Background); WeaponPanel->AddChild(WeaponBackground);
	UOverlay* WeaponContent = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass()); WeaponPanel->AddChild(WeaponContent);
	WeaponName = Text(WidgetTree, TEXT("ARC RIFLE"), 28, Cyan); WeaponContent->AddChild(WeaponName); Place(WeaponName, FAnchors(0,0), FMargin(215,12,0,0), FVector2D(0,0));
	FireMode = Text(WidgetTree, TEXT("AUTO"), 18, Secondary); WeaponContent->AddChild(FireMode); Place(FireMode, FAnchors(0,0), FMargin(215,47,0,0), FVector2D(0,0));
	ReloadLabel = Text(WidgetTree, TEXT("RELOADING"), 17, Cyan); ReloadLabel->SetVisibility(ESlateVisibility::Collapsed); WeaponContent->AddChild(ReloadLabel); Place(ReloadLabel, FAnchors(0,0), FMargin(330,47,0,0), FVector2D(0,0));
	MagazineAmmo = Text(WidgetTree, TEXT("30"), 88, Primary); WeaponContent->AddChild(MagazineAmmo); Place(MagazineAmmo, FAnchors(1,0), FMargin(0,12,575,112), FVector2D(1,0));
	ReserveAmmo = Text(WidgetTree, TEXT("/ 120"), 40, Secondary); WeaponContent->AddChild(ReserveAmmo); Place(ReserveAmmo, FAnchors(1,0), FMargin(0,102,575,155), FVector2D(1,0));

	for (int32 Index = 0; Index < 2; ++Index)
	{
		UOverlay* Ability = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass()); RootCanvas->AddChild(Ability);
		const float X = Index == 0 ? -835.0f : -710.0f;
		Place(Ability, FAnchors(1,1), FMargin(X,-128,X + 100,-40), FVector2D(1,1));
		UBorder* AbilityBg = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass()); AbilityBg->SetBrushColor(FLinearColor(0.027f,0.063f,0.106f,0.68f)); Ability->AddChild(AbilityBg);
		UTextBlock* Key = Text(WidgetTree, Index == 0 ? TEXT("Q") : TEXT("E"), 25, Secondary); Ability->AddChild(Key); Place(Key, FAnchors(0.5f,0), FMargin(-15,8,15,38), FVector2D(0,0));
		UTextBlock* Name = Text(WidgetTree, TEXT("ABILITY"), 13, Secondary); Ability->AddChild(Name); Place(Name, FAnchors(0.5f,1), FMargin(-45,-36,45,-8), FVector2D(0,1));
		AbilityKeys.Add(Key); AbilityNames.Add(Name);
	}

	UOverlay* Header = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass()); RootCanvas->AddChild(Header); Place(Header, FAnchors(0.5f,0), FMargin(-430,26,430,104), FVector2D(0.5f,0));
	UBorder* HeaderBg = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass()); HeaderBg->SetBrushColor(Background); Header->AddChild(HeaderBg);
	PlayerLeft = Text(WidgetTree, TEXT("PLAYER 1"), 18, Cyan); Header->AddChild(PlayerLeft); Place(PlayerLeft, FAnchors(0,0), FMargin(28,10,0,0), FVector2D(0,0));
	PlayerRight = Text(WidgetTree, TEXT("PLAYER 2"), 18, Violet); Header->AddChild(PlayerRight); Place(PlayerRight, FAnchors(1,0), FMargin(0,10, -28,0), FVector2D(1,0));
	ScoreLeft = Text(WidgetTree, TEXT("—"), 28, Primary); Header->AddChild(ScoreLeft); Place(ScoreLeft, FAnchors(0,0), FMargin(210,8,0,0), FVector2D(0,0));
	ScoreRight = Text(WidgetTree, TEXT("—"), 28, Primary); Header->AddChild(ScoreRight); Place(ScoreRight, FAnchors(1,0), FMargin(0,8,-210,0), FVector2D(1,0));
	TimerLabel = Text(WidgetTree, TEXT("--:--"), 24, Primary); Header->AddChild(TimerLabel); Place(TimerLabel, FAnchors(0.5f,0), FMargin(-60,8,60,0), FVector2D(0,0));

	DefeatedLabel = Text(WidgetTree, TEXT("DEFEATED"), 52, Danger); DefeatedLabel->SetVisibility(ESlateVisibility::Collapsed); RootCanvas->AddChild(DefeatedLabel); Place(DefeatedLabel, FAnchors(0.5f,0.5f), FMargin(-170,-35,170,35), FVector2D(0.5f,0.5f));
}

void UArenaDuelHUDWidget::RefreshData()
{
	APlayerController* Controller = GetOwningPlayer();
	AArenaDuelCharacter* Character = Controller ? Cast<AArenaDuelCharacter>(Controller->GetPawn()) : nullptr;
	if (!Character) return;
	const float MaxHealth = FMath::Max(Character->GetMaxHealth(), 1.0f);
	HealthValue->SetText(FText::AsNumber(FMath::Max(0, FMath::RoundToInt(Character->GetHealth()))));
	HealthBar->SetPercent(FMath::Clamp(Character->GetHealth() / MaxHealth, 0.0f, 1.0f));
	HealthBar->SetFillColorAndOpacity(Character->GetHealth() <= MaxHealth * 0.25f ? Danger : Character->GetHealth() <= MaxHealth * 0.5f ? FLinearColor(0.95f,0.65f,0.45f,1) : Primary);
	if (const UArenaDuelCharacterMovementComponent* Movement = Character->GetArenaDuelMovementComponent()) StaminaBar->SetPercent(Movement->GetMaxStamina() > 0.0f ? Movement->GetStamina() / Movement->GetMaxStamina() : 0.0f);
	if (const UArenaDuelWeaponComponent* Weapon = Character->GetWeaponComponent())
	{
		WeaponName->SetText(FText::FromName(Weapon->GetCurrentWeaponName()));
		FireMode->SetText(FText::FromString(Weapon->GetCurrentDefinition().bAutomatic ? TEXT("AUTO") : TEXT("SEMI")));
		MagazineAmmo->SetText(FText::AsNumber(Weapon->GetCurrentMagazineAmmo()));
		ReserveAmmo->SetText(FText::FromString(FString::Printf(TEXT("/ %d"), Weapon->GetReserveAmmo())));
		ReloadLabel->SetVisibility(Weapon->IsReloading() ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	DefeatedLabel->SetVisibility(Character->IsDead() ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}

void UArenaDuelHUDWidget::SetText(UTextBlock* TextBlock, const FText& TextValue) const { if (TextBlock) TextBlock->SetText(TextValue); }
void UArenaDuelHUDWidget::SetMatchHeaderVisible(bool bVisible) { if (PlayerLeft) PlayerLeft->SetVisibility(bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed); }
void UArenaDuelHUDWidget::SetPlayerNames(const FText& LeftName, const FText& RightName) { SetText(PlayerLeft, LeftName); SetText(PlayerRight, RightName); }
void UArenaDuelHUDWidget::SetScores(const FText& LeftScore, const FText& RightScore) { SetText(ScoreLeft, LeftScore); SetText(ScoreRight, RightScore); }
void UArenaDuelHUDWidget::SetRoundTimer(const FText& TimerText) { SetText(TimerLabel, TimerText); }
void UArenaDuelHUDWidget::SetRoundWins(int32 LeftWins, int32 RightWins) { SetScores(FText::AsNumber(LeftWins), FText::AsNumber(RightWins)); }
void UArenaDuelHUDWidget::SetAbilitySlotState(int32 Index, const FText& Name, const FText& Key, float Cooldown, bool bReady) { if (AbilityNames.IsValidIndex(Index)) { SetText(AbilityNames[Index], Name); SetText(AbilityKeys[Index], Key); AbilityNames[Index]->SetColorAndOpacity(bReady ? Cyan : Secondary); } }
