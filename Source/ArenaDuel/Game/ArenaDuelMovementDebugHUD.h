// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "ArenaDuelMovementDebugHUD.generated.h"

UCLASS()
class ARENADUEL_API AArenaDuelMovementDebugHUD : public AHUD
{
	GENERATED_BODY()

protected:
	virtual void DrawHUD() override;
	bool bShowDebugOverlay = false;
};
