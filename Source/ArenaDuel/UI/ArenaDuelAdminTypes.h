// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArenaDuelAdminTypes.generated.h"

UENUM()
enum class EArenaDuelAdminCommand : uint8
{
	FullHeal,
	SetHealth,
	Kill,
	ToggleGodMode,
	ResetPlayer,
	EquipWeapon,
	RefillAmmo,
	ToggleInfiniteAmmo,
	RestartRound,
	NextRound,
	AwardRound,
	SetPlayer1Wins,
	SetPlayer2Wins,
	ResetMatch,
	RefillStamina,
	ToggleInfiniteStamina,
	SetArchetypeShadow,
	SetArchetypeWarden,
	ToggleDebugOverlay,
	ToggleHitZones,
	SelectPlayer1,
	SelectPlayer2,
	CloseMenu,
	SelectPlayerPage,
	SelectWeaponsPage,
	SelectRoundPage,
	SelectMovementPage,
	SelectDebugPage
};
