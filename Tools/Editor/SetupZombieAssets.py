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


def texture(name, srgb, normal):
    """Import Tools/Art/<name>.png. Blender bakes normals with green up, Unreal expects green down."""
    source = os.path.join(ART, name + '.png')
    if not os.path.isfile(source):
        return None
    task = unreal.AssetImportTask()
    task.set_editor_property('filename', source)
    task.set_editor_property('destination_path', DEST)
    task.set_editor_property('destination_name', name)
    task.set_editor_property('automated', True)
    task.set_editor_property('save', True)
    task.set_editor_property('replace_existing', True)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    asset = unreal.load_asset(DEST + '/' + name)
    if asset:
        asset.set_editor_property('srgb', srgb)
        if normal:
            asset.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_NORMALMAP)
            asset.set_editor_property('flip_green_channel', True)
        LIB.save_loaded_asset(asset)
    return asset


def textured(name, tint=None):
    """Baked colour, normal and roughness. With a tint, the skin (alpha of the colour map) takes the variant's tone."""
    path = DEST + '/' + name
    if LIB.does_asset_exist(path):
        LIB.delete_asset(path)
    mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, DEST, unreal.Material, unreal.MaterialFactoryNew())
    color = EDIT.create_material_expression(mat, unreal.MaterialExpressionTextureSample, -900, 0)
    color.set_editor_property('texture', TEXTURES['color'])
    out, pin = color, 'RGB'
    if tint:
        tone = EDIT.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector, -900, 300)
        tone.set_editor_property('constant', unreal.LinearColor(tint[0] * 2.4, tint[1] * 2.4, tint[2] * 2.4, 1.0))
        tinted = EDIT.create_material_expression(mat, unreal.MaterialExpressionMultiply, -650, 200)
        EDIT.connect_material_expressions(color, 'RGB', tinted, 'A')
        EDIT.connect_material_expressions(tone, '', tinted, 'B')
        lerp = EDIT.create_material_expression(mat, unreal.MaterialExpressionLinearInterpolate, -400, 100)
        EDIT.connect_material_expressions(color, 'RGB', lerp, 'A')
        EDIT.connect_material_expressions(tinted, '', lerp, 'B')
        EDIT.connect_material_expressions(color, 'A', lerp, 'Alpha')
        out, pin = lerp, ''
    EDIT.connect_material_property(out, pin, unreal.MaterialProperty.MP_BASE_COLOR)
    normal = EDIT.create_material_expression(mat, unreal.MaterialExpressionTextureSample, -900, 550)
    normal.set_editor_property('texture', TEXTURES['normal'])
    normal.set_editor_property('sampler_type', unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    EDIT.connect_material_property(normal, 'RGB', unreal.MaterialProperty.MP_NORMAL)
    rough = EDIT.create_material_expression(mat, unreal.MaterialExpressionTextureSample, -900, 800)
    rough.set_editor_property('texture', TEXTURES['roughness'])
    rough.set_editor_property('sampler_type', unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
    EDIT.connect_material_property(rough, 'R', unreal.MaterialProperty.MP_ROUGHNESS)
    mat.set_editor_property('used_with_skeletal_mesh', True)
    EDIT.recompile_material(mat)
    LIB.save_loaded_asset(mat)
    return mat


TEXTURES = {'color': texture('T_Zombie_BaseColor', True, False), 'normal': texture('T_Zombie_Normal', False, True), 'roughness': texture('T_Zombie_Roughness', False, False)}
BAKED = all(TEXTURES.values())
shared = {
    'Z_Shirt': textured('M_ZombieShirt') if BAKED else material('M_ZombieShirt', (0.055, 0.060, 0.070), 0.90, blotch=(0.020, 0.018, 0.016)),
    'Z_Trousers': textured('M_ZombieTrousers') if BAKED else material('M_ZombieTrousers', (0.030, 0.034, 0.045), 0.92, blotch=(0.012, 0.011, 0.010)),
    'Z_Shoes': textured('M_ZombieShoes') if BAKED else material('M_ZombieShoes', (0.012, 0.012, 0.012), 0.6),
    'Z_Eyes': material('M_ZombieEyes', (0.9, 0.8, 0.5), 0.3, emissive=(6.0, 3.2, 0.5)),
    # Gear: the armoured zombie's plates and helmet, the bosses' bone spurs.
    'Z_Armour': material('M_ZombieArmour', (0.045, 0.050, 0.058), 0.42),
    'Z_Bone': material('M_ZombieBone', (0.42, 0.37, 0.27), 0.55, blotch=(0.16, 0.10, 0.07)),
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
    skin_material = textured('M_ZombieSkin_' + variant, tint=skin) if BAKED else material('M_ZombieSkin_' + variant, skin, 0.62, blotch=(skin[0] * 0.45, skin[1] * 0.40, skin[2] * 0.42))
    materials = mesh.get_editor_property('materials')
    # Array elements come out as copies: each changed entry has to be written back by index.
    for index in range(len(materials)):
        entry = materials[index]
        slot = str(entry.get_editor_property('material_slot_name'))
        if 'Z_Skin' in slot:
            entry.set_editor_property('material_interface', skin_material)
        for key, mat in shared.items():
            if key in slot:
                entry.set_editor_property('material_interface', mat)
        materials[index] = entry
    mesh.set_editor_property('materials', materials)
    if physics:
        mesh.set_editor_property('physics_asset', physics)
    LIB.save_loaded_asset(mesh)
    assigned = [e.get_editor_property('material_interface').get_name() for e in mesh.get_editor_property('materials') if e.get_editor_property('material_interface')]
    done.append('%s:%s' % (variant, assigned))
unreal.log('ZOMBIE ASSETS PASS baked=%s %s' % (BAKED, [d.split(':')[0] for d in done]))

