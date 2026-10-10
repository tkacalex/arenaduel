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

The mode comes from the map: `L_ZombieArena` sets the survival game mode in its world settings, `L_ArenaDistrict` uses the project default. The survival game mode names `BP_ArenaDuelCharacter` as its pawn itself; the input mapping and the input actions live on that Blueprint, and without it the player could neither move nor shoot. Loading another map destroys the old world with its game mode, game state, zombies, timers and HUD, so nothing carries over.

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

From wave 11 `ComputeWave` scales from the last entry: five more normal zombies per wave, `4 + n` fast, `2 + n/2` armoured, a growing number of mini bosses on every other wave and a big boss on every fifth. `ComputeHealthScale` adds 8 percent enemy health per wave and `ComputeSpeedScale` 1.5 percent speed. Everything has a ceiling: 120 normal, 30 fast, 20 armoured, 4 mini bosses and 3 bosses per wave, three times the health, 1.25 times the speed.

Rules:

- The numbers are totals. `BuildSpawnQueue` turns a wave into a queue; at most `MaxActiveZombies` (24) are alive, and every `SpawnInterval` (0.35 s) one more is taken from the queue while there is room.
- An entry leaves the queue only when its zombie has really been spawned. A blocked spawn point costs nothing; the next tick tries another.
- A wave ends only when the queue is empty and no zombie is alive. The alive number is counted from the actors in the world every time, never kept as a running counter, so it cannot go negative or drift.
- A wave can only start from an intermission, and an intermission only from a finished wave, so waves cannot be skipped or started twice.
- A zombie that stands still for 1.2 s without being in reach steps aside and takes a new path. One that has not come closer to a player for `StuckSeconds` (14) starts again from a spawn point, but only if no player can see where it stands or where it goes. One that falls out of the arena is removed. None of them can hold a wave open.
- If nothing could be spawned for `SpawnStallSeconds` (4) although the wave has room, spawn points at half the usual distance are accepted, so a player standing among the spawn points cannot stall a wave.
- A cleared wave gives back `WaveClearAmmoShare` (35 percent) of every weapon's reserve, and between waves the living recover `IntermissionRegenPerSecond` (6) health per second.
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

Behaviour runs on the server ten times a second (`ServerThink`): pick the nearest living player and path to them with the engine's AI controller over the navigation mesh, with a new path about twice a second. If no path is found, for instance while the player is in the air, it steers straight, but only with a clear line, so it never pushes against a wall. Each zombie aims a little beside the player until it is close, so a group fans out; runners swing wide and change sides every two seconds; normal zombies differ in speed by up to 12 percent. Normal enemies ask the game mode before they swing (`ClaimAttackOn`): swings at one player are at least `SecondsBetweenSwingsAtPlayer` (0.45) apart, so being surrounded hurts but is not instant death. Bosses ignore that. A melee hit starts with a wind-up; it only lands if the player is still in reach, on the same level and with nothing in between, so it can be dodged and never goes through a wall. During the wind-up a red light at the zombie is on, and the body leans back and snaps forward as the hit lands.

A hit makes a zombie flinch in the direction of the shot, sprays droplets (`AArenaDuelHitBurst`, at most 18 at once) and slows it to 35 percent speed for a moment (`ComputeStaggerSeconds`): longer for a head shot, not at all for the big boss, for armour only at the head. The killing hit decides the fall: the ragdoll keeps the speed the zombie had, a stronger hit throws it further, a head shot snaps it back and up.

Bosses add a charge and, for the big one, a slam: 560 cm radius, 0.95 s of warning, 0.7 s when enraged. The boss bar on the HUD shows the strongest boss alive.

Weapons hit a zombie on the same per-bone physics bodies as a player (`TraceHitZones`), so head, torso and limb hits use the weapon's own multipliers, times the type's head and body factors. `WeaponDamageScale` and `ZombieDamageScale` on the game mode balance survival without touching duel values. Dead zombies become ragdolls and are removed after `CorpseSeconds` (8), bosses after `BossCorpseSeconds` (25).

The body is the player's world mesh with one material per type (`M_Survival*`).

## Spawning

Spawn points are `ATargetPoint` actors tagged `ZombieSpawn`; the arena has twenty in the corridor behind its perimeter wall. `PickSpawnLocation` skips points closer than `MinSpawnDistance` (1100) to any player and points another zombie is standing on, prefers points no player can see, and moves a cursor so consecutive spawns use different parts of the arena. The location is snapped to the navigation mesh when there is one. The game mode makes the navigation volume itself at the start of a run, sized from the spawn points and player starts (`EnsureNavigationBounds`); `DefaultEngine.ini` sets the mesh to be generated at runtime. The volume the map script places has no usable bounds in a running game, which is why no navigation mesh existed before and zombies only ever walked in straight lines.

## Points and purchases

A kill pays the type's points (100, 150, 250, 1000, 5000), a head shot kill 50 more. Between waves:

| Key | Item | Cost |
|---|---|---|
| 5 | Refill all ammunition | 500 |
| 6 | Heal to full | 750 |
| 7 | Weapon damage +25 percent | 1500 times the next level, up to level 8 |

`TryPurchase` runs on the server: it checks the intermission, the living player and the points, refuses a purchase that would do nothing, takes the points in one step and then delivers. The player keeps all four firearms, the flashbang and the knife.

## HUD

Wave number, zombies left or the countdown to the next wave, points and kills, the shop line during an intermission, a boss bar, a large announcement at each wave start ("WAVE 5 - MINI BOSS") and after each wave, and the game over panel with waves survived, kills, points, `Enter` to play again and `M` for the main menu, and the time the run lasted. The last five seconds before a wave are counted down large, a horn sounds with each wave start, and below 35 percent health the screen edges pulse red. Health, weapon, ammunition and the slot line with the flashbang count come from the existing HUD widget; its duel header and "defeated" label are hidden.

## Tests

- `ArenaDuel.Survival.WaveTable`: the ten waves match the table, every queue holds each enemy exactly once, the boss is last, scaling past the table rises, never falls and stays within the limits; the hit reaction rules.
- `ArenaDuel.Survival.EnemyTypes`: the types differ as designed, and body, head and armour damage, death and "no damage to the dead" through `TakeWeaponHit`.
- `ArenaDuel.Survival.ArenaMap`: game mode, player start, navigation volume, at least twelve spawn points on the floor, far from the start and mostly out of its sight.

## Sounds

`Tools/Editor/SetupSurvivalAndMenu.py` synthesises six sounds, writes them as WAV files and imports them to `/Game/ArenaDuel/Audio`: growl, swing, hit, death, a looping ambience placed in the arena and the wave horn. Zombie sounds are positioned and fade out over about thirty metres. They are generated tones and noise, not recordings.

## Checked in a play session

All of this was run from the main menu (Play, Zombie Survival), in an editor play session.

- Real key and mouse input, sent to the game window: `W` moved the player 7 m, `3` switched to the knife, holding the left button fired the SMG (32 to 2 rounds), a zombie was killed with a head shot (150 points, kills 1, zombies left 9). `Enter` on the game over panel restarted the run and `M` returned to the main menu.
- Auto play (`ArenaDuel.Zombie.AutoPlay 1`, zombies harmless, timers at 0.15): zombies that come within 4.5 m of the player are killed. Waves 1 to 12 ran with the table's numbers, 492 kills, each wave cleared before the next began. Every sampled enemy had a valid, complete navigation path to the player; none steered straight, none had to free itself, none was moved or fell out. Spawns with the relaxed distance: 12 of 492.
- Standing still at full damage: the run ended about 20 s after the wave started, 8 s or so after the first zombie arrived.
- Frame time in the editor with two game windows open: 26 to 35 ms with few enemies, 35 to 58 ms with 24 alive. That is an editor figure, not a packaged one.
- Automated: 19 of 19 selected tests passed, among them the three survival tests, the map tests and the 1v1 host and client smoke test.

## Not verified

- The sounds: they are imported and played by the code, but nobody has listened to them.
- The flinch, the droplets and the lunge were seen only in still screenshots.
- Runners, armoured zombies and both bosses fighting: they spawned and reached the player in auto play; the charge, the slam and the flanking were not watched, and no boss was fought with weapons.
- The knife, the scope, the shotgun and reloading against zombies with real input; the purchase keys 5, 6, 7 with real keys.
- The ramps, the deck edge and the building doors with a real player; whether the balance holds over a real ten-wave run.
- The stuck relocation and the fall-out removal, which never triggered.
- A second player in the same run.

## Limits

- No authored zombie models or animations: enemies are the mannequin in a type colour with the rifle locomotion clips; the swing and the flinch are a lean of the whole body.
- The AI is an AI controller with path following and a small state machine in `AArenaDuelZombie`, not a behaviour tree.
- Flashbangs do not affect zombies.
- Points are per player, but the shop and the game over panel were built for one.
## Development commands

`ArenaDuel.Zombie.KillAll`, `ArenaDuel.Zombie.TimeScale <0.02..1>`, `ArenaDuel.Zombie.Damage <scale>`, `ArenaDuel.Zombie.Restart`, `ArenaDuel.Zombie.Points <n>`, `ArenaDuel.Zombie.Buy <0|1|2>`, `ArenaDuel.Zombie.AutoPlay <0|1>`, `ArenaDuel.Zombie.Stats`, and for any mode `ArenaDuel.Fire <seconds>`.
