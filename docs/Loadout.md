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

Left mouse uses whatever is in hand. Reload and aim only apply to firearms.

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
| `ThrowSpeed` / `ThrowUpSpeed` | 1500 / 220 |
| `FuseSeconds` | 1.4 |
| `MaxBlindDistance` | 1800 |
| `MaxBlindSeconds` | 3.2 |

On detonation the server checks every living character: strength falls off with the square of the distance, looking away leaves a quarter of it, and a wall between burst and eye shields completely. The owner of a blinded pawn gets a full-screen white-out that holds for a third of its time and then fades. Blind time is `MaxBlindSeconds` times the strength. The thrower can blind themselves. After the throw the firearm comes back automatically.

## Knife

| Property (`FArenaDuelKnifeDefinition`) | Value |
|---|---|
| `Range` | 175 |
| `Damage` | 55 |
| `AttackInterval` | 0.5 |
| `SwingHalfWidth` | 14 |

A swing is a server trace from the eye. A world trace first limits the reach, so walls stop the blade. Five rays fanned across the swing width then test the per-bone hit zones of the opponent. A hit goes through the normal damage path, so health, hit marker and death ragdoll work as with a bullet. Knife damage is flat for every body part.

## HUD

The weapon panel shows the item in hand (name, mode, ammo or flashbang count). A line above it lists all slots, with the item in hand in brackets and both firearms for a two gun loadout.

## Tests

- `ArenaDuel.Loadout.Definitions`: one loadout per archetype, valid firearm indices, only one two gun loadout.
- `ArenaDuel.Loadout.FlashbangStrength`: distance and facing falloff.

## Known limits

- Flashbang and knife are shown as simple engine shapes held in the hand. There are no authored models, throw or stab animations.
- There is no flashbang sound and no muffled hearing.
- No equip animation beyond the existing viewmodel dip.
- The mouse wheel is bound directly to the scroll keys, not through an input action, so it cannot be rebound in the input mapping yet.
- Flash blindness on the receiving screen was not captured in a play session.
