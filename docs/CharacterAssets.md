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

Animation: the mannequin unarmed idle, walk and jog clips from the engine templates. The arms are taken off the clip and reach forward. A swing plays an unarmed attack clip on the upper body (two alternating one-handed clips, a charged one for slams and bosses), timed so that its impact falls on the hit the server deals; hips and legs keep walking. The attack timing, reach and damage on the server are unchanged, and no animation notify deals damage.

| | Created | Exported | In Unreal | Checked in play |
|---|---|---|---|---|
| Normal | yes | yes | yes | yes: walking, reaching, materials, eyes (`docs/media/zombies_2026-10-10_wave8.png`) |
| Runner | yes | yes | yes | spawned in waves 3 to 8, not looked at separately |
| Armoured | yes | yes | yes | spawned in waves 7 and 8, not looked at separately |
| Brute | yes | yes | yes | spawned in waves 5 and 9, not looked at separately |
| Abomination | yes | yes | yes | spawned in wave 10 (boss bar shown), not seen on screen |

A fast-forwarded run went through waves 1 to 10 with these bodies and no errors in the log. 17 of 17 selected automation tests pass, the survival, hit zone, visual and network smoke tests among them.

### What is missing

- **Textures** are procedural patterns baked to maps, not painted or sculpted: the veins read as a net of cracks, there are no wounds in the geometry and no real cloth folds. The feet are bare, coloured dark.
- **Variants** differ in build and skin tone only. The armoured zombie has no armour geometry, the bosses no distinguishing features beyond size.
- **Clothes** are regions of the body pushed out a few millimetres, not separate garments; the collar and sleeve edges are ragged where the region ends.
- **Animation**: the clips are those of an athletic human. There is no zombie gait, no hit reaction clip (the flinch is still a lean of the body), no death clip (ragdoll), no charge, slam or enrage animation. The impact time of the attack clips is an estimate (42 percent of the clip) and was not checked against the visible swing.
- **Posture**: the anim instance stoops the spine, tilts the head differently per zombie and rocks the body with the walk; runners lean further forward. Seen only as stills.
- **Hits**: the first import left the bodies without a physics asset, so no shot landed on them. The setup command now gives them the mannequin's, the zombie falls back to it at run time, and `ArenaDuel.Survival.EnemyTypes` checks that a spawned zombie has hit zones. With real shots afterwards: two kills, 200 points, a ragdoll on the floor (`docs/media/zombies_2026-10-10_textured_hit.png`).
- **Not checked**: the hit zone alignment on the heavier builds, head shots specifically, levels of detail (there are none), and frame time against the old bodies.

## First person hands

See `docs/FirstPersonArms.md`. The gloved hands replaced the mannequin forearms. They carry their materials only since the setup command above exists: the first import left them on the default grid material, which went unnoticed until the zombies showed the same fault.
