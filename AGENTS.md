# ArenaDuel Agent Instructions

## Before changing anything

1. Inspect the workspace, repository status, current branch, and relevant files before editing.
2. Read the current documentation and preserve user work.
3. Keep changes small, reviewable, and limited to the active milestone.
4. Never use destructive Git operations such as hard reset, forced push, or repository cleaning without explicit user authorization.
5. Never delete or overwrite user files merely to simplify an implementation.

## Unreal engineering rules

1. Follow Unreal Engine naming, module, reflection, ownership, and lifecycle conventions.
2. Design multiplayer systems first and treat the server as authoritative.
3. Use the Gameplay Ability System for future abilities, attributes, costs, cooldowns, buffs, and debuffs where appropriate.
4. Use Enhanced Input for future player input.
5. Put foundational systems in C++. Use Blueprint and Data Assets for designer facing configuration and presentation where appropriate.
6. Avoid God classes, unnecessary Tick, unnecessary RPCs, and unnecessary hard references.
7. Prefer data driven gameplay values.
8. Keep gameplay logic independent from cosmetic VFX execution.
9. Add dependencies only when the current milestone justifies them.

## Verification and reporting

1. Test before claiming success.
2. Never fabricate assets, editor actions, compilation, tests, packaging, or build results.
3. Clearly distinguish automated work from manual Unreal Editor steps.
4. Never claim an Unreal Editor action was completed unless it was actually performed and verified.
5. Inspect Git changes before committing and keep every change attributable to the active task.
6. Report unverified builds explicitly as `BUILD NOT YET VERIFIED`.
