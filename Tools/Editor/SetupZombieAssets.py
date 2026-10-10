"""Import the zombie bodies built by Tools/Art/build_zombie.py and make their materials.

Every Tools/Art/SKM_Zombie_<Variant>.fbx that exists is imported onto SK_Mannequin as
/Game/ArenaDuel/Characters/Zombies/SKM_Zombie_<Variant>, with the mannequin's physics asset, whose
bodies are the hit zones. Skin differs per variant; clothes, shoes and eyes are shared.
Run with the editor closed.
"""
import os
import unreal

DEST = '/Game/ArenaDuel/Characters/Zombies'
ART = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'Art'))
LIB = unreal.EditorAssetLibrary
EDIT = unreal.MaterialEditingLibrary
SKINS = {
    'Normal': (0.20, 0.27, 0.17),
    'Runner': (0.30, 0.27, 0.15),
    'Armoured': (0.22, 0.24, 0.26),
    'Brute': (0.30, 0.19, 0.13),
    'Abomination': (0.30, 0.10, 0.09),
}


def material(name, color, roughness, emissive=None, blotch=None):
    path = DEST + '/' + name
    if LIB.does_asset_exist(path):
        LIB.delete_asset(path)
    mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, DEST, unreal.Material, unreal.MaterialFactoryNew())
    base = EDIT.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector, -700, 0)
    base.set_editor_property('constant', unreal.LinearColor(color[0], color[1], color[2], 1.0))
    out = base
    if blotch:
        # Uneven, bruised skin: a second tone fades in and out across the body.
        dark = EDIT.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector, -700, 200)
        dark.set_editor_property('constant', unreal.LinearColor(blotch[0], blotch[1], blotch[2], 1.0))
        noise = EDIT.create_material_expression(mat, unreal.MaterialExpressionNoise, -700, 400)
        noise.set_editor_property('scale', 0.09)
        noise.set_editor_property('levels', 3)
        noise.set_editor_property('quality', 1)
        noise.set_editor_property('output_min', 0.0)
        noise.set_editor_property('output_max', 1.0)
        local = EDIT.create_material_expression(mat, unreal.MaterialExpressionLocalPosition, -950, 400)
        EDIT.connect_material_expressions(local, '', noise, 'Position')
        lerp = EDIT.create_material_expression(mat, unreal.MaterialExpressionLinearInterpolate, -400, 100)
        EDIT.connect_material_expressions(base, '', lerp, 'A')
        EDIT.connect_material_expressions(dark, '', lerp, 'B')
        EDIT.connect_material_expressions(noise, '', lerp, 'Alpha')
        out = lerp
    EDIT.connect_material_property(out, '', unreal.MaterialProperty.MP_BASE_COLOR)
    rough = EDIT.create_material_expression(mat, unreal.MaterialExpressionConstant, -400, 500)
    rough.set_editor_property('r', roughness)
    EDIT.connect_material_property(rough, '', unreal.MaterialProperty.MP_ROUGHNESS)
    if emissive:
        glow = EDIT.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector, -400, 650)
        glow.set_editor_property('constant', unreal.LinearColor(emissive[0], emissive[1], emissive[2], 1.0))
        EDIT.connect_material_property(glow, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mat.set_editor_property('used_with_skeletal_mesh', True)
    EDIT.recompile_material(mat)
    LIB.save_loaded_asset(mat)
    return mat


shared = {
    'Z_Shirt': material('M_ZombieShirt', (0.055, 0.060, 0.070), 0.90, blotch=(0.020, 0.018, 0.016)),
    'Z_Trousers': material('M_ZombieTrousers', (0.030, 0.034, 0.045), 0.92, blotch=(0.012, 0.011, 0.010)),
    'Z_Shoes': material('M_ZombieShoes', (0.012, 0.012, 0.012), 0.6),
    'Z_Eyes': material('M_ZombieEyes', (0.9, 0.8, 0.5), 0.3, emissive=(6.0, 3.2, 0.5)),
}
skeleton = unreal.load_asset('/Game/Characters/Mannequins/Meshes/SK_Mannequin')
physics = unreal.load_asset('/Game/Characters/Mannequins/Rigs/PA_Mannequin')
done = []
for variant, skin in SKINS.items():
    source = os.path.join(ART, 'SKM_Zombie_%s.fbx' % variant)
    if not os.path.isfile(source):
        continue
    name = 'SKM_Zombie_' + variant
    task = unreal.AssetImportTask()
    task.set_editor_property('filename', source)
    task.set_editor_property('destination_path', DEST)
    task.set_editor_property('destination_name', name)
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
    ui.set_editor_property('skeleton', skeleton)
    data = ui.get_editor_property('skeletal_mesh_import_data')
    data.set_editor_property('import_morph_targets', False)
    data.set_editor_property('preserve_smoothing_groups', True)
    data.set_editor_property('normal_import_method', unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
    task.set_editor_property('options', ui)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    mesh = unreal.load_asset(DEST + '/' + name)
    if not mesh:
        raise RuntimeError('Not imported: ' + name)
    skin_material = material('M_ZombieSkin_' + variant, skin, 0.62, blotch=(skin[0] * 0.45, skin[1] * 0.40, skin[2] * 0.42))
    materials = mesh.get_editor_property('materials')
    for entry in materials:
        slot = str(entry.get_editor_property('material_slot_name'))
        if 'Z_Skin' in slot:
            entry.set_editor_property('material_interface', skin_material)
        for key, mat in shared.items():
            if key in slot:
                entry.set_editor_property('material_interface', mat)
    mesh.set_editor_property('materials', materials)
    if physics:
        mesh.set_editor_property('physics_asset', physics)
    LIB.save_loaded_asset(mesh)
    done.append(variant)
unreal.log('ZOMBIE ASSETS PASS %s physics=%s' % (done, physics.get_name() if physics else None))

