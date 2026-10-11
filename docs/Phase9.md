# Phase 9: review findings and their state

Source: the Phase 9 review of commit `64de43e` (static review of the repository, no build, no run). It lists 18 code findings (E01 to E18) and 10 areas that need checking (R01 to R10). This file tracks every one of them.

Three things are kept apart for each entry:

- **Code**: the finding was read again in the current code and is there (confirmed), is not there (refuted), or has not been read again yet (from the review).
- **Runtime**: the faulty behaviour was seen in a running game or test before the fix (reproduced), or was not (not reproduced). A finding can be fixed from the code alone; that is then said.
- **State**: open, fixed (with the test that shows it), or checked (for the R entries).

Words used for evidence: *recorded* means a capture exists, *looked at* means someone looked at the capture or the running game, *passed* means the stated check was made and met. Sound cannot be listened to by the tool-driven sessions that did this work; where a check needs ears it stays open and says so.

Rules for all of Phase 9: damage, ranges, ammunition, attack times, movement balance, match rules and the existing upgrades stay as they are. No online or ranked systems are added. Tests do not write productive assets.

## Code findings

| ID | P | Finding | Code | Runtime | State |
| --- | --- | --- | --- | --- | --- |
| E01 | 1 | A third player can affect the duel | confirmed | not reproduced before the fix | **fixed (9A)** |
| E02 | 1 | Side rays of the knife can hit through cover | confirmed | not reproduced before the fix | **fixed (9A)** |
| E03 | 1 | Switching weapons shortens the knife's attack time | confirmed | not reproduced before the fix | **fixed (9A)** |
| E04 | 1 | Fire after a quick switch stays a local motion only | confirmed | not reproduced before the fix | **fixed (9A)** |
| E05 | 1 | The reload request does not check the held slot | confirmed | not reproduced before the fix | **fixed (9A)** |
| E06 | 1 | The pause and settings menu does not lock all weapon actions | from the review | - | open (9B) |
| E07 | 1 | Upgrade requests do not name their selection | confirmed | not reproduced before the fix | **fixed (9A)** |
| E08 | 1 | Shot origin and smoothed camera differ during crouch and slide transitions | from the review | - | open (9B) |
| E09 | 2 | Reset Defaults works along two different, faulty paths | from the review | - | open (9B) |
| E10 | 2 | The survival shop charges for ammunition that is already full | from the review | - | open (9B) |
| E11 | 2 | A zombie's movement at death is read after it was stopped | from the review | - | open (9B) |
| E12 | 1 | Boss spawns and rescue teleports do not account for the large capsule | from the review | - | open (9B) |
| E13 | 2 | A developer health command is registered in shipping builds too | from the review | - | open (9C) |
| E14 | 2 | The full test suite re-saves productive assets | confirmed (known: `ArenaDuel.VisualAssetSetup.*` re-saves `BP_ArenaDuelCharacter`) | reproduced earlier in this project | open (9C); until then the setup tests are left out of every run by name |
| E15 | 2 | Asset setup can skip missing required meshes silently | from the review | - | open (9C) |
| E16 | 2 | Bullet marks derive the kind of hit from end points only | confirmed by reading `SpawnImpactMark` | not reproduced | open (9D) |
| E17 | 1 | Survival can hang when the last living player disconnects | from the review | - | open (9B) |
| E18 | 2 | The documented double death rule does not match the immediate evaluation | from the review | - | open (9B) |

### Phase 9A (2026-10-11): E01 to E05 and E07

Each of the six was read in the code first and found as the review describes it. None was reproduced in a running game on the old code: the tests below were written with the fixes, and the old behaviour is argued from the code. That is a limit of this package.

**E01, a third player.** A joining player was given a pawn by the engine before `PostLogin` assigned the duel slot, so a third player (slot 255) stood in the arena armed; `HandlePlayerDeath` took the first living pawn of any other controller as the winner; `Logout` of anyone returned the match to character selection. Now: a player who joins while both places are taken is marked as watching on the server (`AArenaDuelPlayerState::IsDuelSpectator`), loses the pawn the engine gave them and gets none from any later restart; a death only decides a round when the dead character belongs to a duel player, and the winner is the other duel player if alive; only a duel player leaving returns the match to character selection. Survival is not limited (`IsLimitedToDuelPlayers` is false there). The host's log has one line per join, leave, decided round and return to selection (`LogArenaDuel: Match: ...`).
Limit: a watching player stays watching when a duel place becomes free; they have to rejoin. Promoting them would be a new rule and was not added.

**E02, the knife and cover.** Only the middle ray of the five-ray swing was traced against the world; the four side rays tested bodies directly. Now every ray ends at the first thing of the world it meets. Range, width, damage and the result shown to the attacker (wall or air, from the middle ray) are unchanged. In the open nothing changes.

**E03, the knife's attack time.** The attack interval and the switch time shared `EquipmentReadyServerTime`, so a switch away and back replaced a 1.0 s wait by 0.25 s. The knife now has its own time (`KnifeReadyServerTime`); an attack needs both to have passed. The intervals are unchanged (0.4 s and 1.0 s).

**E04, the trigger inside the switch time.** The owner started its shown fire (kick, repeating timer) at once; the server refused the request because of the switch time and never looked at it again, so an automatic weapon kicked without shooting until the trigger was pressed anew. Now the server remembers a held trigger whose only obstacle is the switch time and starts the fire when that time is over; a release, a switch, a reload or the end of the round clears the wish. The owner delays its shown fire by the same switch time, counted from when it learns of the switch. After a flashbang throw nothing changes: the throw is a single action, the trigger is not held for the firearm on either side, and a new press is needed, as before.
Limit: with latency the owner learns of the switch later than the server, so its shown fire starts up to one latency later than the real shots. The other way round (kick without shots) no longer happens.

**E05, reload.** `ServerRequestReload` checked death, round and ammunition, but not the slot. It now also requires the firearm to be the item in the hands, a valid weapon and a controller, as `Reload()` on the owner already did.

**E07, upgrade choices.** The request carried only the upgrade. The game state now publishes a selection serial (`GetUpgradeSelectSerial`) that grows by one with every selection and is never reset, so it names one selection of one match. The owner sends the serial of the selection it is looking at; the server refuses any other.

Tests (`Source/ArenaDuel/Tests/ArenaDuelPhase9Tests.cpp`, listen server and one or two clients in separate game instances of one editor process):

| Test | Shows |
| --- | --- |
| `ArenaDuel.Phase9.Combat.KnifeSideRaysStopAtCover` | A victim is placed where only the right ray of the swing reaches the body. In the open the swing lands. With a thin wall that the middle ray passes and the right ray does not, the same swing does nothing. (E02) |
| `...KnifeAttackTimeSurvivesSwitchesAndReloadNeedsTheFirearm` | After a heavy stab, a switch to the firearm and back does not allow another attack once the switch time has passed, only once the stab's own time has. A reload request behind knife or flashbang changes nothing; with the firearm it reloads. (E03, E05) |
| `...TriggerHeldInsideTheSwitchTimeFiresWhenItEnds` | Host: a press inside the switch time fires no shot at once, is remembered, and fires when the time is over, not before; a press let go inside it fires nothing. Remote player through the real request: switch, press and hold, and the server fires real shots; letting go stops them. (E04) |
| `...UpgradeChoiceFromAnEarlierSelectionIsRefused` | Wrong serials (previous, next, zero) change nothing; in the second selection a choice naming the first is refused and does not use up the choice; the current one is taken once. (E07) |
| `ArenaDuel.Phase9.ThirdPlayer.WatchesWithoutAPawnAndLeavesWithoutEndingTheMatch` | With three connected players the third has no slot and no pawn, a restart gives them none, the death of a character outside the duel decides nothing, their leaving neither ends nor resets the round, and the kill between the two duel players is counted once. (E01) |

The existing upgrade tests name the open selection in every request and pass unchanged otherwise. Whole regression run after 9A (everything except the asset setup commands): 99 of 99 passed, and the working tree was clean afterwards.

## Areas to check

| ID | P | Area | State |
| --- | --- | --- | --- |
| R01 | 1 | Server pose against visible animation on a dedicated server | open (9C) |
| R02 | 1 | Prediction of the extended movement states under latency and loss | open (9C) |
| R03 | 1 | Order and completeness of replicated presentation data | open (9C) |
| R04 | 1 | Cook and packaging coverage of assets loaded by path | open (9C) |
| R05 | 2 | Sound quality, placement in space, one fire sound for four weapons | open (9D); needs listening |
| R06 | 1 | Hands, fingers, IK and every weapon action, own view and opponent's view | open (9D) |
| R07 | 1 | Zombie hit zones, boss clips, death clip into ragdoll | open (9D) |
| R08 | 2 | The new arena: geometry, visibility, movement routes | open (9D) |
| R09 | 2 | CPU, GPU, network and memory under load, outside the editor | open (9E) |
| R10 | 1 | Tests between real processes, upgrade and menu edge cases | partly: see "Separate processes" below |

## Separate processes

2026-10-11, after 9A: a host and two clients as three separate game processes on one machine (`UnrealEditor.exe -game`, not a shared PIE session), the host as listen server on `L_ArenaDistrict`, client A with 100 ms simulated latency and 2 % packet loss. Read from the host's log:

| Step | Host log | Result |
| --- | --- | --- |
| Client A connects | `joined, duel slot 1` | passed |
| Client B connects while the round runs | `joined, watching (both duel places are taken)` | passed |
| Client B closes its window | `left, not a duel player: the match goes on`, and no return to character selection follows | passed |
| Client A's process is killed (no goodbye) | connection closed by the host about 7 s later, `left, a duel player: back to character selection`, `character selection, score and upgrades cleared` | passed |

Limits: nobody played in these processes. No shot, knife attack, switch or upgrade choice was made between separate processes; those are covered by the automation tests above, which run host and clients as separate game instances inside one editor process. Nothing was recorded or looked at on screen in this run; it is a log check. A join with a wrong address or version, the host quitting, and a rejoin are not checked yet (R10).

## Documentation notes from the review

- D01: older parts of `docs/ROADMAP.md` still call upgrades not started and describe earlier visual states. To be cleaned up in 9E; the current state is in the dated sections.
- D02: weapon selection, the real overtime objective and ranked rules for a disconnect are open Phase 8 and Phase 10 work and are not part of Phase 9. First to five already ends after nine decided rounds at the latest; a hard nine round cap would need a draw rule and is not added.

## Order of work

1. 9A, combat and participants: E01 to E05, E07. Done, see above.
2. 9B, input, camera and survival: E06, E08 to E12, E17, E18.
3. 9C, build, assets and tests: E13 to E15, R01 to R04.
4. 9D, presentation and sound: E16, R05 to R08.
5. 9E, load and overall acceptance: R09, R10, D01.
