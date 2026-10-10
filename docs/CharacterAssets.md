# Character assets: zombies and first person hands

How the bodies are made, where the files are, and what state each one is in. Written 2026-10-10.

## Pipeline

1. **Blender** (5.2, with the MPFB 2.0.17 extension from extensions.blender.org; MPFB is the MakeHuman add-on, its assets are CC0). `Tools/Art/build_zombie.py` makes a human with the variant's build, gives it MPFB's game engine rig, poses that rig joint by joint onto the bind pose of the Unreal mannequin (`Tools/Art/manny.fbx`, exported from the project), bakes the result, marks material regions, binds the mesh to the mannequin armature and exports FBX. `Tools/Art/fp_arms.blend` is the same idea for the arms alone.
2. **Unreal, editor closed**: `Tools/Editor/SetupZombieAssets.py` and `Tools/Editor/SetupFirstPersonHands.py` import the FBX files onto `SK_Mannequin` and create the materials.
3. **Unreal, editor open**: the automation command `ArenaDuel.VisualAssetSetup.CharacterMaterials` assigns the materials by slot name and saves the meshes. This step is C++ because a Python script cannot save a skeletal mesh it has just loaded; an assignment made in the import run itself is lost.

Because every mesh shares the mannequin skeleton and bind pose, the mannequin clips, sockets and physics asset fit without retargeting. The physics asset (`PA_Mannequin`) supplies the hit zones, so head, torso and limb hits work as before.

To rebuild a variant in Blender: `ART_DIR = r"<repo>/Tools/Art"; VARIANT = "Normal"; exec(open(ART_DIR + "/build_zombie.py").read())`.

## Zombies

| Type in code | Variant | Build | Mesh |
|---|---|---|---|
| Normal | Normal | lean | `/Game/ArenaDuel/Characters/Zombies/SKM_Zombie_Normal` |
| Fast | Runner | thin | `SKM_Zombie_Runner` |
| Armored | Armoured | muscular | `SKM_Zombie_Armoured` |
| MiniBoss | Brute | heavy | `SKM_Zombie_Brute` |
| Boss | Abomination | very heavy | `SKM_Zombie_Abomination` |

Each is about 13,500 vertices with five material slots: skin, shirt, trousers, shoes, and glowing eyes. All but the eyes use one baked texture set, `T_Zombie_BaseColor`, `T_Zombie_Normal` and `T_Zombie_Roughness` (2048 px, made by `Tools/Art/bake_zombie_textures.py`): veined, blotched skin with sores, woven cloth with dirt and stains. The skin is baked pale; the alpha of the colour map marks it and each variant's material tints it there. The normal map is baked green up and flipped on import. `FArenaDuelZombieTypeConfig::MeshPath` names the body; with a body set, the old tint material is not applied. `VisualScale` still scales the actor, so the big types are scaled once, by the game, not in the mesh.

Animation: the walk is the zombies' own, `A_Zombie_Shamble`, authored in Blender on the mannequin armature by `Tools/Art/build_zombie_anims.py` (`Tools/Art/zombie_anims.blend`, `A_Zombie_Shamble.fbx`, imported by `SetupZombieAssets.py`): a 40 frame loop in place, short stiff steps with the left leg, the right leg dragged with its foot turned out, swaying hips, a stooped trunk rocking against them, a lolling head. Its play rate follows the real speed (2.6 m a second at rate 1). Idle and the runners' jog are still the mannequin's unarmed clips from the engine templates. The arms are taken off the clip and reach forward. A swing plays an unarmed attack clip on the upper body (two alternating one-handed clips, a charged one for slams and bosses), timed so that its impact falls on the hit the server deals; hips and legs keep walking. The attack timing, reach and damage on the server are unchanged, and no animation notify deals damage.

| | Created | Exported | In Unreal | Checked in play |
|---|---|---|---|---|
| Normal | yes | yes | yes | yes: walking, reaching, materials, eyes (`docs/media/zombies_2026-10-10_wave8.png`) |
| Runner | yes | yes | yes | spawned in waves 3 to 8, not looked at separately |
| Armoured | yes | yes | yes | spawned in waves 7 and 8, not looked at separately |
| Brute | yes | yes | yes | spawned in waves 5 and 9, not looked at separately |
| Abomination | yes | yes | yes | spawned in wave 10 (boss bar shown), not seen on screen |

A fast-forwarded run went through waves 1 to 10 with these bodies and no errors in the log. 17 of 17 selected automation tests pass, the survival, hit zone, visual and network smoke tests among them.

### What is missing

- **Textures** are procedural patterns baked to maps, not painted or sculpted: there are no wounds in the geometry and no real cloth folds. The feet are bare, coloured dark.
- **Variants** differ in build, skin tone and gear: the armoured zombie wears a helmet, chest and back plates, shoulder pads and shin guards (`M_ZombieArmour`); the brute and the abomination carry bone spurs along the back, the abomination longer ones and two on the shoulders (`M_ZombieBone`). The gear is made of plain spheres, boxes and cones, each carried rigidly by one bone, and was looked at in Blender only; it does not change damage.
- **Clothes** are regions of the body pushed out a few millimetres, not separate garments; the collar and sleeve edges are ragged where the region ends.
- **Animation**: the clips are those of an athletic human. A hit plays one of the mannequin's two front hit reactions on the upper body (light or heavy; a new hit restarts it only when the last is mostly through, bosses do not flinch), on top of the lean. There is no death clip (ragdoll) and no charge, slam or enrage animation. The shamble was looked at as four frames in Blender and, in the game, only from the waist up: the bodies stay whole and upright with it, the feet were not watched for sliding. The flinch clip was not seen in a capture. The impact time of the attack clips is an estimate (42 percent of the clip) and was not checked against the visible swing.
- **Posture**: the anim instance stoops the spine, tilts the head differently per zombie and rocks the body with the walk; runners lean further forward. Seen only as stills.
- **Hits**: the first import left the bodies without a physics asset, so no shot landed on them. The setup command now gives them the mannequin's, the zombie falls back to it at run time, and `ArenaDuel.Survival.EnemyTypes` checks that a spawned zombie has hit zones. With real shots afterwards: two kills, 200 points, a ragdoll on the floor (`docs/media/zombies_2026-10-10_textured_hit.png`).
- **Not checked**: the hit zone alignment on the heavier builds, head shots specifically, levels of detail (there are none), and frame time against the old bodies.

## First person hands

See `docs/FirstPersonArms.md`. The gloved hands replaced the mannequin forearms. They carry their materials only since the setup command above exists: the first import left them on the default grid material, which went unnoticed until the zombies showed the same fault.

## Firearms

`Tools/Art/build_weapons.py` builds the four firearms in Blender from bevelled boxes and tubes and exports `Tools/Art/Weapons/SM_<Name>.fbx` (`Tools/Art/weapons.blend` holds all four). `Tools/Editor/SetupWeaponMeshes.py` imports them over the old block models, in two editor runs: the import, then, with `ARENADUEL_WEAPON_FINISH=1`, the `Muzzle` socket and the two materials.

The meshes keep every convention of the old ones, so no code changed: origin at the pistol grip, +X to the muzzle, barrel at a height of 13, sizes in centimetres of the oversized game mesh, muzzle three centimetres past the barrel, slot 0 body, slot 1 the accent the game recolours per archetype.

| Weapon | What it has now | Vertices |
|---|---|---|
| Arc Rifle | curved magazine, octagonal handguard with vents, gas block, muzzle brake, buffer tube and stock, iron sights | 1,328 |
| Shade SMG | long straight magazine, barrel shroud, suppressor, wire stock, stubby foregrip | 1,120 |
| Rune DMR | long barrel with brake, slim handguard, fixed stock with cheek rest, scope with mounts, lenses and turret, folded bipod | 1,376 |
| Hex Shotgun | barrel over magazine tube, ribbed pump, full stock, bead sight, ejection port | 928 |

Created, exported, in Unreal: all four. Checked in play: stills at the hip and aiming in first person (`docs/media/weapons_2026-10-10_first_person.png`); 13 of 13 visual, loadout, hit zone and survival tests, the network smoke test with the weapon names among them. Not checked: the world models in an opponent's hands, the muzzle flash position, the support hand against the new handguards (the grip points are the old ones), and firing or reloading with them. No moving parts, no textures: the body is one flat material.

## Props and pickups

`Tools/Art/build_props.py` builds four props in Blender (`Tools/Art/props.blend`, `Tools/Art/Props/SM_<Name>.fbx`); `Tools/Editor/SetupProps.py` imports them to `/Game/ArenaDuel/Props` in two editor runs (import, then with `ARENADUEL_PROP_FINISH=1` materials and box collision). Each prop is centred and exactly as big as the block it replaces.

| Prop | Size (cm) | Where it is used | Checked in play |
|---|---|---|---|
| `SM_Crate` | 140 cube, framed, braced | `SetupSurvivalAndMenu.py` places it for every 140 cm block of the survival arena | yes, still |
| `SM_Container` | 900 x 240 x 260, corrugated, doors | the same script, for the yard's containers | yes, still |
| `SM_AmmoBox` | 42 x 28 x 22, lid, latches, handles | `AArenaDuelAmmoPickup` (falls back to the cube without it) | yes, lying in the arena |
| `SM_HealPad` | 120 across, ring and cross | `AArenaDuelHealPad` in both maps (falls back to the plate without it) | yes, in the survival arena |

Crate and container have one box of their own size as collision. After the swap, three waves ran in auto play with every sampled enemy on a valid navigation path, and the survival and map tests pass. Not checked: walking into the props, vaulting the crates, the heal pad in the duel map, and the duel map's own blocks, which are unchanged cubes.

Note on re-running `SetupZombieAssets.py`: it re-imports the bodies, which drops their materials and physics asset again. Run `ArenaDuel.VisualAssetSetup.CharacterMaterials` in the editor afterwards, every time.
