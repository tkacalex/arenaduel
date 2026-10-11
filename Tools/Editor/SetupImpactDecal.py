"""The mark a bullet leaves on the world: /Game/ArenaDuel/Weapons/M_BulletImpact, a decal material.

Drawn in the material itself, no texture: a dark hole, a scorched ring around it and a ragged patch of
chipped, paler surface, so it shows on light concrete and on dark walls alike. The weapon component spawns
it where a shot ended on the world (UArenaDuelWeaponComponent::SpawnImpactMark). Run with the editor closed.
"""
import unreal

DEST = '/Game/ArenaDuel/Weapons'
NAME = 'M_BulletImpact'
LIB = unreal.EditorAssetLibrary
EDIT = unreal.MaterialEditingLibrary

if LIB.does_asset_exist(DEST + '/' + NAME):
    LIB.delete_asset(DEST + '/' + NAME)
mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset(NAME, DEST, unreal.Material, unreal.MaterialFactoryNew())
mat.set_editor_property('material_domain', unreal.MaterialDomain.MD_DEFERRED_DECAL)
mat.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
coords = EDIT.create_material_expression(mat, unreal.MaterialExpressionTextureCoordinate, -900, 0)
SHAPE = '''float2 p = UV * 2.0 - 1.0;
float r = length(p);
float ang = atan2(p.y, p.x);
float jag = 0.80 + 0.20 * sin(ang * 7.0) * sin(ang * 3.0 + 1.3);
'''


def custom(code, output, y):
    node = EDIT.create_material_expression(mat, unreal.MaterialExpressionCustom, -500, y)
    node.set_editor_property('code', SHAPE + code)
    node.set_editor_property('output_type', output)
    entry = unreal.CustomInput()
    entry.set_editor_property('input_name', 'UV')
    node.set_editor_property('inputs', [entry])
    EDIT.connect_material_expressions(coords, '', node, 'UV')
    return node


color = custom('''float hole = 1.0 - smoothstep(0.14, 0.24, r);
float scorch = 1.0 - smoothstep(0.20, 0.48 * jag, r);
return lerp(float3(0.46, 0.44, 0.41), float3(0.012, 0.011, 0.010), saturate(hole + scorch * 0.75));''', unreal.CustomMaterialOutputType.CMOT_FLOAT3, 0)
opacity = custom('return 1.0 - smoothstep(0.50 * jag, 0.80 * jag, r);', unreal.CustomMaterialOutputType.CMOT_FLOAT1, 300)
rough = EDIT.create_material_expression(mat, unreal.MaterialExpressionConstant, -500, 600)
rough.set_editor_property('r', 0.9)
EDIT.connect_material_property(color, '', unreal.MaterialProperty.MP_BASE_COLOR)
EDIT.connect_material_property(opacity, '', unreal.MaterialProperty.MP_OPACITY)
EDIT.connect_material_property(rough, '', unreal.MaterialProperty.MP_ROUGHNESS)
EDIT.recompile_material(mat)
unreal.log('IMPACT DECAL saved=%s' % LIB.save_loaded_asset(mat))
