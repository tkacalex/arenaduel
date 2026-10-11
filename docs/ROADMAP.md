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

Phase 7D adds the local Epic Manny skeletal body, Unreal-derived arm-only first-person mesh, three archetype appearances and four original multi-part weapon models. Separate owner/world visibility and existing weapon replication drive both held representations, with native locomotion/reload animation and cosmetic lying death. Setup is automated by `Tools/Editor/SetupPhase7DVisuals.ps1`; no manual Blueprint wiring is needed. Human two-player review of hands, ADS alignment, animation, clipping and silhouettes remains the acceptance gate. Phase 8 has not started.

Phase 7E polishes first-person/world weapon presentation, pauses locomotion animation during the deterministic cosmetic death fall, allows only the round winner to move during RoundBreak, and adds the empty enclosed `L_ArenaCore` foundation map. A native local pause/settings menu now applies and saves sensitivity, ADS sensitivity, FOV, display mode/resolution, VSync, FPS limit, and master volume. The map/materials are created through Unreal Editor scripting; the mannequin rig dependency is included by the Phase 7D asset installer. Focused editor automation and the Win64 Development build pass; human PIE review is still required for visual feel, settings usability, and real two-player winner movement. Phase 8 RPG upgrades have not started.

## Phase 8: Round system, selections, and RPG upgrades

First to 5 rounds, round flow, fighter and weapon selection, and match based upgrade choices.

The public pre-match frontend implements replicated Shadow/Warden/Rift selection and ready state, authoritative countdown, combat locking, and return to selection after match results. It uses a fullscreen native UI with procedural development portraits. Account identity, ranked disconnect rules, and RPG upgrades remain pending. Visual fidelity and real two-player usability remain human acceptance checks.

The prototype round loop and selection frontend already exist, but Phase 8 RPG upgrade choices have not started.

### Phase 8A: round flow (done 2026-10-11; Phase 8 stays open)

The 1v1 mode has a complete round flow on the existing selection, ready, countdown, evaluation and match display. See `docs/RoundFlow.md`.

- A regular round lasts 90 seconds. The server starts and ends it and publishes the end time through the GameState; both players derive minutes and seconds from the synchronised server time and see them in the match header.
- Interim rule at the limit: with both players alive, an announced overtime begins and the next regular kill decides the round. Health, ammunition and positions are kept; nothing else changes. This stands in for the overtime objective of the game design, which is not built.
- First to five round wins, the pauses and the return to character selection are unchanged. Every round is decided exactly once, whatever asks for it; a time limit only counts for the round it was started for; leaving, a match end and a restart clear the clock.
- Zombie Survival inherits the duel classes and has no round clock.

Not part of 8A: weapon selection, the real overtime objective, the nine round cap of the game design, ranked rules for a disconnect.

### Phase 8B: match upgrades (done 2026-10-11; Phase 8 stays open)

After every decided round both players take one level of one of three upgrades in a selection of at most ten seconds that replaces the round break: Supply (+10 % reserve ammunition per level), Reload (5 % shorter reload per level), Abilities (5 % shorter regular cooldowns per level), three levels each, additive against the base value. The server owns offers, time, choices and effects; levels live on the player state and end with the match. See `docs/MatchUpgrades.md`.

Still open in Phase 8: more and more varied upgrades (the three are a deliberately small start), balance from play tests, weapon selection, the real overtime objective, the nine round cap, ranked rules for a disconnect.

## Phase 9: Arena, UI, audiovisual polish, and optimization

Shattered Sanctum, competitive UI, audio, VFX, presentation, settings, and performance work.

### Phase 9 stabilisation (started 2026-10-11)

A review of commit `64de43e` listed 18 code findings and 10 areas to check. `docs/Phase9.md` tracks every one with its state and evidence.

- 9A, combat and participants (done 2026-10-11): every ray of a knife swing stops at cover; the knife's attack time is separate from the switch time; a trigger held inside the switch time fires when it ends, on both sides; the reload request needs the firearm in the hands; a third player only watches and their leaving does not touch the match; upgrade choices name their selection.
- 9B, input, camera and survival (code and tests done 2026-10-11): no weapon action starts behind the pause menu; the shot origin eases with the eye through crouch and slide; one Reset Defaults; no charge for full ammunition; a zombie's speed at death is kept for clip and ragdoll; big enemies spawn and are rescued where their capsule fits; survival ends when the last living player leaves; the round flow document says how two deaths in one frame are handled.
- 9C, first part (done 2026-10-11): developer commands are compiled out of shipping builds; the asset setup commands are no longer part of the test suite and fail on missing assets.
- Open: the rest of 9C (dedicated server pose, prediction under latency, replication order, a packaged build), 9D presentation and sound, 9E load and overall acceptance, and every check that needs a person at the screen or ears. See the end of `docs/Phase9.md`.

## Phase 10: Online private duel, server, ranked, network hardening, and V1 release

Private duels, dedicated server deployment, ranked and MMR systems, network hardening, and release validation.

## V1 acceptance criteria

V1 is complete only when two players can launch ArenaDuel, create or join a duel, select Shadow, Warden, or Rift, select a weapon, enter Shattered Sanctum, play a first to 5 match with 90 second rounds, use gunplay, advanced movement, and character abilities, receive match upgrades, finish the match, see results, and start a rematch over real networked play.

The V1 release must also contain four usable weapons, three usable fighters, one complete arena, a functional HUD, audio feedback, visual feedback, FPS settings, and acceptable multiplayer behavior.
