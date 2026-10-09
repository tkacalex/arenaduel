"""Build L_ZombieArena, the Zombie Survival map, the zombie materials and L_MainMenu, the menu map.

The arena is a closed hall with a ring of walls around the middle. The player starts in the middle;
zombies spawn at marked points behind the ring, out of sight, and come in through its gaps. A navigation
bounds volume covers the floor; the navigation mesh itself is generated when the map starts.
"""
import math
import unreal

ARENA = '/Game/ArenaDuel/Maps/L_ZombieArena'
MENU = '/Game/ArenaDuel/Maps/L_MainMenu'
ASSETS = '/Game/ArenaDuel/Characters/Common'
LIB = unreal.EditorAssetLibrary
PREFIX = 'Survival_'
count = {'pieces': 0, 'spawns': 0}


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


def cube(label, x0, x1, y0, y1, z0, z1, material):
    centre = unreal.Vector((x0 + x1) * 0.5, (y0 + y1) * 0.5, (z0 + z1) * 0.5)
    actor = unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.StaticMeshActor, centre, unreal.Rotator(roll=0.0, pitch=0.0, yaw=0.0))
    actor.set_actor_label('%s%s_%03d' % (PREFIX, label, count['pieces']))
    component = actor.static_mesh_component
    component.set_static_mesh(unreal.load_object(None, '/Engine/BasicShapes/Cube.Cube'))
    component.set_material(0, material)
    component.set_mobility(unreal.ComponentMobility.STATIC)
    component.set_collision_profile_name('BlockAll')
    actor.set_actor_scale3d(unreal.Vector((x1 - x0) / 100.0, (y1 - y0) / 100.0, (z1 - z0) / 100.0))
    count['pieces'] += 1


def four(label, x0, x1, y0, y1, z0, z1, material):
    """The piece in all four quadrants."""
    for sx in (1, -1):
        for sy in (1, -1):
            ax0, ax1 = sorted((sx * x0, sx * x1))
            ay0, ay1 = sorted((sy * y0, sy * y1))
            cube(label, ax0, ax1, ay0, ay1, z0, z1, material)


def spawn_point(x, y):
    point = unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.TargetPoint, unreal.Vector(x, y, 20.0), unreal.Rotator(roll=0.0, pitch=0.0, yaw=0.0))
    point.set_actor_label('%sZombieSpawn_%02d' % (PREFIX, count['spawns']))
    point.set_editor_property('tags', [unreal.Name('ZombieSpawn')])
    count['spawns'] += 1


def clear(prefix):
    for actor in unreal.EditorLevelLibrary.get_all_level_actors():
        if actor.get_actor_label().startswith(prefix):
            unreal.EditorLevelLibrary.destroy_actor(actor)


def set_game_mode(class_path):
    world = unreal.EditorLevelLibrary.get_editor_world()
    world.get_world_settings().set_editor_property('default_game_mode', unreal.load_class(None, class_path))
    return world


def build_zombie_materials():
    materials = [
        make_material('M_SurvivalZombie', (0.05, 0.16, 0.045, 1), 0.0, 0.7, (0.003, 0.016, 0.003, 1)),
        make_material('M_SurvivalRunner', (0.26, 0.22, 0.04, 1), 0.0, 0.6, (0.02, 0.016, 0.002, 1)),
        make_material('M_SurvivalArmoured', (0.16, 0.18, 0.21, 1), 0.9, 0.35, (0.001, 0.005, 0.008, 1)),
        make_material('M_SurvivalBrute', (0.34, 0.12, 0.03, 1), 0.1, 0.5, (0.05, 0.014, 0.002, 1)),
        make_material('M_SurvivalAbomination', (0.32, 0.03, 0.03, 1), 0.1, 0.45, (0.09, 0.005, 0.004, 1)),
    ]
    # They are drawn on a skinned mesh. Without this flag the engine falls back to the default material in game.
    for material in materials:
        if not material.get_editor_property('used_with_skeletal_mesh'):
            material.set_editor_property('used_with_skeletal_mesh', True)
            unreal.MaterialEditingLibrary.recompile_material(material)
            LIB.save_loaded_asset(material)


def build_arena():
    if LIB.does_asset_exist(ARENA):
        unreal.EditorLevelLibrary.load_level(ARENA)
        clear(PREFIX)
    else:
        unreal.EditorLevelLibrary.new_level(ARENA)

    floor = make_material('M_ArenaCoreFloor', (0.055, 0.075, 0.105, 1), 0.35, 0.78)
    shell = make_material('M_ArenaCoreWall', (0.018, 0.026, 0.046, 1), 0.42, 0.68)
    ceiling = make_material('M_ArenaCoreCeiling', (0.009, 0.014, 0.026, 1), 0.28, 0.8)
    concrete = make_material('M_DistrictDarkConcrete', (0.030, 0.034, 0.042, 1), 0.10, 0.80)
    crate = make_material('M_DistrictDarkCrate', (0.055, 0.034, 0.020, 1), 0.15, 0.65)
    metal = make_material('M_DistrictDarkMetal', (0.016, 0.019, 0.026, 1), 0.85, 0.35)

    # Shell: 6400 x 6400, floor at z = 0, ceiling at 1400.
    cube('Floor', -3200, 3200, -3200, 3200, -100, 0, floor)
    cube('Ceiling', -3200, 3200, -3200, 3200, 1400, 1500, ceiling)
    cube('Shell', -3200, 3200, 3200, 3300, 0, 1400, shell)
    cube('Shell', -3200, 3200, -3300, -3200, 0, 1400, shell)
    cube('Shell', -3300, -3200, -3200, 3200, 0, 1400, shell)
    cube('Shell', 3200, 3300, -3200, 3200, 0, 1400, shell)

    # Ring of walls 2200 from the middle. It hides the spawn points; zombies come through the gaps
    # at the four sides and at the four corners.
    four('Ring', 500, 1700, 2180, 2220, 0, 320, concrete)
    four('Ring', 2180, 2220, 500, 1700, 0, 320, concrete)

    # Cover around the middle: four L shaped walls, head-high crates and low walls to fight around.
    four('Cover', 700, 1200, 680, 720, 0, 300, concrete)
    four('Cover', 1160, 1200, 300, 720, 0, 300, concrete)
    four('Crate', 250, 400, 1250, 1400, 0, 140, crate)
    four('Crate', 1250, 1400, 250, 400, 0, 140, crate)
    four('LowWall', 1500, 1540, 900, 1300, 0, 80, concrete)
    four('Pillar', 1640, 1760, 1640, 1760, 0, 320, metal)
    cube('Centre', -70, 70, -70, 70, 0, 110, metal)

    # Spawn points behind the ring: three per side and one in every corner.
    for side in (1, -1):
        for offset in (-1100, 0, 1100):
            spawn_point(offset, side * 2750)
            spawn_point(side * 2750, offset)
    for sx in (1, -1):
        for sy in (1, -1):
            spawn_point(sx * 2700, sy * 2700)

    # The game mode looks for these two tags. The second start is there for a later co-op run.
    for x, name in ((-160, 'P1'), (160, 'P2')):
        start = unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(x, -260, 100), unreal.Rotator(roll=0.0, pitch=0.0, yaw=90.0))
        start.set_actor_label(PREFIX + 'PlayerStart_' + name)
        start.set_editor_property('player_start_tag', 'ArenaCore_' + name)

    index = 0
    for x in (-2300, -770, 770, 2300):
        for y in (-2300, -770, 770, 2300):
            light = unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.PointLight, unreal.Vector(x, y, 1000), unreal.Rotator())
            light.set_actor_label('%sFillLight_%02d' % (PREFIX, index))
            component = light.get_component_by_class(unreal.PointLightComponent)
            component.set_mobility(unreal.ComponentMobility.MOVABLE)
            component.set_cast_shadows(False)
            component.set_editor_property('intensity', 150.0)
            component.set_editor_property('attenuation_radius', 2200.0)
            component.set_editor_property('light_color', unreal.Color(r=205, g=212, b=230, a=255))
            component.set_editor_property('use_inverse_squared_falloff', True)
            index += 1

    # Navigation: one bounds volume over the whole floor. The mesh is generated at runtime (DefaultEngine.ini).
    volume = unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.NavMeshBoundsVolume, unreal.Vector(0, 0, 300), unreal.Rotator())
    volume.set_actor_label(PREFIX + 'NavBounds')
    volume.set_actor_scale3d(unreal.Vector(33.0, 33.0, 5.0))

    world = set_game_mode('/Script/ArenaDuel.ArenaDuelZombieGameMode')
    if not unreal.EditorLoadingAndSavingUtils.save_map(world, ARENA):
        raise RuntimeError('Failed to save ' + ARENA)
    unreal.log('SURVIVAL ARENA PASS pieces=%d spawns=%d' % (count['pieces'], count['spawns']))


def build_menu():
    if LIB.does_asset_exist(MENU):
        unreal.EditorLevelLibrary.load_level(MENU)
    else:
        unreal.EditorLevelLibrary.new_level(MENU)
    world = set_game_mode('/Script/ArenaDuel.ArenaDuelMenuGameMode')
    if not unreal.EditorLoadingAndSavingUtils.save_map(world, MENU):
        raise RuntimeError('Failed to save ' + MENU)
    unreal.log('MAIN MENU MAP PASS')


build_zombie_materials()
build_arena()
build_menu()
