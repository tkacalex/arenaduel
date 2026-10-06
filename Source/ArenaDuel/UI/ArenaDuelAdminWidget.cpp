// Copyright Epic Games, Inc. All Rights Reserved.

#include "ArenaDuelAdminWidget.h"

#include "../Characters/ArenaDuelCharacter.h"
#include "../Characters/ArenaDuelCharacterMovementComponent.h"
#include "../Game/ArenaDuelGameState.h"
#include "../Player/ArenaDuelPlayerController.h"
#include "../Player/ArenaDuelPlayerState.h"
#include "../Weapons/ArenaDuelWeaponComponent.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Misc/DefaultValueHelper.h"
#include "TimerManager.h"

namespace
{
	const FLinearColor PanelColor(0.018f, 0.035f, 0.063f, 0.97f);
	const FLinearColor Cyan(0.34f, 0.86f, 0.98f, 1.0f);
	const FLinearColor Violet(0.69f, 0.39f, 0.94f, 1.0f);
	const FLinearColor Red(0.48f, 0.13f, 0.18f, 0.95f);
	const FLinearColor ButtonColor(0.07f, 0.13f, 0.19f, 0.98f);
	const FLinearColor TextColor(0.93f, 0.96f, 1.0f, 1.0f);
	const FLinearColor MutedColor(0.58f, 0.67f, 0.75f, 1.0f);

	UTextBlock* MakeText(UWidgetTree* Tree, const FText& Text, float Size, const FLinearColor& Color)
	{
		UTextBlock* Result = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		Result->SetText(Text);
		Result->SetColorAndOpacity(Color);
		Result->SetFont(FSlateFontInfo(GEngine ? static_cast<const UObject*>(GEngine->GetLargeFont()) : nullptr, Size));
		return Result;
	}

	void PlaceInCanvas(UWidget* Widget, const FVector2D& Anchor, const FVector2D& Position, const FVector2D& Size, const FVector2D& Alignment)
	{
		if (UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(Widget->Slot))
		{
			Slot->SetAnchors(FAnchors(Anchor.X, Anchor.Y));
			Slot->SetPosition(Position);
			Slot->SetSize(Size);
			Slot->SetAlignment(Alignment);
		}
	}

	FString NetModeName(ENetMode Mode)
	{
		switch (Mode)
		{
		case NM_Standalone: return TEXT("STANDALONE");
		case NM_ListenServer: return TEXT("LISTEN SERVER");
		case NM_DedicatedServer: return TEXT("DEDICATED SERVER");
		default: return TEXT("CLIENT");
		}
	}

	FString MovementModeName(EMovementMode Mode)
	{
		switch (Mode)
		{
		case MOVE_None: return TEXT("NONE");
		case MOVE_Walking: return TEXT("WALKING");
		case MOVE_NavWalking: return TEXT("NAV WALKING");
		case MOVE_Falling: return TEXT("FALLING");
		case MOVE_Swimming: return TEXT("SWIMMING");
		case MOVE_Flying: return TEXT("FLYING");
		case MOVE_Custom: return TEXT("CUSTOM");
		default: return TEXT("UNKNOWN");
		}
	}
}

bool UArenaDuelAdminWidget::Initialize()
{
	if (!Super::Initialize()) return false;
	BuildWidgetTree();
	return RootCanvas != nullptr && WidgetTree && WidgetTree->RootWidget == RootCanvas;
}

void UArenaDuelAdminWidget::NativeConstruct()
{
	Super::NativeConstruct();
	RefreshAdminState();
	if (GetWorld()) GetWorld()->GetTimerManager().SetTimer(RefreshTimer, this, &UArenaDuelAdminWidget::RefreshAdminState, 0.2f, true);
}

void UArenaDuelAdminWidget::NativeDestruct()
{
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(RefreshTimer);
		GetWorld()->GetTimerManager().ClearTimer(ResetConfirmTimer);
	}
	Super::NativeDestruct();
}

void UArenaDuelAdminWidget::BuildWidgetTree()
{
	if (!WidgetTree || RootCanvas) return;
	RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
	WidgetTree->RootWidget = RootCanvas;
	PanelRoot = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	PanelRoot->SetBrushColor(PanelColor);
	RootCanvas->AddChild(PanelRoot);
	PlaceInCanvas(PanelRoot, FVector2D(0.5f, 0.5f), FVector2D::ZeroVector, FVector2D(920.0f, 650.0f), FVector2D(0.5f, 0.5f));

	UVerticalBox* Main = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	PanelRoot->SetContent(Main);
	UHorizontalBox* Header = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Main->AddChildToVerticalBox(Header)->SetPadding(FMargin(22.0f, 18.0f, 22.0f, 12.0f));
	UVerticalBox* TitleStack = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Header->AddChildToHorizontalBox(TitleStack)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	TitleStack->AddChildToVerticalBox(MakeText(WidgetTree, FText::FromString(TEXT("ARENADUEL  /  ADMIN CONTROL")), 24.0f, Cyan));
	TitleStack->AddChildToVerticalBox(MakeText(WidgetTree, FText::FromString(TEXT("DEVELOPMENT ONLY  ·  HOST AUTHORIZED")), 11.0f, MutedColor));
	UTextBlock* CloseHint = MakeText(WidgetTree, FText::FromString(TEXT("F1 / ESC  CLOSE")), 12.0f, MutedColor);
	Header->AddChildToHorizontalBox(CloseHint)->SetVerticalAlignment(VAlign_Center);

	UHorizontalBox* Body = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Main->AddChildToVerticalBox(Body)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	UVerticalBox* NavBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Body->AddChildToHorizontalBox(NavBox)->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
	NavBox->AddChildToVerticalBox(MakeText(WidgetTree, FText::FromString(TEXT("CONTROL")), 12.0f, MutedColor))->SetPadding(FMargin(16.0f, 10.0f, 8.0f, 10.0f));
	const FText NavLabels[] = { FText::FromString(TEXT("PLAYER")), FText::FromString(TEXT("WEAPONS")), FText::FromString(TEXT("ROUND")), FText::FromString(TEXT("MOVEMENT")), FText::FromString(TEXT("DEBUG / NET")) };
	const EArenaDuelAdminCommand NavCommands[] = { EArenaDuelAdminCommand::SelectPlayerPage, EArenaDuelAdminCommand::SelectWeaponsPage, EArenaDuelAdminCommand::SelectRoundPage, EArenaDuelAdminCommand::SelectMovementPage, EArenaDuelAdminCommand::SelectDebugPage };
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(NavLabels); ++Index)
	{
		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		NavBox->AddChildToVerticalBox(Row)->SetPadding(FMargin(8.0f, 3.0f));
		NavButtons.Add(AddButton(Row, NavLabels[Index], NavCommands[Index], 0.0f, Index == 0 ? FLinearColor(0.04f, 0.22f, 0.30f, 1.0f) : ButtonColor));
	}

	UVerticalBox* Content = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Body->AddChildToHorizontalBox(Content)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	UHorizontalBox* TargetRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Content->AddChildToVerticalBox(TargetRow)->SetPadding(FMargin(10.0f, 10.0f, 18.0f, 8.0f));
	TargetButtons.Add(AddButton(TargetRow, FText::FromString(TEXT("PLAYER 1")), EArenaDuelAdminCommand::SelectPlayer1, 0.0f, FLinearColor(0.04f, 0.20f, 0.28f, 1.0f)));
	TargetButtons.Add(AddButton(TargetRow, FText::FromString(TEXT("PLAYER 2")), EArenaDuelAdminCommand::SelectPlayer2, 0.0f, FLinearColor(0.19f, 0.10f, 0.28f, 1.0f)));
	TargetReadout = MakeText(WidgetTree, FText::FromString(TEXT("TARGET: PLAYER 1  |  RESOLVING")), 13.0f, TextColor);
	TargetReadout->SetAutoWrapText(true);
	Content->AddChildToVerticalBox(TargetReadout)->SetPadding(FMargin(12.0f, 4.0f, 12.0f, 10.0f));

	UVerticalBox* PlayerPage = CreatePage(TEXT("PLAYER"));
	AddSectionLabel(PlayerPage, FText::FromString(TEXT("PLAYER STATE")));
	UHorizontalBox* HealthRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass()); PlayerPage->AddChildToVerticalBox(HealthRow)->SetPadding(FMargin(0, 4, 0, 8));
	HealthInput = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass()); HealthInput->SetText(FText::FromString(TEXT("100"))); HealthInput->SetHintText(FText::FromString(TEXT("Health 0 to MaxHealth"))); HealthRow->AddChildToHorizontalBox(HealthInput)->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
	SetHealthButton = AddButton(HealthRow, FText::FromString(TEXT("SET HEALTH")), EArenaDuelAdminCommand::SetHealth, 0.0f, ButtonColor);
	UHorizontalBox* PlayerActions = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass()); PlayerPage->AddChildToVerticalBox(PlayerActions)->SetPadding(FMargin(0, 3, 0, 8));
	AddButton(PlayerActions, FText::FromString(TEXT("FULL HEAL")), EArenaDuelAdminCommand::FullHeal, 0.0f, ButtonColor);
	AddButton(PlayerActions, FText::FromString(TEXT("KILL")), EArenaDuelAdminCommand::Kill, 0.0f, Red);
	AddButton(PlayerActions, FText::FromString(TEXT("GOD MODE: TOGGLE")), EArenaDuelAdminCommand::ToggleGodMode, 0.0f, FLinearColor(0.10f, 0.15f, 0.22f, 1.0f));
	UHorizontalBox* ResetPlayerRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass()); PlayerPage->AddChildToVerticalBox(ResetPlayerRow)->SetPadding(FMargin(0, 3, 0, 8));
	AddButton(ResetPlayerRow, FText::FromString(TEXT("RESET PLAYER  ·  ALIVE ONLY")), EArenaDuelAdminCommand::ResetPlayer, 0.0f, FLinearColor(0.29f, 0.17f, 0.12f, 1.0f));
	PlayerPage->AddChildToVerticalBox(MakeText(WidgetTree, FText::FromString(TEXT("Dead players recover through RESTART ROUND. Reset Player never awards a round.")), 12.0f, MutedColor))->SetPadding(FMargin(0, 10, 0, 0));

	UVerticalBox* WeaponsPage = CreatePage(TEXT("WEAPONS"));
	AddSectionLabel(WeaponsPage, FText::FromString(TEXT("EQUIP WEAPON")));
	UHorizontalBox* WeaponRowA = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass()); WeaponsPage->AddChildToVerticalBox(WeaponRowA)->SetPadding(FMargin(0, 4, 0, 8));
	AddButton(WeaponRowA, FText::FromString(TEXT("ARC RIFLE")), EArenaDuelAdminCommand::EquipWeapon, 0.0f, ButtonColor);
	AddButton(WeaponRowA, FText::FromString(TEXT("SHADE SMG")), EArenaDuelAdminCommand::EquipWeapon, 1.0f, ButtonColor);
	UHorizontalBox* WeaponRowB = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass()); WeaponsPage->AddChildToVerticalBox(WeaponRowB)->SetPadding(FMargin(0, 4, 0, 12));
	AddButton(WeaponRowB, FText::FromString(TEXT("RUNE DMR")), EArenaDuelAdminCommand::EquipWeapon, 2.0f, ButtonColor);
	AddButton(WeaponRowB, FText::FromString(TEXT("HEX SHOTGUN")), EArenaDuelAdminCommand::EquipWeapon, 3.0f, ButtonColor);
	UHorizontalBox* AmmoRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass()); WeaponsPage->AddChildToVerticalBox(AmmoRow)->SetPadding(FMargin(0, 4, 0, 8));
	AddButton(AmmoRow, FText::FromString(TEXT("REFILL ALL AMMO")), EArenaDuelAdminCommand::RefillAmmo, 0.0f, ButtonColor);
	AddButton(AmmoRow, FText::FromString(TEXT("TOGGLE INFINITE AMMO")), EArenaDuelAdminCommand::ToggleInfiniteAmmo, 0.0f, FLinearColor(0.10f, 0.15f, 0.22f, 1.0f));

	UVerticalBox* RoundPage = CreatePage(TEXT("ROUND"));
	AddSectionLabel(RoundPage, FText::FromString(TEXT("ROUND FLOW")));
	UHorizontalBox* RoundRowA = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass()); RoundPage->AddChildToVerticalBox(RoundRowA)->SetPadding(FMargin(0, 4, 0, 8));
	AddButton(RoundRowA, FText::FromString(TEXT("RESTART CURRENT ROUND")), EArenaDuelAdminCommand::RestartRound, 0.0f, ButtonColor);
	AddButton(RoundRowA, FText::FromString(TEXT("NEXT ROUND")), EArenaDuelAdminCommand::NextRound, 0.0f, ButtonColor);
	UHorizontalBox* RoundRowB = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass()); RoundPage->AddChildToVerticalBox(RoundRowB)->SetPadding(FMargin(0, 4, 0, 12));
	AddButton(RoundRowB, FText::FromString(TEXT("AWARD PLAYER 1 ROUND")), EArenaDuelAdminCommand::AwardRound, 0.0f, FLinearColor(0.04f, 0.18f, 0.24f, 1.0f));
	AddButton(RoundRowB, FText::FromString(TEXT("AWARD PLAYER 2 ROUND")), EArenaDuelAdminCommand::AwardRound, 1.0f, FLinearColor(0.17f, 0.09f, 0.24f, 1.0f));
	AddSectionLabel(RoundPage, FText::FromString(TEXT("SET WINS  ·  0 TO 5  ·  DOES NOT END A ROUND")));
	UHorizontalBox* ScoreRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass()); RoundPage->AddChildToVerticalBox(ScoreRow)->SetPadding(FMargin(0, 4, 0, 8));
	Player1WinsInput = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass()); Player1WinsInput->SetText(FText::FromString(TEXT("0"))); ScoreRow->AddChildToHorizontalBox(Player1WinsInput);
	SetPlayer1WinsButton = AddButton(ScoreRow, FText::FromString(TEXT("SET P1 WINS")), EArenaDuelAdminCommand::SetPlayer1Wins, 0.0f, ButtonColor);
	Player2WinsInput = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass()); Player2WinsInput->SetText(FText::FromString(TEXT("0"))); ScoreRow->AddChildToHorizontalBox(Player2WinsInput);
	SetPlayer2WinsButton = AddButton(ScoreRow, FText::FromString(TEXT("SET P2 WINS")), EArenaDuelAdminCommand::SetPlayer2Wins, 0.0f, ButtonColor);
	UHorizontalBox* ResetMatchRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass()); RoundPage->AddChildToVerticalBox(ResetMatchRow)->SetPadding(FMargin(0, 10, 0, 8));
	ResetMatchButton = AddButton(ResetMatchRow, FText::FromString(TEXT("RESET MATCH")), EArenaDuelAdminCommand::ResetMatch, 0.0f, Red);

	UVerticalBox* MovementPage = CreatePage(TEXT("MOVEMENT"));
	AddSectionLabel(MovementPage, FText::FromString(TEXT("LIVE MOVEMENT")));
	MovementReadout = MakeText(WidgetTree, FText::FromString(TEXT("Waiting for selected pawn...")), 14.0f, TextColor);
	MovementPage->AddChildToVerticalBox(MovementReadout)->SetPadding(FMargin(0, 4, 0, 14));
	UHorizontalBox* MovementActions = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass()); MovementPage->AddChildToVerticalBox(MovementActions)->SetPadding(FMargin(0, 4, 0, 8));
	AddButton(MovementActions, FText::FromString(TEXT("REFILL STAMINA")), EArenaDuelAdminCommand::RefillStamina, 0.0f, ButtonColor);
	AddButton(MovementActions, FText::FromString(TEXT("TOGGLE INFINITE STAMINA")), EArenaDuelAdminCommand::ToggleInfiniteStamina, 0.0f, FLinearColor(0.10f, 0.15f, 0.22f, 1.0f));
	MovementPage->AddChildToVerticalBox(MakeText(WidgetTree, FText::FromString(TEXT("Movement tuning is intentionally not exposed here because it requires prediction-aware controls.")), 12.0f, MutedColor))->SetPadding(FMargin(0, 12, 0, 0));

	UVerticalBox* DebugPage = CreatePage(TEXT("DEBUG / NET"));
	AddSectionLabel(DebugPage, FText::FromString(TEXT("LOCAL DIAGNOSTICS")));
	UHorizontalBox* DebugActions = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass()); DebugPage->AddChildToVerticalBox(DebugActions)->SetPadding(FMargin(0, 4, 0, 12));
	AddButton(DebugActions, FText::FromString(TEXT("TOGGLE DEBUG OVERLAY")), EArenaDuelAdminCommand::ToggleDebugOverlay, 0.0f, ButtonColor);
	AddButton(DebugActions, FText::FromString(TEXT("TOGGLE HIT ZONES")), EArenaDuelAdminCommand::ToggleHitZones, 0.0f, ButtonColor);
	NetworkReadout = MakeText(WidgetTree, FText::FromString(TEXT("NET: --")), 14.0f, TextColor);
	DebugPage->AddChildToVerticalBox(NetworkReadout)->SetPadding(FMargin(0, 6, 0, 8));
	DebugPage->AddChildToVerticalBox(MakeText(WidgetTree, FText::FromString(TEXT("Hit-zone display is local debug drawing only. Collision remains unchanged.")), 12.0f, MutedColor));

	StatusReadout = MakeText(WidgetTree, FText::FromString(TEXT("SERVER AUTHORIZED  ·  COMMANDS RUN ON AUTHORITY")), 12.0f, Cyan);
	Main->AddChildToVerticalBox(StatusReadout)->SetPadding(FMargin(22.0f, 10.0f, 22.0f, 16.0f));
	SelectPage(0);
}

UVerticalBox* UArenaDuelAdminWidget::CreatePage(const FString& Name)
{
	UVerticalBox* Page = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Page->SetVisibility(ESlateVisibility::Collapsed);
	if (UWidget* Parent = PanelRoot->GetContent())
	{
		if (UVerticalBox* Main = Cast<UVerticalBox>(Parent))
		{
			if (UHorizontalBox* Body = Cast<UHorizontalBox>(Main->GetChildAt(1)))
			{
				if (UVerticalBox* Content = Cast<UVerticalBox>(Body->GetChildAt(1))) Content->AddChildToVerticalBox(Page)->SetPadding(FMargin(12.0f, 4.0f, 22.0f, 6.0f));
			}
		}
	}
	Pages.Add(Page);
	Page->SetToolTipText(FText::FromString(Name));
	return Page;
}

void UArenaDuelAdminWidget::AddSectionLabel(UVerticalBox* Parent, const FText& Label)
{
	if (Parent) Parent->AddChildToVerticalBox(MakeText(WidgetTree, Label, 13.0f, Cyan))->SetPadding(FMargin(0, 10, 0, 4));
}

UArenaDuelAdminActionButton* UArenaDuelAdminWidget::AddButton(UHorizontalBox* Row, const FText& Label, EArenaDuelAdminCommand Command, float Value, const FLinearColor& Color)
{
	if (!Row) return nullptr;
	UArenaDuelAdminActionButton* Button = WidgetTree->ConstructWidget<UArenaDuelAdminActionButton>(UArenaDuelAdminActionButton::StaticClass());
	UTextBlock* ButtonLabel = MakeText(WidgetTree, Label, 13.0f, TextColor);
	FOnArenaDuelAdminAction ActionDelegate;
	ActionDelegate.BindUObject(this, &UArenaDuelAdminWidget::HandleAction);
	Button->Configure(Command, Value, ButtonLabel, Color, MoveTemp(ActionDelegate));
	USizeBox* SizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	SizeBox->SetWidthOverride(162.0f);
	SizeBox->SetHeightOverride(38.0f);
	SizeBox->AddChild(Button);
	UHorizontalBoxSlot* ButtonSlot = Row->AddChildToHorizontalBox(SizeBox);
	ButtonSlot->SetPadding(FMargin(0.0f, 0.0f, 10.0f, 0.0f));
	ButtonSlot->SetVerticalAlignment(VAlign_Center);
	return Button;
}

void UArenaDuelAdminWidget::HandleAction(EArenaDuelAdminCommand Command, float Value)
{
	switch (Command)
	{
	case EArenaDuelAdminCommand::SelectPlayer1: SelectedDuelSlot = 0; SelectPage(SelectedPage); RefreshAdminState(); return;
	case EArenaDuelAdminCommand::SelectPlayer2: SelectedDuelSlot = 1; SelectPage(SelectedPage); RefreshAdminState(); return;
	case EArenaDuelAdminCommand::SelectPlayerPage: SelectPage(0); return;
	case EArenaDuelAdminCommand::SelectWeaponsPage: SelectPage(1); return;
	case EArenaDuelAdminCommand::SelectRoundPage: SelectPage(2); return;
	case EArenaDuelAdminCommand::SelectMovementPage: SelectPage(3); return;
	case EArenaDuelAdminCommand::SelectDebugPage: SelectPage(4); return;
	case EArenaDuelAdminCommand::CloseMenu:
		if (AArenaDuelPlayerController* PC = Cast<AArenaDuelPlayerController>(GetOwningPlayer())) PC->CloseAdminMenu();
		return;
	case EArenaDuelAdminCommand::ResetMatch:
		if (!bConfirmingReset)
		{
			bConfirmingReset = true;
			ResetMatchButton->SetLabel(FText::FromString(TEXT("CONFIRM RESET MATCH")));
			if (GetWorld()) GetWorld()->GetTimerManager().SetTimer(ResetConfirmTimer, [this]() { bConfirmingReset = false; ResetMatchButton->SetLabel(FText::FromString(TEXT("RESET MATCH"))); }, 5.0f, false);
			return;
		}
		bConfirmingReset = false;
		if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(ResetConfirmTimer);
		ResetMatchButton->SetLabel(FText::FromString(TEXT("RESET MATCH")));
		break;
	case EArenaDuelAdminCommand::SetHealth:
		if (!HealthInput || !FDefaultValueHelper::ParseFloat(HealthInput->GetText().ToString(), Value)) { StatusReadout->SetText(FText::FromString(TEXT("ENTER A VALID HEALTH VALUE"))); return; }
		break;
	case EArenaDuelAdminCommand::SetPlayer1Wins:
		if (!Player1WinsInput || !FDefaultValueHelper::ParseFloat(Player1WinsInput->GetText().ToString(), Value)) { StatusReadout->SetText(FText::FromString(TEXT("ENTER A VALID P1 SCORE"))); return; }
		break;
	case EArenaDuelAdminCommand::SetPlayer2Wins:
		if (!Player2WinsInput || !FDefaultValueHelper::ParseFloat(Player2WinsInput->GetText().ToString(), Value)) { StatusReadout->SetText(FText::FromString(TEXT("ENTER A VALID P2 SCORE"))); return; }
		break;
	default: break;
	}
	Action.ExecuteIfBound(Command, Value);
	RefreshAdminState();
}

void UArenaDuelAdminWidget::SelectPage(int32 PageIndex)
{
	SelectedPage = FMath::Clamp(PageIndex, 0, Pages.Num() - 1);
	for (int32 Index = 0; Index < Pages.Num(); ++Index) Pages[Index]->SetVisibility(Index == SelectedPage ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	for (int32 Index = 0; Index < NavButtons.Num(); ++Index)
	{
		NavButtons[Index]->SetBackgroundColor(Index == SelectedPage ? FLinearColor(0.04f, 0.22f, 0.30f, 1.0f) : ButtonColor);
	}
	for (int32 Index = 0; Index < TargetButtons.Num(); ++Index)
	{
		if (TargetButtons[Index]) TargetButtons[Index]->SetBackgroundColor(Index == SelectedDuelSlot ? (Index == 0 ? FLinearColor(0.04f, 0.28f, 0.36f, 1.0f) : FLinearColor(0.24f, 0.12f, 0.34f, 1.0f)) : ButtonColor);
	}
}

void UArenaDuelAdminWidget::RefreshAdminState()
{
	AArenaDuelPlayerController* PC = Cast<AArenaDuelPlayerController>(GetOwningPlayer());
	UWorld* World = GetWorld();
	AArenaDuelGameState* GameState = World ? World->GetGameState<AArenaDuelGameState>() : nullptr;
	if (CachedGameState.Get() != GameState)
	{
		CachedGameState = GameState;
		CachedTargetState = nullptr;
	}
	AArenaDuelPlayerState* TargetState = nullptr;
	if (GameState)
	{
		for (APlayerState* State : GameState->PlayerArray)
		{
			AArenaDuelPlayerState* DuelState = Cast<AArenaDuelPlayerState>(State);
			if (DuelState && DuelState->GetDuelSlot() == SelectedDuelSlot) { TargetState = DuelState; break; }
		}
	}
	if (CachedTargetState.Get() != TargetState)
	{
		CachedTargetState = TargetState;
		CachedTargetPawn = nullptr;
	}
	AArenaDuelCharacter* TargetPawn = nullptr;
	if (World && TargetState)
	{
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			APlayerController* Controller = It->Get();
			if (Controller && Controller->PlayerState == TargetState) { TargetPawn = Cast<AArenaDuelCharacter>(Controller->GetPawn()); break; }
		}
	}
	CachedTargetPawn = TargetPawn;
	if (TargetReadout)
	{
		const FString TargetName = SelectedDuelSlot == 0 ? TEXT("PLAYER 1") : TEXT("PLAYER 2");
		if (TargetState && TargetPawn)
		{
			const UArenaDuelWeaponComponent* Weapon = TargetPawn->GetWeaponComponent();
			TargetReadout->SetText(FText::FromString(FString::Printf(TEXT("TARGET: %s   |   HP %d / %d   |   %s   |   WINS %d   |   %s   |   GOD %s   |   AMMO %s   |   STAMINA %s"), *TargetName, FMath::RoundToInt(TargetPawn->GetHealth()), FMath::RoundToInt(TargetPawn->GetMaxHealth()), TargetPawn->IsDead() ? TEXT("DEAD") : TEXT("ALIVE"), TargetState->GetRoundWins(), Weapon ? *Weapon->GetCurrentWeaponName().ToString() : TEXT("NO WEAPON"), TargetState->HasAdminGodMode() ? TEXT("ON") : TEXT("OFF"), TargetState->HasAdminInfiniteAmmo() ? TEXT("ON") : TEXT("OFF"), TargetState->HasAdminInfiniteStamina() ? TEXT("ON") : TEXT("OFF"))));
		}
		else TargetReadout->SetText(FText::FromString(FString::Printf(TEXT("TARGET: %s   |   PLAYER NOT CONNECTED"), *TargetName)));
	}
	if (MovementReadout)
	{
		if (TargetPawn)
		{
			const UArenaDuelCharacterMovementComponent* Movement = TargetPawn->GetArenaDuelMovementComponent();
			const FVector Position = TargetPawn->GetActorLocation();
			MovementReadout->SetText(FText::FromString(Movement ? FString::Printf(TEXT("STATE  %s\nHORIZONTAL SPEED  %.0f uu/s\nSTAMINA  %.0f / %.0f\nMOVEMENT MODE  %s\nPOSITION  X %.0f   Y %.0f   Z %.0f"), *Movement->GetDevelopmentMovementState(), Movement->Velocity.Size2D(), Movement->GetStamina(), Movement->GetMaxStamina(), *MovementModeName(Movement->MovementMode), Position.X, Position.Y, Position.Z) : TEXT("No movement component")));
		}
		else MovementReadout->SetText(FText::FromString(TEXT("Waiting for selected pawn...")));
	}
	if (NetworkReadout && PC && World)
	{
		const FString Role = PC->HasAuthority() ? TEXT("AUTHORITY") : TEXT("AUTONOMOUS CLIENT");
		const AArenaDuelPlayerState* LocalState = PC->GetPlayerState<AArenaDuelPlayerState>();
		const int32 Ping = LocalState ? FMath::RoundToInt(LocalState->GetPingInMilliseconds()) : 0;
		NetworkReadout->SetText(FText::FromString(FString::Printf(TEXT("NET MODE  %s\nLOCAL ROLE  %s\nPING  %d ms\nDUEL SLOT  PLAYER %d\nROUND  %d  ·  %s  ·  LAST WINNER %d"), *NetModeName(World->GetNetMode()), *Role, Ping, LocalState ? LocalState->GetDuelSlot() + 1 : 0, GameState ? GameState->GetRoundNumber() : 0, GameState && GameState->IsRoundInProgress() ? TEXT("ACTIVE") : TEXT("BREAK"), GameState && GameState->GetLastRoundWinnerSlot() != INDEX_NONE ? GameState->GetLastRoundWinnerSlot() + 1 : 0)));
	}
}
