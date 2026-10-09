#pragma once

#include "CoreMinimal.h"

class UStaticMesh;

/** Held items that are modelled in code. They work in every build and need no asset. Sizes are centimetres. */
namespace ArenaDuelItemMeshes
{
	/** Blade with a clip point, cross guard, grip and pommel. +X is the tip. Slots: 0 blade, 1 grip, 2 guard and pommel. */
	UStaticMesh* Knife();
	/** Canister with two bands, fuse head, safety lever and pin. +Z is the top. Slots: 0 body, 1 bands and fuse, 2 lever and pin. */
	UStaticMesh* Flashbang();
}
