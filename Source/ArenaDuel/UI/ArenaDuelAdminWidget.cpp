// Copyright Epic Games, Inc. All Rights Reserved.

#include "ArenaDuelAdminWidget.h"

#include "../Characters/ArenaDuelCharacter.h"
#include "../Characters/ArenaDuelCharacterMovementComponent.h"
#include "../Game/ArenaDuelGameState.h"
#include "../Game/ArenaDuelMovementDebugHUD.h"
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

	Backdrop = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Backdrop->SetBrushColor(RootColor);
	RootOverlay->AddChildToOverlay(Backdrop);

	UBorder* InsetFrame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	InsetFrame->SetBrushColor(FLinearColor(0.027f, 0.063f, 0.106f, 0.0f));
	InsetFrame->SetPadding(FMargin(32.0f, 24.0f));
	RootOverlay->AddChildToOverlay(InsetFrame);

	UVerticalBox* Shell = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	InsetFrame->SetContent(Shell);

	UHorizontalBox* Header = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Shell->AddChildToVerticalBox(Header)->SetPadding(FMargin(8.0f, 4.0f, 8.0f, 18.0f));
	UVerticalBox* Brand = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Header->AddChildToHorizontalBox(Brand)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	UHorizontalBox* TitleRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Brand->AddChildToVerticalBox(TitleRow);
	TitleRow->AddChildToHorizontalBox(MakeText(WidgetTree, FText::FromString(TEXT("ARENADUEL")), 32.0f, Cyan))->SetPadding(FMargin(0, 0, 12, 0));
	TitleRow->AddChildToHorizontalBox(MakeText(WidgetTree, FText::FromString(TEXT("/")), 25.0f, MutedText))->SetPadding(FMargin(0, 2, 12, 0));
	TitleRow->AddChildToHorizontalBox(MakeText(WidgetTree, FText::FromString(TEXT("ADMIN CONTROL")), 27.0f, PrimaryText))->SetVerticalAlignment(VAlign_Center);
	Brand->AddChildToVerticalBox(MakeText(WidgetTree, FText::FromString(TEXT("DEVELOPMENT CONTROL CENTER     ·     HOST AUTHORIZED")), 12.0f, SecondaryText))->SetPadding(FMargin(1, 3, 0, 0));
	UVerticalBox* HeaderRight = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Header->AddChildToHorizontalBox(HeaderRight)->SetVerticalAlignment(VAlign_Center);
	HeaderRight->AddChildToVerticalBox(MakeText(WidgetTree, FText::FromString(TEXT("F1 / ESC")), 16.0f, PrimaryText, ETextJustify::Right));
	HeaderRight->AddChildToVerticalBox(MakeText(WidgetTree, FText::FromString(TEXT("CLOSE MENU")), 11.0f, MutedText, ETextJustify::Right));

	USizeBox* Divider = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	Divider->SetHeightOverride(1.0f);
	UBorder* DividerLine = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	DividerLine->SetBrushColor(FLinearColor(0.475f, 0.914f, 1.0f, 0.16f));
	Divider->AddChild(DividerLine);
	Shell->AddChildToVerticalBox(Divider)->SetPadding(FMargin(0, 0, 0, 18));

	UHorizontalBox* Body = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	UVerticalBoxSlot* BodySlot = Shell->AddChildToVerticalBox(Body);
	BodySlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	BodySlot->SetPadding(FMargin(0, 0, 0, 14));

	USizeBox* SidebarSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	SidebarSize->SetWidthOverride(244.0f);
	Body->AddChildToHorizontalBox(SidebarSize)->SetPadding(FMargin(0, 0, 20, 0));
	UBorder* SidebarBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	SidebarBorder->SetBrushColor(FLinearColor(0.039f, 0.071f, 0.110f, 0.72f));
	SidebarBorder->SetPadding(FMargin(10, 12));
	SidebarSize->AddChild(SidebarBorder);
	UVerticalBox* Sidebar = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	SidebarBorder->SetContent(Sidebar);
	Sidebar->AddChildToVerticalBox(MakeText(WidgetTree, FText::FromString(TEXT("CONTROL MODULES")), 11.0f, MutedText))->SetPadding(FMargin(10, 2, 8, 14));
	const FText NavLabels[] = { FText::FromString(TEXT("PLAYER")), FText::FromString(TEXT("WEAPONS")), FText::FromString(TEXT("ROUND")), FText::FromString(TEXT("MOVEMENT")), FText::FromString(TEXT("DEBUG / NET")) };
	const EArenaDuelAdminCommand NavCommands[] = { EArenaDuelAdminCommand::SelectPlayerPage, EArenaDuelAdminCommand::SelectWeaponsPage, EArenaDuelAdminCommand::SelectRoundPage, EArenaDuelAdminCommand::SelectMovementPage, EArenaDuelAdminCommand::SelectDebugPage };
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(NavLabels); ++Index)
	{
		UHorizontalBox* NavRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		Sidebar->AddChildToVerticalBox(NavRow)->SetPadding(FMargin(0, 3));
		USizeBox* AccentSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		AccentSize->SetWidthOverride(3.0f);
		AccentSize->SetHeightOverride(48.0f);
		UBorder* Accent = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Accent->SetBrushColor(Index == 0 ? Cyan : FLinearColor::Transparent);
		AccentSize->AddChild(Accent);
		NavRow->AddChildToHorizontalBox(AccentSize)->SetPadding(FMargin(0, 0, 8, 0));
		NavAccents.Add(Accent);
		NavButtons.Add(AddButton(NavRow, NavLabels[Index], NavCommands[Index], 0.0f, Index == 0 ? Active : Inactive, 196.0f, ETextJustify::Left));
	}
	UVerticalBoxSlot* SidebarSpacerSlot = Sidebar->AddChildToVerticalBox(WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass()));
	SidebarSpacerSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	Sidebar->AddChildToVerticalBox(MakeText(WidgetTree, FText::FromString(TEXT("DEVELOPMENT BUILD\nHOST COMMANDS ONLY")), 11.0f, MutedText))->SetPadding(FMargin(10, 12, 8, 4));

	ContentHost = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Body->AddChildToHorizontalBox(ContentHost)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

	UHorizontalBox* PageHeading = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	ContentHost->AddChildToVerticalBox(PageHeading)->SetPadding(FMargin(8, 0, 8, 16));
	UVerticalBox* PageHeadingStack = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	PageHeading->AddChildToHorizontalBox(PageHeadingStack)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	PageTitle = MakeText(WidgetTree, FText::FromString(TEXT("PLAYER CONTROL")), 26.0f, PrimaryText);
	PageHeadingStack->AddChildToVerticalBox(PageTitle);
	PageDescription = MakeText(WidgetTree, FText::FromString(TEXT("Manage player state and development overrides.")), 13.0f, SecondaryText);
	PageHeadingStack->AddChildToVerticalBox(PageDescription)->SetPadding(FMargin(0, 3, 0, 0));

	UVerticalBox* TargetArea = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	ContentHost->AddChildToVerticalBox(TargetArea)->SetPadding(FMargin(8, 0, 8, 10));
	TargetArea->AddChildToVerticalBox(MakeText(WidgetTree, FText::FromString(TEXT("TARGET PLAYER")), 11.0f, MutedText))->SetPadding(FMargin(0, 0, 0, 6));
	UHorizontalBox* TargetRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	TargetArea->AddChildToVerticalBox(TargetRow);
	TargetButtons.Add(AddButton(TargetRow, FText::FromString(TEXT("PLAYER 1")), EArenaDuelAdminCommand::SelectPlayer1, 0.0f, Active, 190.0f));
	TargetButtons.Add(AddButton(TargetRow, FText::FromString(TEXT("PLAYER 2")), EArenaDuelAdminCommand::SelectPlayer2, 0.0f, Inactive, 190.0f));

	UHorizontalBox* StatusRow1 = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	ContentHost->AddChildToVerticalBox(StatusRow1)->SetPadding(FMargin(8, 0, 8, 8));
	TargetStatusValues.Add(AddStatusCard(StatusRow1, FText::FromString(TEXT("TARGET")), Cyan));
	TargetStatusValues.Add(AddStatusCard(StatusRow1, FText::FromString(TEXT("HEALTH")), Cyan));
	TargetStatusValues.Add(AddStatusCard(StatusRow1, FText::FromString(TEXT("STATE")), Cyan));
	TargetStatusValues.Add(AddStatusCard(StatusRow1, FText::FromString(TEXT("ROUND WINS")), Violet));
	TargetStatusValues.Add(AddStatusCard(StatusRow1, FText::FromString(TEXT("WEAPON")), Cyan));

	UHorizontalBox* StatusRow2 = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	ContentHost->AddChildToVerticalBox(StatusRow2)->SetPadding(FMargin(8, 0, 8, 12));
	TargetStatusValues.Add(AddStatusCard(StatusRow2, FText::FromString(TEXT("GOD MODE")), Cyan));
	TargetStatusValues.Add(AddStatusCard(StatusRow2, FText::FromString(TEXT("INFINITE AMMO")), Cyan));
	TargetStatusValues.Add(AddStatusCard(StatusRow2, FText::FromString(TEXT("INFINITE STAMINA")), Violet));

	PageScrollBox = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
	PageScrollBox->SetScrollBarVisibility(ESlateVisibility::Visible);
	ContentHost->AddChildToVerticalBox(PageScrollBox)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

	UVerticalBox* PlayerPage = CreatePage(TEXT("PLAYER CONTROL"), TEXT("Manage health and player development overrides."));
	UVerticalBox* HealthCard = AddCard(PlayerPage, FText::FromString(TEXT("HEALTH CONTROL")), FText::FromString(TEXT("Set health directly or restore the selected player.")));
	UHorizontalBox* HealthRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	HealthCard->AddChildToVerticalBox(HealthRow)->SetPadding(FMargin(0, 8, 0, 0));
	HealthInput = AddNumericInput(HealthRow, FText::FromString(TEXT("100")), FText::FromString(TEXT("HEALTH VALUE")), 145.0f);
	SetHealthButton = AddButton(HealthRow, FText::FromString(TEXT("SET HEALTH")), EArenaDuelAdminCommand::SetHealth, 0.0f, Active, 158.0f);
	AddButton(HealthRow, FText::FromString(TEXT("FULL HEAL")), EArenaDuelAdminCommand::FullHeal, 0.0f, ElevatedColor, 150.0f);

	UVerticalBox* PlayerStateCard = AddCard(PlayerPage, FText::FromString(TEXT("PLAYER STATE")), FText::FromString(TEXT("Development-only state and recovery actions.")));
	UHorizontalBox* PlayerActions = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	PlayerStateCard->AddChildToVerticalBox(PlayerActions)->SetPadding(FMargin(0, 8, 0, 0));
	GodModeButton = AddButton(PlayerActions, FText::FromString(TEXT("GOD MODE: OFF")), EArenaDuelAdminCommand::ToggleGodMode, 0.0f, ElevatedColor, 190.0f);
	AddButton(PlayerActions, FText::FromString(TEXT("RESET PLAYER")), EArenaDuelAdminCommand::ResetPlayer, 0.0f, ElevatedColor, 190.0f);
	AddButton(PlayerActions, FText::FromString(TEXT("KILL PLAYER")), EArenaDuelAdminCommand::Kill, 0.0f, Danger, 190.0f);
	AddHelperText(PlayerStateCard, FText::FromString(TEXT("Reset Player is available while alive. Dead players recover through Restart Round.")));

	UVerticalBox* WeaponsPage = CreatePage(TEXT("WEAPONS"), TEXT("Equip weapons and control ammunition for the selected player."));
	UVerticalBox* EquipCard = AddCard(WeaponsPage, FText::FromString(TEXT("EQUIP WEAPON")), FText::FromString(TEXT("The selected weapon is highlighted in cyan.")));
	UGridPanel* WeaponGrid = WidgetTree->ConstructWidget<UGridPanel>(UGridPanel::StaticClass());
	EquipCard->AddChildToVerticalBox(WeaponGrid)->SetPadding(FMargin(0, 10, 0, 0));
	const FText WeaponLabels[] = { FText::FromString(TEXT("ARC RIFLE")), FText::FromString(TEXT("SHADE SMG")), FText::FromString(TEXT("RUNE DMR")), FText::FromString(TEXT("HEX SHOTGUN")) };
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(WeaponLabels); ++Index)
	{
		WeaponButtons.Add(AddGridButton(WeaponGrid, Index % 2, Index / 2, WeaponLabels[Index], EArenaDuelAdminCommand::EquipWeapon, static_cast<float>(Index), ElevatedColor, 250.0f));
	}
	UVerticalBox* AmmoCard = AddCard(WeaponsPage, FText::FromString(TEXT("AMMUNITION")), FText::FromString(TEXT("Development overrides do not change weapon balance.")));
	UHorizontalBox* AmmoRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	AmmoCard->AddChildToVerticalBox(AmmoRow)->SetPadding(FMargin(0, 8, 0, 0));
	AddButton(AmmoRow, FText::FromString(TEXT("REFILL ALL AMMO")), EArenaDuelAdminCommand::RefillAmmo, 0.0f, ElevatedColor, 220.0f);
	InfiniteAmmoButton = AddButton(AmmoRow, FText::FromString(TEXT("INFINITE AMMO: OFF")), EArenaDuelAdminCommand::ToggleInfiniteAmmo, 0.0f, ElevatedColor, 220.0f);

	UVerticalBox* RoundPage = CreatePage(TEXT("ROUND CONTROL"), TEXT("Manage round flow and score state."));
	UVerticalBox* RoundFlowCard = AddCard(RoundPage, FText::FromString(TEXT("ROUND FLOW")));
	UHorizontalBox* RoundFlowRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	RoundFlowCard->AddChildToVerticalBox(RoundFlowRow)->SetPadding(FMargin(0, 6, 0, 0));
	AddButton(RoundFlowRow, FText::FromString(TEXT("RESTART ROUND")), EArenaDuelAdminCommand::RestartRound, 0.0f, ElevatedColor, 220.0f);
	AddButton(RoundFlowRow, FText::FromString(TEXT("NEXT ROUND")), EArenaDuelAdminCommand::NextRound, 0.0f, Active, 220.0f);

	UVerticalBox* RoundResultCard = AddCard(RoundPage, FText::FromString(TEXT("ROUND RESULT")));
	UHorizontalBox* RoundResultRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	RoundResultCard->AddChildToVerticalBox(RoundResultRow)->SetPadding(FMargin(0, 6, 0, 0));
	AddButton(RoundResultRow, FText::FromString(TEXT("AWARD PLAYER 1")), EArenaDuelAdminCommand::AwardRound, 0.0f, Active, 220.0f);
	AddButton(RoundResultRow, FText::FromString(TEXT("AWARD PLAYER 2")), EArenaDuelAdminCommand::AwardRound, 1.0f, FLinearColor(0.20f, 0.12f, 0.29f, 1.0f), 220.0f);

	UVerticalBox* ScoreCard = AddCard(RoundPage, FText::FromString(TEXT("SCORE MANAGEMENT")), FText::FromString(TEXT("Manual score changes do not award or end a round.")));
	UHorizontalBox* P1ScoreRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	ScoreCard->AddChildToVerticalBox(P1ScoreRow)->SetPadding(FMargin(0, 6, 0, 4));
	P1ScoreRow->AddChildToHorizontalBox(MakeText(WidgetTree, FText::FromString(TEXT("PLAYER 1 WINS")), 13, SecondaryText))->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	Player1WinsInput = AddNumericInput(P1ScoreRow, FText::FromString(TEXT("0")), FText::FromString(TEXT("0 TO 5")), 120.0f);
	SetPlayer1WinsButton = AddButton(P1ScoreRow, FText::FromString(TEXT("SET")), EArenaDuelAdminCommand::SetPlayer1Wins, 0.0f, ElevatedColor, 104.0f);
	UHorizontalBox* P2ScoreRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	ScoreCard->AddChildToVerticalBox(P2ScoreRow)->SetPadding(FMargin(0, 4, 0, 2));
	P2ScoreRow->AddChildToHorizontalBox(MakeText(WidgetTree, FText::FromString(TEXT("PLAYER 2 WINS")), 13, SecondaryText))->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	Player2WinsInput = AddNumericInput(P2ScoreRow, FText::FromString(TEXT("0")), FText::FromString(TEXT("0 TO 5")), 120.0f);
	SetPlayer2WinsButton = AddButton(P2ScoreRow, FText::FromString(TEXT("SET")), EArenaDuelAdminCommand::SetPlayer2Wins, 0.0f, ElevatedColor, 104.0f);

	UVerticalBox* DangerCard = AddCard(RoundPage, FText::FromString(TEXT("DANGER ZONE")), FText::FromString(TEXT("Resets round number and both player scores.")));
	UHorizontalBox* ResetRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	DangerCard->AddChildToVerticalBox(ResetRow)->SetPadding(FMargin(0, 6, 0, 0));
	ResetMatchButton = AddButton(ResetRow, FText::FromString(TEXT("RESET MATCH")), EArenaDuelAdminCommand::ResetMatch, 0.0f, Danger, 220.0f);

	UVerticalBox* MovementPage = CreatePage(TEXT("MOVEMENT"), TEXT("Inspect movement and stamina state."));
	UVerticalBox* MovementCard = AddCard(MovementPage, FText::FromString(TEXT("MOVEMENT DIAGNOSTICS")));
	UHorizontalBox* MovementRow1 = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	MovementCard->AddChildToVerticalBox(MovementRow1)->SetPadding(FMargin(0, 6, 0, 6));
	MovementStatusValues.Add(AddStatusCard(MovementRow1, FText::FromString(TEXT("STATE")), Cyan));
	MovementStatusValues.Add(AddStatusCard(MovementRow1, FText::FromString(TEXT("SPEED")), Cyan));
	MovementStatusValues.Add(AddStatusCard(MovementRow1, FText::FromString(TEXT("STAMINA")), Violet));
	MovementStatusValues.Add(AddStatusCard(MovementRow1, FText::FromString(TEXT("MODE")), Cyan));
	UHorizontalBox* MovementRow2 = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	MovementCard->AddChildToVerticalBox(MovementRow2)->SetPadding(FMargin(0, 4, 0, 0));
	MovementStatusValues.Add(AddStatusCard(MovementRow2, FText::FromString(TEXT("POSITION  ·  X / Y / Z")), SecondaryText));
	UVerticalBox* StaminaCard = AddCard(MovementPage, FText::FromString(TEXT("STAMINA CONTROL")));
	UHorizontalBox* StaminaRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	StaminaCard->AddChildToVerticalBox(StaminaRow)->SetPadding(FMargin(0, 6, 0, 0));
	AddButton(StaminaRow, FText::FromString(TEXT("REFILL STAMINA")), EArenaDuelAdminCommand::RefillStamina, 0.0f, ElevatedColor, 220.0f);
	InfiniteStaminaButton = AddButton(StaminaRow, FText::FromString(TEXT("INFINITE STAMINA: OFF")), EArenaDuelAdminCommand::ToggleInfiniteStamina, 0.0f, ElevatedColor, 230.0f);
	AddHelperText(StaminaCard, FText::FromString(TEXT("Advanced movement tuning remains disabled to preserve network prediction.")));

	UVerticalBox* DebugPage = CreatePage(TEXT("DEBUG / NETWORK"), TEXT("Inspect runtime networking and local debug visualization."));
	UVerticalBox* LocalDebugCard = AddCard(DebugPage, FText::FromString(TEXT("LOCAL DEBUG")));
	UHorizontalBox* DebugActions = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	LocalDebugCard->AddChildToVerticalBox(DebugActions)->SetPadding(FMargin(0, 6, 0, 0));
	DebugOverlayButton = AddButton(DebugActions, FText::FromString(TEXT("DEBUG OVERLAY: OFF")), EArenaDuelAdminCommand::ToggleDebugOverlay, 0.0f, ElevatedColor, 230.0f);
	HitZonesButton = AddButton(DebugActions, FText::FromString(TEXT("HIT ZONES: OFF")), EArenaDuelAdminCommand::ToggleHitZones, 0.0f, ElevatedColor, 230.0f);

	UVerticalBox* NetworkCard = AddCard(DebugPage, FText::FromString(TEXT("NETWORK STATUS")));
	UHorizontalBox* NetRow1 = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	NetworkCard->AddChildToVerticalBox(NetRow1)->SetPadding(FMargin(0, 6, 0, 6));
	NetworkStatusValues.Add(AddStatusCard(NetRow1, FText::FromString(TEXT("NET MODE")), Cyan));
	NetworkStatusValues.Add(AddStatusCard(NetRow1, FText::FromString(TEXT("LOCAL ROLE")), Cyan));
	NetworkStatusValues.Add(AddStatusCard(NetRow1, FText::FromString(TEXT("PING")), Violet));
	NetworkStatusValues.Add(AddStatusCard(NetRow1, FText::FromString(TEXT("DUEL SLOT")), Cyan));
	UHorizontalBox* NetRow2 = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	NetworkCard->AddChildToVerticalBox(NetRow2)->SetPadding(FMargin(0, 4, 0, 0));
	NetworkStatusValues.Add(AddStatusCard(NetRow2, FText::FromString(TEXT("ROUND")), Violet));
	NetworkStatusValues.Add(AddStatusCard(NetRow2, FText::FromString(TEXT("ROUND STATE")), Cyan));
	NetworkStatusValues.Add(AddStatusCard(NetRow2, FText::FromString(TEXT("LAST WINNER")), SecondaryText));
	AddHelperText(NetworkCard, FText::FromString(TEXT("Hit-zone display is local debug drawing only. Collision remains unchanged.")));

	UHorizontalBox* Footer = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Shell->AddChildToVerticalBox(Footer)->SetPadding(FMargin(8, 8, 8, 0));
	StatusReadout = MakeText(WidgetTree, FText::FromString(TEXT("SERVER AUTHORIZED")), 11.0f, Cyan);
	Footer->AddChildToHorizontalBox(StatusReadout)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	Footer->AddChildToHorizontalBox(MakeText(WidgetTree, FText::FromString(TEXT("COMMANDS EXECUTE ON AUTHORITY")), 10.0f, MutedText, ETextJustify::Center))->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	Footer->AddChildToHorizontalBox(MakeText(WidgetTree, FText::FromString(TEXT("BUILD: DEVELOPMENT")), 10.0f, MutedText, ETextJustify::Right))->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

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
	SetActionStatus(TEXT("COMMAND SENT TO SERVER"), Cyan);
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
