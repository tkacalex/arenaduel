# Maps

## L_ArenaDistrict

The 1v1 map and the editor startup map; the game itself starts on `L_MainMenu` (see `docs/MainMenu.md`). A closed 70 x 50 m hall with a 15 m ceiling, mirrored across the centre line so both players get the same geometry. It is built from engine cubes by `Tools/Editor/SetupArenaDistrict.py`; change the layout there and run the script with the editor closed, as `Tools/Editor/SetupPhase7EAssets.ps1` does for its script.

Sizes follow the movement component: 80 cm cover can be vaulted and crouched behind, steps of at most 140 cm can be mantled, 3.5 m gaps are a sprint jump.

| Area | What is there |
|---|---|
| Spawns | A 3.3 m wall in front of each spawn with a window in the middle and a door on each side, so there is no shot from spawn to spawn. A thin cyan strip marks player one, violet player two. |
| Field | Between spawn and centre: five pillars, two head-high boxes, two low walls, two angled walls and a free-standing wall with a window on each flank. |
| Centre house | Windows towards both spawns, a door and two windows north and south, a pillar inside, a crate under the windows. The roof is reached over two crates at the north corners and has parapets to crouch behind. |
| North lane | Parkour: a mantle step, three decks with sprint-jump gaps, a 7 m wall beside the gaps for wall runs, and a bridge over the lane centre that can also be run under. |
| South lane | Tight cover behind a divider with two windows and a door per side: crates, a crate stack, a low wall, a head-high box, a pillar and a centre block with a window slot. |

### Heal pad

One small green plate (1.2 x 1.2 m) lies in the open north of the centre house, on the centre line. A living player standing on it gets 5 health per second, added one point every 0.2 s, up to the maximum. Both flank windows look straight at it, so healing means standing in a crossfire. `AArenaDuelHealPad` does it on the server; `HealPerSecond`, `HealStep` and `PadHalfSize` are properties of the actor. The plate has no collision.

Checked in a play session with the host standing on the pad (`ArenaDuel.Health <value>` sets the health for such a check): at one tenth game speed the health went 22, 31, 39, 48 at captures about 19.2 real seconds apart, which is roughly 4.5 per game second in single steps against the configured 5; the capture timing is not exact. Standing beside the pad at 50 health, it stayed at 50. A second player and a crouching player were not tried.

Look: see "L_ArenaDistrict: the look" below.

`ArenaDuel.Maps.ArenaDistrict` checks the two starts, that they mirror and face each other, that the weapon trace from spawn to spawn is blocked, and that the pieces are there.

Not verified: the layout was looked at from the spawn and from above in a play session, but no round was played on it. Jump gaps, the wall run, the mantle steps and the roof route are sized from the movement values, not run with real input. Sightlines other than spawn to spawn were not checked.

## L_ZombieArena and L_MainMenu

Built by `Tools/Editor/SetupSurvivalAndMenu.py`. `L_ZombieArena` is the Zombie Survival hall, 84 x 84 m. A perimeter wall 8 m inside the shell hides twenty tagged spawn points in the corridor behind it; zombies come in through twelve gates, three per side, each marked by a red light. Inside are five areas: an open plaza around the start, a container yard with lanes in the north, a building with rooms and doorways in the east, a raised deck with two ramps in the south and a hall of pillars in the west. Dim fog and a looping ambience set the mood. See `docs/ZombieSurvival.md`. `L_MainMenu` is empty apart from its game mode.

## Other maps

- `L_ArenaCore`: the empty hall that was the default before. Unchanged, still used by `ArenaDuelPhase7ETests`.
- `L_Phase4MovementTest`, `L_Phase3Test`, `L_Phase5GunRange`: test and development maps used by automation.

## L_ArenaDistrict: modelled crates (2026-10-10)

`Tools/Editor/SetupArenaDistrict.py` places the modelled crate (`/Game/ArenaDuel/Props/SM_Crate`) wherever it used to place a crate-coloured cube, scaled to the block's exact size, a tall block as a stack. Positions, cover heights and collision are what they were; `ArenaDuel.Maps.ArenaDistrict` and `ArenaDuel.Phase7E.Arena.Structure` pass. Walls, decks, pillars and roofs are still engine cubes.

## L_ArenaDistrict: the look (2026-10-11)

The layout, the spawns, every piece of cover, every route and all collision are what they were; `ArenaDuel.Maps.ArenaDistrict` and the duel network tests pass unchanged. What changed is how the hall looks. It follows a reference picture the map's owner supplied: a dark industrial hall with a wet floor, warm lamps on light concrete, timber, orange markings and neon in the team colours.

Three scripts build it, in this order, with the editor closed:

1. `Tools/Art/build_district_textures.py` (in Blender, for its numpy): seven tiling texture sets, 2048 px, synthesised from periodic noise, so they tile without a seam and nothing is taken from anywhere: cast concrete with formwork seams and tie holes, sealed floor slabs with saw cuts and hairline cracks, painted render, tread plate, a ribbed steel deck, painted steel, sawn timber, and a large soft mask. `Tools/Art/build_district_props.py` models the set dressing: a wall lamp, a linear hall light, a planted tub with a conifer, hanging ivy.
2. `Tools/Editor/SetupDistrictMaterials.py` imports the textures to `/Game/ArenaDuel/Maps/District` and builds one parent material, `M_DistrictSurface`, with an instance per surface. The map is made of scaled cubes whose own UVs stretch, so the material maps in world space along whichever axis a face looks most: one repeat always covers the same number of centimetres. The soft mask varies brightness over many metres so the repeat does not show, and walls darken towards the floor. `Tools/Editor/SetupDistrictProps.py` (two runs, the second with `ARENADUEL_DISTRICT_FINISH=1`) imports the models.
3. `Tools/Editor/SetupArenaDistrict.py` builds the map with them.

| Surface | Material |
| --- | --- |
| Floor | dark sealed concrete, low roughness: it reflects the lamps like a wet floor |
| Hall walls | dark concrete panels, orange diagonals, a neon strip in the team colour behind each spawn |
| Cover walls, pillars, dividers | light cast concrete with a dark steel cap along the top |
| Centre house | warm render, timber parapets, a steel roof edge |
| Low walls, crates | sawn timber; the crates in dark steel frames |
| Decks, bridge, roof | tread plate with yellow edges |
| Ceiling | dark ribbed steel deck with fifteen linear lights |

Light: fifteen cool, shadowless fills under the ceiling lights, three shadow-casting spots over the field and the house, and warm pools from 38 wall lamps, wall washes along the hall, lane fills and the neon. One unbound post process volume fixes the exposure, adds bloom and contact shading, switches motion blur, lens flares and grain off.

Everything that is only there to be looked at (lamps, caps, lines, edges, bands, stripes, ivy) is placed without collision, so it changes no cover and no trace. The six planted tubs do collide; they stand in the four corners of the hall and against the hall wall at the centre line, outside every route.

Cost, measured with `stat unit` in a two player editor session (host viewport and client window rendered together, 2025 x 867 internal resolution): 19.4 ms of GPU time before, 26.2 ms now. Bounced light (Lumen global illumination) is switched off for this map by the post process volume: with it the same picture cost 41 ms, and reducing lights, shadow casters and reflection quality moved that by a millisecond or two each. A standalone game renders one view, not two.

Checked: six spots in a play session (spawn, field, both lanes, beside the house, north of it), the lamp, ivy and crate models in place, the map and duel tests. Not checked: a played round on the new look, how well an opponent reads against every background (the cover walls are light, the hall walls and lanes are dark), the planted tubs up close, and frame times in a standalone game or at other resolutions. The reference also shows windows with daylight high in the hall walls and more plants; those are not built.
