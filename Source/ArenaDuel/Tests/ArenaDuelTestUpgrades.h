// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HAL/IConsoleManager.h"

/**
 * Tests written for the plain round break (three seconds, the winner may move, the next round follows on
 * its own) switch the upgrade selection off for their run and on again afterwards. The plain break is
 * still what follows a round without a winner, so those tests keep covering live behaviour.
 */
inline void SetMatchUpgradesForTests(bool bEnabled)
{
	if (IConsoleVariable* Variable = IConsoleManager::Get().FindConsoleVariable(TEXT("ArenaDuel.Upgrades.Enabled"))) Variable->Set(bEnabled ? 1 : 0);
}
