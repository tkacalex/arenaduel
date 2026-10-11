"""Import the set dressing of the 1v1 map built by Tools/Art/build_district_props.py.

Two editor runs with the editor closed, after Tools/Editor/SetupDistrictMaterials.py:
1. as it is: imports Tools/Art/District/SM_*.fbx to /Game/ArenaDuel/Maps/District/Props;
2. with ARENADUEL_DISTRICT_FINISH=1: gives every slot its material and saves (the importer finishes a
   mesh after the script, so both cannot happen in one run).
The planter gets a simple collision box; lamps and ivy have none.
"""
import os
import unreal

DEST = '/Game/ArenaDuel/Maps/District/Props'
LOOK = '/Game/ArenaDuel/Maps/District'
ART = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'Art', 'District'))
LIB = unreal.EditorAssetLibrary
FINISH = os.environ.get('ARENADUEL_DISTRICT_FINISH') == '1'
MESHES = ('SM_WallLamp', 'SM_CeilingLamp', 'SM_Planter', 'SM_Ivy')
# Slot name in the FBX to material of the map.
MATERIALS = {'D_Metal': 'MI_District_SteelDark', 'D_Glow': 'M_DistrictSconce', 'D_Concrete': 'MI_District_Concrete', 'D_Soil': 'M_DistrictSoil', 'D_Bark': 'M_DistrictSoil', 'D_Leaf': 'M_DistrictLeaf'}
report = []
for name in MESHES:
    path = DEST + '/' + name
    if not FINISH:
        source = os.path.join(ART, name + '.fbx')
        if not os.path.isfile(source):
            continue
        task = unreal.AssetImportTask()
        task.set_editor_property('filename', source)
        task.set_editor_property('destination_path', DEST)
        task.set_editor_property('destination_name', name)
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
        data.set_editor_property('auto_generate_collision', name == 'SM_Planter')
        data.set_editor_property('generate_lightmap_u_vs', False)
        data.set_editor_property('normal_import_method', unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
        data.set_editor_property('vertex_color_import_option', unreal.VertexColorImportOption.REPLACE)
        task.set_editor_property('options', ui)
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
        report.append('%s imported=%s' % (name, LIB.does_asset_exist(path)))
        continue
    mesh = unreal.load_asset(path)
    if not mesh:
        continue
    slots = [str(entry.material_slot_name) for entry in mesh.get_editor_property('static_materials')]
    for index, slot in enumerate(slots):
        material = unreal.load_asset(LOOK + '/' + MATERIALS.get(slot, 'MI_District_SteelDark'))
        if material:
            mesh.set_material(index, material)
    saved = LIB.save_loaded_asset(mesh, only_if_is_dirty=False)
    bounds = mesh.get_bounds()
    report.append('%s saved=%s slots=%s size=(%.0f, %.0f, %.0f)' % (name, saved, slots, bounds.box_extent.x * 2, bounds.box_extent.y * 2, bounds.box_extent.z * 2))
unreal.log('DISTRICT PROPS %s %s' % ('FINISH' if FINISH else 'IMPORT', report))
