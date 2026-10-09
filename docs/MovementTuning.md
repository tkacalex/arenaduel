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
