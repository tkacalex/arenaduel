# Development

## Intended workflow

ArenaDuel uses the same project directory for Unreal Editor, VS Code, Codex, Git, and GitHub. Visual Studio or Microsoft Build Tools provides the Windows compiler and SDK even when VS Code is the editor.

## Engine baseline

Unreal Engine 5.8 is the planned baseline because its official documentation is current and an Epic Games Launcher installation is already in progress on this machine. Do not change the engine association silently. A version change requires an explicit decision and a verified migration.

## Windows toolchain baseline

Unreal Engine 5.8 supports Visual Studio 2022 version 17.14 or later and Visual Studio 2026 version 18.0 or later. Epic recommends Visual Studio 2026 for general development. For the intended VS Code workflow, Visual Studio 2022 Build Tools version 17.14 or later remains a supported lean choice. Install the Desktop development with C++ and Game development with C++ workloads, MSVC, and a Windows 10 or 11 SDK version 10.0.18362 or newer. The current machine has not yet passed this toolchain check.

## Source control

Normal Git stores text source and configuration. Git LFS stores Unreal package files because `.uasset` and `.umap` files are binary, commonly large, and cannot be meaningfully line merged. Source asset formats such as FBX, WAV, and PSD are not tracked by LFS yet. Add those patterns only when the team decides to version those source formats and understands repository storage costs.

Keep `Config/`, `Content/`, `Source/`, `Build/`, `ArenaDuel.uproject`, and project owned plugins under version control. Do not commit generated binaries, caches, saved editor state, generated IDE projects, or local logs.

## Build verification

The first valid build should use the Unreal supplied build tooling for the generated `ArenaDuelEditor` target in the Win64 Development configuration. Record the exact command and result when the project exists.

Current state: `BUILD NOT YET VERIFIED`.
