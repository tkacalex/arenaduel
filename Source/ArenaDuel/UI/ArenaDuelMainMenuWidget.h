#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ArenaDuelMainMenuWidget.generated.h"

class UButton;
class UCanvasPanel;
class UEditableTextBox;
class UTextBlock;
class UVerticalBox;

/**
 * Main menu: play (1v1 host, 1v1 join, Zombie Survival), settings, quit. Built in code like the other
 * menus. Mouse works on every button; with the keyboard the arrow keys or Tab move, Enter or Space
 * activates and Escape goes back a page.
 */
UCLASS()
class ARENADUEL_API UArenaDuelMainMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** This machine's address and the game port, as a friend has to type it to join. */
	static FString GetLocalJoinAddress(const UWorld* World);
	/** Adds the default port when the text has none. Empty for text that cannot be an address. */
	static FString NormalizeJoinAddress(const FString& Text);
	/** Once per process: remember why a connection or a map change failed, so the menu can say so when the game falls back to it. */
	static void InstallFailureReports();

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	enum class EPage : uint8 { Main, Play, Join };
	void BuildTree();
	void ShowPage(EPage Page);
	UButton* AddButton(const TCHAR* Label, bool bPrimary);
	void SetStatus(const FString& Text);
	void TakeMenuInput();

	UFUNCTION() void OnPlay();
	UFUNCTION() void OnHost();
	UFUNCTION() void OnJoinPage();
	UFUNCTION() void OnJoinConfirm();
	UFUNCTION() void OnZombie();
	UFUNCTION() void OnSettings();
	UFUNCTION() void OnQuit();
	UFUNCTION() void OnBack();

	UPROPERTY() TObjectPtr<UCanvasPanel> Root;
	UPROPERTY() TObjectPtr<UVerticalBox> Column;
	UPROPERTY() TObjectPtr<UTextBlock> PageTitle;
	UPROPERTY() TObjectPtr<UTextBlock> Status;
	UPROPERTY() TObjectPtr<UEditableTextBox> AddressBox;
	UPROPERTY() TObjectPtr<UButton> FirstButton;
	EPage CurrentPage = EPage::Main;
	float PageFade = 0.0f;
	bool bFocusPending = false;
};
