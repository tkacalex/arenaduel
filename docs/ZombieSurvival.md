# Zombie Survival

A second game mode beside the 1v1 duel: one player (the code already handles several) against waves of AI enemies on `L_ZombieArena`. It is started from the main menu.

## Architecture

| Piece | Class | Role |
|---|---|---|
| Game mode | `AArenaDuelZombieGameMode` | The wave manager: wave table, spawn queue, alive limit, spawn points, points, purchases, game over |
| Game state | `AArenaDuelZombieGameState` | Replicated run state: wave, enemies left, countdown, boss health, announcement, game over |
| Enemy | `AArenaDuelZombie` | Health, type, server AI, hit zones, ragdoll |
| Score | `AArenaDuelPlayerState` | Points, kills and damage level per player |
| HUD | `AArenaDuelMovementDebugHUD` | Survival overlay drawn on the canvas |

The survival classes derive from the duel game mode and game state. Characters and weapons ask those classes whether play is live, so movement, bunnyhop, weapons, loadout slots, the scope, hit zones, damage and ragdoll are the same code in both modes. The duel flow itself, character select, rounds and match result, is never started: `BeginPlay`, `Logout`, `HandlePlayerDeath` and `EnterCharacterSelect` are overridden, and the duel fields are held at "round in progress" for the run. Nothing in the duel classes checks for survival, apart from two functions that became `virtual`.

The mode comes from the map: `L_ZombieArena` sets the survival game mode in its world settings, `L_ArenaDistrict` uses the project default. Loading another map destroys the old world with its game mode, game state, zombies, timers and HUD, so nothing carries over.

## Wave manager

The ten authored waves are `WaveTable` on the game mode (`DefaultWaveTable`):

| Wave | Normal | Fast | Armoured | Mini boss | Boss |
|---|---|---|---|---|---|
| 1 | 10 | | | | |
| 2 | 15 | | | | |
| 3 | 20 | 2 | | | |
| 4 | 25 | 3 | | | |
| 5 | 30 | | | 1 | |
| 6 | 35 | 5 | | | |
| 7 | 40 | | 3 | | |
| 8 | 45 | 5 | 3 | | |
| 9 | 50 | | | 2 | |
| 10 | 55 | | | | 1 |

From wave 11 `ComputeWave` scales from the last entry: five more normal zombies per wave, `4 + n` fast, `2 + n/2` armoured, a growing number of mini bosses on every other wave and a big boss on every fifth; `ComputeHealthScale` adds 8 percent enemy health per wave.

Rules:

- The numbers are totals. `BuildSpawnQueue` turns a wave into a queue; at most `MaxActiveZombies` (24) are alive, and every `SpawnInterval` (0.35 s) one more is taken from the queue while there is room.
- An entry leaves the queue only when its zombie has really been spawned. A blocked spawn point costs nothing; the next tick tries another.
- A wave ends only when the queue is empty and no zombie is alive. The alive number is counted from the actors in the world every time, never kept as a running counter, so it cannot go negative or drift.
- A wave can only start from an intermission, and an intermission only from a finished wave, so waves cannot be skipped or started twice.
- A zombie that has not come closer to a player for `StuckSeconds` (14) is moved to a fresh spawn point; one that falls out of the arena is removed. Neither can hold a wave open.
- After a wave there are `IntermissionSeconds` (12) before the next; the first wave starts after `FirstWaveDelay` (5).

## Enemies

`TypeConfigs` on the game mode, one `FArenaDuelZombieTypeConfig` per type:

| Type | Health | Speed | Damage | Notes |
|---|---|---|---|---|
| Zombie | 100 | 340 | 12 | |
| Runner | 55 | 760 | 9 | Shorter swing, smaller |
| Armoured | 260 | 230 | 20 | Body takes 45 percent, head 150 percent of a hit |
| Brute (mini boss) | 1500 | 380 | 28 | Charges from a distance |
| Abomination (boss) | 6000 | 360 | 38 | Charge, area slam up close, faster below half health |

Behaviour runs on the server ten times a second (`ServerThink`): pick the nearest living player, path to them with the engine's AI controller over the navigation mesh, and steer straight if no path is found. Each zombie aims a little beside the player until it is close, so a group fans out. A melee hit starts with a wind-up; it only lands if the player is still in reach, on the same level and with nothing in between, so it can be dodged and never goes through a wall. During the wind-up a red light at the zombie is on.

Bosses add a charge and, for the big one, a slam: 560 cm radius, 0.95 s of warning, 0.7 s when enraged. The boss bar on the HUD shows the strongest boss alive.

Weapons hit a zombie on the same per-bone physics bodies as a player (`TraceHitZones`), so head, torso and limb hits use the weapon's own multipliers, times the type's head and body factors. `WeaponDamageScale` and `ZombieDamageScale` on the game mode balance survival without touching duel values. Dead zombies become ragdolls and are removed after `CorpseSeconds` (8), bosses after `BossCorpseSeconds` (25).

The body is the player's world mesh with one material per type (`M_Survival*`).

## Spawning

Spawn points are `ATargetPoint` actors tagged `ZombieSpawn`; the arena has sixteen behind a ring of walls. `PickSpawnLocation` skips points closer than `MinSpawnDistance` (1100) to any player and points another zombie is standing on, prefers points no player can see, and moves a cursor so consecutive spawns use different parts of the arena. The location is snapped to the navigation mesh when there is one. The map carries a navigation bounds volume; `DefaultEngine.ini` sets the mesh to be generated at runtime.

## Points and purchases

A kill pays the type's points (100, 150, 250, 1000, 5000), a head shot kill 50 more. Between waves:

| Key | Item | Cost |
|---|---|---|
| 5 | Refill all ammunition | 500 |
| 6 | Heal to full | 750 |
| 7 | Weapon damage +25 percent | 1500 times the next level, up to level 8 |

`TryPurchase` runs on the server: it checks the intermission, the living player and the points, refuses a purchase that would do nothing, takes the points in one step and then delivers. The player keeps all four firearms, the flashbang and the knife.

## HUD

Wave number, zombies left or the countdown to the next wave, points and kills, the shop line during an intermission, a boss bar, a large announcement at each wave start ("WAVE 5 - MINI BOSS") and after each wave, and the game over panel with waves survived, kills, points, `Enter` to play again and `M` for the main menu. Health, weapon, ammunition and the slot line with the flashbang count come from the existing HUD widget; its duel header and "defeated" label are hidden.

## Tests

- `ArenaDuel.Survival.WaveTable`: the ten waves match the table, every queue holds each enemy exactly once, the boss is last, scaling past the table rises and is never negative.
- `ArenaDuel.Survival.EnemyTypes`: the types differ as designed, and body, head and armour damage, death and "no damage to the dead" through `TakeWeaponHit`.
- `ArenaDuel.Survival.ArenaMap`: game mode, player start, navigation volume, at least twelve spawn points on the floor, far from the start and mostly out of its sight.

## Checked in a play session

- A fast-forwarded run (`ArenaDuel.Zombie.TimeScale 0.05`, zombies made harmless, `ArenaDuel.Zombie.KillAll` repeated) logged waves 1 to 12 starting with the table's numbers, each cleared before the next began, with no errors.
- Zombies spawned behind the ring, walked to the players and surrounded them; idle players were killed in wave 1 and the game over panel appeared.
- The boss bar appeared in wave 10.
- One zombie was killed with the Rune DMR: 150 points (100 plus the head shot bonus), kills 1, zombies left 9.
- Purchases: refused during a wave; in the intermission two damage levels bought for 1500 and 3000, a heal refused at full health, ammunition bought for 500, leaving 0 of 5000.
- `ArenaDuel.Zombie.Restart` started a fresh run; starting the mode from the main menu loaded the arena and wave 1.

## Not verified

- A real run played with keyboard and mouse, and whether the balance is right.
- That zombies actually use navigation paths around obstacles rather than the straight-line fallback; no check of the generated navigation mesh was made.
- Runners, armoured zombies and both bosses alive and fighting: their spawns were logged, their behaviour was not watched. The charge and the slam were not seen.
- Shots with the automatic weapons against zombies, the knife and the scope against zombies.
- The stuck-zombie relocation and the fall-out removal.
- `Enter` and `M` on the game over panel with real keys, and the purchase keys 5, 6, 7 (purchases were made through the console).
- A second player: two players were in the session, but co-op was not examined.
- Performance with a full wave alive.

## Limits

- No authored zombie models, animations or sounds: enemies are the mannequin in a type colour with the rifle locomotion clips. An attack shows as a red light, not a swing animation.
- The AI is an AI controller with path following and a small state machine in `AArenaDuelZombie`, not a behaviour tree.
- Flashbangs do not affect zombies.
- Points are per player, but the shop and the game over panel were built for one.

## Development commands

`ArenaDuel.Zombie.KillAll`, `ArenaDuel.Zombie.TimeScale <0.02..1>`, `ArenaDuel.Zombie.Damage <scale>`, `ArenaDuel.Zombie.Restart`, `ArenaDuel.Zombie.Points <n>`, `ArenaDuel.Zombie.Buy <0|1|2>`, and for any mode `ArenaDuel.Fire <seconds>`.
