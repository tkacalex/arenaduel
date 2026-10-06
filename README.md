# ArenaDuel

ArenaDuel is a competitive one versus one first person fantasy movement shooter with match based RPG progression, built with Unreal Engine 5 and C++.

## Current status

The repository contains an Unreal Engine 5.8.3 C++ project with first-person movement, advanced traversal, four server-authoritative weapons, GAS health and death, a development round loop, the compact competitive HUD, a host-only development admin control menu, and native GAS kits for Shadow, Warden and Rift with public multiplayer character selection. The `ArenaDuelEditor` Win64 Development target builds successfully from this repository.

Verified engine path: `C:\Program Files\Epic Games\UE_5.8`.

The project contains a first multiplayer framework and first-person Character foundation. Phase 3 input assets, Blueprint subclasses, and movement test maps are serialized by Unreal and versioned with Git LFS. Current development tools include the F1 admin menu for standalone play and the listen-server host; remote clients and Shipping builds are denied admin access. This remains a development prototype, not a final match or online-services implementation.

The generated Blank project already includes the Enhanced Input module and default input classes. ArenaDuel now contains `IA_Move`, `IA_Look`, `IA_Jump`, `IA_Sprint`, `IA_Crouch`, `IMC_Gameplay`, and configured Blueprint references created through Unreal Editor Python automation. Phase 4 adds a C++ movement component with sprint, crouch, slide, slide jump, air control tuning, stamina, momentum bounds, and basic traversal modes.

Phase 3 technical validation is reproducible with `Tools/Tests/RunPhase3Validation.ps1`. Phase 4 validation is reproducible with `Tools/Tests/RunPhase4Validation.ps1`, which builds the editor target and runs both suites twice, including listen-server advanced-movement coverage. Phase 4.4 adds the human-playtest fixes: normal FPS mouse pitch, bounded look rotation, forgiving crouch-to-slide input, a readable movement playground, and a development-only movement HUD. Technical validation is complete, while subjective feel, real-latency profiling, packet-loss behavior, and two-player traversal playtesting remain separate human checks; this is not final competitive network hardening.

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

Phase 5 adds the playable four-weapon prototype, compact competitive HUD, and hold-to-aim ADS with server-aware spread. Human tuning of visual feel remains a separate playtest step.
