#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/ComboBoxString.h"
#include "../Player/ArenaDuelLocalSettings.h"
#include "ArenaDuelPlayerMenuWidget.generated.h"

class UCanvasPanel;
class UTextBlock;
class USlider;
class UComboBoxString;
class UButton;
class AArenaDuelPlayerController;

UCLASS(meta=(DisableNativeTick))
class ARENADUEL_API UArenaDuelPlayerMenuWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	virtual bool Initialize() override;
	void Configure(bool bFromCharacterSelect);
	void ShowSettings();
	void ShowPause();
	void HandleEscape();
	bool HasExpectedTree() const;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;
protected:
	virtual void NativeConstruct() override;
	void BuildTree();
	void PullValues();
	void Apply();
	void Back();
	void ResetDefaults();
	void Close();
	void UpdateSliderLabels();
	UFUNCTION() void OnSensitivityChanged(float Value);
	UFUNCTION() void OnADSChanged(float Value);
	UFUNCTION() void OnFOVChanged(float Value);
	UFUNCTION() void OnVolumeChanged(float Value);
	UFUNCTION() void OnWindowModeChanged(FString Value, ESelectInfo::Type SelectionType);
	UFUNCTION() void OnResolutionChanged(FString Value, ESelectInfo::Type SelectionType);
	UFUNCTION() void OnVSyncChanged(FString Value, ESelectInfo::Type SelectionType);
	UFUNCTION() void OnFPSChanged(FString Value, ESelectInfo::Type SelectionType);
	UFUNCTION() void OnResumeClicked();
	UFUNCTION() void OnMainMenuClicked();
	UFUNCTION() void OnSettingsClicked();
	UFUNCTION() void OnApplyClicked();
	UFUNCTION() void OnBackClicked();
	UFUNCTION() void OnResetClicked();
	UPROPERTY(Transient) TObjectPtr<UCanvasPanel> RootCanvas;
	UPROPERTY(Transient) TObjectPtr<UCanvasPanel> PausePage;
	UPROPERTY(Transient) TObjectPtr<UCanvasPanel> SettingsPage;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> SensitivityLabel;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> ADSLabel;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> FOVLabel;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> VolumeLabel;
	UPROPERTY(Transient) TObjectPtr<USlider> SensitivitySlider;
	UPROPERTY(Transient) TObjectPtr<USlider> ADSSlider;
	UPROPERTY(Transient) TObjectPtr<USlider> FOVSlider;
	UPROPERTY(Transient) TObjectPtr<USlider> VolumeSlider;
	UPROPERTY(Transient) TObjectPtr<UComboBoxString> WindowModeBox;
	UPROPERTY(Transient) TObjectPtr<UComboBoxString> ResolutionBox;
	UPROPERTY(Transient) TObjectPtr<UComboBoxString> VSyncBox;
	UPROPERTY(Transient) TObjectPtr<UComboBoxString> FPSBox;
	FArenaDuelLocalSettings PendingSettings;
	TArray<FIntPoint> AvailableResolutions;
	bool bFromCharacterSelect = false;
};
