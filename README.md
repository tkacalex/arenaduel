# ArenaDuel

ArenaDuel is a competitive one versus one first person fantasy movement shooter with match based RPG progression, built with Unreal Engine 5 and C++.

## Current status

The repository contains a minimal Unreal Engine 5.8.3 C++ gameplay framework foundation. The `ArenaDuelEditor` Win64 Development target builds successfully from this repository.

Verified engine path: `C:\Program Files\Epic Games\UE_5.8`.

The project contains the first multiplayer framework and first person Character foundation. Phase 3 input assets, Blueprint subclasses, and a minimal movement test map are now serialized by Unreal and versioned with Git LFS. It has no health, combat, Gameplay Ability System setup, UI, online services, or match gameplay.

The generated Blank project already includes the Enhanced Input module and default input classes. ArenaDuel now contains `IA_Move`, `IA_Look`, `IA_Jump`, `IMC_Gameplay`, and configured Blueprint references created through Unreal Editor Python automation.

## Engineering direction

1. Design multiplayer behavior from the start.
2. Keep gameplay server authoritative.
3. Use C++ for foundational systems.
4. Use Blueprint and Data Assets for designer facing configuration and presentation where appropriate.
5. Use the Gameplay Ability System for future abilities, attributes, costs, cooldowns, buffs, and debuffs where appropriate.
6. Use Enhanced Input for future player input.
7. Prefer small verified vertical slices over broad unfinished systems.

## Repository contents

Important project files will include `ArenaDuel.uproject`, `Config/`, `Content/`, `Source/`, `Build/`, and project owned `Plugins/` when they exist.

Generated directories such as `Binaries/`, `DerivedDataCache/`, `Intermediate/`, `Saved/`, and `.vs/` are excluded from source control. Unreal asset packages use Git LFS.

See `docs/DEVELOPMENT.md`, `docs/ARCHITECTURE.md`, and `docs/ROADMAP.md` for current decisions.
