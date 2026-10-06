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

	UCanvasPanelSlot* PlaceFixed(UWidget* Widget, const FVector2D& Anchor, const FVector2D& Position, const FVector2D& Size, const FVector2D& Alignment)
	{
		UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(Widget->Slot);
		if (Slot)
		{
			Slot->SetAnchors(FAnchors(Anchor.X, Anchor.Y));
			Slot->SetPosition(Position);
			Slot->SetSize(Size);
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

bool UArenaDuelHUDWidget::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	if (!WidgetTree)
	{
		WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"), RF_Transient);
	}
	if (!ensureMsgf(WidgetTree, TEXT("ArenaDuel HUD requires a widget tree before Slate rebuild")))
	{
		return false;
	}

	BuildWidgetTree();
	const bool bHasExpectedTree = WidgetTree->RootWidget == RootCanvas
		&& RootCanvas
		&& RootCanvas->GetChildrenCount() == 6
		&& RootCanvas->HasChild(HealthPanelRoot)
		&& RootCanvas->HasChild(WeaponPanelRoot)
		&& RootCanvas->HasChild(AbilityRoots.IsValidIndex(0) ? AbilityRoots[0] : nullptr)
		&& RootCanvas->HasChild(AbilityRoots.IsValidIndex(1) ? AbilityRoots[1] : nullptr)
		&& RootCanvas->HasChild(MatchHeaderRoot)
		&& RootCanvas->HasChild(DefeatedRoot);
	ensureMsgf(bHasExpectedTree, TEXT("ArenaDuel HUD native widget tree is incomplete"));
	return true;
}

void UArenaDuelHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();
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
	HealthPanelRoot = HealthPanel; RootCanvas->AddChild(HealthPanel);
	PlaceFixed(HealthPanel, FVector2D(0,1), FVector2D(45,-38), FVector2D(540,167), FVector2D(0,1));
	UBorder* HealthBackground = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass()); HealthBackground->SetBrushColor(Background); HealthPanel->AddChild(HealthBackground);
	HealthContentCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass()); HealthPanel->AddChild(HealthContentCanvas);
	HealthValue = Text(WidgetTree, TEXT("100"), 78, Primary); HealthContentCanvas->AddChild(HealthValue); PlaceFixed(HealthValue, FVector2D(0,0), FVector2D(20,8), FVector2D(175,86), FVector2D(0,0));
	HealthLabel = Text(WidgetTree, TEXT("HP"), 22, Secondary); HealthContentCanvas->AddChild(HealthLabel); PlaceFixed(HealthLabel, FVector2D(0,0), FVector2D(205,22), FVector2D(80,30), FVector2D(0,0));
	HealthBar = Bar(WidgetTree, Primary); HealthContentCanvas->AddChild(HealthBar); PlaceFixed(HealthBar, FVector2D(0,0), FVector2D(205,50), FVector2D(310,16), FVector2D(0,0));
	StaminaLabel = Text(WidgetTree, TEXT("STAMINA"), 16, Cyan); HealthContentCanvas->AddChild(StaminaLabel); PlaceFixed(StaminaLabel, FVector2D(0,0), FVector2D(205,80), FVector2D(120,24), FVector2D(0,0));
	StaminaBar = Bar(WidgetTree, Cyan); HealthContentCanvas->AddChild(StaminaBar); PlaceFixed(StaminaBar, FVector2D(0,0), FVector2D(205,104), FVector2D(310,12), FVector2D(0,0));

	UOverlay* WeaponPanel = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	WeaponPanelRoot = WeaponPanel; RootCanvas->AddChild(WeaponPanel); PlaceFixed(WeaponPanel, FVector2D(1,1), FVector2D(-45,-38), FVector2D(555,187), FVector2D(1,1));
	UBorder* WeaponBackground = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass()); WeaponBackground->SetBrushColor(Background); WeaponPanel->AddChild(WeaponBackground);
	WeaponContentCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass()); WeaponPanel->AddChild(WeaponContentCanvas);
	WeaponName = Text(WidgetTree, TEXT("ARC RIFLE"), 28, Cyan); WeaponContentCanvas->AddChild(WeaponName); PlaceFixed(WeaponName, FVector2D(0,0), FVector2D(25,12), FVector2D(220,35), FVector2D(0,0));
	FireMode = Text(WidgetTree, TEXT("AUTO"), 18, Secondary); WeaponContentCanvas->AddChild(FireMode); PlaceFixed(FireMode, FVector2D(0,0), FVector2D(25,50), FVector2D(100,28), FVector2D(0,0));
	ReloadLabel = Text(WidgetTree, TEXT("RELOADING"), 17, Cyan); ReloadLabel->SetVisibility(ESlateVisibility::Collapsed); WeaponContentCanvas->AddChild(ReloadLabel); PlaceFixed(ReloadLabel, FVector2D(0,0), FVector2D(140,50), FVector2D(150,28), FVector2D(0,0));
	MagazineAmmo = Text(WidgetTree, TEXT("30"), 88, Primary); WeaponContentCanvas->AddChild(MagazineAmmo); PlaceFixed(MagazineAmmo, FVector2D(0,0), FVector2D(280,12), FVector2D(250,90), FVector2D(0,0));
	ReserveAmmo = Text(WidgetTree, TEXT("/ 120"), 40, Secondary); WeaponContentCanvas->AddChild(ReserveAmmo); PlaceFixed(ReserveAmmo, FVector2D(0,0), FVector2D(360,102), FVector2D(180,50), FVector2D(0,0));

	for (int32 Index = 0; Index < 2; ++Index)
	{
		UOverlay* Ability = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass()); RootCanvas->AddChild(Ability); AbilityRoots.Add(Ability);
		const float X = Index == 0 ? -715.0f : -607.0f;
		PlaceFixed(Ability, FVector2D(1,1), FVector2D(X,-38), FVector2D(100,88), FVector2D(1,1));
		UBorder* AbilityBg = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass()); AbilityBg->SetBrushColor(FLinearColor(0.027f,0.063f,0.106f,0.68f)); Ability->AddChild(AbilityBg); AbilityBackgrounds.Add(AbilityBg);
		UCanvasPanel* AbilityCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass()); Ability->AddChild(AbilityCanvas);
		UTextBlock* Key = Text(WidgetTree, Index == 0 ? TEXT("Q") : TEXT("E"), 25, Secondary); AbilityCanvas->AddChild(Key); PlaceFixed(Key, FVector2D(0,0), FVector2D(0,7), FVector2D(100,30), FVector2D(0,0));
		UTextBlock* Cooldown = Text(WidgetTree, TEXT(""), 16, Cyan); Cooldown->SetVisibility(ESlateVisibility::Collapsed); AbilityCanvas->AddChild(Cooldown); PlaceFixed(Cooldown, FVector2D(0,0), FVector2D(0,27), FVector2D(100,20), FVector2D(0,0));
		UTextBlock* Name = Text(WidgetTree, TEXT("ABILITY"), 13, Secondary); AbilityCanvas->AddChild(Name); PlaceFixed(Name, FVector2D(0,0), FVector2D(0,50), FVector2D(100,20), FVector2D(0,0));
		Key->SetJustification(ETextJustify::Center); Cooldown->SetJustification(ETextJustify::Center); Name->SetJustification(ETextJustify::Center);
		AbilityKeys.Add(Key); AbilityNames.Add(Name); AbilityCooldowns.Add(Cooldown);
	}

	UOverlay* Header = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass()); RootCanvas->AddChild(Header); MatchHeaderRoot = Header; PlaceFixed(Header, FVector2D(0.5f,0), FVector2D(0,26), FVector2D(860,78), FVector2D(0.5f,0));
	UBorder* HeaderBg = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass()); HeaderBg->SetBrushColor(Background); Header->AddChild(HeaderBg);
	MatchHeaderContentCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass()); Header->AddChild(MatchHeaderContentCanvas);
	PlayerLeft = Text(WidgetTree, TEXT("PLAYER 1"), 18, Cyan); MatchHeaderContentCanvas->AddChild(PlayerLeft); PlaceFixed(PlayerLeft, FVector2D(0,0), FVector2D(28,10), FVector2D(180,28), FVector2D(0,0));
	PlayerRight = Text(WidgetTree, TEXT("PLAYER 2"), 18, Violet); MatchHeaderContentCanvas->AddChild(PlayerRight); PlaceFixed(PlayerRight, FVector2D(0,0), FVector2D(652,10), FVector2D(180,28), FVector2D(0,0));
	ScoreLeft = Text(WidgetTree, TEXT("—"), 28, Primary); MatchHeaderContentCanvas->AddChild(ScoreLeft); PlaceFixed(ScoreLeft, FVector2D(0,0), FVector2D(240,8), FVector2D(60,30), FVector2D(0,0));
	ScoreRight = Text(WidgetTree, TEXT("—"), 28, Primary); MatchHeaderContentCanvas->AddChild(ScoreRight); PlaceFixed(ScoreRight, FVector2D(0,0), FVector2D(560,8), FVector2D(60,30), FVector2D(0,0));
	TimerLabel = Text(WidgetTree, TEXT("--:--"), 24, Primary); MatchHeaderContentCanvas->AddChild(TimerLabel); PlaceFixed(TimerLabel, FVector2D(0,0), FVector2D(370,8), FVector2D(120,30), FVector2D(0,0));
	for (int32 Index = 0; Index < 5; ++Index)
	{
		UTextBlock* LeftIndicator = Text(WidgetTree, TEXT("◇"), 16, Secondary); MatchHeaderContentCanvas->AddChild(LeftIndicator); PlaceFixed(LeftIndicator, FVector2D(0,0), FVector2D(28 + Index * 28,45), FVector2D(22,22), FVector2D(0,0)); LeftRoundIndicators.Add(LeftIndicator);
		UTextBlock* RightIndicator = Text(WidgetTree, TEXT("◇"), 16, Secondary); MatchHeaderContentCanvas->AddChild(RightIndicator); PlaceFixed(RightIndicator, FVector2D(0,0), FVector2D(652 + Index * 28,45), FVector2D(22,22), FVector2D(0,0)); RightRoundIndicators.Add(RightIndicator);
	}

	DefeatedLabel = Text(WidgetTree, TEXT("DEFEATED"), 52, Danger); DefeatedLabel->SetJustification(ETextJustify::Center); DefeatedLabel->SetVisibility(ESlateVisibility::Collapsed); DefeatedRoot = DefeatedLabel; RootCanvas->AddChild(DefeatedLabel); PlaceFixed(DefeatedLabel, FVector2D(0.5f,0.5f), FVector2D(0,0), FVector2D(340,70), FVector2D(0.5f,0.5f));
	TimerLabel->SetJustification(ETextJustify::Center);
	ScoreLeft->SetJustification(ETextJustify::Center);
	ScoreRight->SetJustification(ETextJustify::Center);
	MagazineAmmo->SetJustification(ETextJustify::Right);
	ReserveAmmo->SetJustification(ETextJustify::Right);
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
void UArenaDuelHUDWidget::SetMatchHeaderVisible(bool bVisible) { if (MatchHeaderRoot) MatchHeaderRoot->SetVisibility(bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed); }
void UArenaDuelHUDWidget::SetPlayerNames(const FText& LeftName, const FText& RightName) { SetText(PlayerLeft, LeftName); SetText(PlayerRight, RightName); }
void UArenaDuelHUDWidget::SetScores(const FText& LeftScore, const FText& RightScore) { SetText(ScoreLeft, LeftScore); SetText(ScoreRight, RightScore); }
void UArenaDuelHUDWidget::SetRoundTimer(const FText& TimerText) { SetText(TimerLabel, TimerText); }
void UArenaDuelHUDWidget::SetRoundWins(int32 LeftWins, int32 RightWins)
{
	LeftWins = FMath::Clamp(LeftWins, 0, 5);
	RightWins = FMath::Clamp(RightWins, 0, 5);
	for (int32 Index = 0; Index < 5; ++Index)
	{
		if (LeftRoundIndicators.IsValidIndex(Index))
		{
			SetText(LeftRoundIndicators[Index], FText::FromString(Index < LeftWins ? TEXT("◆") : TEXT("◇")));
			LeftRoundIndicators[Index]->SetColorAndOpacity(Index < LeftWins ? Cyan : Secondary);
		}
		if (RightRoundIndicators.IsValidIndex(Index))
		{
			SetText(RightRoundIndicators[Index], FText::FromString(Index < RightWins ? TEXT("◆") : TEXT("◇")));
			RightRoundIndicators[Index]->SetColorAndOpacity(Index < RightWins ? Violet : Secondary);
		}
	}
}
void UArenaDuelHUDWidget::SetAbilitySlotState(int32 Index, const FText& Name, const FText& Key, float Cooldown, bool bReady)
{
	if (!AbilityNames.IsValidIndex(Index)) return;
	SetText(AbilityNames[Index], Name);
	SetText(AbilityKeys[Index], Key);
	const bool bCoolingDown = !bReady && Cooldown > 0.0f;
	if (AbilityCooldowns.IsValidIndex(Index))
	{
		SetText(AbilityCooldowns[Index], FText::FromString(FString::Printf(TEXT("%.1f"), FMath::Max(0.0f, Cooldown))));
		AbilityCooldowns[Index]->SetVisibility(bCoolingDown ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	const FLinearColor Accent = bReady ? (Index == 0 ? Cyan : Violet) : Secondary;
	AbilityNames[Index]->SetColorAndOpacity(Accent);
	AbilityKeys[Index]->SetColorAndOpacity(Accent);
	if (AbilityBackgrounds.IsValidIndex(Index)) AbilityBackgrounds[Index]->SetBrushColor(FLinearColor(0.027f, 0.063f, 0.106f, bReady ? 0.82f : 0.58f));
}
