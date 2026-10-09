#include "ArenaDuelMenuGameMode.h"
#include "../UI/ArenaDuelMainMenuWidget.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/HUD.h"

AArenaDuelMenuGameMode::AArenaDuelMenuGameMode()
{
	DefaultPawnClass = nullptr;
	PlayerControllerClass = AArenaDuelMenuPlayerController::StaticClass();
	HUDClass = AHUD::StaticClass();
}

void AArenaDuelMenuPlayerController::BeginPlay()
{
	Super::BeginPlay();
	if (!IsLocalController()) return;
	UArenaDuelMainMenuWidget::InstallFailureReports();
	MainMenu = CreateWidget<UArenaDuelMainMenuWidget>(this, UArenaDuelMainMenuWidget::StaticClass());
	if (MainMenu) MainMenu->AddToViewport(10);
}

void AArenaDuelMenuPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (MainMenu) MainMenu->RemoveFromParent();
	Super::EndPlay(EndPlayReason);
}
