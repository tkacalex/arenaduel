#pragma once

#include "CoreMinimal.h"

class AArenaDuelCharacter;

// Both predicted grapple and authority repeat these queries against their own world.
namespace ArenaDuelRiftTargeting
{
	constexpr float GrappleRange = 2200.0f;
	constexpr float GateRange = 1400.0f;
	constexpr float MinimumTravel = 250.0f;
	bool FindGrappleDestination(const AArenaDuelCharacter& Character, FVector& Destination, FVector& Anchor);
	bool FindGateDestination(const AArenaDuelCharacter& Character, FVector& Destination);
	bool IsTravelClear(const AArenaDuelCharacter& Character, const FVector& Destination, float MaxRange);
	bool IsGateDestinationSafe(const AArenaDuelCharacter& Character, const FVector& Destination);
}
