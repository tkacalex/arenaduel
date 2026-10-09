# Hit detection

## How a shot is resolved

All weapons are hitscan and resolved on the server in `UArenaDuelWeaponComponent::FireAuthoritative`. There are no projectiles. For every pellet:

1. A world trace on `ECC_Visibility` finds cover, arena geometry, Arc Barriers and gun range targets.
2. A body trace runs from the eye to that world impact (or to full range) against every other living character through `AArenaDuelCharacter::TraceHitZones`. The nearest body hit wins.
3. The bone that was hit decides the zone through `UArenaDuelWeaponComponent::ClassifyHitBone`.

Because the body trace stops at the world impact, anything in front of a character protects it.

## Hit zones

The hit zones are the physics asset bodies of the world mesh (`PA_Mannequin`). They follow the evaluated animation pose, so standing, crouching, sliding, jumping and wall running are all tested against the pose the character is really in.

| Zone | Bones | Damage |
|---|---|---|
| Head | `head` | `BodyDamage * HeadshotMultiplier` |
| Torso | pelvis, spine, clavicles, neck | `BodyDamage` |
| Limb | upper and lower arms, hands, fingers, thighs, calves, feet | `BodyDamage * LimbDamageMultiplier` (0.8) |

`EArenaDuelShotResult::Limb` is new. Hit markers treat it like a body hit.

The world mesh answers queries only and ignores every collision channel. Nothing blocks or overlaps it, and it is reachable only through `TraceHitZones`. The capsule already ignored weapon traces. The pose keeps ticking on machines that never render the body (`AlwaysTickPoseAndRefreshBones`), which the server needs.

A dead character is skipped. Its mesh becomes a cosmetic ragdoll and can no longer be hit.

## What changed and why

The previous hit zones were two boxes fixed to the capsule: a 76 x 76 x 140 cm body box and a 48 x 48 x 28 cm head box. They did not follow animation, crouch or slide, registered hits in empty space beside the body, and could not tell limbs from the torso. The boxes still exist as disabled components so saved assets keep loading.

The Rift grapple used those boxes to refuse a player as an anchor. It now asks `TraceHitZones`, so a player in the line still blocks it.

## Multiplayer

Only the server traces and applies damage. Clients receive health through the ability system, the shot result through `ClientShotConfirmation`, and tracer end points through `MulticastShotFired`. There is no lag compensation: the server tests the pose it has at the moment the shot arrives.

## Debugging

The admin menu's **Hit zones** toggle draws one box per physics body: gold for head, cyan for torso, violet for limbs.

## Tests

- `ArenaDuel.HitZones.BoneClassification` checks the bone to zone mapping.
- `ArenaDuel.HitZones.TraceMatchesBody` spawns a character, fires rays through head, spine, pelvis, forearm, hand, calf and foot, and checks that rays beside the thigh, beside the shin and above the head miss.
- `ArenaDuel.Rift.Network` covers the grapple rejecting a player.

## Not covered

Limb damage is a new balance value and has not been playtested. The physics asset is Epic's stock asset and its capsules are slightly larger than the visible mesh. Hit zones while crouching, sliding and jumping follow the pose by construction but have no dedicated test.
