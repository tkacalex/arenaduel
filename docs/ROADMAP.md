# Roadmap

## Phase 1: Foundation and source control

Complete.

## Phase 2: Unreal project and build foundation

Complete.

## Phase 3: First person network Character foundation

Complete. The C++ first person camera, walk, look, jump, possession-safe Enhanced Input lifecycle, multiplayer-safe Character foundation, Unreal input assets, Blueprint defaults, temporary movement test map, and automated two-player listen-server validation are configured and verified.

## Phase 4: Advanced movement

Complete as a technical movement foundation. Sprint, crouch/slide, slide jump, wall run, wall jump, stamina, vault, and mantle behavior are covered by the local suite and listen-server network tests. The Phase 3 regression suite and the Phase 4 suite both pass twice through `Tools/Tests/RunPhase4Validation.ps1`. Subjective movement feel, real-latency profiling, packet-loss behavior, and competitive tuning still require human playtesting. This is not final competitive network hardening.

Phase 4.4 human-playtest fixes are implemented: mouse-up/mouse-down behavior follows normal FPS expectations with an 88 degree pitch limit, crouch input supports a short buffered slide transition, and `L_Phase4MovementTest` provides a large safety floor, perimeter barriers, labeled movement stations, wall-run and traversal practice geometry, and a development-only speed/state/stamina/controls HUD. Automated technical checks pass; human feel retesting remains required.

## Phase 5: Weapons and gunplay

Weapon handling, aiming, firing, reloads, hit validation, and the initial weapon pool. The current foundation contains four native weapon definitions, persistent per-weapon ammo state, server cadence, spread, recoil, shotgun pellet aggregation, primitive first-person visuals, and a temporary gun-range map. Runtime/network validation and human gunplay tuning remain before Phase 5 can be marked complete.

## Phase 6: GAS, combat attributes, damage, and death

Gameplay Ability System integration, combat attributes, health, damage, death, and related server authority.

Current playtest support also includes a minimal server-authoritative round restart loop after death and a compact competitive HUD showing round wins. This does not mark Phase 8 complete and is not the final match system.

## Phase 7: Characters and abilities

Shadow, Warden, Rift, and non damaging utility and movement abilities.

Shadow and Warden are implemented and human-approved. Phase 7C adds Rift: Q Rift Grapple is predicted root-motion traversal toward stable world geometry (2200 uu range, six-second GAS cooldown); E Phase Gate is a server-validated personal reposition (1400 uu range, 0.2-second cast, fourteen-second cooldown) with paired temporary gate visuals. Neither deals damage or grants invulnerability. Kit switching removes stale specs/cooldowns and actors; respawns and return to selection preserve the archetype without duplicate specs. All three kits are available through public Character Select and host admin. Rift feel, camera feedback, portrait and visual quality still require human two-player acceptance.

## Phase 8: Round system, selections, and RPG upgrades

First to 5 rounds, round flow, fighter and weapon selection, and match based upgrade choices.

The public pre-match frontend implements replicated Shadow/Warden/Rift selection and ready state, authoritative countdown, combat locking, and return to selection after match results. It uses a fullscreen native UI with procedural development portraits. Account identity, ranked disconnect rules, and RPG upgrades remain pending. Visual fidelity and real two-player usability remain human acceptance checks.

## Phase 9: Arena, UI, audiovisual polish, and optimization

Shattered Sanctum, competitive UI, audio, VFX, presentation, settings, and performance work.

## Phase 10: Online private duel, server, ranked, network hardening, and V1 release

Private duels, dedicated server deployment, ranked and MMR systems, network hardening, and release validation.

## V1 acceptance criteria

V1 is complete only when two players can launch ArenaDuel, create or join a duel, select Shadow, Warden, or Rift, select a weapon, enter Shattered Sanctum, play a first to 5 match with 90 second rounds, use gunplay, advanced movement, and character abilities, receive match upgrades, finish the match, see results, and start a rematch over real networked play.

The V1 release must also contain four usable weapons, three usable fighters, one complete arena, a functional HUD, audio feedback, visual feedback, FPS settings, and acceptable multiplayer behavior.
