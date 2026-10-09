# Bunnyhop and air strafing

Everything lives in `UArenaDuelCharacterMovementComponent` and runs inside the engine's character movement simulation, so client prediction, server replay and correction work as for every other move. No new replicated state and no new move flags were added.

## How it works

**Air strafing.** While falling with a movement key held, the engine's air acceleration is replaced by a Source style step (`ComputeAirStrafe`): the push acts along the wished direction only while the velocity along that direction is below `AirStrafeWishSpeed`. Holding forward at speed adds nothing. Holding a strafe key and turning the view with it keeps the wished direction just ahead of sideways, and the velocity turns and grows. Gains stop at `BhopMaxSpeed`. Two things differ from Source on purpose: the step is integrated in slices of 1/240 s, and the sideways part of the push only turns the velocity while the part along it adds speed. Plain vector addition would turn the sideways part into speed too, by an amount that grows with the length of a frame. Below `AirLowSpeedControl` the air is steered plainly: input accelerates where it points, up to that speed, so a jump at walking pace handles as it always did.

**Keeping speed over a landing.** For `BhopLandingGrace` after touching down the ground does not brake. A jump inside that window leaves with the speed it arrived with. The window is the same for every landing and does not depend on input, so the server and the owning client simulate it identically. Without a jump, normal ground braking brings the player back to sprint or walk speed.

**Landing cost.** Each landing removes `LandingSpeedLoss` of the speed above sprint speed. Chained hops therefore settle below the limit unless the player keeps strafing well. Running at or below sprint speed is not affected.

**Auto hop and manual hop.** With `bAutoBhop` on, holding jump jumps again on every landing. Independently of that, a jump pressed up to `JumpBufferSeconds` before a landing counts for that landing, and a press inside the landing grace keeps the speed, so the hop can also be timed by hand. Only the owning machine knows the key; the resulting jump goes to the server in the next move like any jump.

**Limits.** Falling movement is clamped to `GlobalMomentumCap` every step, as before. Air strafing is limited to `BhopMaxSpeed`, which is below that cap. Movement is swept by the engine, so speed does not pass through walls; hitting a wall removes the speed into it as usual.

## Parameters

All are `EditDefaultsOnly` properties of the movement component.

| Property | Value | Meaning |
|---|---|---|
| `WalkSpeed` / `SprintSpeed` | 600 / 900 | Ground speeds, unchanged |
| `BhopMaxSpeed` | 1300 | Highest speed air strafing can reach |
| `GlobalMomentumCap` | 1350 | Hard limit in the air, unchanged |
| `GroundAcceleration` | 4200 | Ground acceleration, unchanged |
| `AirStrafeAccelerate` | 3 | Air strafe acceleration, times `WalkSpeed` per second |
| `AirStrafeWishSpeed` | 70 | Air strafe strength; smaller needs more precise turning |
| `AirLowSpeedControl` | 600 | Plain air steering below this speed |
| `GroundBrakingFriction` / `GroundBrakingDeceleration` | 12 / 6000 | Ground braking after the grace, unchanged |
| `BhopLandingGrace` | 0.05 s | Time after a landing without braking |
| `LandingSpeedLoss` | 0.04 | Share of the speed above sprint speed lost per landing |
| `JumpBufferSeconds` | 0.12 s | How early a jump may be pressed before a landing |
| `bAutoBhop` | on | Holding jump hops continuously |

Jump height and gravity are the engine properties `JumpZVelocity` and `GravityScale` on the same component. They were not changed.

With these values a strafe held in the middle of the window gains roughly 70 to 100 per second at sprint speed and needs the view to turn at about 115 degrees per second. A scripted strafe that corrects its angle once per step gains about a quarter more at 60 steps per second than at 240, because a coarser step drifts deeper into the window before the next correction; holding a strafe key without turning gains next to nothing at any rate.

## Camera and animation

- The first-person weapon's landing kick is reduced to 30 percent for a landing that follows the previous one within 0.9 s.
- The world body stays in the fall clip until the feet have been on the ground for 0.1 s, so chained hops do not flick to the run clip.
- Hit zones follow the animated pose on the server as before; nothing in that path changed.

## Shooting, aiming and switching

Nothing blocks them in the air. Weapon spread already adds `MovementSpreadDegrees` for speed and half of it again while airborne, so hopping costs accuracy.

## Shadow Step

The dash used to be clamped to `GlobalMomentumCap` in the air on the first step, whatever `DashSpeed` said, so it never flew faster than 1350. It now asks the movement component for an allowance (`AllowSpeedUntilLanding`) that lasts until it lands, and `DashSpeed` is 6750, five times that real 1350. Earlier notes about the dash distance at 2200 and 11000 were wrong for that reason.

## Tests

- `ArenaDuel.Bhop.AirStrafeRule`: forward at speed adds nothing, a sideways push turns without adding speed, a push slightly ahead of sideways adds a little, a minute of good strafing stops at the limit, a strafe key held without turning gains next to nothing, speed above the limit is neither cut nor raised, and the gain is similar at 60 and 240 steps per second and never larger at 20.
- `ArenaDuel.Bhop.Config`: the limits and windows are in a sane relation, and the landing cost through `ComputeLandingSpeed`.
- `ArenaDuel.Phase4.SprintJumpKeepsSpeed`, `ArenaDuel.Phase4.AirControlTrajectory` and the other eighteen `ArenaDuel.Phase4.*` component tests, the slide, Shadow Step, hit zone, visual, loadout and round tests: 41 tests in the last run, all passing.
- The landing grace has no automated test: characters spawned in the editor test world do not land reliably.

## Checked in a play session

Two player PIE session (listen host and one client) on `L_ArenaDistrict`, driven by the development switches `ArenaDuel.Dev.Hop` and `ArenaDuel.Dev.Strafe`, which hold sprint, forward and jump on every locally controlled pawn. `ArenaDuel.Dev.Hop 3` logs every movement step. The session ran at about 20 frames per second.

- Auto hop, host and client: eight landings in a row at 890 for both, the speed the first jump left with. The client's speed at every landing was the same on its own machine and on the server.
- Server corrections of the client in that seven second run: four, of 48, 62, 5 and 25 cm.
- One strafed hop: 890 to 930 for host and client, again identical on client and server.
- The step log of an earlier run showed client and server with the same position and speed in every logged step.

Two faults were found this way and fixed: the automatic jump on landing was cleared by the engine at the end of the same step and never happened for real input, and a hard speed threshold in the air steering made client and server diverge.

## Not verified

- Hopping with real keys and mouse. The scripted strafe ran into walls after its first hop, so a chain of strafed hops building speed towards the limit was not observed in play, only in the rule test.
- Manual hop timing with auto hop off, and the jump buffer.
- Higher frame rates and real network latency: the session was a single process at about 20 frames per second.
- Shooting, aiming and switching weapons while hopping.
- The calmer landing kick and the fall clip hold were not looked at.
- Whether the values feel right.