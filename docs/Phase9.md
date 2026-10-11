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
| E06 | 1 | The pause and settings menu does not lock all weapon actions | confirmed | not reproduced before the fix | **fixed (9B)** |
| E07 | 1 | Upgrade requests do not name their selection | confirmed | not reproduced before the fix | **fixed (9A)** |
| E08 | 1 | Shot origin and smoothed camera differ during crouch and slide transitions | confirmed | not reproduced before the fix | **fixed (9B)** |
| E09 | 2 | Reset Defaults works along two different, faulty paths | confirmed | not reproduced before the fix | **fixed (9B)** |
| E10 | 2 | The survival shop charges for ammunition that is already full | confirmed | not reproduced before the fix | **fixed (9B)**, the purchase run as a test (9C) |
| E11 | 2 | A zombie's movement at death is read after it was stopped | confirmed | not reproduced before the fix | **fixed (9B)**, tested on server and client (9C), not looked at |
| E12 | 1 | Boss spawns and rescue teleports do not account for the large capsule | confirmed | not reproduced before the fix | **fixed (9B)**, real boss spawns and rescues tested (9C), not looked at in a run |
| E13 | 2 | A developer health command is registered in shipping builds too | confirmed, and five more commands in the weapon component | no shipping build made | **fixed (9C)**, confirmed in the shipping executable |
| E14 | 2 | The full test suite re-saves productive assets | confirmed | reproduced earlier in this project (`BP_ArenaDuelCharacter` re-saved by a full run) | **fixed (9C)** |
| E15 | 2 | Asset setup can skip missing required meshes silently | confirmed | not reproduced | **fixed (9C)**, negative test without touching assets |
| E16 | 2 | Bullet marks derive the kind of hit from end points only | confirmed by reading `SpawnImpactMark` | not reproduced | open (9D) |
| E17 | 1 | Survival can hang when the last living player disconnects | confirmed | not reproduced before the fix | **fixed (9B)** |
| E18 | 2 | The documented double death rule does not match the immediate evaluation | confirmed | - | **fixed (9B)**: the document now says what the code does |

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

The existing upgrade tests name the open selection in every request and pass unchanged otherwise. Whole regression run after 9A (everything except the asset setup commands): 99 of 99 passed, and the working tree was clean afterwards. After 9B: 106 of 106. After the first part of 9C, by the plain filter `ArenaDuel`: 110 of 110.

### Phase 9B (2026-10-11): E06, E08 to E12, E17, E18

**E06, the menu.** The character's handlers for fire, aim and reload called the weapon without asking `CanProcessGameplayInput`, which every other input does and which knows about the pause menu; the weapon itself only knew the admin menu. Both now refuse a start while either menu is open. Letting go is never refused.

**E08, shot origin and eye.** The shot origin was the settled camera height (and the full slide drop from the first moment), while the picture eased there over about a quarter of a second. Now the eye's crouch offset and slide drop are functions of the time since the last crouch, stand-up or slide change, and the owner's camera and the shot origin on the owner and on the server read the same functions. Other machines keep the settled height. One more thing came out of this: the eye was also stepped when crouching in the air, where the capsule shrinks around its centre and nothing has to be made up for; it is now stepped only when the capsule centre really moved.
Limits: the server starts its curve when the crouch reaches it, the owner when the key is pressed; the request to shoot travels the same way, so the two stay close, but they are not locked together. During the ease the eye can be above the already shrunk capsule, as the camera always was.

**E09, Reset Defaults.** Two implementations: one reloaded the controller's values right after setting the defaults, the other left the resolution box as it was. One function now shows the pending settings in every control (`ShowPending`), and both reset paths set the defaults and call it.

**E10, the shop.** Ammunition was charged without checking that anything was missing. `TryPurchase` now refuses the purchase when every magazine and reserve is full or the buyer has no weapon component.

**E11, death of a zombie.** `Die` stopped the AI and the movement before the death clip was chosen from the speed and the ragdoll took it over, so the server read zero and each client whatever it had. The velocity is now taken first and replicated with the other death data; the choice of clip and the ragdoll's start speed use it on every machine.

**E12, big enemies.** A spawn tested the unscaled class; a rescue teleport put any zombie 96 cm above the floor without a test. Both now place the real, scaled capsule on the floor under the picked point and require it to be free there; a tight point is skipped (a spawn tries the next point on the next tick, a rescue waits).

**E17, leaving survival.** The end of a run was only looked for after a death. It is now also looked for when a player leaves, without counting the one who is leaving.

**E18, two deaths.** The document promised a round without a point for two deaths in the same instant. The code handles deaths one after another: the first ends the round for the other player, and later damage does not count. No rule was changed; `docs/RoundFlow.md` now says this.

Tests added to `ArenaDuelPhase9Tests.cpp`:

| Test | Shows |
| --- | --- |
| `ArenaDuel.Phase9.Menu.ResetDefaults` | Changed settings are shown without being changed; reset shows `Borderless, 1920 x 1080, Off, 144` and that is what Apply would take. (E09) |
| `ArenaDuel.Phase9.Combat.MenuLocksWeaponStartsAndRefillKnowsWhenItIsNeeded` | With the pause menu open: no shot, no aim, no reload, no switch; after closing, a reload starts. A fresh pawn counts as fully loaded, one with a round missing does not, one after a refill does again. (E06, the check behind E10) |
| `...ShotOriginFollowsTheEasedEyeThroughACrouch` | On the server, for the remote player's pawn: at the moment of the crouch the shot origin has not moved, the offset then shrinks and ends at the settled camera height. (E08) |
| `...TwoLethalHitsInOneFrameGiveTheRoundToWhoeverDiedSecond` | Both orders: the second hit does not count, one round for the survivor, one evaluation. (E18) |
| `ArenaDuel.Phase9.Survival.LastLivingPlayerLeavingEndsTheRun` | A player leaving while the host stands ends nothing; with the host dead, the remote player leaving ends the run. (E17) |
| `ArenaDuel.Phase9.Survival.SpawnCentre` | The centre of a capsule of any height over a picked point: feet on the floor. (E12, the arithmetic only) |

Not covered by a test: the shop purchase as a whole (E10), the zombie's death clip and ragdoll speed (E11), a real boss spawn and rescue (E12).

### Phase 9C, first part (2026-10-11): E13 to E15

**E13.** `ArenaDuel.Health` and the five commands `ArenaDuel.Slot`, `.Aim`, `.Weapon`, `.Fire`, `.Pitch` were registered in every build. They are now compiled out of shipping builds, like the round and survival commands already were. No shipping build was made to confirm it.

**E14.** The two commands that create and re-save assets were registered as tests named `ArenaDuel.VisualAssetSetup.*`, so a run of the project's tests ran them. They are now named `ProjectAssetSetup.Arms` and `ProjectAssetSetup.CharacterMaterials` and run only when asked for by that name (`Tools/Editor/SetupPhase7DVisuals.ps1` does). Checked: `Automation RunTests ArenaDuel` ran 110 tests, none of them a setup command, and left the working tree clean.

**E15.** A missing mesh was passed over and a missing material was a warning. Both, and a missing hit zone physics asset, are now errors, and the command fails when there was one. Not run: the command writes assets.

### Phase 9B acceptance and 9C packaging (2026-10-11, second session)

No mouse, keyboard or window was used for any of this: automation tests, scripts, and game processes without windows.

**Checks added** (`ArenaDuelPhase9Tests.cpp`, and one in `ArenaDuelVisualAssetAutomation.cpp`):

| Test | Shows |
| --- | --- |
| `ArenaDuel.Phase9.Survival.ShopChargesForAmmunitionOnlyWhenSomeIsMissing` | The real purchase on the server: fully loaded, it is refused and costs nothing; with five rounds missing it is sold, costs exactly the price once and fills the magazine; a second purchase right after is refused and free; with rounds missing and one point too few it is refused and nothing is delivered. (E10) |
| `...ZombieDeathDataIsKeptAndTheSameOnServerAndClient` | Three zombies die: at 450 cm/s, standing, standing by a head shot. The server keeps the speed each had although it stops the body; it lets only the standing body shot buckle first. The client receives all three, each with speed, direction and head shot flag already there at the moment it is told of the death, makes the same three choices, and hands all three to physics. (E11, and the death part of R03) |
| `...BigEnemiesSpawnAndAreRescuedOnlyWhereTheirCapsuleFits` | On a floor built for the test: the boss spawns on a free place with its scaled capsule and its feet on the floor; it is refused under a ceiling an ordinary zombie fits under, beside a wall an ordinary zombie fits beside, and on a place another enemy stands on, and an ordinary zombie is accepted in the first two. A rescue to each of the three tight places is refused, moves nothing, and is counted as refused and not as done; a rescue to a free place works, is counted, and resets the enemy's progress. (E12) |
| `ArenaDuel.Phase9.Combat.ClientTriggerGoesByTheItemItAskedFor` | See "found under latency" below. |
| `ArenaDuel.Phase9.AssetSetup.MissingAssetsAreErrors` | The setup's own check against a mesh that does not exist and against slots whose materials do not exist: one error per missing asset, naming it, none for an unrelated slot. The real hands and the five zombie meshes have everything. It loads and never saves. (E15) |

For this the spawn and the rescue became callable steps (`SpawnZombieAt`, `RescueZombieTo`, `FindCapsuleRoomAtSpawn`), the choice between buckling and dropping became one function of the replicated death data (`ShouldBuckleBeforeRagdoll`), and the setup's asset check was separated from its saving. Behaviour is unchanged, with one exception that the review asked for: a rescue is only counted when the teleport reported success.

**Found: the game could not be packaged.** `ArenaDuelItemMeshes.cpp` used a constructor of `FStaticMaterial` that only exists in editor builds, so the game target did not compile; nothing had ever built it. Fixed. The game target now builds as Development and as Shipping.

**Found: the packaged game lacked both playable maps and most of what is loaded by path (R04).** The first package contained what the main menu references and nothing else: of 248 project assets, 157 were not cooked, among them `L_ArenaDistrict`, `L_ZombieArena`, every sound, every zombie mesh and material, knife, flashbang, the moving weapon parts, the ammunition box and the bullet mark. In play most of those loads are quiet, so the game would have run with silent fallbacks where it ran at all. `Config/DefaultGame.ini` now names the three maps to cook and the directories that are loaded by path (`Audio`, `Characters/Common`, `Characters/Zombies`, `Weapons`, `Props`). After that, every content path that appears in the game code is in the cooked set; 19 assets are left out, all of them test maps, development materials or unused mannequin variants.

**Packaged game, outside the editor** (`Saved/Packages/Development`, started without a window, read from its logs):

| Check | Evidence | Result |
| --- | --- | --- |
| Starts into the main menu | log: `LoadMap /Game/ArenaDuel/Maps/L_MainMenu` | passed |
| Everything loaded by path is there | the development command `ArenaDuel.Dev.AssetCheck` loads 53 assets by path (13 sounds, 5 zombie meshes and materials, 5 zombie clips, 11 weapon parts, knife, flashbang, bullet mark, ammunition box, hands, hit zone physics asset, fire sound, the two Blueprint classes) and looks for the three maps: `0 missing` | passed |
| Duel between two packaged processes | host log: both players joined with their duel slots, no load problem on either side | passed |
| Firearms in the packaged duel | host log: four real shots from at least three different firearms (magazines of 30, 12 and 6 rounds), fired by script | partly: not all four shown to fire, knife and flashbang not exercised; their assets load (row above) |
| Survival with every enemy type | log: waves 1 to 16 started and their queues emptied, with ordinary, runner, armoured, brute (wave 5) and boss (waves 10 and 15); navigation valid; no load problem | passed |
| Developer commands are out of shipping | the shipping executable contains none of the names `ArenaDuel.Health`, `.Slot`, `.Fire`, `.Weapon`, `.Pitch`, `.Dev.*`, `.Round.*`, `.Zombie.*`; the development executable contains all of them | passed |

Not done with the packaged game: nobody looked at it or played it, the shipping package was built but not started, characters were not chosen by hand (the automatic choice was used), and nothing was listened to.

**Host and separately running client under latency** (editor binaries as two game processes without windows, the client with 100 ms simulated latency, playing a fixed sequence by itself through `ArenaDuel.Dev.After`; both sides log every real and every shown shot with `ArenaDuel.Dev.ShotLog 1`):

- *Found under latency:* a trigger pressed right after asking for the firearm did nothing at all, on either side. The client still showed the knife when the press came, so it asked for a knife stroke, which the server, already switched, threw away. The owner now goes by the item it asked for while the server's answer is on its way (`GetExpectedSlot`), and counts the switch time from its own request. After the fix: first shown shot 0.273 s after the request, first real shot 0.303 s after the switch on the server, none before the 0.25 s switch time on either side.
- *Shot origin through a crouch and a stand-up:* both sides ease along the same curve, but not at the same moment. First shots of a burst fired into a crouch: shown from 31.9 cm above the settled height, real from 19.2 cm; into a stand-up: 32.3 cm below against 19.1 cm below. Within three shots both are within 2 cm. Before the fix of E08 the real shot would have started from the settled height, 32 to 48 cm away from the shown one. What remained was the difference in when the crouch and the trigger reach the server; the third session reduced it ("Shot origin, second step" below).
- *Fixed in the third session, see "Rate of fire" below; this is how it was found: fewer real shots than shown ones.* In four held bursts the owner showed 31 shots and the server fired 22. The server's log shows gaps of two intervals where a shot was left out. Its repeating timer is phase locked, and a shot is refused when it comes earlier than one interval after the previous one was actually fired, which a frame-quantised timer does again and again. So the real rate of fire of automatic weapons is below the defined one, by an amount that depends on the server's frame rate. At the time this was measured and left for a decision; the decision was to restore the configured rate.

Slides were not covered between separate processes in this session; they are in the third.

Whole regression run at the end, filter `ArenaDuel`: 115 of 115 passed. Working tree clean afterwards.

### Rate of fire, shot origin and packaged function checks (2026-10-11, third session)

No mouse, keyboard or window was used: automation tests, scripts, game processes without windows or consoles.

**Rate of fire of automatic weapons.** No tuning value was changed: rounds per minute, damage, magazines, spread and recoil are what they were. What changed is that the server now reaches the configured rate.

*Cause.* The server fired from a repeating timer with the weapon's interval, and each call was let through only if a full interval had passed since the previous shot *actually left*. A timer runs with the frame that follows its time, so every shot leaves a part of a frame late, and the next call, one interval after the previous call's *time*, came a moment too early as often as not and was thrown away as a whole. The loss depended on how frames happened to fall.

*Now.* An automatic weapon has a schedule of due times one interval apart (`NextAllowedFireServerTimes`). A shot is fired when its due time has come, and the next is due one interval after that due time, not after the frame it left in, so lateness does not add up and no call is thrown away. A press starts a new schedule at once, unless the last shot's interval is still running: then the first shot is due when that is over, so pressing again can never beat holding. A frame longer than `AutomaticFireCatchUpSeconds` (0.1 s) loses the shots that fell into the rest of it; a hitch can add two shots to one frame at most. Single shot weapons are unchanged (one interval after the shot left).

*Measured*, a held burst of 1.5 s on the server (`ArenaDuel.Phase9.FireRate.HeldFireKeepsTheConfiguredRateAtAnyFrameRate`; the old numbers are the same test run against the old scheduling with `-FireRateMeasureOnly`):

| Weapon | Server frames | Shots due | Before | Now |
| --- | --- | --- | --- | --- |
| Arc Rifle, 650 rounds per minute | 30 per second | 17 | 14 | 17 |
| | 60 per second | 17 | 10 | 17 |
| | 144 per second | 17 | 13 | 17 |
| | jumping between 25 and 200 | 17 | 15 | 17 |
| | 60 with one hitch of 0.25 s | 17 | 10 | 16 |
| Shade SMG, 900 rounds per minute | 30 per second | 23 | 23 | 23 |
| | 60 per second | 23 | 23 | 23 |
| | 144 per second | 23 | 17 | 23 |
| | jumping between 25 and 200 | 23 | 15 | 23 |
| | 60 with one hitch of 0.25 s | 23 | 20 | 22 |

So the Arc Rifle fired at 59 to 88 % of its rate before and the SMG at 65 to 100 %, depending on the server's frames; both now fire at their rate. In a real fight this is more shots per second than the game had so far, up to two thirds more for the rifle in the worst case measured. Time to kill with automatic weapons is shorter by that much. That is the configured value taking effect, and it is worth a play test.

The same test checks that the rounds used equal the shots fired and that no case fires more than is due. `...NothingFiresAfterTheFireIsCutOffAndNothingExtraFromPressingAgain` checks: nothing after letting go; pressing and letting go on every frame for a second fires no more than holding; a switch, a reload and the end of the round stop the fire and nothing follows; three rounds in the magazine give exactly three shots and then the reload; a dead player's weapon does not fire.

Between separate processes under 100 ms latency, in six bursts: 48 real shots to 52 shown without loss, 49 to 52 with 2 % loss (before the fix: 22 to 31 in four bursts). What is left: at the end of a magazine the owner shows up to two shots more than leave, because it hears of the empty magazine late (3 real to 5 shown), and one shot at the edge of a burst when the press or the release arrives late. Not corrected.

**Shot origin, second step (E08).** Laid side by side with the machine's clock, the logs show why about 13 cm were left: a crouch reached the server 115 to 190 ms after the owner did it, the trigger a similar but not the same time after, so the first shot after a crouch left 30 to 80 ms into the ease on the server and 30 to 65 ms into it for the owner. At the steep start of the curve 25 ms are 14 cm.

Now the press carries how far along the owner's eye is (the seconds since its crouch or stand-up and since its slide start or end). The server does not take that as the origin. It moves its own clock for the running ease towards it, by at most `EyeEaseHintMaxSeconds` (0.1 s) either way, only while both sides are within the first second of an ease, and drops it when the next ease starts. A value that is negative, not a number or from outside an ease is ignored. The most a made-up value can gain is a height the eye really passes through, held a tenth of a second longer. The host's own pawn takes no hint.

Measured under 100 ms latency and 2 % packet loss, eye above where it settles, server | owner, first four shots of a burst:

| Burst | 1st | 2nd | 3rd | 4th |
| --- | --- | --- | --- | --- |
| into a crouch | 48.0 \| 48.0 | 13.6 \| 19.3 | 8.8 \| 7.2 | 3.6 \| 3.4 |
| into a stand-up | -48.0 \| -48.0 | -11.1 \| -19.9 | -7.5 \| -8.0 | -3.2 \| -2.9 |
| into a slide | 26.6 \| 26.6 | 0.6 \| 5.7 | -2.8 \| -3.3 | -6.9 \| -6.9 |

The first shot now leaves from exactly where the owner's eye is. The second can still differ, by up to 8.8 cm here: each side fires it with the frame that follows its due time, and at the steepest part of the curve one frame is that much. From the third shot on the two are within 2 cm. A crouch in the middle of a held burst gets no hint (there is no new press) and stays on the server's own clock. Knife and flashbang carry no hint.

`ArenaDuel.Phase9.Combat.OwnerEyeHintIsTakenWithinLimitsAndTheEasedEyeDecidesAShotOverCover` checks the limits (a small hint is taken as said, a large one is cut to the limit, nonsense is ignored, a new ease drops it, the host takes none) and a shot just over cover: with the top of a wall 12 cm below the standing eye, a standing shot passes over it, a shot in the first moment of a crouch still does, a shot later in the ease does what the eye does, and a settled crouched shot hits the wall. Beside cover nothing changes: the ease is vertical only.

**Packaged Development game, function check.** A duel between two packaged processes, the host acting by script; from the host's log:

| Check | Evidence | Result |
| --- | --- | --- |
| Every firearm fires | real shots per weapon: 5, 7, 1 and 1 for the four | passed |
| Both knife attacks | `light knife attack ... met nothing`, `heavy knife attack ... met nothing` | passed |
| Long and short flashbang throw | `long flashbang throw`, and in round two `short flashbang throw` | passed |
| A round with the upgrade selection | `round 1 begins`, `round 1 decided, winner slot 0`, `upgrade selection 1 opens for 10 s`, `duel slot 0 took upgrade 1 in selection 1`, `duel slot 1 did not choose in time and got upgrade 0`, `round 2 begins` | passed |
| No missing content | no load problem in either log | passed |

The knife met nobody and the flashbang blinded nobody that is on record; hits, damage and blinding are covered by the automation tests, not by this run.

**Shipping package, started.** A shipping build writes no log and has no console, so little can be read from it. Started without a window three ways, into the menu, with the survival map, and as host with a second process connecting to it: each time the processes were still running after 45 to 60 seconds and no crash report was written. Whether the menu, the maps or the connection were reached could not be confirmed: a check for the host's listening port found none, and that check was not validated against a build that is known to listen. So: starts and does not crash, passed; menu, map change and both modes in the shipping build, not checked.

Whole regression run, filter `ArenaDuel`: 118 of 118 passed. Working tree clean afterwards.

### Phase 9D, first part: hit feedback (2026-10-11)

A head shot feedback was added to the HUD earlier that day (`docs/Loadout.md`, "Hit confirmation"). A review of it found three faults; all three were read in the code first and were there.

1. **Two markers.** The canvas HUD still drew its old marker while the widget showed the new one. The old one is removed; crosshair and scope are untouched.
2. **Off centre.** The widget's ticks stood around y = 50 in a centred root 140 high, so their middle was 20 units above the screen's middle, and the root scaled about that point. Ticks and pivot are now the root's middle.
3. **Lost confirmations.** The HUD read only the last sequence number and result once per frame, so of several confirmations between two frames only the last was seen. Confirmed hits are now events on the shooter's weapon, taken once each; a head shot's marker outlasts a following body hit.

Tests and the packaged check are listed in `docs/Loadout.md`. Whole run, filter `ArenaDuel`: 119 of 119 passed, working tree clean. Not done: looking at the marker in a running game (hip fire, scope, different resolutions) and listening to the sounds; a listening list is in `docs/Loadout.md`.

## Areas to check

| ID | P | Area | State |
| --- | --- | --- | --- |
| R01 | 1 | Server pose against visible animation on a dedicated server | open (9C) |
| R02 | 1 | Prediction of the extended movement states under latency and loss | open (9C) |
| R03 | 1 | Order and completeness of replicated presentation data | partly: zombie death data is complete when a client is told of the death (test); skins, scale and late join are open |
| R04 | 1 | Cook and packaging coverage of assets loaded by path | checked and fixed (9C): see the packaging section; the shipping package was built, not started |
| R05 | 2 | Sound quality, placement in space, one fire sound for four weapons | open (9D); needs listening |
| R06 | 1 | Hands, fingers, IK and every weapon action, own view and opponent's view | open (9D) |
| R07 | 1 | Zombie hit zones, boss clips, death clip into ragdoll | open (9D) |
| R08 | 2 | The new arena: geometry, visibility, movement routes | open (9D) |
| R09 | 2 | CPU, GPU, network and memory under load, outside the editor | open (9E) |
| R10 | 1 | Tests between real processes, upgrade and menu edge cases | partly: see "Separate processes" below and the latency run above |

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
2. 9B, input, camera and survival: E06, E08 to E12, E17, E18. Code and tests done; what has to be looked at in a running game is listed at the end.
3. 9C, build, assets and tests: E13 to E15 done; R01 to R04 open, among them the packaged build.
4. 9D, presentation and sound: E16, R05 to R08.
5. 9E, load and overall acceptance: R09, R10, D01.

## Open after 2026-10-11

Nothing in this list has been done; none of it should be read as checked. Phase 9 is not finished.

- A play test of the automatic weapons at their restored rate of fire.
- Looking at a running game: a knife fight around a corner (E02), a held trigger after a switch seen by both players (E04), the pause menu during a fight (E06), a boss spawn and a rescue in a real wave (E12), a zombie killed while running and while standing (E11), the packaged game on screen.
- The shipping build beyond "starts and does not crash": menu, map change and both modes need eyes or a build with logging.
- The owner showing up to two shots more than leave at the end of a magazine under latency; the second shot of a burst during an ease, up to 9 cm off.
- A dedicated server with two clients (R01), movement under latency and packet loss with forced corrections (R02), skins, scale and late join (R03).
- Sound (R05): nothing has been listened to. The sessions that did this work cannot hear.
- Hands, fingers and every weapon action from both views (R06), zombie hit zones, boss clips and the hand-over to the ragdoll on screen (R07), a full match on the reworked arena (R08), load outside the editor (R09).
- Between separate processes (R10): a wrong address or version, the host quitting, a rejoin; choosing each character by hand.
- The hit marker on screen, in hip fire and through the scope at several resolutions, and its two sounds by ear (listening list in `docs/Loadout.md`).
- E16, bullet marks from end points only: open (9D).
