#include "ArenaDuelMainMenuWidget.h"
#include "../Player/ArenaDuelPlayerController.h"
#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateColorBrush.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/EditableTextBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Engine.h"
#include "Engine/NetDriver.h"
#include "Engine/World.h"
#include "IPAddress.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "SocketSubsystem.h"

namespace
{
	const FLinearColor MenuCyan(0.35f, 0.86f, 1.0f, 1.0f);
	const FLinearColor MenuText(0.86f, 0.9f, 0.96f, 1.0f);
	const FLinearColor MenuMuted(0.5f, 0.58f, 0.68f, 1.0f);
	const TCHAR* DuelMap = TEXT("/Game/ArenaDuel/Maps/L_ArenaDistrict");
	const TCHAR* SurvivalMap = TEXT("/Game/ArenaDuel/Maps/L_ZombieArena");
	constexpr int32 GamePort = 7777;

	UTextBlock* MakeText(UWidgetTree* Tree, const FString& Text, int32 Size, const FLinearColor& Color)
	{
		UTextBlock* Block = Tree->ConstructWidget<UTextBlock>();
		Block->SetText(FText::FromString(Text));
		FSlateFontInfo Font = Block->GetFont();
		Font.Size = Size;
		Block->SetFont(Font);
		Block->SetColorAndOpacity(FSlateColor(Color));
		return Block;
	}

	void Place(UCanvasPanel* Panel, UWidget* Widget, const FAnchors& Anchors, const FMargin& Offsets, const FVector2D& Alignment = FVector2D::ZeroVector, bool bAutoSize = false)
	{
		UCanvasPanelSlot* Slot = Panel->AddChildToCanvas(Widget);
		Slot->SetAnchors(Anchors);
		Slot->SetOffsets(Offsets);
		Slot->SetAlignment(Alignment);
		Slot->SetAutoSize(bAutoSize);
	}
}

FString UArenaDuelMainMenuWidget::GetLocalJoinAddress(const UWorld* World)
{
	FString Address = TEXT("127.0.0.1");
	if (ISocketSubsystem* Sockets = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM))
	{
		bool bCanBind = false;
		const TSharedRef<FInternetAddr> Local = Sockets->GetLocalHostAddr(*GLog, bCanBind);
		if (Local->IsValid()) Address = Local->ToString(false);
	}
	const int32 Port = World && World->URL.Port > 0 ? World->URL.Port : GamePort;
	return FString::Printf(TEXT("%s:%d"), *Address, Port);
}

FString UArenaDuelMainMenuWidget::NormalizeJoinAddress(const FString& Text)
{
	const FString Trimmed = Text.TrimStartAndEnd();
	// Host names and addresses only: no spaces, no path, no options that could change what is loaded.
	if (Trimmed.IsEmpty() || Trimmed.Len() > 253) return FString();
	for (const TCHAR Character : Trimmed)
	{
		if (!(FChar::IsAlnum(Character) || Character == TEXT('.') || Character == TEXT('-') || Character == TEXT(':') || Character == TEXT('[') || Character == TEXT(']'))) return FString();
	}
	// A bracketed or plain IPv6 address has several colons; only a single colon separates a port.
	int32 Colons = 0;
	for (const TCHAR Character : Trimmed) if (Character == TEXT(':')) ++Colons;
	if (Colons == 0) return FString::Printf(TEXT("%s:%d"), *Trimmed, GamePort);
	if (Colons == 1)
	{
		FString Host, Port;
		Trimmed.Split(TEXT(":"), &Host, &Port);
		const int32 PortNumber = FCString::Atoi(*Port);
		return Host.IsEmpty() || !Port.IsNumeric() || PortNumber < 1 || PortNumber > 65535 ? FString() : Trimmed;
	}
	return Trimmed;
}

namespace
{
	FString PendingFailureMessage;
	bool bFailureReportsInstalled = false;
}

void UArenaDuelMainMenuWidget::InstallFailureReports()
{
	if (bFailureReportsInstalled || !GEngine) return;
	bFailureReportsInstalled = true;
	GEngine->OnNetworkFailure().AddLambda([](UWorld*, UNetDriver*, ENetworkFailure::Type Failure, const FString& Error)
	{
		const TCHAR* Reason = Failure == ENetworkFailure::ConnectionTimeout || Failure == ENetworkFailure::PendingConnectionFailure ? TEXT("No lobby answered at that address.")
			: Failure == ENetworkFailure::ConnectionLost || Failure == ENetworkFailure::FailureReceived ? TEXT("The connection to the host was lost.")
			: Failure == ENetworkFailure::OutdatedClient || Failure == ENetworkFailure::OutdatedServer ? TEXT("Host and client run different versions of the game.")
			: TEXT("The connection failed.");
		PendingFailureMessage = Error.IsEmpty() ? FString(Reason) : FString::Printf(TEXT("%s (%s)"), Reason, *Error);
	});
	GEngine->OnTravelFailure().AddLambda([](UWorld*, ETravelFailure::Type, const FString& Error)
	{
		if (PendingFailureMessage.IsEmpty()) PendingFailureMessage = FString::Printf(TEXT("Could not open that game. %s"), *Error);
	});
}

TSharedRef<SWidget> UArenaDuelMainMenuWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget) BuildTree();
	return Super::RebuildWidget();
}

void UArenaDuelMainMenuWidget::BuildTree()
{
	Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
	WidgetTree->RootWidget = Root;

	UBorder* Background = WidgetTree->ConstructWidget<UBorder>();
	Background->SetBrushColor(FLinearColor(0.006f, 0.008f, 0.014f, 1.0f));
	Place(Root, Background, FAnchors(0.0f, 0.0f, 1.0f, 1.0f), FMargin(0.0f));

	// A dim panel behind the column and a thin accent line give the page its shape.
	UBorder* Panel = WidgetTree->ConstructWidget<UBorder>();
	Panel->SetBrushColor(FLinearColor(0.014f, 0.022f, 0.04f, 0.96f));
	Place(Root, Panel, FAnchors(0.0f, 0.0f, 0.0f, 1.0f), FMargin(0.0f, 0.0f, 620.0f, 0.0f));
	UBorder* Accent = WidgetTree->ConstructWidget<UBorder>();
	Accent->SetBrushColor(MenuCyan);
	Place(Root, Accent, FAnchors(0.0f, 0.0f, 0.0f, 1.0f), FMargin(620.0f, 0.0f, 3.0f, 0.0f));

	Place(Root, MakeText(WidgetTree, TEXT("ARENADUEL"), 54, MenuCyan), FAnchors(0.0f, 0.0f), FMargin(90.0f, 90.0f, 0.0f, 0.0f), FVector2D::ZeroVector, true);
	PageTitle = MakeText(WidgetTree, TEXT(""), 15, MenuMuted);
	Place(Root, PageTitle, FAnchors(0.0f, 0.0f), FMargin(94.0f, 172.0f, 0.0f, 0.0f), FVector2D::ZeroVector, true);

	Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Column"));
	Place(Root, Column, FAnchors(0.0f, 0.0f), FMargin(90.0f, 250.0f, 440.0f, 560.0f));

	Status = MakeText(WidgetTree, TEXT(""), 14, FLinearColor(1.0f, 0.62f, 0.4f, 1.0f));
	Status->SetAutoWrapText(true);
	Place(Root, Status, FAnchors(0.0f, 1.0f), FMargin(90.0f, -130.0f, 440.0f, 70.0f));
	Place(Root, MakeText(WidgetTree, TEXT("ARROWS / TAB  MOVE     ENTER  SELECT     ESC  BACK"), 11, MenuMuted), FAnchors(0.0f, 1.0f), FMargin(90.0f, -46.0f, 0.0f, 0.0f), FVector2D::ZeroVector, true);

	ShowPage(EPage::Main);
}

UButton* UArenaDuelMainMenuWidget::AddButton(const TCHAR* Label, bool bPrimary)
{
	UButton* Button = WidgetTree->ConstructWidget<UButton>();
	FButtonStyle Style;
	const FLinearColor Base = bPrimary ? FLinearColor(0.03f, 0.11f, 0.16f, 1.0f) : FLinearColor(0.02f, 0.035f, 0.06f, 1.0f);
	Style.SetNormal(FSlateColorBrush(Base));
	Style.SetHovered(FSlateColorBrush(FLinearColor(0.06f, 0.24f, 0.33f, 1.0f)));
	Style.SetPressed(FSlateColorBrush(FLinearColor(0.1f, 0.38f, 0.5f, 1.0f)));
	Style.SetNormalPadding(FMargin(22.0f, 14.0f));
	Style.SetPressedPadding(FMargin(22.0f, 15.0f, 22.0f, 13.0f));
	Button->SetStyle(Style);
	Button->SetContent(MakeText(WidgetTree, Label, 20, bPrimary ? MenuCyan : MenuText));
	UVerticalBoxSlot* Slot = Column->AddChildToVerticalBox(Button);
	Slot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 12.0f));
	Slot->SetHorizontalAlignment(HAlign_Fill);
	if (!FirstButton) FirstButton = Button;
	return Button;
}

void UArenaDuelMainMenuWidget::ShowPage(EPage Page)
{
	CurrentPage = Page;
	Column->ClearChildren();
	FirstButton = nullptr;
	AddressBox = nullptr;
	SetStatus(FString());
	switch (Page)
	{
	case EPage::Main:
		PageTitle->SetText(FText::FromString(TEXT("MAIN MENU")));
		AddButton(TEXT("PLAY"), true)->OnClicked.AddDynamic(this, &ThisClass::OnPlay);
		AddButton(TEXT("SETTINGS"), false)->OnClicked.AddDynamic(this, &ThisClass::OnSettings);
		AddButton(TEXT("QUIT GAME"), false)->OnClicked.AddDynamic(this, &ThisClass::OnQuit);
		break;
	case EPage::Play:
		PageTitle->SetText(FText::FromString(TEXT("CHOOSE A MODE")));
		AddButton(TEXT("1 VS 1  -  CREATE LOBBY"), true)->OnClicked.AddDynamic(this, &ThisClass::OnHost);
		AddButton(TEXT("1 VS 1  -  JOIN A FRIEND"), false)->OnClicked.AddDynamic(this, &ThisClass::OnJoinPage);
		AddButton(TEXT("ZOMBIE SURVIVAL"), true)->OnClicked.AddDynamic(this, &ThisClass::OnZombie);
		AddButton(TEXT("BACK"), false)->OnClicked.AddDynamic(this, &ThisClass::OnBack);
		break;
	case EPage::Join:
	{
		PageTitle->SetText(FText::FromString(TEXT("JOIN A FRIEND  -  ENTER THE ADDRESS THE HOST SEES IN THE LOBBY")));
		AddressBox = WidgetTree->ConstructWidget<UEditableTextBox>();
		AddressBox->SetHintText(FText::FromString(TEXT("192.168.0.12:7777")));
		UVerticalBoxSlot* Slot = Column->AddChildToVerticalBox(AddressBox);
		Slot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 16.0f));
		AddButton(TEXT("CONNECT"), true)->OnClicked.AddDynamic(this, &ThisClass::OnJoinConfirm);
		AddButton(TEXT("BACK"), false)->OnClicked.AddDynamic(this, &ThisClass::OnBack);
		break;
	}
	}
	PageFade = 0.0f;
	bFocusPending = true;
}

void UArenaDuelMainMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();
	TakeMenuInput();
	// Back in the menu after a failed join or a lost host: say why, once.
	if (!PendingFailureMessage.IsEmpty())
	{
		SetStatus(PendingFailureMessage);
		PendingFailureMessage.Reset();
	}
}

void UArenaDuelMainMenuWidget::TakeMenuInput()
{
	if (APlayerController* Controller = GetOwningPlayer())
	{
		FInputModeGameAndUI Mode;
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		Mode.SetHideCursorDuringCapture(false);
		Controller->SetInputMode(Mode);
		Controller->SetShowMouseCursor(true);
	}
	bFocusPending = true;
}

void UArenaDuelMainMenuWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	// A new page slides in from the left and fades up.
	PageFade = FMath::Min(1.0f, PageFade + InDeltaTime * 6.0f);
	if (Column)
	{
		Column->SetRenderOpacity(PageFade);
		Column->SetRenderTranslation(FVector2D(-28.0f * (1.0f - PageFade) * (1.0f - PageFade), 0.0f));
	}
	const AArenaDuelPlayerController* Controller = Cast<AArenaDuelPlayerController>(GetOwningPlayer());
	const bool bSettingsOpen = Controller && Controller->IsPlayerMenuOpen();
	SetVisibility(bSettingsOpen ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Visible);
	// The settings menu hands the mouse back to the game when it closes; in the menu it belongs here.
	if (!bSettingsOpen && GetOwningPlayer() && !GetOwningPlayer()->ShouldShowMouseCursor()) TakeMenuInput();
	if (bFocusPending && !bSettingsOpen)
	{
		bFocusPending = false;
		if (AddressBox) AddressBox->SetKeyboardFocus();
		else if (FirstButton) FirstButton->SetKeyboardFocus();
	}
}

FReply UArenaDuelMainMenuWidget::NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape && CurrentPage != EPage::Main)
	{
		OnBack();
		return FReply::Handled();
	}
	if (InKeyEvent.GetKey() == EKeys::Enter && CurrentPage == EPage::Join && AddressBox && AddressBox->HasKeyboardFocus())
	{
		OnJoinConfirm();
		return FReply::Handled();
	}
	return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
}

void UArenaDuelMainMenuWidget::SetStatus(const FString& Text)
{
	if (Status) Status->SetText(FText::FromString(Text));
}

void UArenaDuelMainMenuWidget::OnPlay() { ShowPage(EPage::Play); }
void UArenaDuelMainMenuWidget::OnJoinPage() { ShowPage(EPage::Join); }
void UArenaDuelMainMenuWidget::OnBack() { ShowPage(CurrentPage == EPage::Join ? EPage::Play : EPage::Main); }

void UArenaDuelMainMenuWidget::OnHost()
{
	// A listen server on the duel map: the lobby there shows both players, their ready state and the address to join.
	UGameplayStatics::OpenLevel(this, FName(DuelMap), true, TEXT("listen"));
}

void UArenaDuelMainMenuWidget::OnJoinConfirm()
{
	const FString Address = NormalizeJoinAddress(AddressBox ? AddressBox->GetText().ToString() : FString());
	if (Address.IsEmpty())
	{
		SetStatus(TEXT("That is not an address. Enter what the host sees in the lobby, for example 192.168.0.12:7777."));
		return;
	}
	if (APlayerController* Controller = GetOwningPlayer())
	{
		SetStatus(FString::Printf(TEXT("Connecting to %s ..."), *Address));
		Controller->ClientTravel(Address, TRAVEL_Absolute);
	}
}

void UArenaDuelMainMenuWidget::OnZombie()
{
	UGameplayStatics::OpenLevel(this, FName(SurvivalMap), true);
}

void UArenaDuelMainMenuWidget::OnSettings()
{
	if (AArenaDuelPlayerController* Controller = Cast<AArenaDuelPlayerController>(GetOwningPlayer())) Controller->OpenPlayerMenu(true);
}

void UArenaDuelMainMenuWidget::OnQuit()
{
	UKismetSystemLibrary::QuitGame(this, GetOwningPlayer(), EQuitPreference::Quit, false);
}
