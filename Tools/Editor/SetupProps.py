"""Import the props built by Tools/Art/build_props.py.

Two editor runs, both with the editor closed: as it is, the import; with ARENADUEL_PROP_FINISH=1,
materials and box collision, then the save (the importer finishes a mesh after the script, so both
cannot happen in one run). Result: /Game/ArenaDuel/Props/SM_Crate, SM_Container, SM_AmmoBox, SM_HealPad.
"""
import os
import unreal

DEST = '/Game/ArenaDuel/Props'
COMMON = '/Game/ArenaDuel/Characters/Common/'
ART = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'Art', 'Props'))
LIB = unreal.EditorAssetLibrary
# Name, material of slot 0, material of slot 1, whether it blocks.
PROPS = (
    ('Crate', 'M_DistrictDarkCrate', 'M_DistrictDarkMetal', True),
    ('Container', 'M_SurvivalContainer', 'M_DistrictDarkMetal', True),
    ('AmmoBox', 'M_DistrictDarkMetal', 'M_SurvivalAmmo', False),
    ('HealPad', 'M_DistrictHealGreen', 'M_DistrictDarkMetal', False),
)
FINISH = os.environ.get('ARENADUEL_PROP_FINISH') == '1'
ONLY = [name for name in os.environ.get('ARENADUEL_PROPS', '').split(',') if name]
done = []


def panel_material():
    """The frosted panel of the heal pad: dark milky glass, a little glossy, lit faintly green from the ring, so
    the green cross on it reads clearly. Created once, its values set on every run."""
    path = COMMON + 'M_HealPadPanel'
    edit = unreal.MaterialEditingLibrary
    mat = unreal.load_asset(path) if LIB.does_asset_exist(path) else unreal.AssetToolsHelpers.get_asset_tools().create_asset('M_HealPadPanel', COMMON[:-1], unreal.Material, unreal.MaterialFactoryNew())
    colours = ((unreal.MaterialProperty.MP_BASE_COLOR, (0.13, 0.16, 0.15)), (unreal.MaterialProperty.MP_EMISSIVE_COLOR, (0.004, 0.035, 0.012)))
    scalars = ((unreal.MaterialProperty.MP_ROUGHNESS, 0.22), (unreal.MaterialProperty.MP_METALLIC, 0.0), (unreal.MaterialProperty.MP_SPECULAR, 0.6))
    for prop, value in colours:
        node = edit.get_material_property_input_node(mat, prop)
        if not node:
            node = edit.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector)
            edit.connect_material_property(node, '', prop)
        node.set_editor_property('constant', unreal.LinearColor(value[0], value[1], value[2], 1.0))
    for prop, value in scalars:
        node = edit.get_material_property_input_node(mat, prop)
        if not node:
            node = edit.create_material_expression(mat, unreal.MaterialExpressionConstant)
            edit.connect_material_property(node, '', prop)
        node.set_editor_property('r', value)
    edit.recompile_material(mat)
    LIB.save_loaded_asset(mat)
    return mat


for name, main, trim, blocks in PROPS:
    if ONLY and name not in ONLY:
        continue
    asset_path = DEST + '/SM_' + name
    if not FINISH:
        source = os.path.join(ART, 'SM_%s.fbx' % name)
        if not os.path.isfile(source):
            continue
        task = unreal.AssetImportTask()
        task.set_editor_property('filename', source)
        task.set_editor_property('destination_path', DEST)
        task.set_editor_property('destination_name', 'SM_' + name)
        task.set_editor_property('automated', True)
        task.set_editor_property('save', True)
        task.set_editor_property('replace_existing', True)
        ui = unreal.FbxImportUI()
        ui.set_editor_property('import_mesh', True)
        ui.set_editor_property('import_as_skeletal', False)
        ui.set_editor_property('import_materials', False)
        ui.set_editor_property('import_textures', False)
        ui.set_editor_property('mesh_type_to_import', unreal.FBXImportType.FBXIT_STATIC_MESH)
        data = ui.get_editor_property('static_mesh_import_data')
        data.set_editor_property('combine_meshes', True)
        data.set_editor_property('auto_generate_collision', False)
        data.set_editor_property('generate_lightmap_u_vs', False)
        data.set_editor_property('normal_import_method', unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
        task.set_editor_property('options', ui)
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
        if LIB.does_asset_exist(asset_path):
            done.append(name)
        continue
    mesh = unreal.load_asset(asset_path)
    if not mesh:
        continue
    for index, material in enumerate((main, trim)):
        asset = unreal.load_asset(COMMON + material)
        if asset and index < len(mesh.get_editor_property('static_materials')):
            mesh.set_material(index, asset)
    if name == 'HealPad' and len(mesh.get_editor_property('static_materials')) > 2:
        # Brushed steel for plinth and rim where the district materials are there, and the frosted panel.
        steel = '/Game/ArenaDuel/Maps/District/MI_District_SteelDark'
        if LIB.does_asset_exist(steel):
            mesh.set_material(1, unreal.load_asset(steel))
        mesh.set_material(2, panel_material())
    meshes = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    if blocks and meshes.get_simple_collision_count(mesh) == 0:
        # One box of the prop's own size: the same obstacle as the block it replaces.
        meshes.add_simple_collisions(mesh, unreal.ScriptCollisionShapeType.BOX)
    saved = LIB.save_loaded_asset(mesh, only_if_is_dirty=False)
    bounds = mesh.get_bounds()
    done.append('%s saved=%s collision=%d size=(%.0f, %.0f, %.0f)' % (name, saved, meshes.get_simple_collision_count(mesh), bounds.box_extent.x * 2, bounds.box_extent.y * 2, bounds.box_extent.z * 2))
unreal.log('PROPS %s %s' % ('FINISH' if FINISH else 'IMPORT', done))
