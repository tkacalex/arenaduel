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

`AArenaDuelGameMode` derives from `AGameMode` and owns authoritative framework class relationships. It exists only on the server. It currently configures the ArenaDuel GameState, PlayerController, PlayerState, and Character classes. Match flow, rounds, scoring, and win conditions are intentionally absent.

### AArenaDuelGameState

`AArenaDuelGameState` derives from `AGameState` and establishes the replicated match state type. It exists on the server and as a replicated state object on clients. It currently contains no custom variables.

### AArenaDuelPlayerController

`AArenaDuelPlayerController` derives from `APlayerController` and establishes the per player control type. The server has one for each connected player and a client normally owns only its own instance. Future local control flow and UI coordination belong here. Shared match data does not.

### AArenaDuelPlayerState

`AArenaDuelPlayerState` derives from `APlayerState` and establishes the replicated per player state type. It exists on the server and is relevant to clients. Persistent player specific match data may be added later. Combat attributes and GAS ownership are not decided here.

### AArenaDuelCharacter

`AArenaDuelCharacter` derives from `ACharacter` and represents the possessed physical fighter. The server owns the authoritative actor, the owning client controls its local instance, and remote clients receive the replicated Character. Standard `UCharacterMovementComponent` networking remains responsible for locomotion. The class currently provides only a local first person camera, camera relative movement hooks, look hooks, and jump hooks.

The current camera is intentionally local and cosmetic. It does not force remote players to use first person visuals. A later presentation phase can add first person arms and weapons for the owning client while keeping a separate full world body and weapon representation for remote players.

The first person camera attaches directly to the Character capsule at a 64 centimeter local Z offset. This places the initial eye point above the capsule center without introducing a Spring Arm, camera lag, or replicated cosmetic transform.

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
