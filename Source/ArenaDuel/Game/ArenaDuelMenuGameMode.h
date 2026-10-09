#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "../Player/ArenaDuelPlayerController.h"
#include "ArenaDuelMenuGameMode.generated.h"

class UArenaDuelMainMenuWidget;

/** Player controller of the menu map. It is the game's controller, so the settings menu and saved settings work here too. */
UCLASS()
class ARENADUEL_API AArenaDuelMenuPlayerController : public AArenaDuelPlayerController
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
protected:
	UPROPERTY(Transient) TObjectPtr<UArenaDuelMainMenuWidget> MainMenu;
};

/** The menu map has no pawn, no match and no HUD: just the menu. */
UCLASS()
class ARENADUEL_API AArenaDuelMenuGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AArenaDuelMenuGameMode();
};
