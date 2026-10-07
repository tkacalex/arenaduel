#include "ArenaDuelPlayerMenuWidget.h"
#include "../Player/ArenaDuelPlayerController.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ComboBoxString.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "GameFramework/GameUserSettings.h"
#include "Framework/Application/SlateApplication.h"
#include "InputCoreTypes.h"
#include "RHI.h"
#include "Styling/CoreStyle.h"

namespace
{
	const FLinearColor MenuCyan(0.22f, 0.82f, 0.98f);
	const FLinearColor MenuViolet(0.65f, 0.38f, 0.95f);
	const FLinearColor TextColor(0.91f, 0.95f, 0.99f);
	UCanvasPanelSlot* Put(UCanvasPanel* Parent, UWidget* Child, float X, float Y, float W, float H)
	{
		UCanvasPanelSlot* Slot = Parent->AddChildToCanvas(Child); Slot->SetPosition({X,Y}); Slot->SetSize({W,H}); return Slot;
	}
	void PutFullScreen(UCanvasPanel* Parent,UWidget* Child)
	{
		UCanvasPanelSlot* Slot=Parent->AddChildToCanvas(Child);Slot->SetAnchors(FAnchors(0,0,1,1));Slot->SetOffsets(FMargin(0));
	}
	void PutCentered(UCanvasPanel* Parent,UWidget* Child,float Width,float Height)
	{
		UCanvasPanelSlot* Slot=Parent->AddChildToCanvas(Child);Slot->SetAnchors(FAnchors(0.5f,0.5f));Slot->SetAlignment(FVector2D(0.5f,0.5f));Slot->SetPosition(FVector2D::ZeroVector);Slot->SetSize(FVector2D(Width,Height));
	}
	UTextBlock* Label(UWidgetTree* Tree, UCanvasPanel* Parent, const FString& Value, float X, float Y, float W, float H, int32 Size=20, FLinearColor Color=TextColor)
	{
		UTextBlock* T = Tree->ConstructWidget<UTextBlock>(); T->SetText(FText::FromString(Value)); T->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", Size)); T->SetColorAndOpacity(Color); T->SetAutoWrapText(true); T->SetVisibility(ESlateVisibility::HitTestInvisible); Put(Parent,T,X,Y,W,H); return T;
	}
	UButton* ActionButton(UWidgetTree* Tree, UCanvasPanel* Parent, const FString& Name, float X, float Y, float W, float H, FLinearColor Tint)
	{
		UButton* B=Tree->ConstructWidget<UButton>(); FButtonStyle Style=FCoreStyle::Get().GetWidgetStyle<FButtonStyle>("Button");
		Style.Normal.TintColor=FSlateColor(FLinearColor(0.025f,0.07f,0.12f,0.96f)); Style.Hovered.TintColor=FSlateColor(Tint.CopyWithNewOpacity(0.32f)); Style.Pressed.TintColor=FSlateColor(Tint.CopyWithNewOpacity(0.5f)); B->SetStyle(Style);
		UTextBlock* T=Tree->ConstructWidget<UTextBlock>(); T->SetText(FText::FromString(Name)); T->SetFont(FCoreStyle::GetDefaultFontStyle("Bold",19)); T->SetColorAndOpacity(Tint); T->SetJustification(ETextJustify::Center); B->AddChild(T); Put(Parent,B,X,Y,W,H); return B;
	}
}

bool UArenaDuelPlayerMenuWidget::Initialize()
{
	if (!Super::Initialize()) return false;
	SetIsFocusable(true);
	if (!WidgetTree) WidgetTree=NewObject<UWidgetTree>(this,TEXT("WidgetTree"),RF_Transient);
	BuildTree();
	return RootCanvas && PausePage && SettingsPage;
}

void UArenaDuelPlayerMenuWidget::Configure(bool bFromSelect)
{
	bFromCharacterSelect=bFromSelect;
	PullValues();
}

void UArenaDuelPlayerMenuWidget::BuildTree()
{
	if (!WidgetTree || RootCanvas) return;
	RootCanvas=WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(),TEXT("PlayerMenuRoot")); WidgetTree->RootWidget=RootCanvas;
	UBorder* Dim=WidgetTree->ConstructWidget<UBorder>(); Dim->SetBrushColor(FLinearColor(0.005f,0.012f,0.025f,0.82f)); Dim->SetVisibility(ESlateVisibility::HitTestInvisible); PutFullScreen(RootCanvas,Dim);
	PausePage=WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(),TEXT("PausePage")); PutCentered(RootCanvas,PausePage,620,560);
	SettingsPage=WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(),TEXT("SettingsPage")); PutCentered(RootCanvas,SettingsPage,1000,780);
	for (UCanvasPanel* Page : {PausePage.Get(),SettingsPage.Get()}) { UBorder* Panel=WidgetTree->ConstructWidget<UBorder>(); Panel->SetBrushColor(FLinearColor(0.015f,0.035f,0.075f,0.97f)); Panel->SetVisibility(ESlateVisibility::HitTestInvisible); Put(Page,Panel,0,0,Page==PausePage?620:1000,Page==PausePage?560:780); }
	Label(WidgetTree,PausePage,TEXT("ARENADUEL"),55,46,510,55,34,MenuCyan);
	Label(WidgetTree,PausePage,TEXT("PAUSED LOCALLY  |  THE MATCH CONTINUES"),55,104,510,32,14,MenuViolet);
	UButton* Resume=ActionButton(WidgetTree,PausePage,TEXT("RESUME"),105,220,410,62,MenuCyan); Resume->OnClicked.AddDynamic(this,&ThisClass::OnResumeClicked);
	UButton* OpenSettings=ActionButton(WidgetTree,PausePage,TEXT("SETTINGS"),105,304,410,62,MenuViolet); OpenSettings->OnClicked.AddDynamic(this,&ThisClass::OnSettingsClicked);
	Label(WidgetTree,PausePage,TEXT("ESC  RESUME"),105,475,410,30,14,FLinearColor(0.54f,0.64f,0.76f));
	Label(WidgetTree,SettingsPage,TEXT("PLAYER SETTINGS"),38,20,640,48,30,MenuCyan);
	Label(WidgetTree,SettingsPage,TEXT("LOCAL SETTINGS  /  APPLY TO THIS DEVICE"),40,68,720,25,13,MenuViolet);
	const TCHAR* Names[]={TEXT("MOUSE SENSITIVITY"),TEXT("ADS SENSITIVITY"),TEXT("FIELD OF VIEW"),TEXT("MASTER VOLUME")};
	TObjectPtr<USlider>* Sliders[]={&SensitivitySlider,&ADSSlider,&FOVSlider,&VolumeSlider};
	TObjectPtr<UTextBlock>* Labels[]={&SensitivityLabel,&ADSLabel,&FOVLabel,&VolumeLabel};
	for(int32 I=0;I<4;++I)
	{
		const float Y=132+I*88; Label(WidgetTree,SettingsPage,Names[I],45,Y,300,30,16,TextColor);
		*Labels[I]=Label(WidgetTree,SettingsPage,TEXT(""),800,Y,140,30,16,MenuCyan);
		*Sliders[I]=WidgetTree->ConstructWidget<USlider>(); (*Sliders[I])->SetStepSize(0.001f); Put(SettingsPage,*Sliders[I],365,Y+3,400,30);
	}
	const TCHAR* ChoiceNames[]={TEXT("WINDOW MODE"),TEXT("RESOLUTION"),TEXT("VSYNC"),TEXT("FRAME RATE LIMIT")};
	TObjectPtr<UComboBoxString>* Boxes[]={&WindowModeBox,&ResolutionBox,&VSyncBox,&FPSBox};
	for(int32 I=0;I<4;++I)
	{
		const float Y=500+I*48; Label(WidgetTree,SettingsPage,ChoiceNames[I],45,Y,300,31,15,TextColor);
		*Boxes[I]=WidgetTree->ConstructWidget<UComboBoxString>(); Put(SettingsPage,*Boxes[I],365,Y,400,34);
	}
	WindowModeBox->AddOption(TEXT("Fullscreen")); WindowModeBox->AddOption(TEXT("Borderless")); WindowModeBox->AddOption(TEXT("Windowed"));
	VSyncBox->AddOption(TEXT("Off")); VSyncBox->AddOption(TEXT("On"));
	for(int32 Cap:{60,120,144,165,240}) FPSBox->AddOption(FString::FromInt(Cap)); FPSBox->AddOption(TEXT("Unlimited"));
	TArray<FScreenResolutionRHI> ScreenModes; if(RHIGetAvailableResolutions(ScreenModes,false)) for(const auto& R:ScreenModes) { FIntPoint P(R.Width,R.Height); if(!AvailableResolutions.Contains(P)) { AvailableResolutions.Add(P); ResolutionBox->AddOption(FString::Printf(TEXT("%d x %d"),P.X,P.Y)); } }
	UButton* ApplyButton=ActionButton(WidgetTree,SettingsPage,TEXT("APPLY"),535,724,160,42,MenuCyan); ApplyButton->OnClicked.AddDynamic(this,&ThisClass::OnApplyClicked);
	UButton* BackButton=ActionButton(WidgetTree,SettingsPage,TEXT("BACK"),715,724,130,42,MenuViolet); BackButton->OnClicked.AddDynamic(this,&ThisClass::OnBackClicked);
	UButton* ResetButton=ActionButton(WidgetTree,SettingsPage,TEXT("RESET DEFAULTS"),55,724,210,42,FLinearColor(0.7f,0.75f,0.82f)); ResetButton->OnClicked.AddDynamic(this,&ThisClass::OnResetClicked);
	SensitivitySlider->OnValueChanged.AddDynamic(this,&ThisClass::OnSensitivityChanged); ADSSlider->OnValueChanged.AddDynamic(this,&ThisClass::OnADSChanged); FOVSlider->OnValueChanged.AddDynamic(this,&ThisClass::OnFOVChanged); VolumeSlider->OnValueChanged.AddDynamic(this,&ThisClass::OnVolumeChanged);
	WindowModeBox->OnSelectionChanged.AddDynamic(this,&ThisClass::OnWindowModeChanged); ResolutionBox->OnSelectionChanged.AddDynamic(this,&ThisClass::OnResolutionChanged); VSyncBox->OnSelectionChanged.AddDynamic(this,&ThisClass::OnVSyncChanged); FPSBox->OnSelectionChanged.AddDynamic(this,&ThisClass::OnFPSChanged);
}

void UArenaDuelPlayerMenuWidget::NativeConstruct(){Super::NativeConstruct();}
bool UArenaDuelPlayerMenuWidget::HasExpectedTree() const{return WidgetTree&&RootCanvas&&WidgetTree->RootWidget==RootCanvas&&PausePage&&SettingsPage&&SensitivitySlider&&ADSSlider&&FOVSlider&&VolumeSlider&&WindowModeBox&&ResolutionBox&&VSyncBox&&FPSBox;}
void UArenaDuelPlayerMenuWidget::ShowSettings(){if(PausePage)PausePage->SetVisibility(ESlateVisibility::Collapsed);if(SettingsPage)SettingsPage->SetVisibility(ESlateVisibility::Visible);PullValues();}
void UArenaDuelPlayerMenuWidget::ShowPause(){if(SettingsPage)SettingsPage->SetVisibility(ESlateVisibility::Collapsed);if(PausePage)PausePage->SetVisibility(ESlateVisibility::Visible);}
void UArenaDuelPlayerMenuWidget::PullValues()
{
	const auto* PC=Cast<AArenaDuelPlayerController>(GetOwningPlayer()); if(!PC||!SensitivitySlider)return; PendingSettings=PC->GetLocalSettings();
	SensitivitySlider->SetValue((PendingSettings.MouseSensitivity-0.1f)/4.9f); ADSSlider->SetValue((PendingSettings.ADSMultiplier-0.25f)/1.25f); FOVSlider->SetValue((PendingSettings.FOV-80)/30); VolumeSlider->SetValue(PendingSettings.MasterVolume); UpdateSliderLabels();
	WindowModeBox->SetSelectedOption(PendingSettings.WindowMode==0?TEXT("Fullscreen"):PendingSettings.WindowMode==1?TEXT("Borderless"):TEXT("Windowed")); VSyncBox->SetSelectedOption(PendingSettings.bVSync?TEXT("On"):TEXT("Off")); FPSBox->SetSelectedOption(PendingSettings.FPSLimit<=0?TEXT("Unlimited"):FString::FromInt(PendingSettings.FPSLimit));
	const FString Desired=FString::Printf(TEXT("%d x %d"),PendingSettings.ResolutionX,PendingSettings.ResolutionY); if(ResolutionBox->FindOptionIndex(Desired)==INDEX_NONE)ResolutionBox->AddOption(Desired); ResolutionBox->SetSelectedOption(Desired);
}
void UArenaDuelPlayerMenuWidget::UpdateSliderLabels(){if(SensitivityLabel)SensitivityLabel->SetText(FText::FromString(FString::Printf(TEXT("%.2f"),PendingSettings.MouseSensitivity)));if(ADSLabel)ADSLabel->SetText(FText::FromString(FString::Printf(TEXT("%.2f"),PendingSettings.ADSMultiplier)));if(FOVLabel)FOVLabel->SetText(FText::FromString(FString::Printf(TEXT("%.0f"),PendingSettings.FOV)));if(VolumeLabel)VolumeLabel->SetText(FText::AsPercent(PendingSettings.MasterVolume));}
void UArenaDuelPlayerMenuWidget::OnSensitivityChanged(float V){PendingSettings.MouseSensitivity=0.1f+FMath::Clamp(V,0,1)*4.9f;UpdateSliderLabels();}
void UArenaDuelPlayerMenuWidget::OnADSChanged(float V){PendingSettings.ADSMultiplier=0.25f+FMath::Clamp(V,0,1)*1.25f;UpdateSliderLabels();}
void UArenaDuelPlayerMenuWidget::OnFOVChanged(float V){PendingSettings.FOV=80+FMath::Clamp(V,0,1)*30;UpdateSliderLabels();}
void UArenaDuelPlayerMenuWidget::OnVolumeChanged(float V){PendingSettings.MasterVolume=FMath::Clamp(V,0,1);UpdateSliderLabels();}
void UArenaDuelPlayerMenuWidget::OnWindowModeChanged(FString V,ESelectInfo::Type){PendingSettings.WindowMode=V==TEXT("Fullscreen")?0:V==TEXT("Windowed")?2:1;}
void UArenaDuelPlayerMenuWidget::OnResolutionChanged(FString V,ESelectInfo::Type){FString X,Y;if(V.Split(TEXT(" x "),&X,&Y)){PendingSettings.ResolutionX=FCString::Atoi(*X);PendingSettings.ResolutionY=FCString::Atoi(*Y);}}
void UArenaDuelPlayerMenuWidget::OnVSyncChanged(FString V,ESelectInfo::Type){PendingSettings.bVSync=V==TEXT("On");}
void UArenaDuelPlayerMenuWidget::OnFPSChanged(FString V,ESelectInfo::Type){PendingSettings.FPSLimit=V==TEXT("Unlimited")?0:FCString::Atoi(*V);}
void UArenaDuelPlayerMenuWidget::Apply(){if(AArenaDuelPlayerController* PC=Cast<AArenaDuelPlayerController>(GetOwningPlayer()))PC->ApplyLocalSettings(PendingSettings,true);}
void UArenaDuelPlayerMenuWidget::Back(){if(bFromCharacterSelect){Close();return;}ShowPause();}
void UArenaDuelPlayerMenuWidget::ResetDefaults(){PendingSettings=FArenaDuelLocalSettings();PullValues();}
void UArenaDuelPlayerMenuWidget::Close(){if(AArenaDuelPlayerController* PC=Cast<AArenaDuelPlayerController>(GetOwningPlayer()))PC->ClosePlayerMenu();}
void UArenaDuelPlayerMenuWidget::OnResumeClicked(){Close();}
void UArenaDuelPlayerMenuWidget::OnSettingsClicked(){ShowSettings();}
void UArenaDuelPlayerMenuWidget::OnApplyClicked(){Apply();}
void UArenaDuelPlayerMenuWidget::OnBackClicked(){Back();}
void UArenaDuelPlayerMenuWidget::OnResetClicked(){PendingSettings=FArenaDuelLocalSettings();SensitivitySlider->SetValue((1.0f-0.1f)/4.9f);ADSSlider->SetValue((1.0f-0.25f)/1.25f);FOVSlider->SetValue((90.0f-80)/30);VolumeSlider->SetValue(1.0f);WindowModeBox->SetSelectedOption(TEXT("Borderless"));VSyncBox->SetSelectedOption(TEXT("Off"));FPSBox->SetSelectedOption(TEXT("144"));UpdateSliderLabels();}
void UArenaDuelPlayerMenuWidget::HandleEscape(){if(SettingsPage&&SettingsPage->GetVisibility()==ESlateVisibility::Visible)Back();else Close();}
FReply UArenaDuelPlayerMenuWidget::NativeOnPreviewKeyDown(const FGeometry& G,const FKeyEvent& E){if(E.GetKey()==EKeys::Escape){HandleEscape();return FReply::Handled();}return Super::NativeOnPreviewKeyDown(G,E);}
