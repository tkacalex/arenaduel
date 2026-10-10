# Loadout, flashbang and knife

Everything lives on `UArenaDuelWeaponComponent`. Firearm logic (fire, reload, ammo) is unchanged; the loadout decides which firearms a player may hold and adds two more slots.

## Slots and keys

| Key | Slot | Notes |
|---|---|---|
| 1 | Firearm | A second press swaps firearms when the loadout has two |
| 2 | Flashbang | Only while one is left |
| 3 | Knife | Always available |
| 4 | Swap firearm | Same as a second press of 1 |
| Mouse wheel | Next / previous item | Steps through both firearms, the flashbang if left, and the knife |

Left mouse uses whatever is in hand. Right mouse aims a firearm; with the flashbang or the knife it is the second action. Reload only applies to firearms.

| Item | Left mouse | Right mouse |
|---|---|---|
| Flashbang | Long throw | Short lob |
| Knife | Quick slash, 25 damage | Heavy stab, 70 damage |

## Loadouts

`Loadouts` holds one `FArenaDuelLoadoutDefinition` per character archetype, in enum order. Each lists firearm indices into `WeaponDefinitions` and a flashbang count.

| Archetype | Firearms | Flashbangs |
|---|---|---|
| Shadow | Shade SMG | 1 |
| Warden | Hex Shotgun | 1 |
| Rift | Rune DMR, Arc Rifle | 1 |

The server applies the loadout from the player's archetype (`ApplyLoadoutFromArchetype`). A pawn is respawned every round, so ammo and flashbangs reset with it. Changing the archetype reapplies the loadout.

To add a weapon, add it to `MakeDefinitions` and `WeaponVisualDefinitions` and reference its index from a loadout. To give another archetype two firearms, list two indices.

## Rules

- All switching, throwing and stabbing is decided on the server. `ActiveSlot`, `LoadoutFirearms` and `FlashbangsRemaining` replicate.
- A firearm outside the loadout cannot be equipped. `GrantAllWeaponsForDevelopment` lifts that for the admin menu and tests.
- No switching while a reload is running; the reload finishes first. This keeps the reload timer the only place ammo moves.
- After a switch the new item is usable after `SwitchSeconds` (0.25 s). Switching stops fire and aim.
- A dead player or a finished round blocks every action.

## Flashbang

`AArenaDuelFlashbang` is a replicated actor with projectile movement. It bounces off the arena and passes through players.

| Property (`FArenaDuelFlashbangDefinition`) | Value |
|---|---|
| `ThrowSpeed` / `ThrowUpSpeed` (long throw) | 1500 / 220 |
| `ShortThrowSpeed` / `ShortThrowUpSpeed` (short lob) | 650 / 260 |
| `FuseSeconds` | 1.4 |
| `MaxBlindDistance` | 3500 |
| `MaxBlindSeconds` | 3.2 |

On detonation the server checks every living character: strength falls off with the square of the distance, looking away leaves a quarter of it, and a wall between burst and eye shields completely. The owner of a blinded pawn gets a full-screen white-out that holds for a third of its time and then fades. Blind time is `MaxBlindSeconds` times the strength. The thrower can blind themselves. After the throw the firearm comes back automatically.

## Knife

| Property (`FArenaDuelKnifeDefinition`) | Value |
|---|---|
| `Range` | 175 |
| `Damage` / `AttackInterval` (quick slash) | 25 / 0.4 |
| `HeavyDamage` / `HeavyAttackInterval` (heavy stab) | 70 / 1.0 |
| `SwingHalfWidth` | 14 |

A swing is a server trace from the eye. A world trace first limits the reach, so walls stop the blade. Five rays fanned across the swing width then test the per-bone hit zones of the opponent. A hit goes through the normal damage path, so health, hit marker and death ragdoll work as with a bullet. Knife damage is flat for every body part. Both attacks share the reach and the trace; they differ in damage and in the time until the next attack.

## HUD

The weapon panel shows the item in hand (name, mode, ammo or flashbang count). A line above it lists all slots, with the item in hand in brackets and both firearms for a two gun loadout.

## Tests

- `ArenaDuel.Loadout.Definitions`: one loadout per archetype, valid firearm indices, only one two gun loadout.
- `ArenaDuel.Loadout.FlashbangStrength`: distance and facing falloff.

## Known limits

- Knife and flashbang models are built in code (`ArenaDuelItemMeshes`), not authored assets. There are no throw or stab animations; an attack only kicks the viewmodel.
- Short lob distance and the two knife attacks were not tried against an opponent in a play session.
- There is no flashbang sound and no muffled hearing.
- No equip animation beyond the existing viewmodel dip.
- The mouse wheel is bound directly to the scroll keys, not through an input action, so it cannot be rebound in the input mapping yet.
- Flash blindness on the receiving screen was not captured in a play session.

## Aim rules

- The aim key is remembered while it is held (`bAimHeld`, local only). A reload, a weapon or slot switch pauses the aim and it returns by itself afterwards, without pressing the key again. Death, round end and opening a menu clear it.
- The server accepts aiming only for a firearm that is not being reloaded, and lowers the aim itself when a reload starts, including the automatic reload on an empty magazine. The opponent therefore sees the aim pose only while the player can really aim.
- Reload with a full magazine does nothing and no longer drops the aim.
- The aim accuracy bonus fades in over `AimSettleSeconds` (0.12 s) after aiming begins, on the server too, instead of applying on the click. The spread values themselves (`BaseSpreadDegrees`, `MovementSpreadDegrees`, `AimSpreadMultiplier`) are unchanged.

`ArenaDuel.Loadout.AimRules` checks the defaults. Holding the key through a reload or a switch was not tried with real input in a play session.

- Mouse sensitivity while zoomed is each weapon's `AimSensitivityMultiplier` times `AimSensitivityScale` (0.85), so every weapon turns 15 percent slower in the zoom than before. The player's own ADS setting still applies on top.

- The Rune DMR's `HeadshotMultiplier` is 2.5 (was 1.6): a head shot does 105 and kills from full health; body 42 and limb 34 are unchanged. `ArenaDuel.Loadout.SniperHeadshotKills` checks it on the player Blueprint.

## Telescopic sight

A weapon definition with `bHasScope` gets a telescopic sight; at present that is the Rune DMR. The logic sits in `UArenaDuelWeaponComponent` next to the existing aim code and reuses it: the server still sees one aim state, the world body shows the aim pose, and shots are traced from the eye along the view as always, so the reticle centre is where the shot goes at every zoom.

| Property (`FArenaDuelWeaponDefinition`) | Value | Meaning |
|---|---|---|
| `ScopeFOVFirst` / `ScopeFOVSecond` | 40 / 15 | Field of view of the two zoom levels |
| `ScopeSensitivityScale` | 1.0 | Mouse sensitivity in the scope follows the zoom (ratio of the view tangents); this scales it. The player's ADS setting applies on top |
| `ScopedMoveSpeedScale` | 0.6 | Ground speed while looking through the scope |
| `ScopeRezoomSeconds` | 0.16 | After a shot the scope drops this long and returns to the same level |

- Right mouse steps: first zoom, second zoom, out. It is a press, not a hold. Other weapons keep aiming while the key is held.
- Scope in and out is the existing move of the weapon to the eye with the field of view gliding along; the scope picture fades in over the last quarter and the first-person weapon and arms are hidden inside it. There are no authored scope animations.
- The picture is drawn on the HUD canvas: black outside a round field of view, a shaded lens edge, a fine black cross and three heavier posts with a faint pale edge so they stay readable on a dark scene. Sizes follow the viewport height.
- A reload, including the automatic one, a weapon or slot switch, death, round end and opening a menu leave the scope; it does not come back by itself.
- Spread values are unchanged. Accuracy still depends on speed and on being in the air, and the aim bonus still fades in over `AimSettleSeconds`.
- Scoped movement uses the replicated aim state, like aiming while sprinting, so a small position correction is possible in the instant the scope is entered or left while moving.

Checked in a play session as the host: hip, first zoom, second zoom and out; the scope gone after a weapon switch and after a slot switch; one shot at the second zoom in slow motion, which showed the scope dropped with the tracer leaving the barrel and then back at the same zoom. `ArenaDuel.Loadout.SniperScope` checks the definition values.

Not verified: real mouse input, the scope as a remote client, movement speed in the scope, the reload case, sensitivity feel, hits on an opponent through the scope, other resolutions and aspect ratios than the 16:9 play window.

## Where shots start

Shots, knife swings and throws start at `AArenaDuelCharacter::GetPawnViewLocation`, which is where the camera settles: a fixed height above the capsule centre, standing or crouched, and a little lower in a slide. The engine's default uses the crouched eye height instead, 32 cm below this camera, so crouched shots left from under the crosshair. `ArenaDuel.Loadout.CrouchedShotOrigin` checks it. Not checked with real crouched shots at a target in a play session.

## Knife and flashbang: models, motion, sound (2026-10-10)

Rules, damage, ranges, timings and the server's part are unchanged. What changed is how the two items look, move and sound.

- **Models**: `Tools/Art/build_equipment.py` builds both in Blender (`Tools/Art/equipment.blend`, `Tools/Art/Equipment/SM_*.fbx`): a combat knife with a ribbed grip, a guard longer on the edge side, a clip point blade with a fuller and a pommel with a lanyard ring; a stun grenade with hexagonal end caps and vent holes, two bands, fuse head, safety lever, pin and pull ring. They keep the sizes and axes of the code-built models, which remain as the fallback (`ArenaDuelItemMeshes`).
- **Materials and sounds**: `Tools/Editor/SetupEquipmentAssets.py` (two editor runs, the second with `ARENADUEL_EQUIPMENT_FINISH=1`) makes six plain materials under `/Game/ArenaDuel/Weapons/Equipment` and synthesises seven sounds into `/Game/ArenaDuel/Audio`: knife swing, stab and hit, pin, bounce, bang, and the ringing in the ears.
- **Knife motion** (first person, cosmetic): a light attack is a slash from the right across the view, 0.26 s; a heavy one is drawn back and driven straight forward, 0.42 s. Both start with the server's swing, as before; nothing waits for them.
- **Throw**: the grenade leaves at once, as before. The arm then follows through for a third of a second, over and down for a long throw, an underhand flick for a short one.
- **Flashbang in the world**: a clink on each real bounce (the server sees the bounce, everyone hears it, at most a few a second), a bang and a burst of sparks at the detonation next to the existing flash of light, and for a blinded player a ringing that is louder the stronger the blindness.

Checked in a play session: both items in the hand in first person, a short throw that blinded the thrower, light and heavy knife attacks with real mouse buttons without errors in the log; 13 of 13 visual, loadout, hit zone and survival tests. Not checked: the sounds (nobody listened), the swing and throw motion as motion (no capture caught them), the sparks, the bounce sound, any of it from the other player's side, quick switching, and the hand's fingers on the new shapes, which still come from the rifle clip. The world body has no knife or throw animation.

## Firearms: moving parts, textures, fingers, sprint and equip (2026-10-10)

Rules and tuning are unchanged; all of this is presentation.

- **Three meshes per weapon**: `Tools/Art/build_weapons.py` now exports the body, `SM_<Name>_Bolt` (bolt carrier and charging handle; on the shotgun the pump with its action bars) and `SM_<Name>_Mag` (the shotgun has none). They share the body's origin; `UArenaDuelWeaponComponent::RefreshWeaponParts` finds them by name and attaches them to the first person and the world weapon.
- **Motion** (`TickWeaponParts`): on a shot the bolt runs back and forward (travel and time per weapon, the shotgun's pump takes 0.45 s). During a reload the magazine drops out, is gone for a moment, a full one comes up, and the bolt is run once; the shotgun's pump is worked twice. The shooter sees all of it. Other players see the bolt on a shot; the reload motion of the parts runs only where the weapon component ticks, which for someone else's weapon is the short time after a shot.
- **Textures**: the same script bakes a base colour, a normal map and a roughness/metallic map per weapon (2048 px, procedural: gunmetal and polymer, worn bright on the edges, scratched, dusty). `Tools/Editor/SetupWeaponMeshes.py` imports them and builds `M_<Name>` (the baked PNGs are not in the repository, the bake writes them again); the game no longer forces the plain metal onto a mesh that has its own material. The accent faces are still recoloured per archetype.
- **Fingers** (`ArenaDuelVisualAnimInstance.cpp`): each held item adds a curl to the rifle clip's fingers: a tighter fist on the knife, a firm hold on the grenade, a half open free hand, and a support hand that lies around a handguard or wraps the SMG's foregrip and the shotgun's pump. The trigger finger squeezes on a shot. `ArenaDuel.Fingers.Scale` and `ArenaDuel.Fingers.Test` tune them.
- **Sprint and equip**: the viewmodel carries a firearm across the body at a sprint, muzzle to the left and up, swinging with the stride; an item that is equipped comes up from below, rolled, and settles. These are motions of the viewmodel in code, as the recoil is; the arms follow because they are solved to the weapon. There are no authored first person animation clips.
- **Stills for checking**: `ArenaDuel.Preview.Reload`, `.Bolt`, `.Sprint` and `.Equip` hold a motion at a chosen point.

Checked in a play session: all four weapons textured in the hand, hip and aimed; the magazine out of the rifle at a held reload point; the sprint and the equip pose as stills; the fist on the knife after the right hand's curl direction was corrected; a short session with real input (fire, reload, sprint, switching between all three slots) without errors in the log; tests `ArenaDuel.Visuals.WeaponParts` and 19 others. Not checked: the bolt (too small to make out in the captures), the parts and finger poses on the world body, any of it from another player's side, the final finger values on the rifle, DMR and grenade (changed after the last close look), and how the motion reads in motion.
