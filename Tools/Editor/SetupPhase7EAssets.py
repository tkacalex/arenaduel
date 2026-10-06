"""Build the minimal closed Phase 7E arena with Unreal Editor APIs."""
import unreal

MAP = '/Game/ArenaDuel/Maps/L_ArenaCore'
ASSETS = '/Game/ArenaDuel/Characters/Common'
LIB = unreal.EditorAssetLibrary

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

def spawn_cube(label, location, size, material):
    actor = unreal.EditorLevelLibrary.spawn_actor_from_class(
        unreal.StaticMeshActor, unreal.Vector(*location), unreal.Rotator(0, 0, 0))
    actor.set_actor_label(label)
    component = actor.static_mesh_component
    component.set_static_mesh(unreal.load_object(None, '/Engine/BasicShapes/Cube.Cube'))
    component.set_material(0, material)
    component.set_mobility(unreal.ComponentMobility.STATIC)
    component.set_collision_profile_name('BlockAll')
    actor.set_actor_scale3d(unreal.Vector(size[0] / 100.0, size[1] / 100.0, size[2] / 100.0))
    return actor

def spawn_lights():
    for index, (x, y) in enumerate([(-2200, -1500), (-2200, 1500), (2200, -1500), (2200, 1500), (0, 0)]):
        light = unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.PointLight, unreal.Vector(x, y, 1300), unreal.Rotator())
        light.set_actor_label('ArenaCore_FillLight_%d' % (index + 1))
        component = light.get_component_by_class(unreal.PointLightComponent)
        # Unbaked static lights do not light PIE actors. Use a handful of
        # shadowless movable fills so the development arena is readable.
        component.set_mobility(unreal.ComponentMobility.MOVABLE)
        component.set_cast_shadows(False)
        component.set_editor_property('intensity', 9000.0 if index < 4 else 7000.0)
        component.set_editor_property('attenuation_radius', 3200.0 if index < 4 else 4200.0)
        component.set_editor_property('light_color', unreal.Color(190, 205, 255, 255))
        component.set_editor_property('use_inverse_squared_falloff', True)

def build():
    if LIB.does_asset_exist(MAP):
        unreal.EditorLevelLibrary.load_level(MAP)
        for actor in unreal.EditorLevelLibrary.get_all_level_actors():
            if actor.get_actor_label().startswith('ArenaCore_'):
                unreal.EditorLevelLibrary.destroy_actor(actor)
    else:
        unreal.EditorLevelLibrary.new_level(MAP)

    floor = make_material('M_ArenaCoreFloor', (0.055, 0.075, 0.105, 1), 0.35, 0.78)
    wall = make_material('M_ArenaCoreWall', (0.018, 0.026, 0.046, 1), 0.42, 0.68)
    ceiling = make_material('M_ArenaCoreCeiling', (0.009, 0.014, 0.026, 1), 0.28, 0.8)
    accent = make_material('M_ArenaCoreAccent', (0.10, 0.42, 0.58, 1), 0.5, 0.5, (0.015, 0.09, 0.14, 1))

    # 7000 x 5000 uu interior, floor at z=0, ceiling at z=1800.
    spawn_cube('ArenaCore_Floor', (0, 0, -50), (7000, 5000, 100), floor)
    spawn_cube('ArenaCore_Ceiling', (0, 0, 1850), (7000, 5000, 100), ceiling)
    spawn_cube('ArenaCore_Wall_North', (0, 2450, 900), (7000, 100, 1800), wall)
    spawn_cube('ArenaCore_Wall_South', (0, -2450, 900), (7000, 100, 1800), wall)
    spawn_cube('ArenaCore_Wall_West', (-3450, 0, 900), (100, 5000, 1800), wall)
    spawn_cube('ArenaCore_Wall_East', (3450, 0, 900), (100, 5000, 1800), wall)
    # Subtle center cross, only a surface mark.
    spawn_cube('ArenaCore_CenterMark_X', (0, 0, 2), (300, 8, 3), accent)
    spawn_cube('ArenaCore_CenterMark_Y', (0, 0, 2), (8, 300, 3), accent)
    for x, yaw, name in [(-2400, 0, 'P1'), (2400, 180, 'P2')]:
        start = unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(x, 0, 100), unreal.Rotator(0, yaw, 0))
        start.set_actor_label('ArenaCore_PlayerStart_' + name)
    spawn_lights()
    world = unreal.EditorLevelLibrary.get_editor_world()
    if not unreal.EditorLoadingAndSavingUtils.save_map(world, MAP):
        raise RuntimeError('Failed to save ' + MAP)
    starts = [a for a in unreal.EditorLevelLibrary.get_all_level_actors() if isinstance(a, unreal.PlayerStart)]
    shell = [a for a in unreal.EditorLevelLibrary.get_all_level_actors() if a.get_actor_label().startswith('ArenaCore_Wall_')]
    assert len(starts) == 2, 'Expected exactly two ArenaCore PlayerStarts'
    assert len(shell) == 4, 'Expected four solid ArenaCore walls'
    print('PHASE7E ARENA PASS', MAP, 'starts=2 walls=4 shell=7000x5000x1800')

build()
