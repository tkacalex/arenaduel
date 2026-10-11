# Roadmap

## Current completion plan (reviewed 2026-10-11)

The active status and remaining work are expanded in [V1CompletionPlan.md](V1CompletionPlan.md), reviewed against `22fa35e`. Keep code implementation, runtime checks, viewed visuals and listened-to audio distinct. These planning changes do not implement gameplay or claim a new build.

| Package | Remaining outcome |
| --- | --- |
| 8C | Public weapon choice; resolve equal-pool design versus existing fighter loadouts before changing access |
| 8D | Final overtime/match contract; preserve current decisive rules until a decision |
| 8E | Real fighter, weapon and upgrade acceptance at restored automatic RPM |
| 9C.1-9C.3 | Dedicated gameplay pose, movement prediction, late-join presentation, predicted fire and eye-origin limits |
| 9D.1 | Active hit-feedback fixes and E16 impact-mark classification/lifetime |
| 9D.2-9D.3 | Hands/weapon actions, zombie/boss/ragdoll visuals and existing arena acceptance |
| 9D.4-9D.5 | Weapon audio/listening, menus, controls and confirmed/reversible display settings |
| 9E.1-9E.2 | Measured packaged performance and actual Shipping usability |
| 10A | A full public host/join/match/rematch flow between two PCs over the internet |
| 10B | Reproducible deployable dedicated hosting, after toolchain/hosting decisions |
| 10C | Server-owned persistent ranked/MMR contract and services; separate from a first private build |
| 10D | Repeatable packaging, clean-PC delivery and final release-candidate acceptance |

Next: finish the active 9D.1 package, then follow the dependency order in the completion plan. Use focused tests during a fix, one final full regression run per code package, and repeated long/cook runs only when the changed area requires them.

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

Weapon handling, aiming, firing, reloads, hit validation, and the initial weapon pool. The current implementation contains four native weapon definitions, persistent per-weapon ammo, restored server cadence, spread, recoil and shotgun pellet aggregation. Current multi-part weapon/hand assets supersede the original primitive visuals. Automated and packaged function checks are recorded in `docs/Phase9.md`; visual gunplay acceptance and real-network prediction limits remain Phase 9 work.

## Phase 6: GAS, combat attributes, damage, and death

Gameplay Ability System integration, combat attributes, health, damage, death, and related server authority.

The initial Phase 6 restart loop and compact round-score HUD were a foundation. The current round, selection and upgrade flow is tracked under Phase 8A/8B and in `docs/RoundFlow.md`, `docs/MatchUpgrades.md` and `docs/Phase9.md`.

## Phase 7: Characters and abilities

Shadow, Warden, Rift, and non damaging utility and movement abilities.

Shadow and Warden are implemented and human-approved. Phase 7C adds Rift: Q Rift Grapple is predicted root-motion traversal toward stable world geometry (2200 uu range, six-second GAS cooldown); E Phase Gate is a server-validated personal reposition (1400 uu range, 0.2-second cast, fourteen-second cooldown) with paired temporary gate visuals. Neither deals damage or grants invulnerability. Kit switching removes stale specs/cooldowns and actors; respawns and return to selection preserve the archetype without duplicate specs. All three kits are available through public Character Select and host admin. Rift feel, camera feedback, portrait and visual quality still require human two-player acceptance.

Phase 7D adds the local Epic Manny skeletal body, Unreal-derived arm-only first-person mesh, three archetype appearances and four original multi-part weapon models. Separate owner/world visibility and existing weapon replication drive both held representations, with native locomotion/reload animation and cosmetic lying death. Setup is automated by `Tools/Editor/SetupPhase7DVisuals.ps1`; no manual Blueprint wiring is needed. Human two-player review of hands, ADS alignment, animation, clipping and silhouettes remains the acceptance gate. This paragraph records the earlier 7D implementation; later custom hands and ragdoll work are described in `docs/FirstPersonArms.md` and `docs/CharacterAssets.md`. Phase 8A/8B are implemented below.

Phase 7E polishes first-person/world weapon presentation, pauses locomotion animation during the deterministic cosmetic death fall, allows only the round winner to move during RoundBreak, and adds the empty enclosed `L_ArenaCore` foundation map. A native local pause/settings menu now applies and saves sensitivity, ADS sensitivity, FOV, display mode/resolution, VSync, FPS limit, and master volume. The map/materials are created through Unreal Editor scripting; the mannequin rig dependency is included by the Phase 7D asset installer. Focused editor automation and the Win64 Development build pass; human PIE review is still required for visual feel, settings usability, and real two-player winner movement. Phase 8B upgrades are now implemented; the dated Phase 8/9 sections below are the current status.

## Phase 8: Round system, selections, and RPG upgrades

First to 5 rounds, round flow, fighter and weapon selection, and match based upgrade choices.

The public pre-match frontend implements replicated Shadow/Warden/Rift selection and ready state, authoritative countdown, combat locking, and return to selection after match results. It uses a fullscreen native UI with procedural development portraits. Account identity and ranked disconnect rules remain Phase 10 work. Match upgrades are implemented under Phase 8B; public weapon choice remains open under Phase 8C. Visual fidelity and real two-player usability remain human acceptance checks.

The round loop, fighter selection and three match upgrades exist. The remaining feature and acceptance packages are Phase 8C to 8E below.

### Phase 8A: round flow (done 2026-10-11; Phase 8 stays open)

The 1v1 mode has a complete round flow on the existing selection, ready, countdown, evaluation and match display. See `docs/RoundFlow.md`.

- A regular round lasts 90 seconds. The server starts and ends it and publishes the end time through the GameState; both players derive minutes and seconds from the synchronised server time and see them in the match header.
- Interim rule at the limit: with both players alive, an announced overtime begins and the next regular kill decides the round. Health, ammunition and positions are kept; nothing else changes. This stands in for the overtime objective of the game design, which is not built.
- First to five round wins, the pauses and the return to character selection are unchanged. Every round is decided exactly once, whatever asks for it; a time limit only counts for the round it was started for; leaving, a match end and a restart clear the clock.
- Zombie Survival inherits the duel classes and has no round clock.

Not part of 8A: public weapon selection, the final overtime objective and ranked disconnect rules. Under the current decisive first-to-five rule a match already ends within nine rounds; an additional cap would need a draw rule only if draws are introduced.

### Phase 8B: match upgrades (done 2026-10-11; Phase 8 stays open)

After every decided round both players take one level of one of three upgrades in a selection of at most ten seconds that replaces the round break: Supply (+10 % reserve ammunition per level), Reload (5 % shorter reload per level), Abilities (5 % shorter regular cooldowns per level), three levels each, additive against the base value. The server owns offers, time, choices and effects; levels live on the player state and end with the match. See `docs/MatchUpgrades.md`.

Still open in Phase 8: public weapon choice, the final match/overtime contract and gameplay acceptance at the restored firing rate. More upgrade kinds are an extension. Ranked disconnect rules belong to Phase 10; no extra nine-round cap is required for the current decisive rules.

### Phase 8C-8E: remaining selection and match acceptance (open)

- 8C: public server-validated weapon choice through the pre-match UI. The design promises equal access to the pool, while current loadouts are fighter-specific; preserve the existing kits until this conflict is resolved.
- 8D: adopt a final overtime objective or explicitly retain the interim rule in the release design. First to five and 90 seconds stay. Do not add an unnecessary nine-round counter or change the documented first-processed-death result during stabilization.
- 8E: play-test all fighters/weapons and real upgrade effects, including the higher actual automatic rate restored by the cadence fix. More upgrade kinds do not block accepting the existing three.

Acceptance criteria and implementation boundaries: [V1CompletionPlan.md](V1CompletionPlan.md).

## Phase 9: Arena, UI, audiovisual polish, and optimization

Finish the existing playable `L_ArenaDistrict` arena (the roadmap's original arena label was Shattered Sanctum), competitive UI, audio, VFX, presentation, settings and performance. Preserve the established map and mechanics; no second arena is required.

### Phase 9 stabilisation (started 2026-10-11)

A review of commit `64de43e` listed 18 code findings and 10 areas to check. `docs/Phase9.md` tracks every one with its state and evidence.

- 9A, combat and participants (done 2026-10-11): every ray of a knife swing stops at cover; the knife's attack time is separate from the switch time; a trigger held inside the switch time fires when it ends, on both sides; the reload request needs the firearm in the hands; a third player only watches and their leaving does not touch the match; upgrade choices name their selection.
- 9B, input, camera and survival (code and tests done 2026-10-11): no weapon action starts behind the pause menu; the shot origin eases with the eye through crouch and slide; one Reset Defaults; no charge for full ammunition; a zombie's speed at death is kept for clip and ragdoll; big enemies spawn and are rescued where their capsule fits; survival ends when the last living player leaves; the round flow document says how two deaths in one frame are handled.
- 9C, first part (done 2026-10-11): developer commands are compiled out of shipping builds; the asset setup commands are no longer part of the test suite and fail on missing assets.
- 9B acceptance and 9C packaging (2026-10-11): real tests for the shop purchase, zombie death data on server and client, boss spawns and rescues, and missing setup assets. The game target compiles and packages as Development and Shipping; the packaged game has both playable maps and everything loaded by path (a first package lacked 157 of 248 assets); developer commands are absent from the shipping executable. Under 100 ms latency a trigger pressed right after asking for the firearm now fires. Measured there and fixed in the next step: the server fired fewer automatic shots than the owner showed.
- Rate of fire, shot origin, packaged function checks (2026-10-11): automatic weapons reach their configured rounds per minute in the tested regular server frame-rate scenarios, with bounded catch-up after long hitches (before: 59 to 100 % of it, depending on the frames); no tuning value changed, but the real rate of fire is higher than the game had so far and wants a play test. With the press the owner says how far along its eye's easing is and the server follows within a tenth of a second: the first shot after a crouch, a stand-up or a slide now leaves from the owner's eye height, the second within 9 cm, the rest within 2 cm. In the packaged Development duel every firearm, both knife attacks, both flashbang throws and a round with the upgrade selection ran; the shipping package starts and does not crash, more could not be read from it.
- 9D, first part, hit feedback (2026-10-11): one hit marker at the crosshair, centred, with a distinct marker, word and sound for a head shot; confirmed hits reach the shooter as events, once each, and nobody else. Tested with host and client and in the packaged game; not yet seen or heard by a person.
- 9C.3, shown against real fire (2026-10-11): under 100 ms latency and 2 % loss the owner no longer shows shots the magazine does not hold or goes on showing fire after the server's reload (86 real to 87 shown shots, before 86 to 127), and every shot fired while the eye eases leaves within 0.5 cm of the owner's eye (before up to 32 cm for a crouch during held fire).
- 9C.3 addendum (2026-10-11): shot confirmations and eye hints say which shot they are about (weapon, burst, shot in the burst, filling of the magazine). Before, a confirmation arriving after a weapon switch or a reload could leave the owner assuming an empty magazine and a dead trigger, a hint of an earlier burst could be used for a shot of the next, and one lost confirmation shifted every later comparison by a shot. Measured with loss in both directions: 81 arrived confirmations all paired with their own shot, 24 of 26 easing shots at the same height, two off by 3.4 and 1.2 cm because their hint came a frame late.
- Open: the rest of 9C (dedicated server pose, prediction under latency, skins and late join), 9D presentation and sound with E16, 9E load and overall acceptance, and every check that needs a person at the screen or ears. Phase 9 is not finished. See the end of `docs/Phase9.md`.

### Remaining Phase 9 packages (open; detailed acceptance in the completion plan)

- 9C.1: actual dedicated-server gameplay pose and movement under latency/loss/corrections.
- 9C.2: complete replicated appearances, scale and late join; disconnect/rejoin edge cases.
- 9C.3: done as far as measured (see above); left: the eye hint that arrives a frame after its shot, knife and flashbang origins, narrow cover between separate processes.
- 9D.1: finish the active marker fixes (one renderer, correct centre, reliable confirmed-event handling) and E16 decals.
- 9D.2: all hands, grips, firearm parts, ADS/scope, knife/throw/reload transitions in owner/opponent views.
- 9D.3: enemy hit zones, boss warnings/attacks, death-to-ragdoll and actual play on the existing arena.
- 9D.4: suitable weapon audio and one real listening acceptance.
- 9D.5: menu/settings/control usability, including confirmation and timed rollback for display changes.
- 9E.1: packaged CPU/GPU/frame/memory/network measurements and fixes for measured bottlenecks.
- 9E.2: Shipping on-screen/menu/map/mode/connection acceptance beyond process-alive checks.

Existing E01-E18/R01-R10 evidence remains in `docs/Phase9.md`. New findings and successful fixes must be tracked against the actual implementation commit; a planning checkbox does not close them.

## Phase 10: Online private duel, server, ranked, network hardening, and V1 release

Private duels, dedicated server deployment, ranked and MMR systems, network hardening, and release validation.

- 10A: supported external hosting/joining and a real two-PC internet match/rematch, including failure recovery.
- 10B: a deployable Server target, reproducible server artifact and verified hosted lifecycle. Current Game/Editor targets do not satisfy this.
- 10C: durable identity/results, ranked/MMR and authoritative disconnect/result rules. This stays a planned deliverable; it is not required by the listed core private-friend V1 criteria and must not be presented as already implemented.
- 10D: reproducible packaging and clean-PC/player-facing delivery, followed by final release acceptance.

A first private build may use verified listen hosting and defer dedicated deployment/ranked explicitly. That does not mark the whole Phase 10 complete. Invite codes, no-port-forwarding services and Zombie co-op require their own scope/hosting decisions; the current public Survival mode is solo.

## V1 acceptance criteria

V1 is complete only when two players can launch ArenaDuel, create or join a duel, select Shadow, Warden, or Rift, select a weapon, enter the accepted `L_ArenaDistrict` arena, play a first to 5 match with 90 second rounds, use gunplay, advanced movement, and character abilities, receive match upgrades, finish the match, see results, and start a rematch over real networked play.

The V1 release must also contain four usable weapons, three usable fighters, one complete arena, a functional HUD, audio feedback, visual feedback, FPS settings, and acceptable multiplayer behavior.

For detailed completion checks, evidence types, scope decisions and test budgets, see [V1CompletionPlan.md](V1CompletionPlan.md). Existing saved settings, failure messages and the ready-again rematch flow must be validated rather than recreated. Extra maps, account progression and a larger weapon/upgrade catalogue are not automatic V1 blockers.
