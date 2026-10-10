"""Import the firearms built by Tools/Art/build_weapons.py: body, moving parts, textures and material.

Two editor runs, both with the editor closed:

1. as it is: imports Tools/Art/Weapons/SM_<Name>.fbx, SM_<Name>_Bolt.fbx and SM_<Name>_Mag.fbx to
   /Game/ArenaDuel/Weapons/<Name>, the three baked textures next to them, and builds M_<Name> from them;
2. with the environment variable ARENADUEL_WEAPON_FINISH=1: adds the Muzzle socket to the body, sets
   the materials on every mesh and saves. This cannot be part of the first run, the importer finishes
   a mesh after the script and would drop the changes.

The meshes keep the game's conventions (origin at the grip, +X to the muzzle, barrel at a height of 13,
slot 0 body, slot 1 accent). The parts share the body's origin; the game finds them by their names.
"""
import os
import unreal

ROOT = '/Game/ArenaDuel/Weapons'
ART = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'Art', 'Weapons'))
LIB = unreal.EditorAssetLibrary
EDIT = unreal.MaterialEditingLibrary
# Name and barrel length; the muzzle lies three centimetres past the barrel.
WEAPONS = (('ArcRifle', 70), ('ShadeSMG', 42), ('RuneDMR', 90), ('HexShotgun', 60))
PARTS = ('', '_Bolt', '_Mag')
FINISH = os.environ.get('ARENADUEL_WEAPON_FINISH') == '1'


def import_file(source, destination, name, options=None):
    task = unreal.AssetImportTask()
    task.set_editor_property('filename', source)
    task.set_editor_property('destination_path', destination)
    task.set_editor_property('destination_name', name)
    task.set_editor_property('automated', True)
    task.set_editor_property('save', True)
    task.set_editor_property('replace_existing', True)
    if options:
        task.set_editor_property('options', options)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])


def texture(folder, name, srgb, normal):
    """Blender bakes normals with green up, Unreal expects green down."""
    source = os.path.join(ART, name + '.png')
    if not os.path.isfile(source):
        return None
    import_file(source, folder, name)
    asset = unreal.load_asset(folder + '/' + name)
    if asset:
        asset.set_editor_property('srgb', srgb)
        if normal:
            asset.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_NORMALMAP)
            asset.set_editor_property('flip_green_channel', True)
        elif not srgb:
            asset.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_MASKS)
        LIB.save_loaded_asset(asset)
    return asset


def body_material(folder, name):
    """Baked colour, normal, and roughness with metallic packed into red and green."""
    color_map = texture(folder, 'T_%s_BaseColor' % name, True, False)
    normal_map = texture(folder, 'T_%s_Normal' % name, False, True)
    rm_map = texture(folder, 'T_%s_RM' % name, False, False)
    if not (color_map and normal_map and rm_map):
        return None
    path = '%s/M_%s' % (folder, name)
    if LIB.does_asset_exist(path):
        LIB.delete_asset(path)
    mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset('M_' + name, folder, unreal.Material, unreal.MaterialFactoryNew())
    color = EDIT.create_material_expression(mat, unreal.MaterialExpressionTextureSample, -700, 0)
    color.set_editor_property('texture', color_map)
    EDIT.connect_material_property(color, 'RGB', unreal.MaterialProperty.MP_BASE_COLOR)
    normal = EDIT.create_material_expression(mat, unreal.MaterialExpressionTextureSample, -700, 300)
    normal.set_editor_property('texture', normal_map)
    normal.set_editor_property('sampler_type', unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    EDIT.connect_material_property(normal, 'RGB', unreal.MaterialProperty.MP_NORMAL)
    rm = EDIT.create_material_expression(mat, unreal.MaterialExpressionTextureSample, -700, 600)
    rm.set_editor_property('texture', rm_map)
    rm.set_editor_property('sampler_type', unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
    EDIT.connect_material_property(rm, 'R', unreal.MaterialProperty.MP_ROUGHNESS)
    EDIT.connect_material_property(rm, 'G', unreal.MaterialProperty.MP_METALLIC)
    EDIT.recompile_material(mat)
    LIB.save_loaded_asset(mat)
    return mat


done = []
for name, length in WEAPONS:
    folder = '%s/%s' % (ROOT, name)
    if not FINISH:
        for part in PARTS:
            source = os.path.join(ART, 'SM_%s%s.fbx' % (name, part))
            if not os.path.isfile(source):
                continue
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
            import_file(source, folder, 'SM_%s%s' % (name, part), ui)
            if LIB.does_asset_exist('%s/SM_%s%s' % (folder, name, part)):
                done.append(name + part)
        done.append('%s material=%s' % (name, bool(body_material(folder, name))))
        continue
    body = unreal.load_asset('%s/M_%s' % (folder, name)) or unreal.load_asset('/Game/ArenaDuel/Characters/Common/M_ArcaneMetal')
    accent = unreal.load_asset('/Game/ArenaDuel/Characters/Common/M_ArcaneCyan')
    for part in PARTS:
        mesh = unreal.load_asset('%s/SM_%s%s' % (folder, name, part))
        if not mesh:
            continue
        if not part and not mesh.find_socket('Muzzle'):
            socket = unreal.StaticMeshSocket(outer=mesh)
            socket.set_editor_property('socket_name', 'Muzzle')
            socket.set_editor_property('relative_location', unreal.Vector(length + 3, 0, 13))
            mesh.add_socket(socket)
        slots = [str(entry.material_slot_name) for entry in mesh.get_editor_property('static_materials')]
        for index, slot in enumerate(slots):
            # A part can consist of one colour only; the slot name says which.
            material = accent if 'Accent' in slot else body
            if material:
                mesh.set_material(index, material)
        saved = LIB.save_loaded_asset(mesh, only_if_is_dirty=False)
        bounds = mesh.get_bounds()
        done.append('%s%s saved=%s slots=%s size=(%.0f, %.0f, %.0f)' % (name, part, saved, slots, bounds.box_extent.x * 2, bounds.box_extent.y * 2, bounds.box_extent.z * 2))
unreal.log('WEAPON MESHES %s %s' % ('FINISH' if FINISH else 'IMPORT', done))
