# Architecture

## Established principles

1. ArenaDuel is multiplayer first.
2. Gameplay authority lives on the server.
3. Client prediction may improve responsiveness but must not replace server validation.
4. Foundational systems belong in C++.
5. Designer facing configuration and presentation may use Blueprint and Data Assets where appropriate.
6. Future abilities, attributes, costs, cooldowns, buffs, and debuffs should use the Gameplay Ability System where appropriate.
7. The generated Blank project already includes the Enhanced Input module and default input classes. ArenaDuel's Phase 3 Input Actions and Mapping Context are serialized Unreal assets; their bindings remain data driven rather than hardcoded in C++.
8. Systems should avoid unnecessary Tick, RPCs, hard references, and broad ownership.
9. Gameplay code must not depend on cosmetic VFX execution.

## Planned source organization

Create directories only when real code needs them. The likely long term module areas are `AbilitySystem`, `Characters`, `Combat`, `Components`, `Core`, `Game`, `Input`, `Player`, `UI`, and `Data` under `Source/ArenaDuel/`.

## Planned content organization

Create content folders in Unreal Editor only when real assets need them. The likely root is `Content/ArenaDuel/`, with focused areas for characters, abilities, animation, audio, data, effects, input, maps, materials, UI, and VFX.

## Dependency plan

The initial project should use only engine supplied dependencies. Future milestones are expected to evaluate `GameplayAbilities`, `GameplayTags`, `GameplayTasks`, and `EnhancedInput`. Online services, dedicated server hosting, and third party plugins require separate evaluation before adoption.

## Gameplay Framework Foundation

### AArenaDuelGameMode

`AArenaDuelGameMode` derives from `AGameMode` and owns authoritative framework class relationships. It exists only on the server. It configures the ArenaDuel GameState, PlayerController, PlayerState, and Character classes. For current two-player playtests it also awards a round win on death and restarts both players after a short delay, ending the loop when a player reaches five wins. This is a development round loop, not the full Phase 8 match system.

### AArenaDuelGameState

`AArenaDuelGameState` derives from `AGameState` and carries replicated development-round number, active state, and last round winner slot. It exists on the server and as a replicated state object on clients.

### AArenaDuelPlayerController

`AArenaDuelPlayerController` derives from `APlayerController` and establishes the per player control type. The server has one for each connected player and a client normally owns only its own instance. Future local control flow and UI coordination belong here. Shared match data does not.

### AArenaDuelPlayerState

`AArenaDuelPlayerState` derives from `APlayerState` and carries replicated GAS attributes plus the temporary duel slot and round wins. It exists on the server and is relevant to clients. Character selection/readiness live here; match-upgrade data remain future work.

### AArenaDuelCharacter

`AArenaDuelCharacter` derives from `ACharacter` and represents the possessed physical fighter. The server owns the authoritative actor, the owning client controls its local instance, and remote clients receive the replicated Character. Standard `UCharacterMovementComponent` networking remains responsible for locomotion. The class currently provides only a local first person camera, camera relative movement hooks, look hooks, and jump hooks.

The current camera is intentionally local and cosmetic. It does not force remote players to use first person visuals. A later presentation phase can add first person arms and weapons for the owning client while keeping a separate full world body and weapon representation for remote players.

The first person camera attaches directly to the Character capsule at a 64 centimeter local Z offset. This places the initial eye point above the capsule center without introducing a Spring Arm, camera lag, or replicated cosmetic transform.

## Phase 6 combat foundation

`AArenaDuelPlayerState` owns the replicated `UAbilitySystemComponent` and `UArenaDuelAttributeSet`. Health and MaxHealth are GAS attributes initialized to 100, with replicated clamping. The Character initializes GAS actor info with PlayerState as OwnerActor and Character as AvatarActor. Server weapon traces apply instant GAS health changes to player Characters only. Body and head damage use data on the weapon definition, and shotgun pellets apply independently. A replicated Character death state stops movement, firing, aiming, reload completion, and further gameplay input. The current development round loop destroys and replaces both pawns after a round, which reinitializes health and combat state while retaining PlayerState round wins. Full match rules and health regeneration remain out of scope.

## Competitive HUD and development rounds

The local `AArenaDuelMovementDebugHUD` creates one native `UArenaDuelHUDWidget`; the widget displays live GAS health, movement stamina, weapon state, stable PLAYER 1/PLAYER 2 duel labels, round wins, and round number/state. Health/ammo/death are not duplicated in Canvas. Canvas remains responsible for the crosshair, hitmarkers, and opt-in F3 movement diagnostics. The outer frame and decorative side arcs have been removed. Gameplay HUD values refresh at 10 Hz; cached match data refreshes at 2 Hz. Setters avoid rewriting unchanged text or bar values.

When an authoritative Character dies, GameMode ends the round, locks movement and combat for both players, increments the surviving player's replicated win count, and restarts both player controllers after three seconds using normal PlayerStarts. Client input checks replicated round state and the server independently rejects weapon actions during the break. At five wins, replicated GameState match completion and winner slot replace the normal round timer; both players see a centered final result for five seconds, then the server resets scores and returns both players to CharacterSelect with ready cleared. This remains a development flow without a round timer, ranked-forfeit rules, or a final online frontend.

## Phase 7 ability foundation

`AArenaDuelPlayerState` remains the owner of the replicated GAS ASC. It grants one native `UArenaDuelGA_ShadowStep` and one `UArenaDuelGA_VeilWall` spec on authority, guarded by class lookup so the specs persist without duplication when the Character is replaced. Each newly possessed Character refreshes the ASC ActorInfo with PlayerState as OwnerActor and the current Character as AvatarActor. Both abilities use native gameplay tags and duration GameplayEffects for their 5 second and 12 second cooldowns.

Q activates predicted, collision-respecting CharacterMovement launch locomotion in the current planar input/acceleration direction, falling back to control-forward when no movement input is present. E asks the server to create a fixed-distance, three-second replicated visual wall. The wall has no collision, so it is not a player barrier and cannot intercept the existing hitscan traces. Both abilities reject missing GAS/avatar state, dead Characters, inactive rounds, and locally open admin menus. Death cancels running abilities. Round end destroys temporary Veil Walls, and the authoritative next-round pawn reset cancels abilities and removes only Shadow cooldown effects without clearing the persistent specs or player round wins. The gameplay HUD queries the actual ASC cooldown effects for Q and E.

The native HUD also shows a temporary match-result overlay from replicated GameState winner state and final PlayerState scores. It hides the local defeated label while the result is active. This adds presentation only; match authority remains in GameMode/GameState.

### Phase 7B character archetypes and Warden kit

`AArenaDuelPlayerState` stores the replicated `EArenaDuelCharacterArchetype`; it defaults to Shadow and survives pawn replacement and fresh-match resets, but is not saved between application runs. The PlayerState-owned ASC grants only the selected kit's two ability specs. Archetype replacement is shared by public self-owned selection and the host-only development admin command: it cancels active abilities, destroys that PlayerState's temporary ability actors, removes kit specs and cooldown effects, changes the archetype, grants the new kit, and reinitializes the current AvatarActor. Character input invokes generic Primary (Q) and Secondary (E) methods, while the HUD reads archetype-specific names and real GAS cooldowns.

Shadow remains unchanged: Q Shadow Step has a five-second cooldown; E Veil Wall has a twelve-second cooldown and creates a three-second replicated visual occluder with no collision. Warden Q Arc Barrier has a fourteen-second cooldown, 350 server-owned health, and a five-second maximum lifespan. Its cyan physical cover blocks Pawn movement and `ECC_Visibility` hitscan; the weapon server applies the weapon's base body damage to it and stops that trace there. Warden E Burst Leap has a seven-second cooldown and uses a predicted CharacterMovement launch. Neither Warden ability deals damage. Round cleanup removes Veil Walls and Arc Barriers, and fresh rounds clear cooldowns while preserving the selected kit. Rift is only an enum/display value; it has no granted abilities.

### Public pre-match character selection

GameMode owns transitions through the replicated GameState `EArenaDuelMatchPhase`: CharacterSelect, Countdown, InRound, RoundBreak, and MatchResult. The phase gates authoritative weapons, GAS activation, normal damage, and movement. Existing round-active accessors require InRound. The public PlayerController selection/ready RPCs resolve only the requester's owned PlayerState, allow Shadow/Warden only in CharacterSelect, and reject selection while ready. They share the authoritative kit-replacement implementation with admin commands; no target slot is accepted from clients.

PlayerState replicates its selected archetype and ready state. Two ready connected duel players start a three-second countdown plus a brief FIGHT beat, represented by one synchronized server-end timestamp. New combat pawns preserve archetypes and reset health/ammo/cooldowns. Normal round deaths retain the three-second round break. After the five-second match result, scores return to 0:0 and round one, both ready flags clear, and selection reopens rather than automatically starting combat. Disconnect returns the remaining player to selection without ranked-forfeit rules.

The persistent local PlayerController owns one focusable native `UArenaDuelCharacterSelectWidget`, built in Initialize before Slate construction. Its fullscreen 1920x1080 reference layout scales uniformly: Player 1 cyan left card, Player 2 violet right card, open central VS/countdown, stat/ability cards, two-entry roster, and ready controls. Only the local side is interactive. A/D or arrows switch character, Enter toggles ready, Escape cancels ready without leaving the session. The timer refreshes at 5 Hz only while displayed. Portraits/background/icons are original procedural development artwork, not final character models or a baked screenshot. Gameplay HUD visibility changes without rebuilding; F1 admin opening is denied during selection/countdown.

## Development admin control

`AArenaDuelPlayerController` owns the F1 development menu so it survives pawn replacement. Its native `UArenaDuelAdminWidget` tree is built in C++, exists only while open, and refreshes readouts at 5 Hz. Opening it uses Game and UI input mode, shows the cursor, blocks local character/weapon input, and never pauses the multiplayer world. Closing restores the prior move/look ignore state and Game input mode. F1 and Escape close the menu.

The server command endpoint rejects Shipping builds and accepts only standalone authority or the local listen-server host. It independently checks authority and local-controller/net mode, validates slot and numeric inputs, and resolves targets by replicated DuelSlot. The remote client's server-side PlayerController is not authorized. God Mode, Infinite Ammo, and Infinite Stamina are server-owned replicated development flags on PlayerState; they are false by default, remain useful across round pawn replacement, and are not saved across application runs. Admin Kill and zero Health use the normal authoritative death path; God Mode blocks only ordinary damage. Reset Player is available only while alive and restores health, movement intent, stamina, combat actions, and the normal weapon ammunition; dead-player recovery uses the round restart path.

The menu provides player, weapon, round, movement, local debug, and network readouts/actions. Its fullscreen native UMG tree uses a fixed header/sidebar/footer, a scrollable page area, segmented target selection, structured status cards, and shared action-button styling. It remains built once and refreshed at 5 Hz only while open. Hit-zone visualization is local debug drawing only and does not change collision. The admin menu is a prototype developer tool, not authenticated dedicated-server administration.

## Enhanced Input lifecycle

`AArenaDuelCharacter` adds its configured Mapping Context from `PawnClientRestart()`, not `BeginPlay()`. Unreal calls this lifecycle on the owning client when a player controlled Pawn is restarted, which covers initial possession and future Pawn replacement. The code checks local control, resolves the LocalPlayer subsystem, and checks `HasMappingContext()` before adding the context. Remote clients and dedicated servers do not register local input mappings.

## Ownership relationship

Server:

1. `AArenaDuelGameMode`
2. `AArenaDuelGameState`
3. One `AArenaDuelPlayerController` per player
4. One `AArenaDuelPlayerState` per player
5. Authoritative `AArenaDuelCharacter` actors

Client A:

1. Replicated `AArenaDuelGameState`
2. Client A's owned `AArenaDuelPlayerController`
3. Relevant replicated `AArenaDuelPlayerState` objects
4. Relevant replicated `AArenaDuelCharacter` actors

Remote PlayerControllers are not assumed to exist on every client.

## Phase 4 movement foundation

`UArenaDuelCharacterMovementComponent` is the Character's native movement component subclass. The Character constructor installs it through Unreal's default-subobject override, so existing CharacterMovement replication and server authority remain in use. It adds input-driven sprint and crouch state, a custom slide mode, slide jump momentum retention, air-control tuning, bounded momentum, stamina drain and regeneration, and basic collision-query based wall-run, wall-jump, vault, and mantle modes. There are no custom movement RPCs and no manual transform replication. The current mode transitions are intentionally small and data driven. Detailed saved-move compression or production traversal tuning will be validated before competitive use.

Phase 4.1 adds UE 5.8.3 CharacterMovement saved-move prediction. Sprint intent uses `FLAG_Custom_0`; crouch/slide intent uses `FLAG_Custom_1`; advanced jump intent uses `FLAG_Custom_2`, with `FLAG_Custom_3` selecting wall jump instead of slide jump. `FSavedMove_ArenaDuel` prevents move combination across intent or advanced-jump boundaries, restores intent during replay, and `UpdateFromCompressedFlags` reconstructs it on the server. This keeps the server authoritative without per-frame RPCs. The saved-move implementation is deterministic for the current movement inputs, but release readiness still requires real-latency profiling.

Wall runs now have a configurable same-wall reattach cooldown. Slide boost is clamped after the boost is applied. Traversal checks top-surface height, destination capsule clearance, blocking hits, and valid floor state before returning to walking.

Phase 4 local behavior validation is complete. The editor suite now covers multi-frame air-control trajectory, stamina drain and regeneration, wall-run entry rejection and exit, same-wall reattach lockout, wall jump behavior, vault and mantle progression, invalid traversal cases, and capsule collision safety. Advanced-action network validation remains incomplete and is the next checkpoint.

`L_Phase4MovementTest` is a temporary flat development map containing labeled sprint, slide, vault, mantle, and wall test geometry. It is not the final arena.

## Phase 5 weapon foundation

`UArenaDuelWeaponComponent` owns the current weapon selection and server-authoritative runtime ammunition. Each of the four weapon definitions has an independent magazine and reserve state, so switching does not refill ammunition. Automatic fire is represented by a held state on the server and a server timer at the weapon cadence, not by per-frame fire RPCs. Hits are classified as head, body, world, or miss. The server uses the Character pawn view location and controller control rotation, then sends a compact shot confirmation to the owning client. Shotgun shells perform eight server traces with one ammo decrement. The first-person weapon is a local cosmetic primitive only; remote characters do not receive cosmetic camera or weapon transforms.

Phase 4 input assets are `Input/IA_Sprint` and `Input/IA_Crouch`, mapped to Left Shift and Left Control in `Input/IMC_Gameplay`. Phase 5 adds `Input/IA_Aim`, mapped to Right Mouse Button. Existing move, look, and jump assets remain unchanged.

The Phase 5 weapon component owns a reliable aim state transition rather than sending aim input every frame. The owning client applies camera FOV and local viewmodel interpolation immediately; the server replicates the state and applies the per-weapon spread multiplier authoritatively. Reloading or switching weapons cancels aim and requires a new aim press. The normal HUD shows crosshair, confirmed body/head hitmarkers, stamina, and bottom-right ammo. F3 toggles the compact development overlay.

## Phase 3 Unreal assets

The Phase 3 input and Blueprint assets live under `Content/ArenaDuel/`:

1. `Input/IA_Move` is Axis2D.
2. `Input/IA_Look` is Axis2D.
3. `Input/IA_Jump` is Boolean.
4. `Input/IMC_Gameplay` maps WASD, Mouse2D, SpaceBar, and Right Mouse Button for aim.
5. `Characters/BP_ArenaDuelCharacter` derives from `AArenaDuelCharacter` and owns the movement, weapon, and aim input references.
6. `Game/BP_ArenaDuelGameMode` derives from `AArenaDuelGameMode` and selects the Character Blueprint as its default pawn.
7. `Maps/L_Phase3Test` is a temporary flat movement and two PlayerStart test map.

These assets were created and saved through the official Unreal Python Editor API. `Tools/Editor/SetupPhase3Assets.py` is idempotent. To rerun it, temporarily enable the UE `PythonScriptPlugin` and `EditorScriptingUtilities` editor plugins in the project, run the script with UnrealEditor-Cmd, then restore the project plugin list. The runtime project does not retain an editor scripting dependency.

## Automated Phase 3 validation

`Source/ArenaDuel/Tests/ArenaDuelPhase3AutomationTests.cpp` is editor-only CQTest code. It loads the temporary map, starts a listen server with one client, verifies framework ownership and first-person camera composition, waits for local Enhanced Input initialization, injects move/look/jump actions, and checks movement, control rotation, jump state, server reachability, and remote-pawn isolation. `Tools/Tests/RunPhase3Validation.ps1` builds and runs this suite twice. CQTest's temporary PIE package warnings are engine test noise; any automation error or missing result fails the runner.
