// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "ArenaDuelMovementDebugHUD.generated.h"

class UArenaDuelHUDWidget;

UCLASS()
class ARENADUEL_API AArenaDuelMovementDebugHUD : public AHUD
{
	GENERATED_BODY()

protected:
	virtual void BeginPlay() override;
	virtual void DrawHUD() override;
	UPROPERTY()
	TObjectPtr<UArenaDuelHUDWidget> GameplayWidget;
	bool bShowDebugOverlay = false;
};
