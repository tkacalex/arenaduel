# Bunnyhop and air strafing

Everything lives in `UArenaDuelCharacterMovementComponent` and runs inside the engine's character movement simulation, so client prediction, server replay and correction work as for every other move. No new replicated state and no new move flags were added.

## How it works

**Air strafing.** While falling with a movement key held, the engine's air acceleration is replaced by a Source style step (`ComputeAirStrafe`): speed is added along the wished direction only while the velocity along that direction is below `AirStrafeWishSpeed`. Holding forward at speed adds nothing. Holding a strafe key and turning the view with it keeps the wished direction at an angle to the velocity, and each step adds a little. Gains stop at `BhopMaxSpeed`. A jump from a standstill keeps plain air control up to `AirLowSpeedControl`.

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
| `AirStrafeAccelerate` | 8 | Air strafe acceleration, times `WalkSpeed` per second |
| `AirStrafeWishSpeed` | 70 | Air strafe strength; smaller needs more precise turning |
| `AirLowSpeedControl` | 300 | Air control below half walk speed |
| `GroundBrakingFriction` / `GroundBrakingDeceleration` | 12 / 6000 | Ground braking after the grace, unchanged |
| `BhopLandingGrace` | 0.05 s | Time after a landing without braking |
| `LandingSpeedLoss` | 0.04 | Share of the speed above sprint speed lost per landing |
| `JumpBufferSeconds` | 0.12 s | How early a jump may be pressed before a landing |
| `bAutoBhop` | on | Holding jump hops continuously |

Jump height and gravity are the engine properties `JumpZVelocity` and `GravityScale` on the same component. They were not changed.

With these values a perfect strafe gains roughly 160 per second at sprint speed, so going from 900 to the limit takes about three seconds of clean hops. Those figures are calculated from the step, not measured in play.

## Camera and animation

- The first-person weapon's landing kick is reduced to 30 percent for a landing that follows the previous one within 0.9 s.
- The world body stays in the fall clip until the feet have been on the ground for 0.1 s, so chained hops do not flick to the run clip.
- Hit zones follow the animated pose on the server as before; nothing in that path changed.

## Shooting, aiming and switching

Nothing blocks them in the air. Weapon spread already adds `MovementSpreadDegrees` for speed and half of it again while airborne, so hopping costs accuracy.

## Shadow Step

The dash used to be clamped to `GlobalMomentumCap` in the air on the first step, whatever `DashSpeed` said, so it never flew faster than 1350. It now asks the movement component for an allowance (`AllowSpeedUntilLanding`) that lasts until it lands, and `DashSpeed` is 6750, five times that real 1350. Earlier notes about the dash distance at 2200 and 11000 were wrong for that reason.

## Tests

- `ArenaDuel.Bhop.AirStrafeRule`: forward at speed adds nothing, a strafe adds a little, perfect strafing stops at the limit, strafing without turning gains almost nothing, speed above the limit is neither cut nor raised.
- `ArenaDuel.Bhop.Config`: the limits and windows are in a sane relation.
- `ArenaDuel.Phase4.BhopLanding`: a landing at 1200 keeps its speed apart from the landing cost, and without a jump the ground brakes it to walking pace.
- `ArenaDuel.Phase4.SprintJumpKeepsSpeed`, `ArenaDuel.Phase4.AirControlTrajectory` and the rest of `ArenaDuel.Phase4.*` cover that existing movement still works.

## Not verified

No hop was done with held keys in a play session: the editor tooling can press a key but not hold one. That leaves untested in play: continuous auto hopping, manual hop timing, gaining speed by strafing with the mouse, direction changes in the air, hopping as a remote client under real latency, and shooting, aiming and switching weapons while hopping. Whether the values feel right is a matter for a play test.
