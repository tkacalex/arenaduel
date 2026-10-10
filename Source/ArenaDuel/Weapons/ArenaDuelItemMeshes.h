#pragma once

#include "CoreMinimal.h"

class UStaticMesh;
class USoundBase;

/**
 * The knife and the flashbang. The modelled assets under /Game/ArenaDuel/Weapons/Equipment are used when they
 * are there; otherwise the same items are built in code, so they exist in every checkout. Sizes are centimetres.
 */
namespace ArenaDuelItemMeshes
{
	/** Blade with a clip point, cross guard, grip and pommel. +X is the tip. Slots: 0 blade, 1 grip, 2 guard and pommel. */
	UStaticMesh* Knife();
	/** Canister with two bands, fuse head, safety lever and pin. +Z is the top. Slots: 0 body, 1 bands and fuse, 2 lever and pin. */
	UStaticMesh* Flashbang();
	/** A sound from /Game/ArenaDuel/Audio by its asset name, looked up once. Null when the asset is missing. */
	USoundBase* Sound(const TCHAR* Name);
}
