# Maps

## L_ArenaDistrict

The default game map and editor startup map. A closed 70 x 50 m hall with a 15 m ceiling, mirrored across the centre line so both players get the same geometry. It is built from engine cubes by `Tools/Editor/SetupArenaDistrict.py`; change the layout there and run the script with the editor closed, as `Tools/Editor/SetupPhase7EAssets.ps1` does for its script.

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

Not verified: a player was not healed on the pad in a play session.

Look: near-black surfaces in slightly different tones, fifteen dim shadowless fill lights, no sky.

`ArenaDuel.Maps.ArenaDistrict` checks the two starts, that they mirror and face each other, that the weapon trace from spawn to spawn is blocked, and that the pieces are there.

Not verified: the layout was looked at from the spawn and from above in a play session, but no round was played on it. Jump gaps, the wall run, the mantle steps and the roof route are sized from the movement values, not run with real input. Sightlines other than spawn to spawn were not checked.

## Other maps

- `L_ArenaCore`: the empty hall that was the default before. Unchanged, still used by `ArenaDuelPhase7ETests`.
- `L_Phase4MovementTest`, `L_Phase3Test`, `L_Phase5GunRange`: test and development maps used by automation.
