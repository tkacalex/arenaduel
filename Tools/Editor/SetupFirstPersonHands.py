"""Import the first person hands and build their materials.

The mesh comes from Tools/Art/SKM_ArenaDuelFPHands.fbx: a MakeHuman (MPFB, CC0) human whose arms were
posed onto the mannequin's joints in Blender, cut off at the shoulder, given a glove, a cuff and a
sleeve, and bound to the mannequin skeleton. It shares SK_Mannequin, so every clip and socket fits.
"""
import os
import unreal

DEST = '/Game/ArenaDuel/Characters/Common'
NAME = 'SKM_ArenaDuelFPHands'
LIB = unreal.EditorAssetLibrary
EDIT = unreal.MaterialEditingLibrary
source = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'Art', NAME + '.fbx')


def material(name, color, roughness, metallic, sheen):
    path = DEST + '/' + name
    if LIB.does_asset_exist(path):
        LIB.delete_asset(path)
    mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, DEST, unreal.Material, unreal.MaterialFactoryNew())
    base = EDIT.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector, -600, 0)
    base.set_editor_property('constant', unreal.LinearColor(*color))
    # A soft rim brightens grazing angles, which is what makes cloth and rubber read as cloth and rubber.
    fresnel = EDIT.create_material_expression(mat, unreal.MaterialExpressionFresnel, -600, 200)
    fresnel.set_editor_property('exponent', 3.5)
    fresnel.set_editor_property('base_reflect_fraction', 0.0)
    rim = EDIT.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector, -600, 400)
    rim.set_editor_property('constant', unreal.LinearColor(sheen, sheen, sheen * 1.05, 1.0))
    mul = EDIT.create_material_expression(mat, unreal.MaterialExpressionMultiply, -350, 300)
    EDIT.connect_material_expressions(fresnel, '', mul, 'A')
    EDIT.connect_material_expressions(rim, '', mul, 'B')
    add = EDIT.create_material_expression(mat, unreal.MaterialExpressionAdd, -150, 100)
    EDIT.connect_material_expressions(base, '', add, 'A')
    EDIT.connect_material_expressions(mul, '', add, 'B')
    EDIT.connect_material_property(add, '', unreal.MaterialProperty.MP_BASE_COLOR)
    rough = EDIT.create_material_expression(mat, unreal.MaterialExpressionConstant, -350, 500)
    rough.set_editor_property('r', roughness)
    EDIT.connect_material_property(rough, '', unreal.MaterialProperty.MP_ROUGHNESS)
    metal = EDIT.create_material_expression(mat, unreal.MaterialExpressionConstant, -350, 600)
    metal.set_editor_property('r', metallic)
    EDIT.connect_material_property(metal, '', unreal.MaterialProperty.MP_METALLIC)
    mat.set_editor_property('used_with_skeletal_mesh', True)
    EDIT.recompile_material(mat)
    LIB.save_loaded_asset(mat)
    return mat


task = unreal.AssetImportTask()
task.set_editor_property('filename', os.path.normpath(source))
task.set_editor_property('destination_path', DEST)
task.set_editor_property('destination_name', NAME)
task.set_editor_property('automated', True)
task.set_editor_property('save', True)
task.set_editor_property('replace_existing', True)
ui = unreal.FbxImportUI()
ui.set_editor_property('import_mesh', True)
ui.set_editor_property('import_as_skeletal', True)
ui.set_editor_property('import_animations', False)
ui.set_editor_property('import_materials', False)
ui.set_editor_property('import_textures', False)
ui.set_editor_property('create_physics_asset', False)
ui.set_editor_property('mesh_type_to_import', unreal.FBXImportType.FBXIT_SKELETAL_MESH)
ui.set_editor_property('skeleton', unreal.load_asset('/Game/Characters/Mannequins/Meshes/SK_Mannequin'))
data = ui.get_editor_property('skeletal_mesh_import_data')
data.set_editor_property('import_morph_targets', False)
data.set_editor_property('use_t0_as_ref_pose', False)
data.set_editor_property('preserve_smoothing_groups', True)
data.set_editor_property('normal_import_method', unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
task.set_editor_property('options', ui)
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
mesh = unreal.load_asset(DEST + '/' + NAME)
if not mesh:
    raise RuntimeError('The hands were not imported')

def texture(name, srgb, normal):
    """Import Tools/Art/<name>.png. Blender bakes normals with green up, Unreal expects green down."""
    path = os.path.join(os.path.dirname(os.path.normpath(source)), name + '.png')
    if not os.path.isfile(path):
        return None
    load = unreal.AssetImportTask()
    load.set_editor_property('filename', path)
    load.set_editor_property('destination_path', DEST)
    load.set_editor_property('destination_name', name)
    load.set_editor_property('automated', True)
    load.set_editor_property('save', True)
    load.set_editor_property('replace_existing', True)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([load])
    asset = unreal.load_asset(DEST + '/' + name)
    if asset:
        asset.set_editor_property('srgb', srgb)
        if normal:
            asset.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_NORMALMAP)
            asset.set_editor_property('flip_green_channel', True)
        LIB.save_loaded_asset(asset)
    return asset


def textured(name, maps):
    """Baked colour, normal and roughness (Tools/Art/bake_zombie_textures.py with TARGET = 'FPHands')."""
    path = DEST + '/' + name
    if LIB.does_asset_exist(path):
        LIB.delete_asset(path)
    mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, DEST, unreal.Material, unreal.MaterialFactoryNew())
    color = EDIT.create_material_expression(mat, unreal.MaterialExpressionTextureSample, -900, 0)
    color.set_editor_property('texture', maps[0])
    EDIT.connect_material_property(color, 'RGB', unreal.MaterialProperty.MP_BASE_COLOR)
    normal = EDIT.create_material_expression(mat, unreal.MaterialExpressionTextureSample, -900, 300)
    normal.set_editor_property('texture', maps[1])
    normal.set_editor_property('sampler_type', unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    EDIT.connect_material_property(normal, 'RGB', unreal.MaterialProperty.MP_NORMAL)
    rough = EDIT.create_material_expression(mat, unreal.MaterialExpressionTextureSample, -900, 600)
    rough.set_editor_property('texture', maps[2])
    rough.set_editor_property('sampler_type', unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
    EDIT.connect_material_property(rough, 'R', unreal.MaterialProperty.MP_ROUGHNESS)
    mat.set_editor_property('used_with_skeletal_mesh', True)
    EDIT.recompile_material(mat)
    LIB.save_loaded_asset(mat)
    return mat


MAPS = [texture('T_FPHands_BaseColor', True, False), texture('T_FPHands_Normal', False, True), texture('T_FPHands_Roughness', False, False)]
if all(MAPS):
    slots = {'FP_Sleeve': textured('M_FPSleeve', MAPS), 'FP_Glove': textured('M_FPGlove', MAPS), 'FP_Trim': textured('M_FPTrim', MAPS)}
else:
    slots = {
        'FP_Sleeve': material('M_FPSleeve', (0.030, 0.038, 0.034, 1), 0.88, 0.0, 0.02),
        'FP_Glove': material('M_FPGlove', (0.010, 0.010, 0.012, 1), 0.58, 0.0, 0.015),
        'FP_Trim': material('M_FPTrim', (0.045, 0.047, 0.052, 1), 0.36, 0.15, 0.03),
    }
materials = mesh.get_editor_property('materials')
names = []
# Array elements come out as copies: each changed entry has to be written back by index.
for index in range(len(materials)):
    entry = materials[index]
    slot = str(entry.get_editor_property('material_slot_name'))
    for key, mat in slots.items():
        if key in slot:
            entry.set_editor_property('material_interface', mat)
    materials[index] = entry
mesh.set_editor_property('materials', materials)
LIB.save_loaded_asset(mesh)
for entry in mesh.get_editor_property('materials'):
    assigned = entry.get_editor_property('material_interface')
    names.append('%s=%s' % (entry.get_editor_property('material_slot_name'), assigned.get_name() if assigned else None))
skeleton = mesh.get_editor_property('skeleton')
unreal.log('FP HANDS PASS skeleton=%s slots=%s' % (skeleton.get_name() if skeleton else None, names))
