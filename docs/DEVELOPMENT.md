# Development

## Intended workflow

ArenaDuel uses the same project directory for Unreal Editor, VS Code, Codex, Git, and GitHub. Visual Studio or Microsoft Build Tools provides the Windows compiler and SDK even when VS Code is the editor.

## Engine baseline

The verified engine is Unreal Engine 5.8.3 at `C:\Program Files\Epic Games\UE_5.8`. The project uses the generated EngineAssociation for that installation. Do not change the engine association silently. A version change requires an explicit decision and a verified migration.

## Windows toolchain baseline

The verified compiler environment is Visual Studio Community 2026 Insiders 18.11 with MSVC 14.50.35739 and MSBuild 18.11. The verified Windows SDK used by UnrealBuildTool is 10.0.22621.0. Windows SDK 10.0.26100.0 is also installed. UnrealBuildTool selected MSVC 14.50 and SDK 10.0.22621.0 during the verified build.

## Source control

Normal Git stores text source and configuration. Git LFS stores Unreal package files because `.uasset` and `.umap` files are binary, commonly large, and cannot be meaningfully line merged. Source asset formats such as FBX, WAV, and PSD are not tracked by LFS yet. Add those patterns only when the team decides to version those source formats and understands repository storage costs.

Keep `Config/`, `Content/`, `Source/`, `Build/`, `ArenaDuel.uproject`, and project owned plugins under version control. Do not commit generated binaries, caches, saved editor state, generated IDE projects, or local logs.

## Build verification

The verified build used Unreal supplied tooling for `ArenaDuelEditor`, Win64, Development, from `C:\Users\Dima\Desktop\arenaduel\ArenaDuel.uproject`. The current Phase 3 first person refactor also builds successfully.

Current state: `REPOSITORY BUILD = PASS`.

## Phase 3 asset automation

`Tools/Editor/SetupPhase3Assets.py` creates or repairs the Phase 3 Input Actions, Mapping Context, Character Blueprint, GameMode Blueprint, and temporary test map through Unreal Engine 5.8.3 itself. `Tools/Editor/ValidatePhase3Assets.py` reloads those packages and verifies value types, serialized modifiers, Blueprint defaults, parent classes, GameMode pawn selection, and map actors.

`Tools/Tests/RunPhase3Validation.ps1` is the repeatable technical validation entry point. It builds `ArenaDuelEditor`, runs the editor-only CQTest Phase 3 map and two-player listen-server tests twice, injects Enhanced Input through the official subsystem API, and fails on a missing or failed JSON automation result. It does not replace subjective human feel testing, but no manual technical setup is required for the automated checks.

The automation plugins are editor only and are not listed in `ArenaDuel.uproject` after setup. If the script must be rerun, enable `PythonScriptPlugin` and `EditorScriptingUtilities` temporarily, run the script with `UnrealEditor-Cmd.exe`, run the validation script, and remove those temporary project plugin entries before committing.

The Android File Server editor settings section is intentionally absent for this Win64-only development milestone. ArenaDuel does not use Android File Server, so removing that project-owned section prevents Unreal from regenerating a local `SecurityToken` in tracked configuration.

## Development admin menu

In a Development/Editor standalone session or as the listen-server host, press F1 to open the native fullscreen admin control center and F1 or Escape to close it. The menu does not pause the game. Its five pages use a fixed navigation rail and a scrollable content area; numeric inputs retain keyboard focus, and F1/Escape close keys are handled in widget preview routing. Remote clients cannot open the menu or pass server-side admin authorization. Shipping builds compile out menu opening and reject the server admin command endpoint. Player/weapon/round/movement commands run on authority; debug overlay and hit-zone drawing are local diagnostics. Reset Player only affects a living target; use Restart Round to recover a dead player without awarding a win.

## Phase 4 validation

`Tools/Tests/RunPhase4Validation.ps1` builds `ArenaDuelEditor` and runs the Phase 3 regression suite plus the Phase 4 asset and movement-state suite twice. The Phase 4 suite validates the temporary movement map, input assets, Blueprint assignments, sprint and slide state transitions, slide jump, and movement bounds. Unreal's CQTest report is the source of truth; engine PIE NetGUID warnings are tolerated only when the report has no failed or not-run tests.

Phase 4.1 expands the suite to 13 registered tests, including sprint, crouch, slide, slide jump, air control, stamina configuration, wall run, wall jump, vault, mantle, and a listen-server sprint/crouch intent test. Phase 4.4 adds the slide input buffer, mouse direction and pitch checks, map structure checks, and movement HUD wiring checks. The runner executes both Phase 3 and Phase 4 suites twice. Automated results are technical verification, not a replacement for feel testing or high-latency profiling. Phase 5 remains blocked until the human movement retest is complete.
