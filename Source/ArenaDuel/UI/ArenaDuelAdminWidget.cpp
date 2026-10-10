// Copyright Epic Games, Inc. All Rights Reserved.

#include "ArenaDuelAdminWidget.h"

#include "../Characters/ArenaDuelCharacter.h"
#include "../Characters/ArenaDuelCharacterMovementComponent.h"
#include "../Game/ArenaDuelGameState.h"
#include "../Game/ArenaDuelMovementDebugHUD.h"
#include "../Game/ArenaDuelZombieGameMode.h"
#include "../Game/ArenaDuelZombieGameState.h"
#include "../Player/ArenaDuelPlayerController.h"
#include "../Player/ArenaDuelPlayerState.h"
#include "../Weapons/ArenaDuelWeaponComponent.h"
#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateColorBrush.h"
#include "Components/Border.h"
#include "Components/EditableTextBox.h"
#include "Components/GridPanel.h"
#include "Components/GridSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Misc/DefaultValueHelper.h"
#include "TimerManager.h"

namespace
{
	const FLinearColor RootColor(0.027f, 0.063f, 0.106f, 0.985f);
	const FLinearColor CardColor(0.051f, 0.090f, 0.137f, 0.98f);
	const FLinearColor ElevatedColor(0.071f, 0.125f, 0.192f, 1.0f);
	const FLinearColor Cyan(0.475f, 0.914f, 1.0f, 1.0f);
	const FLinearColor Violet(0.725f, 0.439f, 1.0f, 1.0f);
	const FLinearColor PrimaryText(0.953f, 0.969f, 0.988f, 1.0f);
	const FLinearColor SecondaryText(0.608f, 0.667f, 0.737f, 1.0f);
	const FLinearColor MutedText(0.400f, 0.467f, 0.537f, 1.0f);
	const FLinearColor Danger(0.45f, 0.14f, 0.18f, 1.0f);
	const FLinearColor Active(0.06f, 0.25f, 0.31f, 1.0f);
	const FLinearColor Inactive(0.078f, 0.125f, 0.176f, 1.0f);

	UTextBlock* MakeText(UWidgetTree* Tree, const FText& Text, float Size, const FLinearColor& Color, ETextJustify::Type Justification = ETextJustify::Left)
	{
		UTextBlock* Result = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		Result->SetText(Text);
		Result->SetColorAndOpacity(Color);
		Result->SetJustification(Justification);
		Result->SetAutoWrapText(true);
		Result->SetFont(FSlateFontInfo(GEngine ? static_cast<const UObject*>(GEngine->GetSmallFont()) : nullptr, Size));
		return Result;
	}

	void SetTextIfChanged(UTextBlock* Widget, const FString& Value)
	{
		if (Widget && Widget->GetText().ToString() != Value) Widget->SetText(FText::FromString(Value));
	}

	void SetTextIfChanged(UEditableTextBox* Widget, const FText& Value)
	{
		if (Widget && !Widget->GetText().EqualTo(Value)) Widget->SetText(Value);
	}

	void SetStatusColor(UTextBlock* Widget, const FLinearColor& Color)
	{
		if (Widget) Widget->SetColorAndOpacity(Color);
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

UArenaDuelAdminWidget::UArenaDuelAdminWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsFocusable(true);
}

bool UArenaDuelAdminWidget::Initialize()
{
	if (!Super::Initialize()) return false;
	BuildWidgetTree();
	return RootOverlay != nullptr && WidgetTree && WidgetTree->RootWidget == RootOverlay;
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

FReply UArenaDuelAdminWidget::NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	if (Key == EKeys::Escape || Key == EKeys::F1)
	{
		if (AArenaDuelPlayerController* PC = Cast<AArenaDuelPlayerController>(GetOwningPlayer())) PC->CloseAdminMenu();
		return FReply::Handled();
	}
	return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
}

FReply UArenaDuelAdminWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	if (Key == EKeys::Escape || Key == EKeys::F1)
	{
		if (AArenaDuelPlayerController* PC = Cast<AArenaDuelPlayerController>(GetOwningPlayer())) PC->CloseAdminMenu();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UArenaDuelAdminWidget::BuildWidgetTree()
{
	if (!WidgetTree || RootOverlay) return;
	RootOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	WidgetTree->RootWidget = RootOverlay;
	const auto Fill = [](UOverlaySlot* OverlaySlot)
	{
		OverlaySlot->SetHorizontalAlignment(HAlign_Fill);
		OverlaySlot->SetVerticalAlignment(VAlign_Fill);
	};

	// The game behind is dimmed, the tool itself is one opaque window in the middle of the screen.
	Backdrop = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Backdrop->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.66f));
	Fill(RootOverlay->AddChildToOverlay(Backdrop));

	USizeBox* WindowSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	WindowSize->SetWidthOverride(1320.0f);
	WindowSize->SetHeightOverride(800.0f);
	UOverlaySlot* WindowSlot = RootOverlay->AddChildToOverlay(WindowSize);
	WindowSlot->SetHorizontalAlignment(HAlign_Center);
	WindowSlot->SetVerticalAlignment(VAlign_Center);
	UBorder* WindowEdge = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	WindowEdge->SetBrushColor(FLinearColor(0.475f, 0.914f, 1.0f, 0.35f));
	WindowEdge->SetPadding(FMargin(1.0f, 3.0f, 1.0f, 1.0f));
	WindowSize->AddChild(WindowEdge);
	UBorder* Window = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Window->SetBrushColor(RootColor);
	Window->SetPadding(FMargin(26.0f, 18.0f, 26.0f, 14.0f));
	WindowEdge->SetContent(Window);

	UVerticalBox* Shell = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Window->SetContent(Shell);

	// Header: what this is, which mode is running, how to close it.
	UHorizontalBox* Header = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Shell->AddChildToVerticalBox(Header)->SetPadding(FMargin(0, 0, 0, 12));
	UHorizontalBoxSlot* TitleSlot = Header->AddChildToHorizontalBox(MakeText(WidgetTree, FText::FromString(TEXT("ADMIN")), 26.0f, Cyan));
	TitleSlot->SetVerticalAlignment(VAlign_Center);
	TitleSlot->SetPadding(FMargin(0, 0, 16, 0));
	UBorder* ModeChip = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	ModeChip->SetBrushColor(ElevatedColor);
	ModeChip->SetPadding(FMargin(12, 5));
	ModeReadout = MakeText(WidgetTree, FText::FromString(TEXT("--")), 12.0f, PrimaryText);
	ModeReadout->SetAutoWrapText(false);
	ModeChip->SetContent(ModeReadout);
	Header->AddChildToHorizontalBox(ModeChip)->SetVerticalAlignment(VAlign_Center);
	Header->AddChildToHorizontalBox(WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass()))->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	UTextBlock* CloseHint = MakeText(WidgetTree, FText::FromString(TEXT("F1 / ESC   CLOSE")), 12.0f, SecondaryText, ETextJustify::Right);
	CloseHint->SetAutoWrapText(false);
	Header->AddChildToHorizontalBox(CloseHint)->SetVerticalAlignment(VAlign_Center);

	UHorizontalBox* Body = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Shell->AddChildToVerticalBox(Body)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

	// Left: the player every command applies to, with their state as plain rows.
	USizeBox* SidebarSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	SidebarSize->SetWidthOverride(300.0f);
	Body->AddChildToHorizontalBox(SidebarSize)->SetPadding(FMargin(0, 0, 20, 0));
	UBorder* SidebarBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	SidebarBorder->SetBrushColor(CardColor);
	SidebarBorder->SetPadding(FMargin(16, 14));
	SidebarSize->AddChild(SidebarBorder);
	UVerticalBox* Sidebar = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	SidebarBorder->SetContent(Sidebar);
	Sidebar->AddChildToVerticalBox(MakeText(WidgetTree, FText::FromString(TEXT("TARGET PLAYER")), 11.0f, MutedText))->SetPadding(FMargin(0, 0, 0, 8));
	UHorizontalBox* TargetRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Sidebar->AddChildToVerticalBox(TargetRow)->SetPadding(FMargin(0, 0, 0, 14));
	TargetButtons.Add(AddButton(TargetRow, FText::FromString(TEXT("PLAYER 1")), EArenaDuelAdminCommand::SelectPlayer1, 0.0f, Active, 128.0f));
	TargetButtons.Add(AddButton(TargetRow, FText::FromString(TEXT("PLAYER 2")), EArenaDuelAdminCommand::SelectPlayer2, 0.0f, Inactive, 128.0f));
	const auto StatusRow = [this, Sidebar](const TCHAR* Label, const FLinearColor& Accent, UTextBlock** OutLabel = nullptr)
	{
		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		Sidebar->AddChildToVerticalBox(Row)->SetPadding(FMargin(0, 5));
		UTextBlock* Name = MakeText(WidgetTree, FText::FromString(Label), 12.0f, SecondaryText);
		Name->SetAutoWrapText(false);
		Row->AddChildToHorizontalBox(Name)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		UTextBlock* Value = MakeText(WidgetTree, FText::FromString(TEXT("--")), 14.0f, Accent, ETextJustify::Right);
		Value->SetAutoWrapText(false);
		Row->AddChildToHorizontalBox(Value);
		if (OutLabel) *OutLabel = Name;
		return Value;
	};
	// The order is the one RefreshAdminState fills.
	TargetStatusValues.Add(StatusRow(TEXT("TARGET"), PrimaryText));
	TargetStatusValues.Add(StatusRow(TEXT("HEALTH"), Cyan));
	TargetStatusValues.Add(StatusRow(TEXT("STATE"), Cyan));
	UTextBlock* ScoreLabel = nullptr;
	TargetStatusValues.Add(StatusRow(TEXT("ROUND WINS"), Violet, &ScoreLabel));
	ScoreRowLabel = ScoreLabel;
	TargetStatusValues.Add(StatusRow(TEXT("WEAPON"), PrimaryText));
	USizeBox* SidebarRule = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	SidebarRule->SetHeightOverride(1.0f);
	UBorder* SidebarRuleLine = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	SidebarRuleLine->SetBrushColor(FLinearColor(0.475f, 0.914f, 1.0f, 0.14f));
	SidebarRule->AddChild(SidebarRuleLine);
	Sidebar->AddChildToVerticalBox(SidebarRule)->SetPadding(FMargin(0, 10));
	Sidebar->AddChildToVerticalBox(MakeText(WidgetTree, FText::FromString(TEXT("OVERRIDES")), 11.0f, MutedText))->SetPadding(FMargin(0, 0, 0, 4));
	TargetStatusValues.Add(StatusRow(TEXT("GOD MODE"), Cyan));
	TargetStatusValues.Add(StatusRow(TEXT("INFINITE AMMO"), Cyan));
	TargetStatusValues.Add(StatusRow(TEXT("INFINITE STAMINA"), Violet));
	Sidebar->AddChildToVerticalBox(WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass()))->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	Sidebar->AddChildToVerticalBox(MakeText(WidgetTree, FText::FromString(TEXT("Development tool. It exists in the editor and in a game started with -ArenaDuelAdmin; players never see it.")), 10.0f, MutedText));

	// Right: tabs across the top, the selected page below.
	ContentHost = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Body->AddChildToHorizontalBox(ContentHost)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	UHorizontalBox* Tabs = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	ContentHost->AddChildToVerticalBox(Tabs)->SetPadding(FMargin(0, 0, 0, 14));
	const FText NavLabels[] = { FText::FromString(TEXT("PLAYER")), FText::FromString(TEXT("WEAPONS")), FText::FromString(TEXT("ROUND")), FText::FromString(TEXT("MOVEMENT")), FText::FromString(TEXT("DEBUG")), FText::FromString(TEXT("SURVIVAL")) };
	const EArenaDuelAdminCommand NavCommands[] = { EArenaDuelAdminCommand::SelectPlayerPage, EArenaDuelAdminCommand::SelectWeaponsPage, EArenaDuelAdminCommand::SelectRoundPage, EArenaDuelAdminCommand::SelectMovementPage, EArenaDuelAdminCommand::SelectDebugPage, EArenaDuelAdminCommand::SelectSurvivalPage };
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(NavLabels); ++Index)
	{
		UVerticalBox* Tab = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Tabs->AddChildToHorizontalBox(Tab);
		UHorizontalBox* TabRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		Tab->AddChildToVerticalBox(TabRow);
		NavButtons.Add(AddButton(TabRow, NavLabels[Index], NavCommands[Index], 0.0f, Index == 0 ? Active : Inactive, 150.0f));
		USizeBox* AccentSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		AccentSize->SetWidthOverride(150.0f);
		AccentSize->SetHeightOverride(3.0f);
		UBorder* Accent = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Accent->SetBrushColor(Index == 0 ? Cyan : FLinearColor::Transparent);
		AccentSize->AddChild(Accent);
		Tab->AddChildToVerticalBox(AccentSize)->SetHorizontalAlignment(HAlign_Left);
		NavAccents.Add(Accent);
		NavTabs.Add(Tab);
	}

	PageTitle = MakeText(WidgetTree, FText::FromString(TEXT("PLAYER")), 20.0f, PrimaryText);
	ContentHost->AddChildToVerticalBox(PageTitle);
	PageDescription = MakeText(WidgetTree, FText::FromString(TEXT("--")), 12.0f, SecondaryText);
	ContentHost->AddChildToVerticalBox(PageDescription)->SetPadding(FMargin(0, 2, 0, 12));

	PageScrollBox = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
	PageScrollBox->SetScrollBarVisibility(ESlateVisibility::Visible);
	ContentHost->AddChildToVerticalBox(PageScrollBox)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

	UVerticalBox* PlayerPage = CreatePage(TEXT("PLAYER"), TEXT("Health, state and archetype of the selected player."));
	UVerticalBox* HealthCard = AddCard(PlayerPage, FText::FromString(TEXT("HEALTH")), FText::FromString(TEXT("Type a value and set it, or restore full health.")));
	UHorizontalBox* HealthRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	HealthCard->AddChildToVerticalBox(HealthRow)->SetPadding(FMargin(0, 8, 0, 0));
	HealthInput = AddNumericInput(HealthRow, FText::FromString(TEXT("100")), FText::FromString(TEXT("HEALTH")), 120.0f);
	SetHealthButton = AddButton(HealthRow, FText::FromString(TEXT("SET HEALTH")), EArenaDuelAdminCommand::SetHealth, 0.0f, Active, 200.0f);
	AddButton(HealthRow, FText::FromString(TEXT("FULL HEAL")), EArenaDuelAdminCommand::FullHeal, 0.0f, ElevatedColor, 200.0f);

	UVerticalBox* PlayerStateCard = AddCard(PlayerPage, FText::FromString(TEXT("STATE")), FText::FromString(TEXT("Reset works on a living player. A dead one comes back with the next round or run.")));
	UHorizontalBox* PlayerActions = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	PlayerStateCard->AddChildToVerticalBox(PlayerActions)->SetPadding(FMargin(0, 8, 0, 0));
	GodModeButton = AddButton(PlayerActions, FText::FromString(TEXT("GOD MODE: OFF")), EArenaDuelAdminCommand::ToggleGodMode, 0.0f, ElevatedColor, 200.0f);
	AddButton(PlayerActions, FText::FromString(TEXT("RESET PLAYER")), EArenaDuelAdminCommand::ResetPlayer, 0.0f, ElevatedColor, 200.0f);
	AddButton(PlayerActions, FText::FromString(TEXT("KILL PLAYER")), EArenaDuelAdminCommand::Kill, 0.0f, Danger, 200.0f);

	UVerticalBox* ArchetypeCard = AddCard(PlayerPage, FText::FromString(TEXT("ARCHETYPE")), FText::FromString(TEXT("The current one is highlighted.")));
	UHorizontalBox* ArchetypeRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	ArchetypeCard->AddChildToVerticalBox(ArchetypeRow)->SetPadding(FMargin(0, 8, 0, 0));
	ArchetypeButtons.Add(AddButton(ArchetypeRow, FText::FromString(TEXT("SHADOW")), EArenaDuelAdminCommand::SetArchetypeShadow, 0.0f, Active, 200.0f));
	ArchetypeButtons.Add(AddButton(ArchetypeRow, FText::FromString(TEXT("WARDEN")), EArenaDuelAdminCommand::SetArchetypeWarden, 0.0f, ElevatedColor, 200.0f));
	ArchetypeButtons.Add(AddButton(ArchetypeRow, FText::FromString(TEXT("RIFT")), EArenaDuelAdminCommand::SetArchetypeRift, 0.0f, ElevatedColor, 200.0f));

	UVerticalBox* WeaponsPage = CreatePage(TEXT("WEAPONS"), TEXT("Weapon in hand and ammunition of the selected player."));
	UVerticalBox* EquipCard = AddCard(WeaponsPage, FText::FromString(TEXT("EQUIP")), FText::FromString(TEXT("Gives all four firearms and puts the chosen one in hand.")));
	UHorizontalBox* WeaponRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	EquipCard->AddChildToVerticalBox(WeaponRow)->SetPadding(FMargin(0, 8, 0, 0));
	const FText WeaponLabels[] = { FText::FromString(TEXT("ARC RIFLE")), FText::FromString(TEXT("SHADE SMG")), FText::FromString(TEXT("RUNE DMR")), FText::FromString(TEXT("HEX SHOTGUN")) };
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(WeaponLabels); ++Index)
	{
		WeaponButtons.Add(AddButton(WeaponRow, WeaponLabels[Index], EArenaDuelAdminCommand::EquipWeapon, static_cast<float>(Index), ElevatedColor, 200.0f));
	}
	UVerticalBox* AmmoCard = AddCard(WeaponsPage, FText::FromString(TEXT("AMMUNITION")), FText::FromString(TEXT("Weapon values themselves are not changed.")));
	UHorizontalBox* AmmoRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	AmmoCard->AddChildToVerticalBox(AmmoRow)->SetPadding(FMargin(0, 8, 0, 0));
	AddButton(AmmoRow, FText::FromString(TEXT("REFILL ALL AMMO")), EArenaDuelAdminCommand::RefillAmmo, 0.0f, ElevatedColor, 200.0f);
	InfiniteAmmoButton = AddButton(AmmoRow, FText::FromString(TEXT("INFINITE AMMO: OFF")), EArenaDuelAdminCommand::ToggleInfiniteAmmo, 0.0f, ElevatedColor, 240.0f);

	UVerticalBox* RoundPage = CreatePage(TEXT("ROUND"), TEXT("Round flow and score of a duel."));
	UVerticalBox* RoundFlowCard = AddCard(RoundPage, FText::FromString(TEXT("FLOW")));
	UHorizontalBox* RoundFlowRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	RoundFlowCard->AddChildToVerticalBox(RoundFlowRow)->SetPadding(FMargin(0, 8, 0, 0));
	AddButton(RoundFlowRow, FText::FromString(TEXT("RESTART ROUND")), EArenaDuelAdminCommand::RestartRound, 0.0f, ElevatedColor, 200.0f);
	AddButton(RoundFlowRow, FText::FromString(TEXT("NEXT ROUND")), EArenaDuelAdminCommand::NextRound, 0.0f, Active, 200.0f);
	AddButton(RoundFlowRow, FText::FromString(TEXT("AWARD PLAYER 1")), EArenaDuelAdminCommand::AwardRound, 0.0f, ElevatedColor, 200.0f);
	AddButton(RoundFlowRow, FText::FromString(TEXT("AWARD PLAYER 2")), EArenaDuelAdminCommand::AwardRound, 1.0f, ElevatedColor, 200.0f);

	UVerticalBox* ScoreCard = AddCard(RoundPage, FText::FromString(TEXT("SCORE")), FText::FromString(TEXT("Setting a score does not award or end a round. 0 to 5.")));
	UHorizontalBox* ScoreRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	ScoreCard->AddChildToVerticalBox(ScoreRow)->SetPadding(FMargin(0, 8, 0, 0));
	ScoreRow->AddChildToHorizontalBox(MakeText(WidgetTree, FText::FromString(TEXT("PLAYER 1")), 13, SecondaryText))->SetVerticalAlignment(VAlign_Center);
	Player1WinsInput = AddNumericInput(ScoreRow, FText::FromString(TEXT("0")), FText::FromString(TEXT("0-5")), 70.0f);
	SetPlayer1WinsButton = AddButton(ScoreRow, FText::FromString(TEXT("SET")), EArenaDuelAdminCommand::SetPlayer1Wins, 0.0f, ElevatedColor, 90.0f);
	ScoreRow->AddChildToHorizontalBox(MakeText(WidgetTree, FText::FromString(TEXT("PLAYER 2")), 13, SecondaryText))->SetVerticalAlignment(VAlign_Center);
	Player2WinsInput = AddNumericInput(ScoreRow, FText::FromString(TEXT("0")), FText::FromString(TEXT("0-5")), 70.0f);
	SetPlayer2WinsButton = AddButton(ScoreRow, FText::FromString(TEXT("SET")), EArenaDuelAdminCommand::SetPlayer2Wins, 0.0f, ElevatedColor, 90.0f);

	UVerticalBox* DangerCard = AddCard(RoundPage, FText::FromString(TEXT("RESET")), FText::FromString(TEXT("Round number and both scores back to the start. Asks once more.")));
	UHorizontalBox* ResetRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	DangerCard->AddChildToVerticalBox(ResetRow)->SetPadding(FMargin(0, 8, 0, 0));
	ResetMatchButton = AddButton(ResetRow, FText::FromString(TEXT("RESET MATCH")), EArenaDuelAdminCommand::ResetMatch, 0.0f, Danger, 200.0f);

	UVerticalBox* MovementPage = CreatePage(TEXT("MOVEMENT"), TEXT("Movement and stamina of the selected player, live."));
	UVerticalBox* MovementCard = AddCard(MovementPage, FText::FromString(TEXT("NOW")));
	UHorizontalBox* MovementRow1 = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	MovementCard->AddChildToVerticalBox(MovementRow1)->SetPadding(FMargin(0, 8, 0, 6));
	MovementStatusValues.Add(AddStatusCard(MovementRow1, FText::FromString(TEXT("STATE")), Cyan));
	MovementStatusValues.Add(AddStatusCard(MovementRow1, FText::FromString(TEXT("SPEED")), Cyan));
	MovementStatusValues.Add(AddStatusCard(MovementRow1, FText::FromString(TEXT("STAMINA")), Violet));
	MovementStatusValues.Add(AddStatusCard(MovementRow1, FText::FromString(TEXT("MODE")), Cyan));
	UHorizontalBox* MovementRow2 = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	MovementCard->AddChildToVerticalBox(MovementRow2)->SetPadding(FMargin(0, 4, 0, 0));
	MovementStatusValues.Add(AddStatusCard(MovementRow2, FText::FromString(TEXT("POSITION  X / Y / Z")), SecondaryText));
	UVerticalBox* StaminaCard = AddCard(MovementPage, FText::FromString(TEXT("STAMINA")));
	UHorizontalBox* StaminaRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	StaminaCard->AddChildToVerticalBox(StaminaRow)->SetPadding(FMargin(0, 8, 0, 0));
	AddButton(StaminaRow, FText::FromString(TEXT("REFILL STAMINA")), EArenaDuelAdminCommand::RefillStamina, 0.0f, ElevatedColor, 200.0f);
	InfiniteStaminaButton = AddButton(StaminaRow, FText::FromString(TEXT("INFINITE STAMINA: OFF")), EArenaDuelAdminCommand::ToggleInfiniteStamina, 0.0f, ElevatedColor, 260.0f);

	UVerticalBox* DebugPage = CreatePage(TEXT("DEBUG"), TEXT("Local drawing and the state of the connection."));
	UVerticalBox* LocalDebugCard = AddCard(DebugPage, FText::FromString(TEXT("DRAW ON THIS SCREEN")), FText::FromString(TEXT("Only this machine sees these. Collision is not changed.")));
	UHorizontalBox* DebugActions = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	LocalDebugCard->AddChildToVerticalBox(DebugActions)->SetPadding(FMargin(0, 8, 0, 0));
	DebugOverlayButton = AddButton(DebugActions, FText::FromString(TEXT("DEBUG OVERLAY: OFF")), EArenaDuelAdminCommand::ToggleDebugOverlay, 0.0f, ElevatedColor, 240.0f);
	HitZonesButton = AddButton(DebugActions, FText::FromString(TEXT("HIT ZONES: OFF")), EArenaDuelAdminCommand::ToggleHitZones, 0.0f, ElevatedColor, 240.0f);

	UVerticalBox* NetworkCard = AddCard(DebugPage, FText::FromString(TEXT("NETWORK AND ROUND")));
	UHorizontalBox* NetRow1 = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	NetworkCard->AddChildToVerticalBox(NetRow1)->SetPadding(FMargin(0, 8, 0, 6));
	NetworkStatusValues.Add(AddStatusCard(NetRow1, FText::FromString(TEXT("NET MODE")), Cyan));
	NetworkStatusValues.Add(AddStatusCard(NetRow1, FText::FromString(TEXT("LOCAL ROLE")), Cyan));
	NetworkStatusValues.Add(AddStatusCard(NetRow1, FText::FromString(TEXT("PING")), Violet));
	NetworkStatusValues.Add(AddStatusCard(NetRow1, FText::FromString(TEXT("DUEL SLOT")), Cyan));
	UHorizontalBox* NetRow2 = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	NetworkCard->AddChildToVerticalBox(NetRow2)->SetPadding(FMargin(0, 4, 0, 0));
	NetworkStatusValues.Add(AddStatusCard(NetRow2, FText::FromString(TEXT("ROUND")), Violet));
	NetworkStatusValues.Add(AddStatusCard(NetRow2, FText::FromString(TEXT("ROUND STATE")), Cyan));
	NetworkStatusValues.Add(AddStatusCard(NetRow2, FText::FromString(TEXT("LAST WINNER")), SecondaryText));

	UVerticalBox* SurvivalPage = CreatePage(TEXT("SURVIVAL"), TEXT("The running Zombie Survival run."));
	UVerticalBox* RunCard = AddCard(SurvivalPage, FText::FromString(TEXT("NOW")));
	UHorizontalBox* RunRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	RunCard->AddChildToVerticalBox(RunRow)->SetPadding(FMargin(0, 8, 0, 0));
	SurvivalStatusValues.Add(AddStatusCard(RunRow, FText::FromString(TEXT("WAVE")), Cyan));
	SurvivalStatusValues.Add(AddStatusCard(RunRow, FText::FromString(TEXT("ZOMBIES LEFT")), Cyan));
	SurvivalStatusValues.Add(AddStatusCard(RunRow, FText::FromString(TEXT("PHASE")), Violet));
	SurvivalStatusValues.Add(AddStatusCard(RunRow, FText::FromString(TEXT("ZOMBIE DAMAGE")), Cyan));
	UVerticalBox* WaveCard = AddCard(SurvivalPage, FText::FromString(TEXT("WAVE")), FText::FromString(TEXT("Kill All removes the whole wave, also what has not spawned yet. Next Wave Now skips the break as well.")));
	UHorizontalBox* WaveRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	WaveCard->AddChildToVerticalBox(WaveRow)->SetPadding(FMargin(0, 8, 0, 0));
	AddButton(WaveRow, FText::FromString(TEXT("KILL ALL ZOMBIES")), EArenaDuelAdminCommand::SurvivalKillAll, 0.0f, ElevatedColor, 220.0f);
	AddButton(WaveRow, FText::FromString(TEXT("NEXT WAVE NOW")), EArenaDuelAdminCommand::SurvivalFinishWave, 0.0f, Active, 220.0f);
	AddButton(WaveRow, FText::FromString(TEXT("RESTART RUN")), EArenaDuelAdminCommand::SurvivalRestart, 0.0f, Danger, 220.0f);
	UVerticalBox* SurvivalPlayerCard = AddCard(SurvivalPage, FText::FromString(TEXT("POINTS AND ENEMIES")), FText::FromString(TEXT("Points go to the selected player. Zombie damage applies to zombies that spawn from then on.")));
	UHorizontalBox* SurvivalPlayerRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	SurvivalPlayerCard->AddChildToVerticalBox(SurvivalPlayerRow)->SetPadding(FMargin(0, 8, 0, 0));
	AddButton(SurvivalPlayerRow, FText::FromString(TEXT("+5000 POINTS")), EArenaDuelAdminCommand::SurvivalAddPoints, 0.0f, ElevatedColor, 220.0f);
	ZombieDamageButton = AddButton(SurvivalPlayerRow, FText::FromString(TEXT("ZOMBIE DAMAGE: ON")), EArenaDuelAdminCommand::SurvivalToggleHarmless, 0.0f, ElevatedColor, 260.0f);

	// Footer: what the last command was and who it went to.
	UHorizontalBox* Footer = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Shell->AddChildToVerticalBox(Footer)->SetPadding(FMargin(0, 10, 0, 0));
	StatusReadout = MakeText(WidgetTree, FText::FromString(TEXT("READY")), 12.0f, Cyan);
	StatusReadout->SetAutoWrapText(false);
	Footer->AddChildToHorizontalBox(StatusReadout)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	UTextBlock* Authority = MakeText(WidgetTree, FText::FromString(TEXT("COMMANDS RUN ON THE HOST")), 10.0f, MutedText, ETextJustify::Right);
	Authority->SetAutoWrapText(false);
	Footer->AddChildToHorizontalBox(Authority);

	SelectPage(0);
}

UArenaDuelAdminActionButton* UArenaDuelAdminWidget::AddButton(UHorizontalBox* Row, const FText& Label, EArenaDuelAdminCommand Command, float Value, const FLinearColor& Color, float Width, ETextJustify::Type Justification)
{
	if (!Row) return nullptr;
	UArenaDuelAdminActionButton* Button = WidgetTree->ConstructWidget<UArenaDuelAdminActionButton>(UArenaDuelAdminActionButton::StaticClass());
	UTextBlock* ButtonLabel = MakeText(WidgetTree, Label, 14.0f, PrimaryText, ETextJustify::Center);
	FOnArenaDuelAdminAction ActionDelegate;
	ActionDelegate.BindUObject(this, &UArenaDuelAdminWidget::HandleAction);
	Button->Configure(Command, Value, ButtonLabel, Color, MoveTemp(ActionDelegate), Justification);
	USizeBox* SizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	SizeBox->SetWidthOverride(Width);
	SizeBox->SetHeightOverride(48.0f);
	SizeBox->AddChild(Button);
	UHorizontalBoxSlot* ButtonSlot = Row->AddChildToHorizontalBox(SizeBox);
	ButtonSlot->SetPadding(FMargin(0, 0, 12, 0));
	ButtonSlot->SetVerticalAlignment(VAlign_Center);
	return Button;
}

UArenaDuelAdminActionButton* UArenaDuelAdminWidget::AddGridButton(UGridPanel* Grid, int32 Column, int32 Row, const FText& Label, EArenaDuelAdminCommand Command, float Value, const FLinearColor& Color, float Width)
{
	if (!Grid) return nullptr;
	UHorizontalBox* Wrapper = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	UArenaDuelAdminActionButton* Button = AddButton(Wrapper, Label, Command, Value, Color, Width);
	Grid->AddChildToGrid(Wrapper, Row, Column)->SetPadding(FMargin(0, 0, 14, 12));
	return Button;
}

void UArenaDuelAdminWidget::AddSectionLabel(UVerticalBox* Parent, const FText& Label)
{
	if (Parent) Parent->AddChildToVerticalBox(MakeText(WidgetTree, Label, 16.0f, Cyan))->SetPadding(FMargin(0, 12, 0, 4));
}

void UArenaDuelAdminWidget::AddHelperText(UVerticalBox* Parent, const FText& Text)
{
	if (Parent) Parent->AddChildToVerticalBox(MakeText(WidgetTree, Text, 12.0f, SecondaryText))->SetPadding(FMargin(0, 9, 0, 0));
}

UVerticalBox* UArenaDuelAdminWidget::AddCard(UVerticalBox* Parent, const FText& Title, const FText& Description)
{
	UBorder* CardBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	CardBorder->SetBrushColor(CardColor);
	CardBorder->SetPadding(FMargin(20, 17));
	if (Parent) Parent->AddChildToVerticalBox(CardBorder)->SetPadding(FMargin(0, 0, 0, 16));
	UVerticalBox* CardContent = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	CardBorder->SetContent(CardContent);
	UTextBlock* Heading = MakeText(WidgetTree, Title, 16.0f, PrimaryText);
	CardContent->AddChildToVerticalBox(Heading);
	if (!Description.IsEmpty()) CardContent->AddChildToVerticalBox(MakeText(WidgetTree, Description, 12.0f, SecondaryText))->SetPadding(FMargin(0, 3, 0, 0));
	return CardContent;
}

UEditableTextBox* UArenaDuelAdminWidget::AddNumericInput(UHorizontalBox* Parent, const FText& InitialValue, const FText& Hint, float Width)
{
	UBorder* FieldBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	FieldBorder->SetBrushColor(ElevatedColor);
	FieldBorder->SetPadding(FMargin(10, 2));
	UEditableTextBox* Input = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass());
	Input->SetText(InitialValue);
	Input->SetHintText(Hint);
	Input->SetForegroundColor(PrimaryText);
	Input->SetJustification(ETextJustify::Center);
	Input->SetMinDesiredWidth(Width);
	FEditableTextBoxStyle InputStyle = Input->GetWidgetStyle();
	InputStyle.SetBackgroundImageNormal(FSlateColorBrush(ElevatedColor));
	InputStyle.SetBackgroundImageHovered(FSlateColorBrush(FLinearColor(0.09f, 0.16f, 0.23f, 1.0f)));
	InputStyle.SetBackgroundImageFocused(FSlateColorBrush(FLinearColor(0.09f, 0.18f, 0.25f, 1.0f)));
	InputStyle.SetPadding(FMargin(8, 4));
	Input->SetWidgetStyle(InputStyle);
	FieldBorder->SetContent(Input);
	USizeBox* SizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	SizeBox->SetWidthOverride(Width + 24.0f);
	SizeBox->SetHeightOverride(48.0f);
	SizeBox->AddChild(FieldBorder);
	if (Parent)
	{
		UHorizontalBoxSlot* InputSlot = Parent->AddChildToHorizontalBox(SizeBox);
		InputSlot->SetPadding(FMargin(0, 0, 12, 0));
		InputSlot->SetVerticalAlignment(VAlign_Center);
	}
	return Input;
}

UTextBlock* UArenaDuelAdminWidget::AddStatusCard(UHorizontalBox* Parent, const FText& Label, const FLinearColor& Accent)
{
	UBorder* Card = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Card->SetBrushColor(FLinearColor(0.071f, 0.118f, 0.169f, 0.94f));
	Card->SetPadding(FMargin(13, 10));
	UVerticalBox* Stack = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Card->SetContent(Stack);
	Stack->AddChildToVerticalBox(MakeText(WidgetTree, Label, 10.0f, MutedText))->SetPadding(FMargin(0, 0, 0, 4));
	UTextBlock* Value = MakeText(WidgetTree, FText::FromString(TEXT("--")), 16.0f, Accent);
	Stack->AddChildToVerticalBox(Value);
	if (Parent)
	{
		UHorizontalBoxSlot* CardSlot = Parent->AddChildToHorizontalBox(Card);
		CardSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		CardSlot->SetPadding(FMargin(0, 0, 10, 0));
	}
	return Value;
}

UVerticalBox* UArenaDuelAdminWidget::CreatePage(const FString& Name, const FString& Description)
{
	UVerticalBox* Page = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Page->SetVisibility(ESlateVisibility::Collapsed);
	PageScrollBox->AddChild(Page);
	Pages.Add(Page);
	PageNames.Add(Name);
	PageDescriptions.Add(Description);
	return Page;
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
	case EArenaDuelAdminCommand::SelectSurvivalPage: SelectPage(5); return;
	case EArenaDuelAdminCommand::CloseMenu:
		if (AArenaDuelPlayerController* PC = Cast<AArenaDuelPlayerController>(GetOwningPlayer())) PC->CloseAdminMenu();
		return;
	case EArenaDuelAdminCommand::ResetMatch:
		if (!bConfirmingReset)
		{
			bConfirmingReset = true;
			ResetMatchButton->SetLabel(FText::FromString(TEXT("CONFIRM RESET")));
			if (GetWorld()) GetWorld()->GetTimerManager().SetTimer(ResetConfirmTimer, [this]() { bConfirmingReset = false; ResetMatchButton->SetLabel(FText::FromString(TEXT("RESET MATCH"))); }, 5.0f, false);
			return;
		}
		bConfirmingReset = false;
		if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(ResetConfirmTimer);
		ResetMatchButton->SetLabel(FText::FromString(TEXT("RESET MATCH")));
		break;
	case EArenaDuelAdminCommand::SetHealth:
		if (!HealthInput || !FDefaultValueHelper::ParseFloat(HealthInput->GetText().ToString(), Value)) { SetActionStatus(TEXT("INVALID HEALTH VALUE"), Danger); return; }
		break;
	case EArenaDuelAdminCommand::SetPlayer1Wins:
		if (!Player1WinsInput || !FDefaultValueHelper::ParseFloat(Player1WinsInput->GetText().ToString(), Value)) { SetActionStatus(TEXT("INVALID PLAYER 1 SCORE"), Danger); return; }
		break;
	case EArenaDuelAdminCommand::SetPlayer2Wins:
		if (!Player2WinsInput || !FDefaultValueHelper::ParseFloat(Player2WinsInput->GetText().ToString(), Value)) { SetActionStatus(TEXT("INVALID PLAYER 2 SCORE"), Danger); return; }
		break;
	default: break;
	}
	Action.ExecuteIfBound(Command, Value);
	// Says what was done and to whom, so a click is never a guess.
	const bool bPerPlayer = Command <= EArenaDuelAdminCommand::ToggleInfiniteAmmo || (Command >= EArenaDuelAdminCommand::RefillStamina && Command <= EArenaDuelAdminCommand::SetArchetypeRift) || Command == EArenaDuelAdminCommand::SurvivalAddPoints;
	FString Name = StaticEnum<EArenaDuelAdminCommand>()->GetNameStringByValue(static_cast<int64>(Command));
	FString Spaced;
	for (int32 Index = 0; Index < Name.Len(); ++Index)
	{
		if (Index > 0 && FChar::IsUpper(Name[Index]) && !FChar::IsUpper(Name[Index - 1])) Spaced.AppendChar(TEXT(' '));
		Spaced.AppendChar(FChar::ToUpper(Name[Index]));
	}
	SetActionStatus(bPerPlayer ? FString::Printf(TEXT("%s  >  PLAYER %d"), *Spaced, SelectedDuelSlot + 1) : Spaced, Cyan);
	RefreshAdminState();
}

void UArenaDuelAdminWidget::SetActionStatus(const FString& Message, const FLinearColor& Color)
{
	if (StatusReadout)
	{
		SetTextIfChanged(StatusReadout, Message);
		SetStatusColor(StatusReadout, Color);
	}
}

void UArenaDuelAdminWidget::SelectPage(int32 PageIndex)
{
	if (Pages.IsEmpty()) return;
	SelectedPage = FMath::Clamp(PageIndex, 0, Pages.Num() - 1);
	for (int32 Index = 0; Index < Pages.Num(); ++Index) Pages[Index]->SetVisibility(Index == SelectedPage ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	for (int32 Index = 0; Index < NavButtons.Num(); ++Index)
	{
		const bool bSelected = Index == SelectedPage;
		NavButtons[Index]->SetVisualColor(bSelected ? Active : Inactive);
		NavAccents[Index]->SetBrushColor(bSelected ? Cyan : FLinearColor::Transparent);
	}
	for (int32 Index = 0; Index < TargetButtons.Num(); ++Index)
	{
		TargetButtons[Index]->SetVisualColor(Index == SelectedDuelSlot ? (Index == 0 ? Active : FLinearColor(0.21f, 0.12f, 0.30f, 1.0f)) : Inactive);
	}
	if (PageTitle && PageNames.IsValidIndex(SelectedPage)) PageTitle->SetText(FText::FromString(PageNames[SelectedPage]));
	if (PageDescription && PageDescriptions.IsValidIndex(SelectedPage)) PageDescription->SetText(FText::FromString(PageDescriptions[SelectedPage]));
	if (PageScrollBox) PageScrollBox->ScrollToStart();
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

	if (TargetStatusValues.Num() >= 8)
	{
		const FString PlayerName = SelectedDuelSlot == 0 ? TEXT("PLAYER 1") : TEXT("PLAYER 2");
		SetTextIfChanged(TargetStatusValues[0], PlayerName);
		if (TargetState && TargetPawn)
		{
			SetTextIfChanged(TargetStatusValues[1], FString::Printf(TEXT("%d / %d"), FMath::RoundToInt(TargetPawn->GetHealth()), FMath::RoundToInt(TargetPawn->GetMaxHealth())));
			SetTextIfChanged(TargetStatusValues[2], TargetPawn->IsDead() ? TEXT("DEAD") : TEXT("ALIVE"));
			SetStatusColor(TargetStatusValues[2], TargetPawn->IsDead() ? FLinearColor(0.91f, 0.36f, 0.42f, 1.0f) : Cyan);
			SetTextIfChanged(TargetStatusValues[3], FString::FromInt(TargetState->GetRoundWins()));
			const UArenaDuelWeaponComponent* Weapon = TargetPawn->GetWeaponComponent();
			SetTextIfChanged(TargetStatusValues[4], Weapon ? Weapon->GetCurrentWeaponName().ToString().ToUpper() : TEXT("NONE"));
			SetTextIfChanged(TargetStatusValues[5], TargetState->HasAdminGodMode() ? TEXT("ON") : TEXT("OFF"));
			SetTextIfChanged(TargetStatusValues[6], TargetState->HasAdminInfiniteAmmo() ? TEXT("ON") : TEXT("OFF"));
			SetTextIfChanged(TargetStatusValues[7], TargetState->HasAdminInfiniteStamina() ? TEXT("ON") : TEXT("OFF"));
			SetStatusColor(TargetStatusValues[5], TargetState->HasAdminGodMode() ? Cyan : MutedText);
			SetStatusColor(TargetStatusValues[6], TargetState->HasAdminInfiniteAmmo() ? Cyan : MutedText);
			SetStatusColor(TargetStatusValues[7], TargetState->HasAdminInfiniteStamina() ? Violet : MutedText);
			if (ArchetypeButtons.IsValidIndex(0)) ArchetypeButtons[0]->SetVisualColor(TargetState->GetCharacterArchetype() == EArenaDuelCharacterArchetype::Shadow ? Active : ElevatedColor);
			if (ArchetypeButtons.IsValidIndex(1)) ArchetypeButtons[1]->SetVisualColor(TargetState->GetCharacterArchetype() == EArenaDuelCharacterArchetype::Warden ? Active : ElevatedColor);
			if (ArchetypeButtons.IsValidIndex(2)) ArchetypeButtons[2]->SetVisualColor(TargetState->GetCharacterArchetype() == EArenaDuelCharacterArchetype::Rift ? Active : ElevatedColor);
			if (GodModeButton)
			{
				GodModeButton->SetLabel(FText::FromString(TargetState->HasAdminGodMode() ? TEXT("GOD MODE: ON") : TEXT("GOD MODE: OFF")));
				GodModeButton->SetVisualColor(TargetState->HasAdminGodMode() ? Active : ElevatedColor);
			}
			if (InfiniteAmmoButton)
			{
				InfiniteAmmoButton->SetLabel(FText::FromString(TargetState->HasAdminInfiniteAmmo() ? TEXT("INFINITE AMMO: ON") : TEXT("INFINITE AMMO: OFF")));
				InfiniteAmmoButton->SetVisualColor(TargetState->HasAdminInfiniteAmmo() ? Active : ElevatedColor);
			}
			if (InfiniteStaminaButton)
			{
				InfiniteStaminaButton->SetLabel(FText::FromString(TargetState->HasAdminInfiniteStamina() ? TEXT("INFINITE STAMINA: ON") : TEXT("INFINITE STAMINA: OFF")));
				InfiniteStaminaButton->SetVisualColor(TargetState->HasAdminInfiniteStamina() ? Active : ElevatedColor);
			}
			if (Weapon && WeaponButtons.Num() == 4)
			{
				const int32 CurrentWeapon = static_cast<int32>(Weapon->GetCurrentWeaponId());
				for (int32 Index = 0; Index < WeaponButtons.Num(); ++Index) WeaponButtons[Index]->SetVisualColor(Index == CurrentWeapon ? Active : ElevatedColor);
			}
		if (HealthInput && !HealthInput->HasKeyboardFocus()) SetTextIfChanged(HealthInput, FText::AsNumber(FMath::RoundToInt(TargetPawn->GetHealth())));
		}
		else
		{
			SetTextIfChanged(TargetStatusValues[1], TEXT("-- / --"));
			SetTextIfChanged(TargetStatusValues[2], TEXT("OFFLINE"));
			SetTextIfChanged(TargetStatusValues[3], TargetState ? FString::FromInt(TargetState->GetRoundWins()) : TEXT("--"));
			SetTextIfChanged(TargetStatusValues[4], TEXT("--"));
			for (int32 Index = 5; Index < 8; ++Index) SetTextIfChanged(TargetStatusValues[Index], TEXT("--"));
		}
	}

	if (MovementStatusValues.Num() == 5)
	{
		if (TargetPawn)
		{
			const UArenaDuelCharacterMovementComponent* Movement = TargetPawn->GetArenaDuelMovementComponent();
			const FVector Position = TargetPawn->GetActorLocation();
			if (Movement)
			{
				SetTextIfChanged(MovementStatusValues[0], Movement->GetDevelopmentMovementState());
				SetTextIfChanged(MovementStatusValues[1], FString::Printf(TEXT("%.0f uu/s"), Movement->Velocity.Size2D()));
				SetTextIfChanged(MovementStatusValues[2], FString::Printf(TEXT("%.0f / %.0f"), Movement->GetStamina(), Movement->GetMaxStamina()));
				SetTextIfChanged(MovementStatusValues[3], MovementModeName(Movement->MovementMode));
				SetTextIfChanged(MovementStatusValues[4], FString::Printf(TEXT("X  %.0f     Y  %.0f     Z  %.0f"), Position.X, Position.Y, Position.Z));
			}
		}
		else for (UTextBlock* Value : MovementStatusValues) SetTextIfChanged(Value, TEXT("--"));
	}

	if (NetworkStatusValues.Num() == 7 && PC && World)
	{
		const FString Role = PC->HasAuthority() ? TEXT("AUTHORITY") : TEXT("AUTONOMOUS CLIENT");
		const AArenaDuelPlayerState* LocalState = PC->GetPlayerState<AArenaDuelPlayerState>();
		const int32 Ping = LocalState ? FMath::RoundToInt(LocalState->GetPingInMilliseconds()) : 0;
		SetTextIfChanged(NetworkStatusValues[0], NetModeName(World->GetNetMode()));
		SetTextIfChanged(NetworkStatusValues[1], Role);
		SetTextIfChanged(NetworkStatusValues[2], FString::Printf(TEXT("%d ms"), Ping));
		SetTextIfChanged(NetworkStatusValues[3], LocalState ? FString::Printf(TEXT("PLAYER %d"), LocalState->GetDuelSlot() + 1) : TEXT("--"));
		SetTextIfChanged(NetworkStatusValues[4], GameState ? FString::FromInt(GameState->GetRoundNumber()) : TEXT("--"));
		SetTextIfChanged(NetworkStatusValues[5], GameState && GameState->IsRoundInProgress() ? TEXT("ACTIVE") : TEXT("BREAK"));
		SetTextIfChanged(NetworkStatusValues[6], GameState && GameState->GetLastRoundWinnerSlot() != INDEX_NONE ? FString::Printf(TEXT("PLAYER %d"), GameState->GetLastRoundWinnerSlot() + 1) : TEXT("NONE"));
	}

	if (AArenaDuelMovementDebugHUD* DebugHUD = PC ? Cast<AArenaDuelMovementDebugHUD>(PC->GetHUD()) : nullptr)
	{
		if (DebugOverlayButton)
		{
			DebugOverlayButton->SetLabel(FText::FromString(DebugHUD->IsShowingDebugOverlay() ? TEXT("DEBUG OVERLAY: ON") : TEXT("DEBUG OVERLAY: OFF")));
			DebugOverlayButton->SetVisualColor(DebugHUD->IsShowingDebugOverlay() ? Active : ElevatedColor);
		}
		if (HitZonesButton)
		{
			HitZonesButton->SetLabel(FText::FromString(DebugHUD->IsShowingHitZones() ? TEXT("HIT ZONES: ON") : TEXT("HIT ZONES: OFF")));
			HitZonesButton->SetVisualColor(DebugHUD->IsShowingHitZones() ? Active : ElevatedColor);
		}
	}
	// The mode decides which tabs make sense and what the score row means.
	const AArenaDuelZombieGameState* Survival = Cast<AArenaDuelZombieGameState>(GameState);
	SetTextIfChanged(ModeReadout, Survival ? TEXT("ZOMBIE SURVIVAL") : TEXT("1 VS 1 DUEL"));
	if (NavTabs.Num() == 6)
	{
		NavTabs[2]->SetVisibility(Survival ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
		NavTabs[5]->SetVisibility(Survival ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		if ((Survival && SelectedPage == 2) || (!Survival && SelectedPage == 5)) SelectPage(0);
	}
	SetTextIfChanged(ScoreRowLabel, Survival ? TEXT("POINTS") : TEXT("ROUND WINS"));
	if (Survival && TargetState && TargetStatusValues.Num() >= 8) SetTextIfChanged(TargetStatusValues[3], FString::FromInt(TargetState->GetSurvivalPoints()));
	if (SurvivalStatusValues.Num() == 4)
	{
		const AArenaDuelZombieGameMode* Rules = World ? World->GetAuthGameMode<AArenaDuelZombieGameMode>() : nullptr;
		const bool bHarmless = Rules && Rules->GetZombieDamageScale() <= 0.0f;
		SetTextIfChanged(SurvivalStatusValues[0], Survival ? FString::FromInt(Survival->GetWave()) : TEXT("--"));
		SetTextIfChanged(SurvivalStatusValues[1], Survival ? FString::FromInt(Survival->GetZombiesRemaining()) : TEXT("--"));
		SetTextIfChanged(SurvivalStatusValues[2], !Survival ? TEXT("--") : Survival->IsGameOver() ? TEXT("GAME OVER") : Survival->IsIntermission() ? TEXT("BREAK") : TEXT("WAVE RUNNING"));
		SetTextIfChanged(SurvivalStatusValues[3], !Rules ? TEXT("--") : bHarmless ? TEXT("OFF") : TEXT("ON"));
		if (ZombieDamageButton)
		{
			ZombieDamageButton->SetLabel(FText::FromString(bHarmless ? TEXT("ZOMBIE DAMAGE: OFF") : TEXT("ZOMBIE DAMAGE: ON")));
			ZombieDamageButton->SetVisualColor(bHarmless ? Active : ElevatedColor);
		}
	}
	if (Player1WinsInput && !Player1WinsInput->HasKeyboardFocus())
	{
		const AArenaDuelPlayerState* P1 = nullptr;
		if (GameState) for (APlayerState* State : GameState->PlayerArray) if (const AArenaDuelPlayerState* DuelState = Cast<AArenaDuelPlayerState>(State); DuelState && DuelState->GetDuelSlot() == 0) { P1 = DuelState; break; }
		if (P1) SetTextIfChanged(Player1WinsInput, FText::AsNumber(P1->GetRoundWins()));
	}
	if (Player2WinsInput && !Player2WinsInput->HasKeyboardFocus())
	{
		const AArenaDuelPlayerState* P2 = nullptr;
		if (GameState) for (APlayerState* State : GameState->PlayerArray) if (const AArenaDuelPlayerState* DuelState = Cast<AArenaDuelPlayerState>(State); DuelState && DuelState->GetDuelSlot() == 1) { P2 = DuelState; break; }
		if (P2) SetTextIfChanged(Player2WinsInput, FText::AsNumber(P2->GetRoundWins()));
	}
}
