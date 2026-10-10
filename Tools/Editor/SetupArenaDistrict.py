"""Build L_ArenaDistrict, a 1v1 map: spawn cover, a windowed centre house with a roof, a parkour lane and a cover lane.

The layout is mirrored across X = 0, so both players get the same geometry. Sizes follow the movement
component: 80 cm cover can be vaulted and crouched behind, steps of at most 140 cm can be mantled,
gaps of 350 cm are a sprint jump. Walls, decks and pillars are engine cubes, like L_ArenaCore; the
crates are the modelled crate from Tools/Editor/SetupProps.py, scaled to the size of the block they stand for.
"""
import unreal

MAP = '/Game/ArenaDuel/Maps/L_ArenaDistrict'
ASSETS = '/Game/ArenaDuel/Characters/Common'
CRATE = '/Game/ArenaDuel/Props/SM_Crate'
LIB = unreal.EditorAssetLibrary
PREFIX = 'District_'
T = 40.0  # wall thickness
count = {'pieces': 0, 'windows': 0}


def make_material(name, color, metallic, roughness, emissive=None):
    path = ASSETS + '/' + name
    if LIB.does_asset_exist(path):
        return unreal.load_asset(path)
    mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, ASSETS, unreal.Material, unreal.MaterialFactoryNew())
    edit = unreal.MaterialEditingLibrary
    base = edit.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector)
    base.set_editor_property('constant', unreal.LinearColor(*color))
    edit.connect_material_property(base, '', unreal.MaterialProperty.MP_BASE_COLOR)
    metal = edit.create_material_expression(mat, unreal.MaterialExpressionConstant)
    metal.set_editor_property('r', metallic)
    edit.connect_material_property(metal, '', unreal.MaterialProperty.MP_METALLIC)
    rough = edit.create_material_expression(mat, unreal.MaterialExpressionConstant)
    rough.set_editor_property('r', roughness)
    edit.connect_material_property(rough, '', unreal.MaterialProperty.MP_ROUGHNESS)
    if emissive:
        glow = edit.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector)
        glow.set_editor_property('constant', unreal.LinearColor(*emissive))
        edit.connect_material_property(glow, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    edit.recompile_material(mat)
    LIB.save_loaded_asset(mat)
    return mat


def cube(label, x0, x1, y0, y1, z0, z1, material, yaw=0.0):
    """Axis aligned box given by its extents, optionally turned about its centre."""
    if x1 - x0 <= 0.5 or y1 - y0 <= 0.5 or z1 - z0 <= 0.5:
        return
    if material and material.get_name() == 'M_DistrictDarkCrate' and LIB.does_asset_exist(CRATE):
        # Every crate-coloured block becomes the modelled crate, scaled to the block's exact size, so cover
        # heights, gaps and collision stay what they were. A block about twice as tall as wide is two crates.
        stack = max(1, int(round((z1 - z0) / max(x1 - x0, y1 - y0))))
        height = (z1 - z0) / stack
        for level in range(stack):
            centre = unreal.Vector((x0 + x1) * 0.5, (y0 + y1) * 0.5, z0 + height * (level + 0.5))
            actor = unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.StaticMeshActor, centre, unreal.Rotator(roll=0.0, pitch=0.0, yaw=yaw))
            actor.set_actor_label('%s%s_%03d' % (PREFIX, label, count['pieces']))
            component = actor.static_mesh_component
            component.set_static_mesh(unreal.load_asset(CRATE))
            component.set_mobility(unreal.ComponentMobility.STATIC)
            component.set_collision_profile_name('BlockAll')
            actor.set_actor_scale3d(unreal.Vector((x1 - x0) / 140.0, (y1 - y0) / 140.0, height / 140.0))
            count['pieces'] += 1
        return
    centre = unreal.Vector((x0 + x1) * 0.5, (y0 + y1) * 0.5, (z0 + z1) * 0.5)
    actor = unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.StaticMeshActor, centre, unreal.Rotator(roll=0.0, pitch=0.0, yaw=yaw))
    actor.set_actor_label('%s%s_%03d' % (PREFIX, label, count['pieces']))
    component = actor.static_mesh_component
    component.set_static_mesh(unreal.load_object(None, '/Engine/BasicShapes/Cube.Cube'))
    component.set_material(0, material)
    component.set_mobility(unreal.ComponentMobility.STATIC)
    component.set_collision_profile_name('BlockAll')
    actor.set_actor_scale3d(unreal.Vector((x1 - x0) / 100.0, (y1 - y0) / 100.0, (z1 - z0) / 100.0))
    count['pieces'] += 1


def both(label, x0, x1, y0, y1, z0, z1, material, yaw=0.0):
    """The piece and its mirror image across X = 0."""
    cube(label, x0, x1, y0, y1, z0, z1, material, yaw)
    if not (abs(x0 + x1) < 0.5 and abs(yaw) < 0.5):
        cube(label, -x1, -x0, y0, y1, z0, z1, material, -yaw)


def wall(label, axis, at, a0, a1, height, material, openings, mirror=True):
    """A wall with doors and windows. axis 'y': the wall stands at x = at and runs along y; axis 'x' the other way.

    openings: (from, to, bottom, top) along the run. A door starts at 0, a window has a sill.
    """
    place = both if mirror else cube

    def part(b0, b1, z0, z1):
        if axis == 'y':
            place(label, at - T * 0.5, at + T * 0.5, b0, b1, z0, z1, material)
        else:
            place(label, b0, b1, at - T * 0.5, at + T * 0.5, z0, z1, material)

    cursor = a0
    for (o0, o1, bottom, top) in sorted(openings):
        part(cursor, o0, 0.0, height)
        part(o0, o1, 0.0, bottom)
        part(o0, o1, top, height)
        if bottom > 0.0:
            count['windows'] += 2 if mirror and not (axis == 'x' and abs(o0 + o1) < 0.5) else 1
        cursor = o1
    part(cursor, a1, 0.0, height)


def window(a, width=220.0):
    return (a - width * 0.5, a + width * 0.5, 110.0, 235.0)


def door(a, width=200.0):
    return (a - width * 0.5, a + width * 0.5, 0.0, 245.0)


def spawn_lights():
    index = 0
    for x in (-2800, -1400, 0, 1400, 2800):
        for y in (-1700, 0, 1700):
            light = unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.PointLight, unreal.Vector(x, y, 1050), unreal.Rotator())
            light.set_actor_label('%sFillLight_%02d' % (PREFIX, index))
            component = light.get_component_by_class(unreal.PointLightComponent)
            # Shadowless movable fills, as in L_ArenaCore: cheap, and the arena stays readable everywhere.
            component.set_mobility(unreal.ComponentMobility.MOVABLE)
            component.set_cast_shadows(False)
            component.set_editor_property('intensity', 110.0)
            component.set_editor_property('attenuation_radius', 2200.0)
            # unreal.Color takes b, g, r, a by position, so name the channels. Nearly neutral, a touch cold.
            component.set_editor_property('light_color', unreal.Color(r=205, g=212, b=230, a=255))
            component.set_editor_property('use_inverse_squared_falloff', True)
            index += 1


def build():
    if LIB.does_asset_exist(MAP):
        unreal.EditorLevelLibrary.load_level(MAP)
        for actor in unreal.EditorLevelLibrary.get_all_level_actors():
            if actor.get_actor_label().startswith(PREFIX):
                unreal.EditorLevelLibrary.destroy_actor(actor)
    else:
        unreal.EditorLevelLibrary.new_level(MAP)

    floor = make_material('M_ArenaCoreFloor', (0.055, 0.075, 0.105, 1), 0.35, 0.78)
    shell = make_material('M_ArenaCoreWall', (0.018, 0.026, 0.046, 1), 0.42, 0.68)
    ceiling = make_material('M_ArenaCoreCeiling', (0.009, 0.014, 0.026, 1), 0.28, 0.8)
    # A dark palette: near-black surfaces that differ only slightly in tone, and two dim team colours.
    concrete = make_material('M_DistrictDarkConcrete', (0.030, 0.034, 0.042, 1), 0.10, 0.80)
    house = make_material('M_DistrictDarkHouse', (0.040, 0.030, 0.028, 1), 0.10, 0.75)
    crate = make_material('M_DistrictDarkCrate', (0.055, 0.034, 0.020, 1), 0.15, 0.65)
    metal = make_material('M_DistrictDarkMetal', (0.016, 0.019, 0.026, 1), 0.85, 0.35)
    cyan = make_material('M_DistrictDimCyan', (0.004, 0.03, 0.045, 1), 0.3, 0.5, (0.002, 0.02, 0.032, 1))
    violet = make_material('M_DistrictDimViolet', (0.028, 0.006, 0.045, 1), 0.3, 0.5, (0.016, 0.003, 0.032, 1))

    # Shell: 7000 x 5000 interior, floor at z = 0, ceiling at z = 1500.
    cube('Floor', -3500, 3500, -2500, 2500, -100, 0, floor)
    cube('Ceiling', -3500, 3500, -2500, 2500, 1500, 1600, ceiling)
    cube('Shell_North', -3500, 3500, 2500, 2600, 0, 1500, shell)
    cube('Shell_South', -3500, 3500, -2600, -2500, 0, 1500, shell)
    cube('Shell_West', -3600, -3500, -2500, 2500, 0, 1500, shell)
    cube('Shell_East', 3500, 3600, -2500, 2500, 0, 1500, shell)

    # --- Spawn cover: no line of sight from spawn to spawn. A window in the middle, a door on each side.
    wall('SpawnWall', 'y', -2500, -950, 950, 330, concrete, [door(-600), window(0), door(600)])
    # Team colour strips on the spawn walls, cyan for player one, violet for player two.
    cube('SpawnStrip', -2522, -2478, -950, 950, 330, 336, cyan)
    cube('SpawnStrip', 2478, 2522, -950, 950, 330, 336, violet)
    # Wing walls so the doors are peeked, not walked through in the open.
    wall('SpawnWing', 'x', 950, -2900, -2500, 330, concrete, [])
    wall('SpawnWing', 'x', -950, -2900, -2500, 330, concrete, [])

    # --- Centre house: windows towards both spawns, doors north and south, a roof with parapets.
    wall('HouseWest', 'y', -600, -450, 450, 330, house, [window(-230), window(230)])
    wall('HouseNorth', 'x', 450, -600, 600, 330, house, [window(-380), door(0), window(380)], mirror=False)
    wall('HouseSouth', 'x', -450, -600, 600, 330, house, [window(-380), door(0), window(380)], mirror=False)
    cube('HouseRoof', -620, 620, -470, 470, 330, 350, metal)
    both('RoofParapet', -620, -590, -470, -120, 350, 435, house)
    both('RoofParapet', -620, -590, 120, 470, 350, 435, house)
    cube('RoofBlock', -90, 90, -90, 90, 350, 500, crate)
    # Inside: a pillar to dance around and a crate under each west and east window.
    cube('HousePillar', -60, 60, -60, 60, 0, 330, concrete)
    both('HouseCrate', -540, -420, -300, -160, 0, 80, crate)
    # Way up to the roof at the north-west and north-east corners: 135, 270, then the roof edge at 350.
    both('RoofStep', -800, -640, 520, 680, 0, 135, crate)
    both('RoofStep', -800, -640, 340, 500, 0, 270, crate)

    # --- Field between spawn and house: pillars, head-high boxes and low walls for quick peeks.
    for (x, y) in ((-2050, -330), (-2050, 330), (-1500, 0), (-1250, -700), (-1250, 700)):
        both('Pillar', x - 55, x + 55, y - 55, y + 55, 0, 330, concrete)
    both('HeadBox', -1850, -1650, -620, -500, 0, 140, crate)
    both('HeadBox', -1850, -1650, 500, 620, 0, 140, crate)
    both('LowWall', -1020, -980, -330, -80, 0, 80, concrete)
    both('LowWall', -1020, -980, 80, 330, 0, 80, concrete)
    both('AngleWall', -1750, -1350, 930, 970, 0, 300, concrete, yaw=40.0)
    both('AngleWall', -1750, -1350, -970, -930, 0, 300, concrete, yaw=-40.0)
    # A free-standing wall with a window on each flank of the house.
    wall('PeekWall', 'y', -900, 620, 1150, 300, concrete, [window(885, 200)])
    wall('PeekWall', 'y', -900, -1150, -620, 300, concrete, [window(-885, 200)])

    # --- North lane: parkour. Mantle up, sprint jump the gaps, wall run the long wall, cross the bridge.
    wall('NorthDivider', 'x', 1280, -2450, -1950, 330, concrete, [window(-2200, 200)])
    both('ParkourStep', -2600, -2400, 1800, 2000, 0, 140, crate)
    both('ParkourDeck', -2350, -1850, 1760, 2160, 0, 280, metal)
    both('ParkourDeck', -1500, -1100, 1760, 2160, 0, 280, metal)
    both('ParkourDeck', -750, -350, 1820, 2160, 0, 300, metal)
    # The wall run wall stands beside both gaps, 700 high.
    both('WallRun', -2000, -950, 1700, 1740, 0, 700, concrete)
    # Bridge over the lane centre; walk over it or run under it.
    cube('Bridge', -350, 350, 1860, 2120, 270, 300, metal)
    both('BridgePost', -330, -290, 1860, 1900, 0, 270, metal)
    both('BridgePost', -330, -290, 2080, 2120, 0, 270, metal)
    cube('BridgeRail', -200, 200, 1860, 1880, 300, 385, metal)
    # Ground cover under the decks so the lane also works on foot.
    both('LaneBox', -1750, -1600, 2250, 2400, 0, 140, crate)
    both('LaneLow', -980, -940, 2180, 2460, 0, 80, concrete)

    # --- South lane: tight cover. A divider with two windows and a door, staggered boxes and pillars inside.
    wall('SouthDivider', 'x', -1280, -2350, -480, 330, concrete, [window(-1950, 200), door(-1420), window(-880, 200)])
    both('Crate', -2080, -1930, -1780, -1630, 0, 150, crate)
    both('CrateStack', -1580, -1430, -2200, -2050, 0, 300, crate)
    both('Crate', -1580, -1430, -2050, -1900, 0, 150, crate)
    both('LaneLow', -1020, -980, -2000, -1620, 0, 80, concrete)
    both('HeadBox', -520, -320, -1700, -1580, 0, 140, crate)
    both('Pillar', -755, -645, -2255, -2145, 0, 330, concrete)
    # Centre block of the south lane with a window slot through it.
    cube('SouthBlock', -260, 260, -2050, -2010, 0, 110, concrete)
    cube('SouthBlock', -260, 260, -2050, -2010, 235, 330, concrete)
    cube('SouthBlock', -260, -110, -2050, -2010, 110, 235, concrete)
    cube('SouthBlock', 110, 260, -2050, -2010, 110, 235, concrete)
    count['windows'] += 1

    # --- Heal pad: a small green plate in the open north of the house, on the centre line. Both peek-wall
    # windows look straight at it, so healing means standing in a crossfire.
    green = make_material('M_DistrictHealGreen', (0.02, 0.45, 0.10, 1), 0.0, 0.5, (0.03, 0.85, 0.16, 1))
    pad_class = unreal.load_class(None, '/Script/ArenaDuel.ArenaDuelHealPad')
    pad = unreal.EditorLevelLibrary.spawn_actor_from_class(pad_class, unreal.Vector(0, 900, 2), unreal.Rotator(roll=0.0, pitch=0.0, yaw=0.0))
    pad.set_actor_label(PREFIX + 'HealPad')
    pad.get_component_by_class(unreal.StaticMeshComponent).set_material(0, green)

    # The game mode looks for these two tags.
    for x, yaw, name in ((-3050, 0, 'P1'), (3050, 180, 'P2')):
        start = unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(x, 0, 100), unreal.Rotator(roll=0.0, pitch=0.0, yaw=yaw))
        start.set_actor_label(PREFIX + 'PlayerStart_' + name)
        start.set_editor_property('player_start_tag', 'ArenaCore_' + name)
    spawn_lights()

    world = unreal.EditorLevelLibrary.get_editor_world()
    if not unreal.EditorLoadingAndSavingUtils.save_map(world, MAP):
        raise RuntimeError('Failed to save ' + MAP)
    starts = [a for a in unreal.EditorLevelLibrary.get_all_level_actors() if isinstance(a, unreal.PlayerStart)]
    assert len(starts) == 2, 'Expected exactly two PlayerStarts'
    unreal.log('ARENA DISTRICT PASS pieces=%d windows=%d' % (count['pieces'], count['windows']))


build()
