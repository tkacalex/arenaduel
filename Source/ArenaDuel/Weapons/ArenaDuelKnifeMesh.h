#pragma once

#include "CoreMinimal.h"

class UStaticMesh;

namespace ArenaDuelKnifeMesh
{
	/**
	 * Builds the knife model in code: blade with a clip point, cross guard, grip and pommel.
	 * Material slots: 0 blade, 1 grip, 2 guard and pommel. Works in every build, no asset needed.
	 */
	UStaticMesh* Build(UObject* Outer);
}
