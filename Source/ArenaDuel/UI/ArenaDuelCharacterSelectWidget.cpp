#include "ArenaDuelCharacterSelectWidget.h"
#include "../Player/ArenaDuelPlayerController.h"
#include "../Game/ArenaDuelGameState.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScaleBox.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Styling/CoreStyle.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Framework/Application/SlateApplication.h"
#include "InputCoreTypes.h"

namespace CharacterSelectStyle
{
	const FLinearColor CharacterSelectCyan(0.33f, 0.90f, 1.0f);
	const FLinearColor CharacterSelectViolet(0.82f, 0.40f, 1.0f);
	const FLinearColor White(0.96f, 0.97f, 0.99f);
	const FLinearColor Muted(0.59f, 0.67f, 0.77f);
	const FLinearColor PanelBackground(0.02f, 0.05f, 0.10f, 0.93f);
	FLinearColor Accent(int32 SideIndex) { return SideIndex == 0 ? CharacterSelectCyan : CharacterSelectViolet; }

	void Place(UCanvasPanel* Parent, UWidget* Widget, float X, float Y, float Width, float Height)
	{
		UCanvasPanelSlot* SideIndex = Parent->AddChildToCanvas(Widget);
		SideIndex->SetPosition(FVector2D(X, Y));
		SideIndex->SetSize(FVector2D(Width, Height));
	}
	UTextBlock* Text(UWidgetTree* Tree, UCanvasPanel* Parent, const FString& Value, int32 FontSize, FLinearColor Color, float X, float Y, float Width, float Height, ETextJustify::Type Justification = ETextJustify::Left)
	{
		UTextBlock* Text = Tree->ConstructWidget<UTextBlock>();
		Text->SetText(FText::FromString(Value));
		Text->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", FontSize));
		Text->SetColorAndOpacity(Color);
		Text->SetJustification(Justification);
		Text->SetAutoWrapText(true);
		Text->SetVisibility(ESlateVisibility::HitTestInvisible);
		Place(Parent, Text, X, Y, Width, Height);
		return Text;
	}
	void UpdateText(UTextBlock* Block, const FString& Value)
	{
		const FText Next = FText::FromString(Value);
		if (Block && !Block->GetText().EqualTo(Next)) Block->SetText(Next);
	}
	UBorder* Backing(UWidgetTree* Tree, UCanvasPanel* Parent, FLinearColor Color, float X, float Y, float W, float H)
	{
		UBorder* Border = Tree->ConstructWidget<UBorder>();
		Border->SetBrushColor(Color);
		Border->SetVisibility(ESlateVisibility::HitTestInvisible);
		Place(Parent, Border, X, Y, W, H);
		return Border;
	}
	UButton* Button(UWidgetTree* Tree, UCanvasPanel* Parent, const FString& Label, FLinearColor Color, float X, float Y, float W, float H, UTextBlock** OutLabel = nullptr)
	{
		UButton* Button = Tree->ConstructWidget<UButton>();
		FButtonStyle Style = FCoreStyle::Get().GetWidgetStyle<FButtonStyle>("Button");
		Style.Normal.TintColor = FSlateColor(FLinearColor(0.04f, 0.11f, 0.17f, 0.94f));
		Style.Hovered.TintColor = FSlateColor(FLinearColor(0.09f, 0.24f, 0.32f));
		Style.Pressed.TintColor = FSlateColor(FLinearColor(0.12f, 0.30f, 0.37f));
		Button->SetStyle(Style);
		UTextBlock* Block = Tree->ConstructWidget<UTextBlock>();
		Block->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 19));
		Block->SetText(FText::FromString(Label));
		Block->SetColorAndOpacity(Color);
		Block->SetJustification(ETextJustify::Center);
		Button->AddChild(Block);
		Place(Parent, Button, X, Y, W, H);
		if (OutLabel) *OutLabel = Block;
		return Button;
	}
}

const FArenaDuelCharacterPresentation& UArenaDuelCharacterSelectWidget::GetPresentation(EArenaDuelCharacterArchetype Archetype)
{
	static const FArenaDuelCharacterPresentation Shadow {
		TEXT("SHADOW"), TEXT("MOBILITY / TRICKSTER"), TEXT("Fast evasive fighter focused on repositioning and vision control."),
		TEXT("SHADOW STEP"), TEXT("Directional mobility dash"), TEXT("VEIL WALL"), TEXT("Temporary vision blocker"), {0.6f, 1.0f, 0.4f, 0.8f} };
	static const FArenaDuelCharacterPresentation Warden {
		TEXT("WARDEN"), TEXT("DEFENSE / CONTROL"), TEXT("Durable space controller built around cover and explosive repositioning."),
		TEXT("ARC BARRIER"), TEXT("Deploy durable bullet-blocking cover"), TEXT("BURST LEAP"), TEXT("Forward and upward mobility burst"), {0.6f, 0.6f, 1.0f, 0.6f} };
	static const FArenaDuelCharacterPresentation Rift {
		TEXT("RIFT"), TEXT("MOBILITY / REPOSITION"), TEXT("Precision traversal and controlled relocation."),
		TEXT("RIFT GRAPPLE"), TEXT("Pull toward targeted arena geometry"), TEXT("PHASE GATE"), TEXT("Reposition to a validated destination"), {0.4f, 1.0f, 0.6f, 1.0f} };
	switch (Archetype)
	{
	case EArenaDuelCharacterArchetype::Warden: return Warden;
	case EArenaDuelCharacterArchetype::Rift: return Rift;
	default: return Shadow;
	}
}

EArenaDuelCharacterArchetype UArenaDuelCharacterSelectWidget::CycleArchetype(EArenaDuelCharacterArchetype Current, int32 Direction)
{
	return static_cast<EArenaDuelCharacterArchetype>((static_cast<int32>(Current) + (Direction < 0 ? 2 : 1)) % 3);
}

bool UArenaDuelCharacterSelectWidget::Initialize()
{
	if (!Super::Initialize()) return false;
	SetIsFocusable(true);
	if (!WidgetTree) WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"), RF_Transient);
	BuildTree();
	return HasExpectedTree();
}

void UArenaDuelCharacterSelectWidget::BuildTree()
{
	if (!WidgetTree || ReferenceCanvas) return;
	// Uniform reference-space scaling preserves the two-card composition in smaller PIE windows.
	UScaleBox* Scale = WidgetTree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass(), TEXT("FullscreenRoot"));
	Scale->SetStretch(EStretch::ScaleToFit);
	USizeBox* Reference = WidgetTree->ConstructWidget<USizeBox>();
	Reference->SetWidthOverride(1920.0f);
	Reference->SetHeightOverride(1080.0f);
	ReferenceCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("ReferenceCanvas"));
	Reference->AddChild(ReferenceCanvas);
	Scale->AddChild(Reference);
	WidgetTree->RootWidget = Scale;
	using namespace CharacterSelectStyle;
	Backing(WidgetTree, ReferenceCanvas, FLinearColor(0.015f, 0.025f, 0.065f, 0.90f), 0, 0, 1920, 1080);
	Text(WidgetTree, ReferenceCanvas, TEXT("A R E N A"), 39, CharacterSelectCyan, 720, 27, 250, 56, ETextJustify::Right);
	Text(WidgetTree, ReferenceCanvas, TEXT("D U E L"), 39, CharacterSelectViolet, 980, 27, 225, 56);
	Text(WidgetTree, ReferenceCanvas, TEXT("C H A R A C T E R   S E L E C T"), 17, White, 690, 83, 540, 30, ETextJustify::Center);
	Text(WidgetTree, ReferenceCanvas, TEXT("P R E - M A T C H   L O B B Y"), 12, Muted, 720, 119, 480, 25, ETextJustify::Center);
	Text(WidgetTree, ReferenceCanvas, TEXT("1V1 ARENA\nDEVELOPMENT DUEL"), 12, Muted, 65, 45, 300, 52);
	Connection = Text(WidgetTree, ReferenceCanvas, TEXT("CONNECTING"), 12, Muted, 1550, 45, 300, 52, ETextJustify::Right);
	Panels.SetNum(2);
	BuildPlayerPanel(0);
	BuildPlayerPanel(1);
	CenterLabel = Text(WidgetTree, ReferenceCanvas, TEXT("VS"), 64, White, 755, 488, 410, 120, ETextJustify::Center);
	CenterStatus = Text(WidgetTree, ReferenceCanvas, TEXT("WAITING FOR BOTH PLAYERS"), 14, Muted, 750, 626, 420, 76, ETextJustify::Center);
	AutoReadyBackdrop = Backing(WidgetTree, ReferenceCanvas, FLinearColor(0.025f, 0.065f, 0.105f, 0.90f), 770, 711, 380, 56);
	AutoReadyAccent = Backing(WidgetTree, ReferenceCanvas, CharacterSelectCyan.CopyWithNewOpacity(0.8f), 770, 711, 380, 2);
	AutoReadyStatus = Text(WidgetTree, ReferenceCanvas, TEXT("AUTO READY IN 12 SEC"), 17, White, 788, 724, 344, 30, ETextJustify::Center);
	Matchup = Text(WidgetTree, ReferenceCanvas, TEXT("PLAYER 1  >  VS  <  PLAYER 2"), 16, CharacterSelectCyan, 725, 864, 470, 78, ETextJustify::Center);
	Text(WidgetTree, ReferenceCanvas, TEXT("ESC   BACK / CANCEL READY"), 13, Muted, 85, 1005, 420, 30);
	Text(WidgetTree, ReferenceCanvas, TEXT("A / D   SWITCH CHARACTER     |     ENTER   READY"), 13, Muted, 520, 1005, 880, 30, ETextJustify::Center);
	Text(WidgetTree, ReferenceCanvas, TEXT("MOUSE   SELECT"), 13, Muted, 1450, 1005, 390, 30, ETextJustify::Right);
	SettingsButton = Button(WidgetTree, ReferenceCanvas, TEXT("SETTINGS"), CharacterSelectCyan, 1640, 945, 190, 42);
	SettingsButton->OnClicked.AddDynamic(this, &ThisClass::OpenSettings);
}

void UArenaDuelCharacterSelectWidget::BuildPlayerPanel(int32 SideIndex)
{
	using namespace CharacterSelectStyle;
	FArenaDuelSelectionPanel& PanelWidgets = Panels[SideIndex];
	const FLinearColor Color = Accent(SideIndex);
	PanelWidgets.Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), SideIndex == 0 ? TEXT("Player1Panel") : TEXT("Player2Panel"));
	Place(ReferenceCanvas, PanelWidgets.Root, SideIndex == 0 ? 90 : 1200, 155, 630, 785);
	UCanvasPanel* Root = PanelWidgets.Root;
	Backing(WidgetTree, Root, PanelBackground, 0, 0, 630, 785);
	Backing(WidgetTree, Root, Color.CopyWithNewOpacity(0.09f), 18, 58, 594, 302);
	Backing(WidgetTree, Root, Color.CopyWithNewOpacity(0.9f), 18, 18, 3, 24);
	Text(WidgetTree, Root, FString::Printf(TEXT("PLAYER %d"), SideIndex + 1), 21, Color, 36, 16, 240, 34);
	PanelWidgets.Identity = Text(WidgetTree, Root, TEXT("WAITING FOR PLAYER..."), 12, Muted, 280, 22, 320, 27, ETextJustify::Right);
	PanelWidgets.Name = Text(WidgetTree, Root, TEXT("SHADOW"), 36, White, 30, 358, 510, 53);
	PanelWidgets.Role = Text(WidgetTree, Root, TEXT("MOBILITY / TRICKSTER"), 17, Color, 30, 413, 325, 30);
	PanelWidgets.Description = Text(WidgetTree, Root, TEXT(""), 14, Muted, 30, 450, 306, 85);
	const TCHAR* Labels[] = { TEXT("DAMAGE"), TEXT("MOBILITY"), TEXT("SURVIVAL"), TEXT("DIFFICULTY") };
	for (int32 Index = 0; Index < 4; ++Index)
	{
		Text(WidgetTree, Root, Labels[Index], 10, Muted, 363, 413 + Index * 29, 96, 20);
		UProgressBar* Bar = WidgetTree->ConstructWidget<UProgressBar>();
		FProgressBarStyle Style;
		Style.BackgroundImage = *FCoreStyle::Get().GetBrush("WhiteBrush");
		Style.BackgroundImage.TintColor = FLinearColor(0.08f, 0.14f, 0.20f);
		Style.FillImage = *FCoreStyle::Get().GetBrush("WhiteBrush");
		Bar->SetWidgetStyle(Style);
		Bar->SetFillColorAndOpacity(Color);
		Place(Root, Bar, 465, 418 + Index * 29, 133, 7);
		PanelWidgets.Ratings.Add(Bar);
	}
	for (int32 Index = 0; Index < 2; ++Index)
	{
		const float X = 28 + Index * 294;
		Backing(WidgetTree, Root, FLinearColor(0.014f, 0.034f, 0.065f), X, 544, 280, 84);
		Backing(WidgetTree, Root, Color.CopyWithNewOpacity(0.45f), X, 544, 280, 1);
		Backing(WidgetTree, Root, Color.CopyWithNewOpacity(0.30f), X + 83, 554, 1, 62);
		Text(WidgetTree, Root, Index == 0 ? TEXT("Q") : TEXT("E"), 16, Color, X + 11, 551, 48, 23, ETextJustify::Center);
		UTextBlock* Name = Text(WidgetTree, Root, TEXT(""), 14, White, X + 97, 552, 174, 27);
		UTextBlock* Description = Text(WidgetTree, Root, TEXT(""), 12, Muted, X + 97, 583, 170, 40);
		if (Index == 0) { PanelWidgets.PrimaryName = Name; PanelWidgets.PrimaryDescription = Description; }
		else { PanelWidgets.SecondaryName = Name; PanelWidgets.SecondaryDescription = Description; }
	}
	Text(WidgetTree, Root, TEXT("CHOOSE YOUR FIGHTER"), 11, Muted, 42, 638, 300, 18);
	PanelWidgets.RosterButtons.Add(Button(WidgetTree, Root, TEXT("SHADOW"), Color, 42, 662, 174, 46));
	PanelWidgets.RosterButtons.Add(Button(WidgetTree, Root, TEXT("WARDEN"), Color, 229, 662, 174, 46));
	PanelWidgets.RosterButtons.Add(Button(WidgetTree, Root, TEXT("RIFT"), Color, 416, 662, 174, 46));
	for (int32 RosterIndex = 0; RosterIndex < PanelWidgets.RosterButtons.Num(); ++RosterIndex)
	{
		UButton* Roster = PanelWidgets.RosterButtons[RosterIndex];
		Roster->ClearChildren();
		UHorizontalBox* Layout = WidgetTree->ConstructWidget<UHorizontalBox>();
		USizeBox* IconArea = WidgetTree->ConstructWidget<USizeBox>();
		IconArea->SetWidthOverride(32.0f);
		IconArea->SetHeightOverride(28.0f);
		UTextBlock* Icon = WidgetTree->ConstructWidget<UTextBlock>();
		Icon->SetText(FText::FromString(TEXT("◇")));
		Icon->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 16));
		Icon->SetColorAndOpacity(Color);
		Icon->SetJustification(ETextJustify::Center);
		IconArea->AddChild(Icon);
		UHorizontalBoxSlot* IconSlot = Layout->AddChildToHorizontalBox(IconArea);
		IconSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
		IconSlot->SetVerticalAlignment(VAlign_Center);
		IconSlot->SetPadding(FMargin(4.0f, 0.0f, 0.0f, 0.0f));
		USpacer* IconLabelGap = WidgetTree->ConstructWidget<USpacer>();
		IconLabelGap->SetSize(FVector2D(12.0f, 1.0f));
		UHorizontalBoxSlot* GapSlot = Layout->AddChildToHorizontalBox(IconLabelGap);
		GapSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
		UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>();
		Label->SetText(FText::FromString(RosterIndex == 0 ? TEXT("SHADOW") : RosterIndex == 1 ? TEXT("WARDEN") : TEXT("RIFT")));
		Label->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 15));
		Label->SetColorAndOpacity(Color);
		Label->SetJustification(ETextJustify::Left);
		UHorizontalBoxSlot* LabelSlot = Layout->AddChildToHorizontalBox(Label);
		LabelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		LabelSlot->SetVerticalAlignment(VAlign_Center);
		LabelSlot->SetPadding(FMargin(0.0f, 0.0f, 4.0f, 0.0f));
		Roster->AddChild(Layout);
	}
	PanelWidgets.RosterButtons[0]->OnClicked.AddDynamic(this, &ThisClass::SelectShadow);
	PanelWidgets.RosterButtons[1]->OnClicked.AddDynamic(this, &ThisClass::SelectWarden);
	PanelWidgets.RosterButtons[2]->OnClicked.AddDynamic(this, &ThisClass::SelectRift);
	UTextBlock* ReadyText = nullptr;
	PanelWidgets.ReadyButton = Button(WidgetTree, Root, TEXT("READY"), Color, 28, 723, 574, 46, &ReadyText);
	PanelWidgets.ReadyText = ReadyText;
	PanelWidgets.ReadyButton->OnClicked.AddDynamic(this, &ThisClass::ToggleReady);
}

bool UArenaDuelCharacterSelectWidget::HasExpectedTree() const
{
	const bool bPanelsReady = WidgetTree && WidgetTree->RootWidget && ReferenceCanvas && Panels.Num() == 2 && CenterLabel && AutoReadyStatus && AutoReadyBackdrop && AutoReadyAccent && Matchup
		&& Panels[0].Root && Panels[1].Root && Panels[0].RosterButtons.Num() == 3 && Panels[1].RosterButtons.Num() == 3
		&& Panels[0].ReadyButton && Panels[1].ReadyButton;
	if (!bPanelsReady) return false;
	for (const FArenaDuelSelectionPanel& Panel : Panels)
	{
		for (const UButton* Roster : Panel.RosterButtons)
		{
			const UHorizontalBox* Layout = Roster ? Cast<UHorizontalBox>(Roster->GetChildAt(0)) : nullptr;
			const USizeBox* IconArea = Layout && Layout->GetChildrenCount() == 3 ? Cast<USizeBox>(Layout->GetChildAt(0)) : nullptr;
			const USpacer* Gap = Layout && Layout->GetChildrenCount() == 3 ? Cast<USpacer>(Layout->GetChildAt(1)) : nullptr;
			const UTextBlock* Label = Layout && Layout->GetChildrenCount() == 3 ? Cast<UTextBlock>(Layout->GetChildAt(2)) : nullptr;
			if (!IconArea || !Gap || !Label || !FMath::IsNearlyEqual(IconArea->GetWidthOverride(), 32.0f)
				|| !FMath::IsNearlyEqual(Gap->GetSize().X, 12.0f))
			{
				return false;
			}
		}
	}
	return true;
}

void UArenaDuelCharacterSelectWidget::NativeConstruct()
{
	Super::NativeConstruct();
	// No animations or latent actions: timer refresh is enough for this native-only screen.
	if (TSharedPtr<SWidget> SlateRoot = GetCachedWidget()) SlateRoot->SetCanTick(false);
	if (GetWorld()) GetWorld()->GetTimerManager().SetTimer(RefreshTimer, this, &ThisClass::RefreshLobby, 0.2f, true);
	RefreshLobby();
}

void UArenaDuelCharacterSelectWidget::NativeDestruct()
{
	if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(RefreshTimer);
	Super::NativeDestruct();
}

void UArenaDuelCharacterSelectWidget::RefreshLobby()
{
	using namespace CharacterSelectStyle;
	if (!HasExpectedTree()) return;
	const AArenaDuelGameState* State = GetWorld() ? GetWorld()->GetGameState<AArenaDuelGameState>() : nullptr;
	const AArenaDuelPlayerController* Controller = Cast<AArenaDuelPlayerController>(GetOwningPlayer());
	const AArenaDuelPlayerState* Local = Controller ? Controller->GetPlayerState<AArenaDuelPlayerState>() : nullptr;
	const bool bCountdown = State && State->GetMatchPhase() == EArenaDuelMatchPhase::Countdown;
	const AArenaDuelPlayerState* Players[2] = {};
	if (State) for (APlayerState* Player : State->PlayerArray)
		if (const AArenaDuelPlayerState* Duel = Cast<AArenaDuelPlayerState>(Player); Duel && !Duel->IsInactive() && Duel->GetDuelSlot() < 2) Players[Duel->GetDuelSlot()] = Duel;
	for (int32 SideIndex = 0; SideIndex < 2; ++SideIndex)
	{
		FArenaDuelSelectionPanel& Panel = Panels[SideIndex];
		const AArenaDuelPlayerState* Player = Players[SideIndex];
		const bool bLocal = Player && Player == Local;
		const bool bReady = Player && Player->IsCharacterReady();
		Panel.bPresent = Player != nullptr;
		Panel.DisplayArchetype = Player ? Player->GetCharacterArchetype() : EArenaDuelCharacterArchetype::Shadow;
		const FArenaDuelCharacterPresentation& Data = GetPresentation(Panel.DisplayArchetype);
		UpdateText(Panel.Name, Player ? Data.Name : TEXT("AWAITING OPPONENT"));
		UpdateText(Panel.Role, Player ? Data.Role : TEXT("JOIN THE DUEL"));
		UpdateText(Panel.Description, Player ? Data.Description : TEXT("This side becomes available when the second player connects."));
		UpdateText(Panel.PrimaryName, Data.PrimaryName);
		UpdateText(Panel.PrimaryDescription, Data.PrimaryDescription);
		UpdateText(Panel.SecondaryName, Data.SecondaryName);
		UpdateText(Panel.SecondaryDescription, Data.SecondaryDescription);
		UpdateText(Panel.Identity, bLocal ? TEXT("YOU") : Player ? TEXT("OPPONENT") : TEXT("WAITING FOR PLAYER..."));
		UpdateText(Panel.ReadyText, bCountdown ? TEXT("LOCKED / MATCH STARTING") : bReady ? (bLocal ? TEXT("READY / CLICK TO CANCEL") : TEXT("READY")) : bLocal ? TEXT("READY") : Player ? TEXT("SELECTING...") : TEXT("WAITING FOR PLAYER..."));
		Panel.ReadyButton->SetIsEnabled(bLocal && !bCountdown);
		Panel.Root->SetRenderOpacity(bCountdown ? 0.73f : Player ? 1.0f : 0.60f);
		for (int32 Index = 0; Index < Panel.RosterButtons.Num(); ++Index)
		{
			Panel.RosterButtons[Index]->SetIsEnabled(bLocal && !bReady && !bCountdown);
			const bool bSelected = static_cast<int32>(Panel.DisplayArchetype) == Index;
			Panel.RosterButtons[Index]->SetBackgroundColor(bSelected ? Accent(SideIndex) : FLinearColor(0.24f, 0.31f, 0.40f));
		}
		for (int32 Index = 0; Index < 4; ++Index) if (!FMath::IsNearlyEqual(Panel.Ratings[Index]->GetPercent(), Data.Ratings[Index])) Panel.Ratings[Index]->SetPercent(Data.Ratings[Index]);
	}
	if (bCountdown)
	{
		AutoReadyStatus->SetVisibility(ESlateVisibility::Collapsed);
		AutoReadyBackdrop->SetVisibility(ESlateVisibility::Collapsed);
		AutoReadyAccent->SetVisibility(ESlateVisibility::Collapsed);
		const float Remaining = State->GetCountdownEndServerTime() - State->GetServerWorldTimeSeconds();
		UpdateText(CenterLabel, Remaining > 0.35f ? FString::FromInt(FMath::Clamp(FMath::CeilToInt(Remaining - 0.35f), 1, 3)) : TEXT("FIGHT"));
		UpdateText(CenterStatus, TEXT("MATCH STARTING"));
	}
	else
	{
		const float AutoReadyEnd = State ? State->GetCharacterAutoReadyEndServerTime() : 0.0f;
		const bool bShowAutoReady = Players[0] && Players[1] && AutoReadyEnd > 0.0f;
		const ESlateVisibility AutoReadyVisibility = bShowAutoReady ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;
		AutoReadyStatus->SetVisibility(AutoReadyVisibility);
		AutoReadyBackdrop->SetVisibility(AutoReadyVisibility);
		AutoReadyAccent->SetVisibility(AutoReadyVisibility);
		if (bShowAutoReady)
		{
			const int32 Seconds = FMath::Clamp(FMath::CeilToInt(AutoReadyEnd - State->GetServerWorldTimeSeconds()), 0, 12);
			UpdateText(AutoReadyStatus, FString::Printf(TEXT("AUTO READY IN %02d SEC"), Seconds));
		}
		UpdateText(CenterLabel, TEXT("VS"));
		UpdateText(CenterStatus, !Players[0] || !Players[1] ? TEXT("WAITING FOR OPPONENT") : Players[0]->IsCharacterReady() ? TEXT("PLAYER 1 READY / WAITING FOR PLAYER 2") : Players[1]->IsCharacterReady() ? TEXT("PLAYER 2 READY / WAITING FOR PLAYER 1") : TEXT("WAITING FOR BOTH PLAYERS"));
	}
	UpdateText(Matchup, FString::Printf(TEXT("%s  >  VS  <  %s"), Players[0] ? *Players[0]->GetCharacterArchetypeDisplayName().ToString() : TEXT("PLAYER 1"), Players[1] ? *Players[1]->GetCharacterArchetypeDisplayName().ToString() : TEXT("PLAYER 2")));
	UpdateText(Connection, FString::Printf(TEXT("%s\n%.0f MS"), GetWorld() && GetWorld()->GetNetMode() == NM_Client ? TEXT("CLIENT") : TEXT("LISTEN / LOCAL SERVER"), Local ? Local->GetPingInMilliseconds() : 0.0f));
}

void UArenaDuelCharacterSelectWidget::Select(EArenaDuelCharacterArchetype Archetype)
{
	if (AArenaDuelPlayerController* Controller = Cast<AArenaDuelPlayerController>(GetOwningPlayer())) Controller->RequestCharacterSelection(Archetype);
}
void UArenaDuelCharacterSelectWidget::SelectShadow() { Select(EArenaDuelCharacterArchetype::Shadow); }
void UArenaDuelCharacterSelectWidget::SelectWarden() { Select(EArenaDuelCharacterArchetype::Warden); }
void UArenaDuelCharacterSelectWidget::SelectRift() { Select(EArenaDuelCharacterArchetype::Rift); }
void UArenaDuelCharacterSelectWidget::ToggleReady()
{
	if (AArenaDuelPlayerController* Controller = Cast<AArenaDuelPlayerController>(GetOwningPlayer())) Controller->ToggleCharacterReady();
}

void UArenaDuelCharacterSelectWidget::OpenSettings()
{
	if (AArenaDuelPlayerController* Controller = Cast<AArenaDuelPlayerController>(GetOwningPlayer())) Controller->OpenPlayerMenu(true);
}

FReply UArenaDuelCharacterSelectWidget::NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
	const FKey Key = Event.GetKey();
	const AArenaDuelPlayerState* Local = GetOwningPlayer() ? GetOwningPlayer()->GetPlayerState<AArenaDuelPlayerState>() : nullptr;
	if (Key == EKeys::Enter) { if (!Event.IsRepeat()) ToggleReady(); return FReply::Handled(); }
	if (Key == EKeys::Escape) { if (Local && Local->IsCharacterReady() && !Event.IsRepeat()) ToggleReady(); return FReply::Handled(); }
	if (Key == EKeys::A || Key == EKeys::D || Key == EKeys::Left || Key == EKeys::Right)
	{
		if (Local && !Local->IsCharacterReady() && !Event.IsRepeat()) Select(CycleArchetype(Local->GetCharacterArchetype(), Key == EKeys::A || Key == EKeys::Left ? -1 : 1));
		return FReply::Handled();
	}
	return Super::NativeOnPreviewKeyDown(Geometry, Event);
}

int32 UArenaDuelCharacterSelectWidget::NativePaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& CullingRect, FSlateWindowElementList& Elements, int32 LayerId, const FWidgetStyle& Style, bool bEnabled) const
{
	// Original, replaceable development portrait line art. No gameplay or image assets baked into the UI.
	const float Scale = FMath::Min(Geometry.GetLocalSize().X / 1920.0f, Geometry.GetLocalSize().Y / 1080.0f);
	const FVector2D Offset = (Geometry.GetLocalSize() - FVector2D(1920, 1080) * Scale) * 0.5f;
	auto Lines = [&](TArray<FVector2D> Points, FLinearColor Color, float Width = 1.0f)
	{
		for (FVector2D& Point : Points) Point = Offset + Point * Scale;
		FSlateDrawElement::MakeLines(Elements, LayerId + 20, Geometry.ToPaintGeometry(), Points, ESlateDrawEffect::None, Color * Style.GetColorAndOpacityTint(), true, Width * Scale);
	};
	auto Polygon = [&](const TArray<FVector2D>& Points, FLinearColor Color)
	{
		TArray<FSlateVertex> Vertices;
		TArray<SlateIndex> Indices;
		for (const FVector2D& Point : Points)
			Vertices.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(Geometry.GetAccumulatedRenderTransform(), FVector2f(Offset + Point * Scale), FVector2f(0.5f, 0.5f), (Color * Style.GetColorAndOpacityTint()).ToFColor(true)));
		for (int32 I = 1; I + 1 < Points.Num(); ++I) { Indices.Add(0); Indices.Add(I); Indices.Add(I + 1); }
		FSlateDrawElement::MakeCustomVerts(Elements, LayerId + 19, FSlateApplication::Get().GetRenderer()->GetResourceHandle(*FCoreStyle::Get().GetBrush("WhiteBrush")), Vertices, Indices, nullptr, 0, 0);
	};
	using namespace CharacterSelectStyle;
	// Static procedural arena depth in the open central region.
	for (int32 Index = 0; Index < 7; ++Index)
	{
		const float Inset = Index * 27.0f;
		Lines({{740 + Inset, 860}, {740 + Inset, 300 + Inset}, {960, 180 + Inset}, {1180 - Inset, 300 + Inset}, {1180 - Inset, 860}}, CharacterSelectViolet.CopyWithNewOpacity(0.07f + Index * 0.007f));
		Lines({{730, 940 - Index * 22.0f}, {960, 790 - Index * 7.0f}, {1190, 940 - Index * 22.0f}}, CharacterSelectCyan.CopyWithNewOpacity(0.055f));
	}
	Lines({{715, 48}, {672, 48}, {640, 28}, {60, 28}, {30, 58}, {30, 1008}, {62, 1047}, {735, 1047}}, CharacterSelectCyan.CopyWithNewOpacity(0.45f));
	Lines({{1205, 48}, {1248, 48}, {1280, 28}, {1860, 28}, {1890, 58}, {1890, 1008}, {1858, 1047}, {1185, 1047}}, CharacterSelectViolet.CopyWithNewOpacity(0.45f));
	Lines({{797, 545}, {826, 526}, {865, 526}}, CharacterSelectViolet.CopyWithNewOpacity(0.8f), 2);
	Lines({{1123, 545}, {1094, 526}, {1055, 526}}, CharacterSelectCyan.CopyWithNewOpacity(0.8f), 2);
	for (int32 SideIndex = 0; SideIndex < Panels.Num(); ++SideIndex)
	{
		const float X = SideIndex == 0 ? 90 : 1200;
		const float Y = 155;
		const FLinearColor Color = Accent(SideIndex);
		Lines({{X, Y + 42}, {X, Y + 18}, {X + 18, Y}, {X + 595, Y}, {X + 630, Y + 35}, {X + 630, Y + 751}, {X + 596, Y + 785}, {X + 23, Y + 785}, {X, Y + 762}, {X, Y + 42}}, Color.CopyWithNewOpacity(0.65f), 1.5f);
		Lines({{X + 18, Y + 349}, {X + 18, Y + 65}, {X + 601, Y + 65}, {X + 612, Y + 76}, {X + 612, Y + 350}}, Color.CopyWithNewOpacity(0.40f));
		const float CX = X + 310;
		const float CY = Y + 196;
		const bool bWarden = Panels[SideIndex].DisplayArchetype == EArenaDuelCharacterArchetype::Warden;
		const bool bRift = Panels[SideIndex].DisplayArchetype == EArenaDuelCharacterArchetype::Rift;
		const FLinearColor IdentityColor = bWarden ? CharacterSelectCyan : CharacterSelectViolet;
		if (bRift)
		{
			// Lean asymmetric spatial-mage mask, distinct from the assassin hood and plated Warden.
			Polygon({{CX - 98, CY + 126}, {CX - 72, CY + 28}, {CX - 42, CY - 98}, {CX + 32, CY - 78}, {CX + 54, CY + 18}, {CX + 105, CY + 126}}, FLinearColor(0.06f, 0.035f, 0.13f));
			Lines({{CX - 98, CY + 126}, {CX - 72, CY + 28}, {CX - 42, CY - 98}, {CX + 32, CY - 78}, {CX + 54, CY + 18}, {CX + 105, CY + 126}}, CharacterSelectViolet, 3);
			Polygon({{CX - 28, CY - 56}, {CX + 24, CY - 44}, {CX + 35, CY - 7}, {CX + 5, CY + 31}, {CX - 32, CY + 3}}, FLinearColor(0.015f, 0.04f, 0.08f));
			Lines({{CX - 28, CY - 56}, {CX + 7, CY - 29}, {CX - 7, CY - 5}, {CX + 5, CY + 31}}, CharacterSelectCyan, 3);
			Lines({{CX + 24, CY - 44}, {CX + 35, CY - 7}, {CX + 5, CY + 31}, {CX - 32, CY + 3}}, CharacterSelectViolet, 2);
			for (int32 I = 0; I < 3; ++I)
				Lines({{CX - 165 - I * 15, CY + 50}, {CX - 119 - I * 13, CY - 60}, {CX + 101 + I * 15, CY - 65}, {CX + 143 + I * 17, CY + 80}}, (I == 1 ? CharacterSelectCyan : CharacterSelectViolet).CopyWithNewOpacity(0.5f), 2);
			Lines({{CX - 43, CY + 51}, {CX + 21, CY + 72}, {CX - 16, CY + 102}, {CX + 38, CY + 124}}, CharacterSelectCyan, 2);
		}
		else
		{
		Polygon({{CX - 148, CY + 127}, {CX - 112, CY + 54}, {CX - 46, CY + 22}, {CX + 45, CY + 22}, {CX + 122, CY + 57}, {CX + 166, CY + 127}}, FLinearColor(0.04f, 0.065f, 0.11f));
		Polygon({{CX - 67, CY + 17}, {CX - 66, CY - 50}, {CX - 37, CY - 101}, {CX + 26, CY - 111}, {CX + 64, CY - 60}, {CX + 74, CY + 22}, {CX + 5, CY + 49}}, FLinearColor(0.05f, 0.055f, 0.095f));
		Polygon({{CX - 55, CY - 50}, {CX - 23, CY - 72}, {CX + 33, CY - 63}, {CX + 50, CY - 40}, {CX + 27, CY + 14}, {CX, CY + 31}, {CX - 30, CY + 10}}, FLinearColor(0.008f, 0.012f, 0.025f));
		// Hood or plated helm, shoulders, and emblem form a large character silhouette.
		Lines({{CX - 155, CY + 126}, {CX - 133, CY + 55}, {CX - 75, CY + 26}, {CX - 67, CY - 53}, {CX - 37, CY - 101}, {CX + 26, CY - 111}, {CX + 64, CY - 60}, {CX + 75, CY + 26}, {CX + 135, CY + 52}, {CX + 170, CY + 126}}, IdentityColor.CopyWithNewOpacity(0.85f), 3);
		Lines({{CX - 58, CY - 51}, {CX - 27, CY - 73}, {CX + 35, CY - 67}, {CX + 54, CY - 41}, {CX + 32, CY + 12}, {CX, CY + 33}, {CX - 35, CY + 11}, {CX - 58, CY - 51}}, IdentityColor, 2);
		Lines({{CX - 20, CY - 20}, {CX - 4, CY - 11}, {CX + 6, CY - 44}, {CX + 15, CY - 14}}, IdentityColor, 4);
		for (int32 I = 0; I < 4; ++I)
		{
			const float D = 14 * I;
			Lines({{CX - 122 - D, CY + 93}, {CX - 80 - D, CY + 51}, {CX - 38, CY + 68 + D}, {CX, CY + 100 + D}, {CX + 48, CY + 68 + D}, {CX + 97 + D, CY + 51}, {CX + 146 + D, CY + 113}}, IdentityColor.CopyWithNewOpacity(0.28f + I * 0.05f));
		}
		if (bWarden)
		{
			Polygon({{CX + 92, CY + 21}, {CX + 188, CY + 4}, {CX + 188, CY + 100}, {CX + 141, CY + 139}, {CX + 92, CY + 105}}, FLinearColor(0.03f, 0.14f, 0.22f));
			Lines({{CX + 92, CY + 21}, {CX + 188, CY + 4}, {CX + 188, CY + 100}, {CX + 141, CY + 139}, {CX + 92, CY + 105}, {CX + 92, CY + 21}}, CharacterSelectCyan, 4);
			Lines({{CX + 141, CY + 30}, {CX + 120, CY + 68}, {CX + 146, CY + 55}, {CX + 139, CY + 108}}, CharacterSelectCyan, 3);
		}
		else
		{
			Lines({{CX - 205, CY + 67}, {CX - 154, CY + 29}, {CX - 86, CY + 11}, {CX - 59, CY + 24}}, CharacterSelectViolet.CopyWithNewOpacity(0.75f), 3);
			Lines({{CX - 186, CY + 113}, {CX - 147, CY + 70}, {CX - 108, CY + 64}}, CharacterSelectViolet, 2);
		}
		}
		// Native original ability glyphs, including hook and paired rift outlines.
		for (int32 Ability = 0; Ability < 2; ++Ability)
		{
			const float IX = X + 48 + Ability * 294;
			const float IY = Y + 593;
			if (bRift && Ability == 0) Lines({{IX - 18, IY + 18}, {IX + 10, IY - 14}, {IX + 18, IY - 5}, {IX + 9, IY + 3}, {IX + 3, IY - 4}}, CharacterSelectCyan, 2);
			else if (bRift) { Lines({{IX - 17, IY + 15}, {IX - 20, IY - 11}, {IX - 8, IY - 18}, {IX - 3, IY + 15}}, CharacterSelectViolet, 2); Lines({{IX + 3, IY + 15}, {IX + 8, IY - 18}, {IX + 20, IY - 11}, {IX + 17, IY + 15}}, CharacterSelectCyan, 2); }
			else if (Ability == 0 && bWarden) Lines({{IX - 15, IY - 12}, {IX + 15, IY - 12}, {IX + 12, IY + 8}, {IX, IY + 19}, {IX - 12, IY + 8}, {IX - 15, IY - 12}}, Color, 2);
			else if (Ability == 0) { Lines({{IX - 19, IY + 10}, {IX + 13, IY - 12}, {IX + 9, IY + 1}}, Color, 2); Lines({{IX - 19, IY + 18}, {IX - 2, IY + 6}}, Color, 2); }
			else if (bWarden) Lines({{IX - 15, IY + 16}, {IX + 9, IY - 14}, {IX - 3, IY - 10}, {IX + 9, IY - 14}, {IX + 11, IY}}, Color, 2);
			else for (int32 I = -1; I <= 1; ++I) Lines({{IX + I * 11, IY + 15}, {IX + I * 11 + 3, IY - 15}}, Color, 2);
		}
		// Three implemented kits fit the existing strip without widening either panel.
		// Roster icons are now real widget children with reserved layout space.
		for (int32 Character = 0; Character < 0; ++Character)
		{
			const float RX = X + 70 + Character * 167;
			const bool bSelected = static_cast<int32>(Panels[SideIndex].DisplayArchetype) == Character;
			Lines({{RX, Y + 656}, {RX + 152, Y + 656}, {RX + 152, Y + 701}, {RX, Y + 701}, {RX, Y + 656}}, Color.CopyWithNewOpacity(bSelected ? 0.95f : 0.22f), bSelected ? 2.0f : 1.0f);
			const float TX = X + 88 + Character * 167;
			const float TY = Y + 678;
			const FLinearColor Tint = Character == 1 ? CharacterSelectCyan : CharacterSelectViolet;
			if (Character == 2)
			{
				Polygon({{TX - 12, TY + 13}, {TX - 8, TY - 18}, {TX + 8, TY - 12}, {TX + 13, TY + 13}}, FLinearColor(0.055f, 0.025f, 0.12f));
				Lines({{TX - 12, TY + 13}, {TX - 8, TY - 18}, {TX + 8, TY - 12}, {TX + 13, TY + 13}}, CharacterSelectViolet, 1.5f);
				Lines({{TX - 6, TY - 7}, {TX + 4, TY - 2}, {TX - 1, TY + 5}}, CharacterSelectCyan, 2);
				continue;
			}
			Polygon({{TX - 15, TY + 12}, {TX - 11, TY - 11}, {TX, TY - 19}, {TX + 12, TY - 10}, {TX + 17, TY + 12}}, FLinearColor(0.04f, 0.05f, 0.10f));
			Lines({{TX - 15, TY + 12}, {TX - 11, TY - 11}, {TX, TY - 19}, {TX + 12, TY - 10}, {TX + 17, TY + 12}}, Tint, 1.5f);
			Lines({{TX - 4, TY - 3}, {TX + 3, TY + 1}, {TX + 5, TY - 6}}, Tint, 2);
		}
	}
	// Native UserWidget draws its root once; decorative lines sit above the glass but outside text bounds.
	return FMath::Max(LayerId + 20, Super::NativePaint(Args, Geometry, CullingRect, Elements, LayerId, Style, bEnabled));
}
