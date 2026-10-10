# ArenaDuel – Claude instructions

Read `AGENTS.md` first. Its rules (automation priority, Unreal engineering rules, verification and reporting) apply to Claude in full. This file adds the project facts Claude needs at the start of every session.

## Working style

- Automate everything that tools can do. The user wants to do as little manually as possible: build, test, inspect and edit through tools instead of handing out click instructions.
- Rule number one: no manual help from the user unless a concrete blocker makes it unavoidable. Drive the editor through the `unreal-mcp` server and prefer Live Coding whenever it is possible and sensible.
- Once a change works and is verified, commit it and push it to GitHub (`origin/main`) without waiting to be asked.
- Communicate with the user in German.
- Report unverified builds as `BUILD NOT YET VERIFIED` and never claim an editor action that was not performed.

## Engine and toolchain

- Unreal Engine 5.8.3 at `C:\Program Files\Epic Games\UE_5.8`. Do not change the engine association.
- Editor target: `ArenaDuelEditor` Win64 Development. Build: `Engine\Build\BatchFiles\Build.bat ArenaDuelEditor Win64 Development -Project=<uproject> -WaitMutex`.
- Live editor access: the `unreal-mcp` server (`ModelContextProtocol` plugin, `http://127.0.0.1:8000/mcp`) while the editor is open. Use `LiveCodingToolset.CompileLiveCoding` for .cpp-only changes. Header or UPROPERTY changes need a full build with the editor closed.
- Tests: Unreal automation (CQTest). Runners in `Tools/Tests/RunPhase3Validation.ps1` and `RunPhase4Validation.ps1`. Slide tests: `ArenaDuel.Slide.*`, movement: `ArenaDuel.Phase4.*`.

## Code layout (C++ module `ArenaDuel`)

- `Source/ArenaDuel/Characters/` – `AArenaDuelCharacter` (first-person character, camera, input), `UArenaDuelCharacterMovementComponent` (sprint, crouch, slide, slide jump, wall run, vault, mantle, stamina; saved-move prediction with `FLAG_Custom_0..3`), `UArenaDuelVisualAnimInstance`.
- `Abilities/` – GAS abilities (`ArenaDuelGA_*`) for Shadow, Warden and Rift, gameplay tags.
- `Combat/` – `UArenaDuelAttributeSet`.
- `Game/` – GameMode, GameState (round loop), movement debug HUD.
- `Player/` – PlayerController (admin menu F1, local settings), PlayerState (archetype, duel slot).
- `UI/` – native UMG widgets (HUD, admin, character select, player menu).
- `Weapons/` – `UArenaDuelWeaponComponent` (four server-authoritative weapons, ADS, viewmodel presentation).
- `Tests/` – automation tests.

Foundational gameplay lives in C++. Blueprints are thin subclasses for configuration and asset references.

## Content (`/Game/ArenaDuel`)

- `Characters/BP_ArenaDuelCharacter` – the player character Blueprint. `Characters/Common/` – materials and arm meshes.
- `Game/BP_ArenaDuelGameMode` – global default GameMode.
- `Input/` – Enhanced Input actions `IA_*` and `IMC_Gameplay`.
- `Maps/` – `L_MainMenu` (default game map, see `docs/MainMenu.md`), `L_ArenaDistrict` (1v1 map and editor startup map, see `docs/Maps.md`), `L_ZombieArena` (Zombie Survival, see `docs/ZombieSurvival.md`), `L_ArenaCore` (empty hall, used by tests), `L_Phase4MovementTest` (movement playground, used by tests), `L_Phase3Test`, `L_Phase5GunRange`.
- `Weapons/<Weapon>/SM_*` – weapon meshes.

Naming: `BP_` Blueprint, `WBP_` widget Blueprint, `IA_`/`IMC_` input, `M_`/`MI_` material/instance, `SM_`/`SKM_` static/skeletal mesh, `L_` level, `GA_` gameplay ability (C++: `ArenaDuelGA_*`).

## Do not touch without explicit permission

- Engine association, `.uproject` module list, Git LFS settings.
- Destructive Git operations (hard reset, force push, clean).
- Approved weapon tuning (FOV, sensitivity, spread, recoil) and test maps used by automation.
- Never commit `Binaries/`, `Intermediate/`, `Saved/`, `DerivedDataCache/`, `.vs/`.

## Further docs

`README.md`, `docs/AdminMenu.md`, `docs/ARCHITECTURE.md`, `docs/DEVELOPMENT.md`, `docs/ROADMAP.md`, `docs/GAME_DESIGN.md`.
