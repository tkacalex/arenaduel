# ArenaDuel

ArenaDuel is planned as a competitive one versus one third person action RPG arena game built with Unreal Engine 5 and C++.

## Current status

The repository contains a minimal Unreal Engine 5.8.3 Blank C++ project. The `ArenaDuelEditor` Win64 Development target builds successfully from this repository.

Verified engine path: `C:\Program Files\Epic Games\UE_5.8`.

The project contains no gameplay classes, assets, Starter Content, marketplace plugins, Gameplay Ability System setup, Enhanced Input additions, or multiplayer gameplay.

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
