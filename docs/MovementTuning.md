# Ground movement tuning

All values are `EditDefaultsOnly` properties on `UArenaDuelCharacterMovementComponent`, in the categories **Movement|Ground**, **Movement|Air** and **Movement|Network**. `ApplyGroundTuning()` copies them onto the engine movement properties in the constructor and every tick, so changing a value in the character Blueprint or in C++ is enough.

## Parameters

| Property | Value | Before | Effect |
|---|---|---|---|
| `GroundAcceleration` | 4200 | 2048 (engine default) | Time from standing to walk speed drops from about 0.29 s to about 0.14 s |
| `GroundBrakingDeceleration` | 6000 | 2048 | Stop after releasing input, together with braking friction |
| `GroundBrakingFriction` | 12 | 16 effective | Stop from walk speed in about 0.07 s instead of about 0.11 s |
| `GroundTurnFriction` | 11 | 8 | Velocity follows a new input direction faster, less sideways drift |
| `CounterStrafeDeceleration` | 5200 | none | Extra braking against velocity that opposes the held input |
| `StrafeSpeedScale` | 1.0 | none | Top speed multiplier while moving sideways |
| `BackwardSpeedScale` | 1.0 | none | Top speed multiplier while moving backwards |
| `AirAcceleration` | 2048 | 2048 | Used for air control and all custom modes, unchanged |
| `RemoteSmoothLocationTime` | 0.07 | 0.10 | How long a remote player's position correction is blended |
| `RemoteSmoothRotationTime` | 0.05 | 0.05 | Same for rotation |

`WalkSpeed` (600), `SprintSpeed` (900), `CrouchSpeed` (350), slide, wall run, vault and mantle values are unchanged. The times above are calculated from the movement equations, not measured in a play session.

## Counter-strafing

`CalcVelocity` calls `ApplyCounterStrafe` while walking. The part of the velocity that points against the held input is reduced at `CounterStrafeDeceleration` before the normal acceleration and friction run. Tapping the opposite key therefore stops sideways movement in about 0.04 s from walk speed, which is faster than letting go of the key. Because weapon spread grows with speed, a clean counter-strafe gives an accurate shot sooner.

The step only reads the velocity and the acceleration of the move being simulated, so client prediction and the server replay produce the same result.

## Animation

The world body picks a forward, backward, left or right rifle clip from the travel direction relative to the facing. The current direction is favoured slightly so diagonals do not flicker, and the stride phase is carried over when the clip changes. There is no blend between clips.

## Peeking

The rifle clips lean the head ahead of the capsule while the camera sits on the capsule axis. `UArenaDuelVisualAnimInstance` slides the world body so the head stays over that axis. The head hit zone therefore leaves cover at the same moment the owner's camera does.

## Network

Duel pawns replicate at 100 Hz with a floor of 60 Hz. Movement still uses the engine's client prediction and server correction; nothing in that path was replaced.

## Tests

- `ArenaDuel.GroundFeel.CentralTuning` checks that the tunables reach the engine properties and that air keeps its own acceleration.
- `ArenaDuel.GroundFeel.CounterStrafeStep` checks the counter-strafe step directly.

## Not verified

Strafing, jiggle and wide peeks, direction changes under real input and the directional clips have not been watched in a play session, because the editor tooling cannot hold movement keys. The `ArenaDuel.Phase4.Network.*` and `ArenaDuel.Phase3.Network.*` tests fail, and already failed before these changes.

## Slide cancel

Pressing the slide key a second time during a slide ends it at once. The player stands up and keeps the speed they have, which then settles to walk or sprint speed through normal ground braking. Holding the key or letting it go does not cancel. `SlideCancelMinTime` (0.15 s) is the earliest point a second press takes effect; a press before that is applied when the time is reached.

The rule reads only the slide intent that is already part of the saved move (`FLAG_Custom_1`) and the slide time, so client prediction and the server decide the same way. After a cancel the key has to be let go before it can start another slide, and the entry boost keeps its `SlideBoostCooldown`, so cancelling cannot be used to stack speed.

`ArenaDuel.Slide.CancelRule` checks the rule. The cancel was not tried with real input in a play session.

## Reload on the world body

The world body keeps its walk, run or fall clip while reloading. The reload clip is layered onto the bones from `spine_02` upward, stretched to the weapon's reload time. Checked only for a standing player without errors; a reloading opponent in motion was not watched.

## Crouch and slide on Left Ctrl

Crouch moved from C to Left Ctrl, the key the slide already used. `IMC_Gameplay` maps both `IA_Crouch` and `IA_Slide` to it and the character decides what a press means:

- Standing or walking: crouch while the key is held.
- Running (on the ground at `SlideMinSpeed` 700 or faster, or sprinting above `SlideQueueMinSpeed` 560 and still accelerating): slide. Walk speed is 600, so a slide needs a sprint or carried momentum.
- Key still held when the slide ends: stay crouched. Key let go during the slide: stand up when it ends.
- Second press during a slide: slide cancel, the player stands up.

`Tools/Editor/RemapCrouchKey.py` applies the key change to the asset. `ArenaDuel.Slide.CrouchAndSlideShareCtrl` checks the mapping. The behaviour was not tried with real key presses in a play session.

## Reading the opponent's stance

The world body shows what its player is doing with the weapon, from replicated state only (`bAiming` on the weapon component, the pawn's base aim rotation):

- Aiming: the aim idle clip is layered onto the body from `spine_02` upward, so the rifle is at the eye whether the legs stand, walk, run or fall.
- Not aiming: standing uses a low ready, the weapon arm turned down by 26 degrees with the support hand following; walking and running show the carry pose of their clips.
- View pitch: spread over `spine_03` and `spine_05`, so chest, arms, weapon and head tilt with the look, up to 80 degrees either way.

All of it runs on every machine, so the server's hit zones follow the same pose. Console helpers for checking it from the other window: `ArenaDuel.Aim 0|1`, `ArenaDuel.Pitch <degrees>`, `ArenaDuel.Slot 0|1|2`.

Checked in a play session for a standing player: not aiming, aiming, looking up and looking down. A walking or running opponent was not watched.

## Aiming while sprinting

Aiming no longer stops at a sprint and starting a sprint no longer drops the aim. While both are held the sprint speed is multiplied by `AimSprintSpeedScale` (0.9, so 810 instead of 900) and the first-person weapon stays at the eye instead of going to the sprint pose. The aim state comes from the weapon component, which the owner sets at once and the server receives a moment later, so a small position correction is possible in the instant aim is pressed or released at full speed. `ArenaDuel.GroundFeel.AimSprintScale` checks the value; the behaviour was not tried with held keys in a play session.

## Shadow Step

See `docs/Bunnyhop.md`, section Shadow Step. The dash speed is 6750 with a 200 lift from the ground, and it may exceed the momentum cap until it lands. The cooldown is still five seconds. `ArenaDuel.Shadow.StepStrength` checks the values; the distance was not measured in a play session.

## Speed in the air

A sprint jump keeps its speed. Two things took it away before: the strong ground braking friction (`GroundBrakingFriction`) was applied by the engine in every movement mode, and the speed limit in the air was walk speed, so anything faster was braked. Now the braking friction only applies while walking, and the limit in the air is the horizontal speed the player already has (never less than walk speed). Steering in the air turns the velocity without adding to it. Landing while still holding sprint continues at sprint speed. The same change lets Shadow Step and a slide jump carry their speed through the air.

`ArenaDuel.Phase4.SprintJumpKeepsSpeed` checks that 900 stays 900 over a jump with forward held. A real sprint jump with key input was not tried in a play session.

## Wall movement for skilled play (2026-10-11)

Ground speeds, slide, bunnyhop, vault and mantle are unchanged. What changed is everything around walls. All of it runs in `UArenaDuelCharacterMovementComponent`, on the server and predicted on the owner through the existing advanced jump flags; every value is a property.

| What | Before | Now |
| --- | --- | --- |
| Wall kick | none: a wall jump needed a wall run | Jump in the air next to any wall (eight directions around the body, `WallKickReach` 48) kicks off it: `WallKickAwayForce` 620 off the wall, the speed along it kept, `WallJumpUpForce` up. No sprint and no minimum speed needed |
| Kick limits | | `MaxWallKicks` 3 per time in the air, `WallKickStaminaCost` 12 each, `WallKickCooldown` 0.18 s, and never the same wall twice in a row. A landing or a wall run gives the kicks back. Climbing therefore takes two walls |
| Wall jump speed | 75 % of the fixed wall run speed | all of the speed carried along the wall (`WallJumpForwardRetention` 1.0) plus the push (`WallJumpAwayForce` 520, up 460) |
| Chains | none | every wall jump or kick in a row adds `WallChainSpeedBonus` 70; a chain may carry up to `WallChainSpeedCap` 1550 until the next landing, above the ordinary cap of 1350 |
| Steering | fixed direction | the look direction turns the jump by `WallJumpAimInfluence` 0.35, only as far as it points away from the wall |
| Wall run entry | 650 minimum speed | `WallRunMinSpeed` 520 |
| Wall run speed | always pulled to 900 | keeps the speed it was entered with; what is above 900 bleeds off at `WallRunSpeedDecay` 160 per second |
| Wall run height | sinks at 20 % gravity from the start, upward speed cut | caught on the way up it keeps rising (up to `WallRunMaxRiseSpeed` 320); gravity grows from `WallRunStartGravityScale` 0.02 to `WallRunGravityScale` 0.3 over the run, so it starts flat and sinks late |
| Wall run stamina | 20 per second | `WallRunDrain` 16 |

Tests: `ArenaDuel.Phase4.WallKick` (kick without a wall run, speed kept, stamina, the cooldown, the same wall refused, no wall and the ground) and `ArenaDuel.Phase4.WallMomentum` (entry speed kept, rising entry, wall jump keeping its speed, chain cap) next to the existing wall run and wall jump tests, which pass unchanged; the whole movement, hop, slide and ability suites pass (48 tests), the network wall run and wall jump among them.

Not checked: none of this has been played by hand. One attempt with real input did not reach the game window. The kick is covered at component level only, not in a network test (it travels the same way a wall jump does). How it feels, whether 1550 is too fast for the map and whether three kicks are too many are questions for a play test.

## Wall runs that let go (2026-10-11)

Reported: while sprinting the player got stuck on walls now and then. Found in the wall run:

- A run moved with a plain sweep and no answer to a hit. Sinking to the floor, or meeting a pillar, a crate or a corner, it stayed pressed against it in place until its three seconds or the stamina ran out. Now the floor or anything standing in the way ends the run (falling then lands or slides past), and any other contact is slid along.
- A run started on any pass beside a wall in the air at sprint speed, also at the very end of a sprint jump or stepping off a kerb. Now it does not start on the way down with less than `WallRunMinGroundClearance` (45 cm) of air under the feet, not when running straight at the wall, and not while steering away from it.
- Holding the movement input away from the wall lets go of a running wall run.

All three depend only on position, velocity and the held input of the move, so the owner and the server decide the same. Test: `ArenaDuel.Phase4.WallRunNoStick`. Not played by hand.