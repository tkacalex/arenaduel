"""Import the firearms built by Tools/Art/build_weapons.py over the old block models.

Two editor runs, both with the editor closed:

1. as it is: imports Tools/Art/Weapons/SM_<Name>.fbx to /Game/ArenaDuel/Weapons/<Name>/SM_<Name>;
2. with the environment variable ARENADUEL_WEAPON_FINISH=1: adds the Muzzle socket, sets the two
   materials and saves. This cannot be part of the first run, the importer finishes the mesh after
   the script and would drop the changes.

The meshes keep the game's conventions (origin at the grip, +X to the muzzle, barrel at a height of 13,
slot 0 body, slot 1 accent), so no code refers to anything new.
"""
import os
import unreal

ROOT = '/Game/ArenaDuel/Weapons'
ART = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'Art', 'Weapons'))
LIB = unreal.EditorAssetLibrary
# Name and barrel length; the muzzle lies three centimetres past the barrel.
WEAPONS = (('ArcRifle', 70), ('ShadeSMG', 42), ('RuneDMR', 90), ('HexShotgun', 60))
FINISH = os.environ.get('ARENADUEL_WEAPON_FINISH') == '1'
done = []
for name, length in WEAPONS:
    asset_path = '%s/%s/SM_%s' % (ROOT, name, name)
    if not FINISH:
        source = os.path.join(ART, 'SM_%s.fbx' % name)
        if not os.path.isfile(source):
            continue
        task = unreal.AssetImportTask()
        task.set_editor_property('filename', source)
        task.set_editor_property('destination_path', '%s/%s' % (ROOT, name))
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
    if not mesh.find_socket('Muzzle'):
        socket = unreal.StaticMeshSocket(outer=mesh)
        socket.set_editor_property('socket_name', 'Muzzle')
        socket.set_editor_property('relative_location', unreal.Vector(length + 3, 0, 13))
        mesh.add_socket(socket)
    body = unreal.load_asset('/Game/ArenaDuel/Characters/Common/M_ArcaneMetal')
    accent = unreal.load_asset('/Game/ArenaDuel/Characters/Common/M_ArcaneCyan')
    for index, material in enumerate((body, accent)):
        if material and index < len(mesh.get_editor_property('static_materials')):
            mesh.set_material(index, material)
    saved = LIB.save_loaded_asset(mesh, only_if_is_dirty=False)
    bounds = mesh.get_bounds()
    done.append('%s saved=%s muzzle=%s slots=%d size=(%.0f, %.0f, %.0f)' % (name, saved, bool(mesh.find_socket('Muzzle')), len(mesh.get_editor_property('static_materials')),
                                                                         bounds.box_extent.x * 2, bounds.box_extent.y * 2, bounds.box_extent.z * 2))
unreal.log('WEAPON MESHES %s %s' % ('FINISH' if FINISH else 'IMPORT', done))
