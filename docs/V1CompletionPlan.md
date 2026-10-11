# V1 completion plan

Reviewed on 2026-10-11 against `22fa35e1bdb675b82e181cd425413dc720ee8e42`.

This is a static review of the tracked text source, configuration, scripts and current project documentation. It does not claim a new Unreal build, a play session, a recording or a listening check. Binary asset appearance and local work not pushed to GitHub were not inspected. Implementation evidence remains in [Phase9.md](Phase9.md); re-read the current branch before starting a package.

## What already exists

Do not rebuild these systems just because older roadmap paragraphs describe their first version:

- Three fighters, their GAS abilities, advanced movement, four firearms, knife and flashbang.
- A main menu, direct-address listen-server hosting/joining, character selection and ready state.
- Network/travel failure messages and saved local settings.
- First-to-five rounds, the 90-second clock, interim overtime and three match upgrades.
- Solo Zombie Survival with five enemy types, waves, bosses, purchases and restart/game-over flow.
- Cook rules and Development/Shipping packaging fixes. The project report records 118 passing tests; that result was not rerun by this review.
- The restored server automatic-fire schedule. Its effect on practical weapon balance still needs a play test.
- Head/body hit-marker logic and generated sounds, and since `d9c09ff` the fixes for the three findings of their review (one renderer, centred, confirmed hits as events). Tested with host and client and in a packaged Development game; not yet seen or heard by a person.

## Gap classification

| Area | Evidence at the reviewed commit | Classification | Owner |
| --- | --- | --- | --- |
| Public weapon selection | Character Select exposes fighter/ready choices; weapons are assigned in `ApplyLoadoutFromArchetype`. No public pre-match weapon choice is implemented. | Missing feature in the stated V1 criteria; weapon-access rule needs a decision | 8C |
| Weapon access | `GAME_DESIGN.md` says equal access to the pool. Current loadouts assign SMG to Shadow, Shotgun to Warden, DMR/Rifle to Rift. | Design/code conflict; preserve current kits until explicitly resolved | 8C |
| Final overtime | The implemented rule remains unlimited overtime, next kill wins. The design names a future objective without defining it. | Product decision, then feature or explicit design deferral | 8D |
| Nine-round limit | First to five already ends within nine decided rounds. | No extra cap needed under current rules; revisit only if draws/replays are introduced | 8D |
| Real match/upgrade balance | Restored RPM changes practical time-to-kill; real ability cooldown/reload comparisons and several upgrade UI cases remain unaccepted. | Gameplay acceptance, not a reason to change values automatically | 8E |
| Dedicated gameplay and movement | R01/R02 remain open; the source has only Game and Editor targets. | Validation gap; a distributable server is separate work | 9C / 10B |
| Zombie presentation on late join | Death-data tests exist; mesh/material/scale application and late join remain open under R03. | Validation gap; do not assume a bug without a failing case | 9C |
| Predicted weapon feedback | Fixed 2026-10-11 (9C.3): the owner shows no shot the magazine does not hold and stops its shown fire with a reload the server started; eye hints are tied to the shot of the burst. Measured at 100 ms and 2 % loss: 86 real to 87 shown shots, eye within 0.5 cm for all 25 shots fired during an ease. | One shown shot at the edge of a very short burst remains, measured and accepted as a limit; knife and flashbang origins unchecked | 9C |
| New hit feedback | Fixed in `d9c09ff` (2026-10-11): the legacy Canvas marker is removed, the UMG marker and its pivot are the root's centre, and confirmed hits reach the HUD as sequenced events that survive several results before one frame. | Code and automated tests done; rendered views at more than one resolution and listening are open | 9D.1 |
| Impact decals | E16 remains open: world-hit classification and moving/disappearing surfaces. | Existing code finding | 9D.1 |
| Hands, weapons, zombie motion and arena | R06/R07/R08 still lack complete visual acceptance in both views. | Acceptance gap; no compulsory animation-system rewrite | 9D.2 / 9D.3 |
| Weapon audio | A single `FireSound` points to the template grenade-launcher sound for all four firearms; new hit sounds have not been listened to. | Presentation work and listening acceptance | 9D.4 |
| Settings recovery | `ApplyLocalSettings` applies/saves resolution immediately; no project confirm/revert flow is implemented. | Small missing recovery flow and real settings validation | 9D.5 |
| External friend play | Direct IP exists, but no online session/invite/relay integration or two-PC internet acceptance is recorded. | Required connection-path acceptance; convenience service depends on chosen hosting route | 10A |
| Shipping usability | Processes started headlessly without crashing; menu, map travel and actual Shipping connection were not confirmed. | Release acceptance gap | 9E / 10D |
| Performance | Existing measurements are editor GPU figures; packaged CPU/GPU hitches, memory and network load remain unmeasured. | Performance acceptance gap | 9E |
| Release delivery | No dedicated package/release runner or player-facing installation/join guide is tracked. | Reproducibility and delivery work | 10D |
| Ranked/MMR | No account, persistent ranking or matchmaking implementation is tracked. | Planned Phase-10 extension; separate from the first private friend build | 10C |
| Zombie co-op | Gameplay supports several players, but the menu opens Survival without `listen`; the documented public mode is solo. | Optional scope decision; do not promise a co-op frontend that does not exist | Optional |

## Phase 8C: public weapon selection

**Goal:** the public pre-match flow satisfies the weapon-selection part of V1.

- Resolve equal-pool access versus the current fighter loadouts before changing which weapons a fighter can use. Default while undecided: keep existing loadouts.
- Define when selection is allowed, its fallback, ready-state locking and persistence across rounds/rematches. Fit the existing automatic-ready flow rather than silently removing it.
- Implement server-validated choice, replicated display and the correct weapon on the spawned pawn. Admin commands do not count as public selection.
- Reject invalid, stale, other-player and spectator choices. Leave damage, ammo, RPM and ability tuning unchanged.

**Done when:** two connected players select independently through the actual UI, both see the accepted choices, readiness/countdown/respawn work, and match reset follows the documented rule. Relevant automated request/state tests and one live menu-to-round check pass.

## Phase 8D: final match contract

**Goal:** timer, overtime, simultaneous deaths and maximum rounds have one documented rule.

- Keep 90 seconds and first to five.
- For the first private build the already documented next-kill overtime can remain. To close the final-objective requirement, specify and implement its win condition/visibility/ties or explicitly amend the release design to retain the interim rule. Do not invent an objective during a bug fix.
- Preserve the current first-processed-death rule unless a rule change is explicitly chosen.
- Avoid an extra nine-round counter under the existing decisive rules. If an objective adds draws, first define whether a draw consumes a round and how a match terminates.
- Check timeout/kill/reset/disconnect races and exactly-once scoring after any rule change.

**Done when:** design, code, HUD and tests agree, and no active round becomes unwinnable under the adopted rules.

## Phase 8E: fighter, weapon and upgrade play acceptance

**Goal:** accept the existing gameplay at its actual configured values.

- Play all fighters and weapons from both host/client roles; compare restored automatic RPM and time-to-kill.
- Exercise same/different upgrades, maximum cards, timeout, real ability cooldown reduction and reload timing against the base case.
- Check kit changes and respawns for stale cooldowns, duplicate abilities, leftover utility actors or match-upgrade state.
- Record specific balance problems before proposing value changes.

**Done when:** a complete first-to-five match and a rematch succeed with normal actions and agreed gameplay feel. More upgrade kinds are an extension, not a replacement for accepting the three existing ones.

## Phase 9C: remaining network and authority checks

### 9C.1: server pose and movement (R01/R02)

- Run two remote clients against a process that actually uses `NM_DedicatedServer`. A headless listen server is not proof of this mode.
- Compare authoritative per-bone hit zones with visible locomotion, reload, knife and throw poses.
- Test sprint/slide/slide-jump, wallrun/kicks, vault/mantle, stamina and GAS traversal under 50/100/150 ms latency, packet loss and forced corrections.
- Capture correction counts, visible snapping, final position/state/stamina and unintended extra jumps/ability uses. Fix only reproduced state/replay failures.

**Done when:** a reproducible test report shows consistent authoritative outcomes and acceptable movement on both clients, with remaining limits named.

### 9C.2: presentation state and late join (R03/R10)

- Join during character selection, countdown, a round, upgrade selection and an active Survival wave.
- Check complete fighter/zombie type, mesh, material, scale, pose, health/death and held-item state on the new client.
- Include delayed updates, loss and a death during initial replication. Keep type variation stable where it affects silhouettes or poses.
- Verify participant/spectator rules, host quit, hard client disconnect and rejoin. Do not add spectator promotion or mid-round respawn without a rule decision.

**Done when:** the late client converges to a complete correct state without duplicated visuals/actions, stuck menus or score resets from spectators.

### 9C.3: predicted fire and eye position

Status 2026-10-11: the first, second and fourth points are done with measurements in `docs/Phase9.md` ("Phase 9C.3"). Open from this list: rapid switches under latency, knife and flashbang origins, and shots over and beside narrow cover with changing frame times between separate processes.

- Reconcile locally predicted ammo/action feedback with accepted server shots; test the end of a magazine, auto reload, burst edges and rapid switches.
- Re-check remaining eased-eye differences, including a crouch during held fire and knife/flashbang origins.
- Test over and beside narrow cover with changing frame times and latency. Clamp/reject invalid hints; the server remains authoritative.
- Compare matching shot sequences and timing, not unmatched total counts from different burst windows.

**Done when:** persistent false firing/reload states are gone and any remaining prediction or aiming discrepancy has a measured, explicit acceptance limit.

## Phase 9D: presentation and usability

### 9D.1: confirmed hit feedback and impact marks

The marker package is done in code and automated tests (`d9c09ff`, `docs/Loadout.md` "Hit confirmation"); the first three points below are met by tests, the rendered views and the listening test are open, and E16 is not started:

- One marker renderer, correct crosshair/animation-pivot centre and confirmed-hit delivery that survives multiple results before one HUD frame.
- Head feedback is not immediately replaced by weaker feedback; sounds are deduplicated and owner-only. Reset cleanly on pawn/HUD changes.
- Check host and connected client, mixed shotgun hits, all firearms, zombies and the knife's existing body-hit rule.
- Fix E16 with correct hit classification and surface lifetime handling. Flesh, a miss and a destroyed barrier must not leave a false/floating world mark.

**Done when:** targeted event/geometry/owner tests pass and rendered views at more than one resolution show correct placement and readable feedback.

### 9D.2: first-person arms and held weapons (R06)

- Only hands/forearms are visible in the owner's first-person view, with no exposed cuts, twisted wrists, broken fingers or missing polygons.
- Accept grips and support-hand contact for all four firearms, knife and flashbang. Check Hip/ADS/scope, fire/recovery, draw/switch, magazine reload, pump/bolt and both throw lengths.
- Include sprint, crouch, slide, rapid switches during actions, extreme pitch, wall proximity and slow motion; inspect both owner and opponent views.
- Preserve approved weapon/FOV tuning and gameplay. Use authored clips, IK or existing procedural motion according to the demonstrated problem; Blender-made first-person clips are not an automatic requirement.

**Done when:** the action matrix has viewed evidence for each weapon and no unresolved clipping/pose/attachment blocker.

### 9D.3: zombies and the existing arena (R07/R08)

- Inspect all five enemy variants, actual head/torso/limb zones, feet, hit reactions, attack contact timing and movement through doors/ramps.
- Watch boss charge/slam warnings and impact, running/standing/headshot death and clip-to-ragdoll transitions on both machines.
- Play the existing `L_ArenaDistrict` routes, roofs, traversal surfaces, spawn lines and cover; inspect opponent readability in dark/bright areas.
- Preserve the established map geometry unless a reproduced collision/path/readability issue requires a focused change. A second arena is not required.

**Done when:** a normal duel match and a Survival boss encounter have been watched and the relevant action/geometry problems are closed.

### 9D.4: audio (R05)

- Give each firearm a fitting shot/reload identity instead of relying on one template fire sound, unless a temporary shared sound is explicitly accepted.
- Check weapon, equipment, footsteps, abilities, impacts, head/body confirmation, flash ringing, zombie attacks and boss warnings for missing/double sounds.
- Verify spatial attenuation and near/far/left/right behaviour, plus volume/concurrency during sustained fire. UI confirmation stays local to the shooter.
- Run one short timestamped listening checklist. Import success, a waveform or a log is not a sound-quality acceptance; obtain human listening only for the part tools cannot judge.

**Done when:** mandatory feedback is audible/readable at normal master volume without clipping, excessive overlap or incorrect spatial placement.

### 9D.5: menus, settings and controls

- Validate existing host/join/failure messages and keyboard/mouse navigation; fix actual failures rather than rebuilding the menu.
- Check settings persistence across menu, map travel, respawn and application restart.
- Add a temporary resolution/fullscreen preview with confirmation and timed return to the last known-working mode. Save confirmed display values consistently with the project settings save.
- Check UI sizing/focus, fullscreen/borderless/windowed behaviour, supported resolutions and the normal menu input lock.
- Provide concise controls and mode-rule information, including abilities, purchases and movement keys. Key rebinding, separate audio buses and additional accessibility options remain extensions unless needed for an accepted usability problem.

**Done when:** a new player can navigate both modes, recover from a rejected display change and understand the available controls without an editor or developer console.

## Phase 9E: performance and release acceptance

### 9E.1: measure and optimize (R09)

- Choose and record the supported test hardware, resolution, quality preset and target frame budget before claiming a performance pass.
- Measure packaged client and host/server CPU/GPU frame times, hitches, memory and network traffic during a duel and one defined busy Survival scenario.
- Include first use of gunfire/flash/abilities/ragdolls and cleanup of corpses, decals and temporary actors. Use one baseline and targeted repeats after relevant fixes.
- Optimize the measured bottleneck; preserve gameplay and the accepted visual quality. Add a usable quality fallback only when measurements justify it.

**Done when:** agreed frame/load targets are met and repeated rounds/waves do not cause continuing memory/actor growth.

### 9E.2: complete Shipping evidence

- Start the actual Shipping package on screen and demonstrate main menu, map travel, both modes, HUD, settings and a real connection.
- Validate the chosen port/connection check against a known-working Development host before trusting a negative result.
- Inspect current runtime assets, including both new hit sounds. A Development package built after the hit feedback was checked by log on 2026-10-11 (55 paths, 0 missing, both sounds asked for); that is not a check of the Shipping package or of any later one.
- Keep process-alive, loaded-assets, played-actions, viewed-footage and listening evidence separate.

**Done when:** the actual release candidate completes the menu-to-game paths and does not rely on Development commands.

## Phase 10: external play and delivery

### 10A: a friend joins over the internet

- Choose a supported route: existing listen server with clear address/port instructions, a session/relay route, or a reachable dedicated host. Do not silently select a provider.
- Test with two PCs on separate internet connections, using matching packaged builds. Local processes only prove local transport.
- Make the host address/connection status truthful. If invite codes are advertised, implement session discovery, private admission and create/join/leave cleanup; a code alone does not provide network reachability.
- Exercise wrong address, incompatible version, full lobby/spectator behaviour, failed travel, client exit, host exit, rejoin and rematch through the public flow.
- The current Survival menu is solo. Add explicit Survival host/join only if co-op is selected as a product requirement; otherwise describe the mode as solo.

**Done when:** a friend can install the build, join through the documented public route, finish a match and play a rematch without developer commands.

### 10B: deployable dedicated hosting

This remains a planned Phase-10 deliverable, separate from 9C's dedicated gameplay validation.

- Add a Server target, server startup map and reproducible build/cook/run instructions once the supported toolchain is decided. The current repo has no Server target.
- Verify the engine-build requirement before any migration; the existing engine association stays unchanged until that decision is made.
- Provide room allocation/configuration, capacity/admission, health/readiness and clean shutdown/restart for the chosen deployment.
- Prove a deployed server with two external clients and no local playing pawn.

**Done when:** the server artifact and deployment are reproducible and a full remote match completes. If the first private build ships on listen hosting, state this deliverable as deferred rather than complete.

### 10C: ranked and persistent services

Ranked/MMR is already a roadmap target, but it is absent from the listed core friend-play V1 acceptance criteria. Keep the two releases explicit.

- Define player identity, durable server-owned match records and rating rules before building a ranked screen.
- Make result submission idempotent and define disconnect/abandon/rejoin handling, draws if supported and incompatible versions.
- Define matchmaking/cancellation/queue failure and private-versus-ranked separation. Do not award ratings from client claims.
- Cover duplicate/retried results and interrupted matches in service-level tests.

**Done when:** the adopted ranked contract is implemented end to end. For a private friend release, leave ranked unavailable and documented as deferred; do not claim Phase 10 or ranked complete.

### 10D: reproducible delivery and final match

- Add a package/smoke-test runner for the selected configurations, runtime asset inventory and build version.
- Test a clean checkout with Git LFS and documented build dependencies; test the packaged game on a PC without Unreal/Visual Studio installed and verify runtime prerequisites.
- Supply player-facing launch, host/join, controls, settings-reset and troubleshooting instructions.
- Audit the distributable for development tools/commands, local-only paths and the actual licenses/notices required by shipped assets.
- Final flow: launch, host/join, fighter/weapon selection, readiness, first-to-five match, overtime under the adopted rule, upgrades, match result, rematch, clean exit. Also complete the accepted solo Survival/boss/shop/game-over/restart flow.

**Done when:** the same versioned artifact completes those flows on the target PCs and its remaining limitations are explicitly listed.

## Suggested execution order

1. Done 2026-10-11 for code and automated tests: the 9D.1 marker package. Its rendered views and listening test remain, with E16, in 9D.1.
2. Close 9C.1/9C.2/9C.3 and record measured limits.
3. Accept 9D.2/9D.3/9D.4/9D.5 in small reviewable packages.
4. Resolve and implement 8C/8D; perform 8E on the adopted rules.
5. Measure 9E.1 and prove 9E.2 on the resulting release candidate.
6. Prove 10A and 10D for the first friend-playable build.
7. Deliver 10B/10C when their deployment/ranked scope is selected; do not relabel them complete to shorten the plan.

This is a work breakdown, not a guaranteed count of prompts or hours.

## Verification budget

- During implementation, run the focused tests for the changed behaviour.
- Run the full `ArenaDuel` regression filter once after the final code fix in a package. Repeat only after new relevant changes/failures.
- Use the appropriate build for the actual changes. Documentation-only work does not require Unreal compilation.
- Re-cook/re-package after changes that affect the game build or cooked content; do not repeat unrelated long Survival/network runs after a marker-transform adjustment.
- Plan one defined longer load run in 9E rather than using repeated unstructured background runs as evidence.
- Give a concrete progress/blocker update at least every 30 minutes in a longer implementation package.
- Preserve local edits and productive assets. A recorded action is not automatically a viewed or passed action.
- Prefer MCP/API-scoped interaction. The user's restriction on global desktop input/window changes still applies until explicitly changed.

## Scope decisions and optional extensions

Resolve these before their implementation, without blocking independent fixes:

- Equal weapon pool versus current fighter loadouts.
- Final overtime objective versus an explicitly retained interim rule.
- Listen hosting, session/relay or dedicated hosting for the first external build.
- Solo Survival versus a public co-op mode.
- First private release versus a release that includes ranked.

Not automatically required for the first private build: extra maps, additional fighters/weapons, a larger upgrade catalogue, a standalone rematch button where the existing ready flow already rematches, a kill/death scoreboard, replay/spectator tools, voice chat, cosmetics, achievements, progression outside a match, a new launcher, a different engine version or a compulsory Blender animation rewrite.

## Official Unreal references

- [Dedicated server setup](https://dev.epicgames.com/documentation/en-us/unreal-engine/setting-up-dedicated-servers-in-unreal-engine): server-mode/toolchain/target guidance.
- [Session interface](https://dev.epicgames.com/documentation/en-us/unreal-engine/online-subsystem-session-interface-in-unreal-engine): create, join, leave and destroy lifecycle.
- [Game user settings](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/UGameUserSettings): persisted display/settings API; implementation must be checked against the installed engine.
