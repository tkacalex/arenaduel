# ArenaDuel

ArenaDuel is planned as a competitive one versus one third person action RPG arena game built with Unreal Engine 5 and C++.

## Current status

The repository currently contains foundation documentation and source control rules only. No Unreal project, gameplay code, assets, or generated project files exist yet.

The planned engine baseline is Unreal Engine 5.8. This choice is not locked until the pending Epic Games Launcher installation completes and the generated C++ project builds successfully.

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
