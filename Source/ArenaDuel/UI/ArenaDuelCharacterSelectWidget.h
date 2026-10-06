#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "../Player/ArenaDuelPlayerState.h"
#include "ArenaDuelCharacterSelectWidget.generated.h"

class UCanvasPanel;
class UTextBlock;
class UButton;
class UBorder;
class UProgressBar;

// Presentation ratings only. They never feed gameplay calculations.
struct FArenaDuelCharacterPresentation
{
	FString Name, Role, Description, PrimaryName, PrimaryDescription, SecondaryName, SecondaryDescription;
	float Ratings[4];
};

USTRUCT()
struct FArenaDuelSelectionPanel
{
	GENERATED_BODY()
	UPROPERTY() TObjectPtr<UCanvasPanel> Root;
	UPROPERTY() TObjectPtr<UTextBlock> Name;
	UPROPERTY() TObjectPtr<UTextBlock> Role;
	UPROPERTY() TObjectPtr<UTextBlock> Description;
	UPROPERTY() TObjectPtr<UTextBlock> PrimaryName;
	UPROPERTY() TObjectPtr<UTextBlock> PrimaryDescription;
	UPROPERTY() TObjectPtr<UTextBlock> SecondaryName;
	UPROPERTY() TObjectPtr<UTextBlock> SecondaryDescription;
	UPROPERTY() TObjectPtr<UTextBlock> ReadyText;
	UPROPERTY() TObjectPtr<UTextBlock> Identity;
	UPROPERTY() TObjectPtr<UButton> ReadyButton;
	UPROPERTY() TArray<TObjectPtr<UButton>> RosterButtons;
	UPROPERTY() TArray<TObjectPtr<UProgressBar>> Ratings;
	EArenaDuelCharacterArchetype DisplayArchetype = EArenaDuelCharacterArchetype::Shadow;
	bool bPresent = false;
};

UCLASS(meta=(DisableNativeTick))
class ARENADUEL_API UArenaDuelCharacterSelectWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	virtual bool Initialize() override;
	bool HasExpectedTree() const;
	static const FArenaDuelCharacterPresentation& GetPresentation(EArenaDuelCharacterArchetype Archetype);
	static EArenaDuelCharacterArchetype CycleArchetype(EArenaDuelCharacterArchetype Current, int32 Direction);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& CullingRect, FSlateWindowElementList& Elements, int32 LayerId, const FWidgetStyle& Style, bool bEnabled) const override;
	void BuildTree();
	void RefreshLobby();
	void BuildPlayerPanel(int32 Slot);
	void Select(EArenaDuelCharacterArchetype Archetype);
	UFUNCTION() void SelectShadow();
	UFUNCTION() void SelectWarden();
	UFUNCTION() void SelectRift();
	UFUNCTION() void ToggleReady();
	UFUNCTION() void OpenSettings();
	UPROPERTY(Transient) TObjectPtr<UCanvasPanel> ReferenceCanvas;
	UPROPERTY(Transient) TArray<FArenaDuelSelectionPanel> Panels;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> CenterLabel;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> CenterStatus;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> Matchup;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> Connection;
	UPROPERTY(Transient) TObjectPtr<UButton> SettingsButton;
	FTimerHandle RefreshTimer;
};
