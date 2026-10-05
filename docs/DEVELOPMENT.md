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

The verified build used Unreal supplied tooling for `ArenaDuelEditor`, Win64, Development, from `C:\Users\Dima\Desktop\arenaduel\ArenaDuel.uproject`.

Current state: `REPOSITORY BUILD = PASS`.
