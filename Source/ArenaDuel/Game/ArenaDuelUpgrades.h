// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArenaDuelUpgrades.generated.h"

/**
 * Match upgrades of the 1v1 mode. After every decided round both players are offered the same kinds and
 * take one level of one of them. The order here is the fixed order used to break ties.
 */
UENUM(BlueprintType)
enum class EArenaDuelUpgrade : uint8
{
	/** More reserve ammunition; the magazine stays as it is. */
	Reserve,
	/** A shorter reload. */
	Reload,
	/** Shorter regular ability cooldowns. */
	Ability,
	Count UMETA(Hidden)
};

namespace ArenaDuelUpgrades
{
	constexpr int32 KindCount = static_cast<int32>(EArenaDuelUpgrade::Count);
	inline bool IsValidId(uint8 Id) { return Id < KindCount; }
}
