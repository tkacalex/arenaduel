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
#include "../Game/ArenaDuelGameState.h"
#include "../Player/ArenaDuelPlayerState.h"

namespace
{
	const FLinearColor Background(0.027f, 0.063f, 0.106f, 0.82f);
	const FLinearColor HeaderBackground(0.027f, 0.063f, 0.106f, 0.59f);
	const FLinearColor HUDCyan(0.475f, 0.914f, 1.0f, 1.0f);
	const FLinearColor HUDViolet(0.725f, 0.439f, 1.0f, 1.0f);
	const FLinearColor Primary(0.953f, 0.969f, 0.988f, 1.0f);
	const FLinearColor Secondary(0.608f, 0.667f, 0.737f, 1.0f);
	const FLinearColor IndicatorEmpty(0.20f, 0.25f, 0.31f, 0.75f);
	const FLinearColor HUDDanger(0.882f, 0.357f, 0.412f, 1.0f);

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
		&& RootCanvas->GetChildrenCount() == 7
		&& RootCanvas->HasChild(HealthPanelRoot)
		&& RootCanvas->HasChild(WeaponPanelRoot)
		&& RootCanvas->HasChild(AbilityRoots.IsValidIndex(0) ? AbilityRoots[0] : nullptr)
		&& RootCanvas->HasChild(AbilityRoots.IsValidIndex(1) ? AbilityRoots[1] : nullptr)
		&& RootCanvas->HasChild(MatchHeaderRoot)
		&& RootCanvas->HasChild(DefeatedRoot)
		&& RootCanvas->HasChild(MatchResultRoot);
	ensureMsgf(bHasExpectedTree, TEXT("ArenaDuel HUD native widget tree is incomplete"));
	return true;
}

void UArenaDuelHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().SetTimer(RefreshTimer, this, &UArenaDuelHUDWidget::RefreshData, 0.1f, true);
		GetWorld()->GetTimerManager().SetTimer(MatchRefreshTimer, this, &UArenaDuelHUDWidget::RefreshMatchData, 0.5f, true);
	}
	RefreshData();
	RefreshMatchData();
}

void UArenaDuelHUDWidget::NativeDestruct()
{
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(RefreshTimer);
		GetWorld()->GetTimerManager().ClearTimer(MatchRefreshTimer);
	}
	Super::NativeDestruct();
}

void UArenaDuelHUDWidget::BuildWidgetTree()
{
	if (!WidgetTree || RootCanvas) return;
	RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("ArenaDuelHUDCanvas"));
	WidgetTree->RootWidget = RootCanvas;
	UOverlay* HealthPanel = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	HealthPanelRoot = HealthPanel; RootCanvas->AddChild(HealthPanel);
	PlaceFixed(HealthPanel, FVector2D(0,1), FVector2D(24,-24), FVector2D(398,102), FVector2D(0,1));
	UBorder* HealthBackground = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass()); HealthBackground->SetBrushColor(FLinearColor(0.018f, 0.035f, 0.055f, 0.88f)); HealthPanel->AddChild(HealthBackground);
	HealthContentCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass()); HealthPanel->AddChild(HealthContentCanvas);
	UBorder* HealthAccent = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass()); HealthAccent->SetBrushColor(HUDCyan); HealthContentCanvas->AddChild(HealthAccent); PlaceFixed(HealthAccent, FVector2D(0,0), FVector2D(0,12), FVector2D(3,78), FVector2D(0,0));
	UBorder* HealthDivider = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass()); HealthDivider->SetBrushColor(FLinearColor(0.38f,0.48f,0.58f,0.35f)); HealthContentCanvas->AddChild(HealthDivider); PlaceFixed(HealthDivider, FVector2D(0,0), FVector2D(138,18), FVector2D(1,66), FVector2D(0,0));
	HealthValue = Text(WidgetTree, TEXT("100"), 46, Primary); HealthValue->SetJustification(ETextJustify::Right); HealthContentCanvas->AddChild(HealthValue); PlaceFixed(HealthValue, FVector2D(0,0), FVector2D(16,18), FVector2D(105,58), FVector2D(0,0));
	HealthLabel = Text(WidgetTree, TEXT("HP"), 12, Secondary); HealthContentCanvas->AddChild(HealthLabel); PlaceFixed(HealthLabel, FVector2D(0,0), FVector2D(156,12), FVector2D(44,20), FVector2D(0,0));
	HealthBar = Bar(WidgetTree, Primary); HealthContentCanvas->AddChild(HealthBar); PlaceFixed(HealthBar, FVector2D(0,0), FVector2D(156,36), FVector2D(220,9), FVector2D(0,0));
	StaminaLabel = Text(WidgetTree, TEXT("STAMINA"), 11, HUDCyan); HealthContentCanvas->AddChild(StaminaLabel); PlaceFixed(StaminaLabel, FVector2D(0,0), FVector2D(156,55), FVector2D(100,17), FVector2D(0,0));
	StaminaBar = Bar(WidgetTree, HUDCyan); HealthContentCanvas->AddChild(StaminaBar); PlaceFixed(StaminaBar, FVector2D(0,0), FVector2D(156,77), FVector2D(220,7), FVector2D(0,0));

	UOverlay* WeaponPanel = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	WeaponPanelRoot = WeaponPanel; RootCanvas->AddChild(WeaponPanel); PlaceFixed(WeaponPanel, FVector2D(1,1), FVector2D(-24,-24), FVector2D(310,86), FVector2D(1,1));
	UBorder* WeaponBackground = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass()); WeaponBackground->SetBrushColor(FLinearColor(0.018f, 0.035f, 0.055f, 0.92f)); WeaponPanel->AddChild(WeaponBackground);
	WeaponContentCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass()); WeaponPanel->AddChild(WeaponContentCanvas);
	UBorder* WeaponAccent = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass()); WeaponAccent->SetBrushColor(HUDCyan); WeaponContentCanvas->AddChild(WeaponAccent); PlaceFixed(WeaponAccent, FVector2D(0,0), FVector2D(0,12), FVector2D(3,62), FVector2D(0,0));
	UBorder* AmmoDivider = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass()); AmmoDivider->SetBrushColor(FLinearColor(0.38f, 0.48f, 0.58f, 0.35f)); WeaponContentCanvas->AddChild(AmmoDivider); PlaceFixed(AmmoDivider, FVector2D(0,0), FVector2D(151,15), FVector2D(1,56), FVector2D(0,0));
	WeaponName = Text(WidgetTree, TEXT("ARC RIFLE"), 17, HUDCyan); WeaponContentCanvas->AddChild(WeaponName); PlaceFixed(WeaponName, FVector2D(0,0), FVector2D(15,10), FVector2D(132,25), FVector2D(0,0));
	FireMode = Text(WidgetTree, TEXT("AUTO"), 10, Secondary); WeaponContentCanvas->AddChild(FireMode); PlaceFixed(FireMode, FVector2D(0,0), FVector2D(15,49), FVector2D(48,16), FVector2D(0,0));
	ReloadLabel = Text(WidgetTree, TEXT("RELOADING"), 10, HUDCyan); ReloadLabel->SetVisibility(ESlateVisibility::Collapsed); WeaponContentCanvas->AddChild(ReloadLabel); PlaceFixed(ReloadLabel, FVector2D(0,0), FVector2D(70,49), FVector2D(78,16), FVector2D(0,0));
	MagazineAmmo = Text(WidgetTree, TEXT("30"), 40, Primary); WeaponContentCanvas->AddChild(MagazineAmmo); PlaceFixed(MagazineAmmo, FVector2D(0,0), FVector2D(163,7), FVector2D(68,53), FVector2D(0,0));
	ReserveAmmo = Text(WidgetTree, TEXT("/ 120"), 15, Secondary); WeaponContentCanvas->AddChild(ReserveAmmo); PlaceFixed(ReserveAmmo, FVector2D(0,0), FVector2D(232,25), FVector2D(67,25), FVector2D(0,0));

	for (int32 Index = 0; Index < 2; ++Index)
	{
		UOverlay* Ability = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass()); RootCanvas->AddChild(Ability); AbilityRoots.Add(Ability);
		const float X = Index == 0 ? -478.0f : -346.0f;
		const float Width = Index == 0 ? 130.0f : 120.0f;
		PlaceFixed(Ability, FVector2D(1,1), FVector2D(X,-24), FVector2D(Width,86), FVector2D(1,1));
		UBorder* AbilityBg = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass()); AbilityBg->SetBrushColor(FLinearColor(0.018f,0.035f,0.055f,0.92f)); Ability->AddChild(AbilityBg); AbilityBackgrounds.Add(AbilityBg);
		UCanvasPanel* AbilityCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass()); Ability->AddChild(AbilityCanvas);
		UBorder* AbilityAccent = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass()); AbilityAccent->SetBrushColor(Index == 0 ? HUDCyan : HUDViolet); AbilityCanvas->AddChild(AbilityAccent); PlaceFixed(AbilityAccent, FVector2D(0,0), FVector2D(0,12), FVector2D(3,62), FVector2D(0,0)); AbilityAccents.Add(AbilityAccent);
		UTextBlock* Key = Text(WidgetTree, Index == 0 ? TEXT("Q") : TEXT("E"), 23, Index == 0 ? HUDCyan : HUDViolet); AbilityCanvas->AddChild(Key); PlaceFixed(Key, FVector2D(0,0), FVector2D(14,8), FVector2D(34,34), FVector2D(0,0));
		UTextBlock* Cooldown = Text(WidgetTree, TEXT(""), 13, HUDCyan); Cooldown->SetVisibility(ESlateVisibility::Collapsed); AbilityCanvas->AddChild(Cooldown); PlaceFixed(Cooldown, FVector2D(0,0), FVector2D(56,15), FVector2D(Width-70,23), FVector2D(0,0));
		UTextBlock* Name = Text(WidgetTree, TEXT("ABILITY"), 10, FLinearColor(0.47f,0.52f,0.59f,0.78f)); AbilityCanvas->AddChild(Name); PlaceFixed(Name, FVector2D(0,0), FVector2D(14,49), FVector2D(Width-24,19), FVector2D(0,0));
		Cooldown->SetJustification(ETextJustify::Right);
		AbilityKeys.Add(Key); AbilityNames.Add(Name); AbilityCooldowns.Add(Cooldown);
	}

	UOverlay* Header = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass()); RootCanvas->AddChild(Header); MatchHeaderRoot = Header; PlaceFixed(Header, FVector2D(0.5f,0), FVector2D(0,18), FVector2D(560,56), FVector2D(0.5f,0));
	UBorder* HeaderBg = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass()); HeaderBg->SetBrushColor(HeaderBackground); Header->AddChild(HeaderBg);
	MatchHeaderContentCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass()); Header->AddChild(MatchHeaderContentCanvas);
	PlayerLeft = Text(WidgetTree, TEXT("PLAYER 1"), 13, HUDCyan); MatchHeaderContentCanvas->AddChild(PlayerLeft); PlaceFixed(PlayerLeft, FVector2D(0,0), FVector2D(16,4), FVector2D(112,19), FVector2D(0,0));
	PlayerRight = Text(WidgetTree, TEXT("PLAYER 2"), 13, HUDViolet); PlayerRight->SetJustification(ETextJustify::Right); MatchHeaderContentCanvas->AddChild(PlayerRight); PlaceFixed(PlayerRight, FVector2D(0,0), FVector2D(432,4), FVector2D(112,19), FVector2D(0,0));
	ScoreLeft = Text(WidgetTree, TEXT("0"), 24, Primary); MatchHeaderContentCanvas->AddChild(ScoreLeft); PlaceFixed(ScoreLeft, FVector2D(0,0), FVector2D(156,2), FVector2D(40,28), FVector2D(0,0));
	ScoreRight = Text(WidgetTree, TEXT("0"), 24, Primary); MatchHeaderContentCanvas->AddChild(ScoreRight); PlaceFixed(ScoreRight, FVector2D(0,0), FVector2D(364,2), FVector2D(40,28), FVector2D(0,0));
	TimerLabel = Text(WidgetTree, TEXT("ROUND 1"), 14, Primary); MatchHeaderContentCanvas->AddChild(TimerLabel); PlaceFixed(TimerLabel, FVector2D(0,0), FVector2D(218,4), FVector2D(124,22), FVector2D(0,0));
	for (int32 Index = 0; Index < 5; ++Index)
	{
		UBorder* LeftIndicator = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass()); LeftIndicator->SetBrushColor(IndicatorEmpty); MatchHeaderContentCanvas->AddChild(LeftIndicator); PlaceFixed(LeftIndicator, FVector2D(0,0), FVector2D(18 + Index * 14,34), FVector2D(8,8), FVector2D(0,0)); LeftRoundIndicators.Add(LeftIndicator);
		UBorder* RightIndicator = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass()); RightIndicator->SetBrushColor(IndicatorEmpty); MatchHeaderContentCanvas->AddChild(RightIndicator); PlaceFixed(RightIndicator, FVector2D(0,0), FVector2D(478 + Index * 14,34), FVector2D(8,8), FVector2D(0,0)); RightRoundIndicators.Add(RightIndicator);
	}

	DefeatedLabel = Text(WidgetTree, TEXT("DEFEATED"), 46, HUDDanger); DefeatedLabel->SetJustification(ETextJustify::Center); DefeatedLabel->SetVisibility(ESlateVisibility::Collapsed); DefeatedRoot = DefeatedLabel; RootCanvas->AddChild(DefeatedLabel); PlaceFixed(DefeatedLabel, FVector2D(0.5f,0.5f), FVector2D(0,0), FVector2D(340,64), FVector2D(0.5f,0.5f));

	UOverlay* MatchResult = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("MatchResultRoot"));
	MatchResultRoot = MatchResult;
	MatchResult->SetVisibility(ESlateVisibility::Collapsed);
	RootCanvas->AddChild(MatchResult);
	PlaceFixed(MatchResult, FVector2D(0.5f,0.5f), FVector2D::ZeroVector, FVector2D(520,230), FVector2D(0.5f,0.5f));
	UBorder* MatchResultBackground = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	MatchResultBackground->SetBrushColor(FLinearColor(0.018f, 0.035f, 0.067f, 0.94f));
	MatchResult->AddChild(MatchResultBackground);
	UCanvasPanel* MatchResultCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
	MatchResult->AddChild(MatchResultCanvas);
	MatchWinnerLabel = Text(WidgetTree, TEXT("PLAYER 1 WINS"), 54, HUDCyan);
	MatchWinnerLabel->SetJustification(ETextJustify::Center);
	MatchResultCanvas->AddChild(MatchWinnerLabel);
	PlaceFixed(MatchWinnerLabel, FVector2D(0.5f,0), FVector2D(0,34), FVector2D(480,70), FVector2D(0.5f,0));
	MatchFinalScoreLabel = Text(WidgetTree, TEXT("FINAL SCORE   5 - 0"), 30, Primary);
	MatchFinalScoreLabel->SetJustification(ETextJustify::Center);
	MatchResultCanvas->AddChild(MatchFinalScoreLabel);
	PlaceFixed(MatchFinalScoreLabel, FVector2D(0.5f,0), FVector2D(0,112), FVector2D(480,46), FVector2D(0.5f,0));
	MatchRestartLabel = Text(WidgetTree, TEXT("NEW MATCH STARTING..."), 14, Secondary);
	MatchRestartLabel->SetJustification(ETextJustify::Center);
	MatchResultCanvas->AddChild(MatchRestartLabel);
	PlaceFixed(MatchRestartLabel, FVector2D(0.5f,0), FVector2D(0,177), FVector2D(480,25), FVector2D(0.5f,0));
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
	if (const AArenaDuelPlayerState* PlayerState = Character->GetPlayerState<AArenaDuelPlayerState>())
	{
		const float PrimaryCooldown = PlayerState->GetPrimaryAbilityCooldownRemaining();
		const float SecondaryCooldown = PlayerState->GetSecondaryAbilityCooldownRemaining();
		bWardenAbilityPalette = PlayerState->GetCharacterArchetype() == EArenaDuelCharacterArchetype::Warden;
		bRiftAbilityPalette = PlayerState->GetCharacterArchetype() == EArenaDuelCharacterArchetype::Rift;
		SetAbilitySlotState(0, PlayerState->GetPrimaryAbilityDisplayName(), FText::FromString(TEXT("Q")), PrimaryCooldown, PrimaryCooldown <= KINDA_SMALL_NUMBER);
		SetAbilitySlotState(1, PlayerState->GetSecondaryAbilityDisplayName(), FText::FromString(TEXT("E")), SecondaryCooldown, SecondaryCooldown <= KINDA_SMALL_NUMBER);
	}
	const float MaxHealth = FMath::Max(Character->GetMaxHealth(), 1.0f);
	SetText(HealthValue, FText::AsNumber(FMath::Max(0, FMath::RoundToInt(Character->GetHealth()))));
	const float HealthPercent = FMath::Clamp(Character->GetHealth() / MaxHealth, 0.0f, 1.0f);
	if (FMath::Abs(HealthBar->GetPercent() - HealthPercent) > 0.002f) HealthBar->SetPercent(HealthPercent);
	const int32 HealthBand = HealthPercent <= 0.25f ? 2 : HealthPercent <= 0.5f ? 1 : 0;
	if (HealthBand != LastHealthBand)
	{
		HealthBar->SetFillColorAndOpacity(HealthBand == 2 ? HUDDanger : HealthBand == 1 ? FLinearColor(0.95f,0.65f,0.45f,1) : Primary);
		LastHealthBand = HealthBand;
	}
	if (const UArenaDuelCharacterMovementComponent* Movement = Character->GetArenaDuelMovementComponent())
	{
		const float StaminaPercent = Movement->GetMaxStamina() > 0.0f ? FMath::Clamp(Movement->GetStamina() / Movement->GetMaxStamina(), 0.0f, 1.0f) : 0.0f;
		if (FMath::Abs(StaminaBar->GetPercent() - StaminaPercent) > 0.002f) StaminaBar->SetPercent(StaminaPercent);
	}
	if (const UArenaDuelWeaponComponent* Weapon = Character->GetWeaponComponent())
	{
		SetText(WeaponName, FText::FromName(Weapon->GetCurrentWeaponName()));
		SetText(FireMode, FText::FromString(Weapon->GetCurrentDefinition().bAutomatic ? TEXT("AUTO") : TEXT("SEMI")));
		SetText(MagazineAmmo, FText::AsNumber(Weapon->GetCurrentMagazineAmmo()));
		SetText(ReserveAmmo, FText::FromString(FString::Printf(TEXT("/ %d"), Weapon->GetReserveAmmo())));
		const ESlateVisibility ReloadVisibility = Weapon->IsReloading() ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;
		if (ReloadLabel->GetVisibility() != ReloadVisibility) ReloadLabel->SetVisibility(ReloadVisibility);
	}
	const AArenaDuelGameState* GameState = GetWorld() ? GetWorld()->GetGameState<AArenaDuelGameState>() : nullptr;
	const ESlateVisibility DefeatedVisibility = Character->IsDead() && !(GameState && GameState->IsMatchComplete()) ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;
	if (DefeatedLabel->GetVisibility() != DefeatedVisibility) DefeatedLabel->SetVisibility(DefeatedVisibility);
}

void UArenaDuelHUDWidget::RefreshMatchData()
{
	AArenaDuelGameState* GameState = GetWorld() ? GetWorld()->GetGameState<AArenaDuelGameState>() : nullptr;
	if (CachedGameState.Get() != GameState)
	{
		CachedGameState = GameState;
		CachedPlayerStates[0] = nullptr;
		CachedPlayerStates[1] = nullptr;
	}
	if (!GameState) return;
	const bool bMatchComplete = GameState->IsMatchComplete();
	const int32 MatchWinner = GameState->GetMatchWinnerSlot();
	const ESlateVisibility ResultVisibility = bMatchComplete ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;
	if (MatchResultRoot && MatchResultRoot->GetVisibility() != ResultVisibility) MatchResultRoot->SetVisibility(ResultVisibility);

	if (!CachedPlayerStates[0].IsValid() || !CachedPlayerStates[1].IsValid())
	{
		CachedPlayerStates[0] = nullptr;
		CachedPlayerStates[1] = nullptr;
		for (APlayerState* PlayerState : GameState->PlayerArray)
		{
			if (AArenaDuelPlayerState* DuelPlayerState = Cast<AArenaDuelPlayerState>(PlayerState))
			{
				const uint8 PlayerSlotIndex = DuelPlayerState->GetDuelSlot();
				if (PlayerSlotIndex < 2) CachedPlayerStates[PlayerSlotIndex] = DuelPlayerState;
			}
		}
	}

	const AArenaDuelPlayerState* LeftState = CachedPlayerStates[0].Get();
	const AArenaDuelPlayerState* RightState = CachedPlayerStates[1].Get();
	const int32 LeftWins = LeftState ? LeftState->GetRoundWins() : 0;
	const int32 RightWins = RightState ? RightState->GetRoundWins() : 0;
	if (bMatchComplete && MatchWinner >= 0 && MatchWinner <= 1)
	{
		SetText(MatchWinnerLabel, FText::FromString(MatchWinner == 0 ? TEXT("PLAYER 1 WINS") : TEXT("PLAYER 2 WINS")));
		if (MatchWinnerLabel) MatchWinnerLabel->SetColorAndOpacity(MatchWinner == 0 ? HUDCyan : HUDViolet);
		SetText(MatchFinalScoreLabel, FText::FromString(FString::Printf(TEXT("FINAL SCORE   %d - %d"), LeftWins, RightWins)));
	}
	if (LeftWins != LastLeftWins || RightWins != LastRightWins)
	{
		SetRoundWins(LeftWins, RightWins);
		SetScores(FText::AsNumber(LeftWins), FText::AsNumber(RightWins));
		LastLeftWins = LeftWins;
		LastRightWins = RightWins;
	}

	const int32 RoundNumber = GameState->GetRoundNumber();
	const bool bRoundInProgress = GameState->IsRoundInProgress();
	if (RoundNumber != LastRoundNumber || bRoundInProgress != bLastRoundInProgress || bMatchComplete != bLastMatchComplete)
	{
		SetRoundTimer(FText::FromString(bMatchComplete ? TEXT("MATCH OVER") : bRoundInProgress ? FString::Printf(TEXT("ROUND %d"), RoundNumber) : TEXT("ROUND END")));
		LastRoundNumber = RoundNumber;
		bLastRoundInProgress = bRoundInProgress;
		bLastMatchComplete = bMatchComplete;
	}
}

void UArenaDuelHUDWidget::SetText(UTextBlock* TextBlock, const FText& TextValue) const { if (TextBlock && !TextBlock->GetText().EqualTo(TextValue)) TextBlock->SetText(TextValue); }
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
		if (LeftRoundIndicators.IsValidIndex(Index)) LeftRoundIndicators[Index]->SetBrushColor(Index < LeftWins ? HUDCyan : IndicatorEmpty);
		if (RightRoundIndicators.IsValidIndex(Index)) RightRoundIndicators[Index]->SetBrushColor(Index < RightWins ? HUDViolet : IndicatorEmpty);
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
	const FLinearColor Accent = bRiftAbilityPalette ? (Index == 0 ? HUDViolet : FLinearColor(0.48f, 0.78f, 1.0f))
		: Index == 0 ? HUDCyan : bWardenAbilityPalette ? FLinearColor(0.68f, 0.88f, 1.0f, 1.0f) : HUDViolet;
	AbilityNames[Index]->SetColorAndOpacity(bReady ? Accent : Secondary);
	AbilityKeys[Index]->SetColorAndOpacity(Accent);
	if (AbilityCooldowns.IsValidIndex(Index)) AbilityCooldowns[Index]->SetColorAndOpacity(Accent);
	if (AbilityAccents.IsValidIndex(Index)) AbilityAccents[Index]->SetBrushColor(Accent);
	if (AbilityBackgrounds.IsValidIndex(Index)) AbilityBackgrounds[Index]->SetBrushColor(FLinearColor(0.018f, 0.035f, 0.055f, bReady ? 0.92f : 0.72f));
}
