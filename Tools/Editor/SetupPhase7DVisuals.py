"""Run with UnrealEditor-Cmd -EnablePlugins=PythonScriptPlugin,EditorScriptingUtilities
-ExecutePythonScript=<this file>. InstallPhase7DTemplateAssets.ps1 runs first.
All generated packages are serialized by Unreal; no binary asset fabrication.
"""
import unreal

ROOT = '/Game/ArenaDuel'
assets = unreal.AssetToolsHelpers.get_asset_tools()
library = unreal.EditorAssetLibrary

def material(name, color, metallic=0.5, emission=0.0):
    path = ROOT + '/Characters/Common/' + name
    if library.does_asset_exist(path):
        existing = unreal.load_asset(path)
        if not existing.get_editor_property('used_with_skeletal_mesh'):
            existing.set_editor_property('used_with_skeletal_mesh', True)
            unreal.MaterialEditingLibrary.recompile_material(existing)
            library.save_loaded_asset(existing)
        return existing
    result = assets.create_asset(name, ROOT + '/Characters/Common', unreal.Material, unreal.MaterialFactoryNew())
    result.set_editor_property('used_with_skeletal_mesh', True)
    edit = unreal.MaterialEditingLibrary
    c = edit.create_material_expression(result, unreal.MaterialExpressionConstant3Vector)
    c.set_editor_property('constant', unreal.LinearColor(*color))
    edit.connect_material_property(c, '', unreal.MaterialProperty.MP_BASE_COLOR)
    m = edit.create_material_expression(result, unreal.MaterialExpressionConstant)
    m.set_editor_property('r', metallic)
    edit.connect_material_property(m, '', unreal.MaterialProperty.MP_METALLIC)
    rough = edit.create_material_expression(result, unreal.MaterialExpressionConstant)
    rough.set_editor_property('r', 0.42)
    edit.connect_material_property(rough, '', unreal.MaterialProperty.MP_ROUGHNESS)
    if emission:
        e = edit.create_material_expression(result, unreal.MaterialExpressionConstant3Vector)
        e.set_editor_property('constant', unreal.LinearColor(color[0]*emission, color[1]*emission, color[2]*emission, 1))
        edit.connect_material_property(e, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    edit.recompile_material(result)
    library.save_loaded_asset(result)
    return result

dark = material('M_ArcaneMetal', (0.035, 0.045, 0.065, 1))
cyan = material('M_ArcaneCyan', (0.10, 0.60, 0.85, 1), 0.6, 0.5)
violet = material('M_ArcaneViolet', (0.38, 0.07, 0.75, 1), 0.6, 0.5)
for name, color in [('Shadow',(0.028,0.018,0.05,1)), ('Warden',(0.075,0.11,0.15,1)), ('Rift',(0.055,0.026,0.09,1))]:
    material('M_'+name+'Armor', color)

# Original multi-part firearm geometry. Grip is the origin, forward is +X.
# Pivot at grip permits attachment to hand_r; muzzle is a saved mesh socket.
def weapon(name, length, width, stock, accent, optic=False, shotgun=False):
    path = ROOT + '/Weapons/' + name
    mesh_path = path + '/SM_' + name
    if library.does_asset_exist(mesh_path):
        existing = unreal.load_asset(mesh_path)
        assert existing.find_socket('Muzzle'), mesh_path + ': missing muzzle'
        return existing
    actors = []
    def part(shape, location, dimensions, mat=dark, pitch=0, yaw=0):
        actor = unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(*location), unreal.Rotator(pitch=pitch, yaw=yaw))
        actor.static_mesh_component.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/' + shape))
        actor.static_mesh_component.set_material(0, mat)
        actor.set_actor_scale3d(unreal.Vector(*(d / 100 for d in dimensions)))
        actors.append(actor)
    part('Cube', (0,0,0), (6,7,16), pitch=-12) # grip / exact origin
    part('Cube', (8,0,12), (28,width,12)) # receiver
    part('Cube', (24,0,13), (18,width*0.9,10)) # handguard
    part('Cylinder', (length*0.70,0,13), (4 if not shotgun else 7,4 if not shotgun else 7,length*0.6), pitch=90)
    part('Cylinder', (length,0,13), (6 if not shotgun else 9,6 if not shotgun else 9,5), pitch=90)
    part('Cube', (-stock/2-10,0,9), (stock,6,8)) # stock
    part('Cube', (-stock-9,0,7), (4,9,19)) # butt
    part('Cube', (9,0,-4), (8,6,22 if not shotgun else 12), pitch=10)
    part('Cube', (15,0,20), (28,4,3)) # rail
    part('Cube', (24,0,19), (16,3,2), accent) # top arcane strip
    for side in [-1,1]:
        part('Cube', (11,side*(width/2+0.2),13), (12,0.8,2), accent)
    if optic:
        part('Cylinder',(13,0,25),(8,8,20),dark,pitch=90)
        part('Cylinder',(24,0,25),(7,7,1),accent,pitch=90)
    else:
        part('Cube',(-2,0,24),(3,8,5))
        part('Cube',(31,0,24),(2,2,5),accent)
    if shotgun:
        part('Cylinder',(30,0,5),(5,5,43),pitch=90)
        part('Cube',(27,0,8),(14,13,8))
    settings = unreal.MeshMergingSettings()
    settings.set_editor_property('pivot_type', unreal.MeshMergePivotType.WORLD_ORIGIN)
    settings.set_editor_property('generate_light_map_uv', False)
    options = unreal.MergeStaticMeshActorsOptions()
    options.set_editor_property('base_package_name', path + '/' + name)
    options.set_editor_property('mesh_merging_settings', settings)
    options.set_editor_property('destroy_source_actors', True)
    options.set_editor_property('spawn_merged_actor', False)
    unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem).merge_static_mesh_actors(actors, options)
    result = unreal.load_asset(mesh_path)
    assert result, mesh_path
    socket = unreal.StaticMeshSocket(outer=result)
    socket.set_editor_property('socket_name', 'Muzzle')
    socket.set_editor_property('relative_location', unreal.Vector(length+3,0,13))
    result.add_socket(socket)
    library.save_loaded_asset(result)
    return result

unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
for args in [('ArcRifle',70,9,22,cyan,False,False), ('ShadeSMG',42,10,12,violet,False,False), ('RuneDMR',90,7,26,violet,True,False), ('HexShotgun',60,14,24,cyan,False,True)]:
    weapon(*args)

registry = unreal.AssetRegistryHelpers.get_asset_registry()
registry.scan_paths_synchronous(['/Game/Characters', ROOT+'/Weapons', ROOT+'/Characters/Common'], True)
for data in registry.get_assets_by_path('/Game/Characters', recursive=True):
    asset = data.get_asset()
    assert asset, str(data.package_name)
    print('PHASE7D LOAD', data.package_name, asset.get_class().get_name())
    if isinstance(asset, unreal.AnimSequence):
        assert asset.get_editor_property('additive_anim_type') == unreal.AdditiveAnimationType.AAT_NONE, str(data.package_name)
for folder in [ROOT+'/Weapons', ROOT+'/Characters/Common']:
    for data in registry.get_assets_by_path(folder, recursive=True):
        assert data.get_asset(), str(data.package_name)
print('PHASE7D VISUAL ASSETS PASS')
