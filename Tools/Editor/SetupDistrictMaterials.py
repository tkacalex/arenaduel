"""Textures and materials of the 1v1 map, L_ArenaDistrict.

Imports Tools/Art/District/*.png (made by Tools/Art/build_district_textures.py) to
/Game/ArenaDuel/Maps/District and builds one parent material, M_DistrictSurface, with an instance per
surface. Run with the editor closed, before Tools/Editor/SetupArenaDistrict.py.

The map is built from scaled cubes, whose own UVs stretch with the scale. The parent material therefore
maps its textures in world space, along whichever axis a face looks most: one repeat always covers the
same number of centimetres, on any piece, turned or not, at the cost of one read per map. A large soft mask varies brightness over many metres so the
repeat does not show, and walls get a little darker towards the floor.
"""
import os
import unreal

DEST = '/Game/ArenaDuel/Maps/District'
ART = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'Art', 'District'))
LIB = unreal.EditorAssetLibrary
EDIT = unreal.MaterialEditingLibrary
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()

# One projection per pixel, the one the face looks along most: a box face has one normal, so nothing is lost
# against blending three projections, and each map is read once instead of three times.
PICK = '''float3 a = abs(N); float3 p = WP / max(Size, 1.0);
float2 uv = a.x > a.y && a.x > a.z ? float2(p.y, -p.z) : (a.y > a.z ? float2(p.x, -p.z) : float2(p.x, p.y));
'''
# The engraving is read the right way round on every wall: its sideways axis turns with the side the wall faces.
ENGRAVE = '''float3 sgn = sign(N);
float2 euv = a.x > a.y && a.x > a.z ? float2(-WP.y * sgn.x, -WP.z) : float2(WP.x * sgn.y, -WP.z);
float eflip = a.x > a.y && a.x > a.z ? -sgn.x : sgn.y;
float3 e = Texture2DSample(Mono, MonoSampler, euv / max(EngraveSize, 1.0)).rgb;
float eamount = Engrave * step(a.z, 0.5);
'''
COLOR_CODE = PICK + ENGRAVE + '''float3 c = Texture2DSample(Tex, TexSampler, uv).rgb * (1.0 - 0.5 * e.r * eamount);
float3 q = WP / max(MacroSize, 1.0);
float2 guv = a.x > a.y && a.x > a.z ? float2(q.y, -q.z) : (a.y > a.z ? float2(q.x, -q.z) : float2(q.x, q.y));
float g = Texture2DSample(Grunge, GrungeSampler, guv).r;
float dirt = saturate(1.0 - WP.z / max(DirtHeight, 1.0)) * DirtAmount * (0.6 + 0.8 * g);
return c * Tint * lerp(1.0 - Macro, 1.0 + Macro * 0.6, g) * (1.0 - dirt);'''
NORMAL_CODE = PICK + ENGRAVE + '''float3 t = UnpackNormalMap(Texture2DSample(Tex, TexSampler, uv)).xyz;
t.xy *= Strength;
t.xy += float2((e.g * 2.0 - 1.0) * eflip, e.b * 2.0 - 1.0) * eamount * 1.6;
float3 s = sign(N);
float3 n = a.x > a.y && a.x > a.z ? float3(s.x * t.z, t.x, -t.y) : (a.y > a.z ? float3(t.x, s.y * t.z, -t.y) : float3(t.x, t.y, s.z * t.z));
return normalize(n);'''
RM_CODE = PICK + '''return Texture2DSample(Tex, TexSampler, uv).rgb;'''


def texture(name, srgb, normal):
    source = os.path.join(ART, name + '.png')
    if not os.path.isfile(source):
        raise RuntimeError('Missing ' + source)
    task = unreal.AssetImportTask()
    task.set_editor_property('filename', source)
    task.set_editor_property('destination_path', DEST)
    task.set_editor_property('destination_name', name)
    task.set_editor_property('automated', True)
    task.set_editor_property('save', True)
    task.set_editor_property('replace_existing', True)
    TOOLS.import_asset_tasks([task])
    asset = unreal.load_asset(DEST + '/' + name)
    asset.set_editor_property('srgb', srgb)
    if normal:
        asset.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_NORMALMAP)
        asset.set_editor_property('flip_green_channel', True)
    elif not srgb:
        asset.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_MASKS)
    LIB.save_loaded_asset(asset)
    return asset


def custom(mat, code, names, x, y):
    node = EDIT.create_material_expression(mat, unreal.MaterialExpressionCustom, x, y)
    node.set_editor_property('code', code)
    node.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    inputs = []
    for name in names:
        entry = unreal.CustomInput()
        entry.set_editor_property('input_name', name)
        inputs.append(entry)
    node.set_editor_property('inputs', inputs)
    return node


def scalar(mat, name, value, y):
    node = EDIT.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -1300, y)
    node.set_editor_property('parameter_name', name)
    node.set_editor_property('default_value', value)
    return node


def texture_parameter(mat, name, asset, y, normal=False):
    node = EDIT.create_material_expression(mat, unreal.MaterialExpressionTextureObjectParameter, -1300, y)
    node.set_editor_property('parameter_name', name)
    node.set_editor_property('texture', asset)
    node.set_editor_property('sampler_type', unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL if normal else (unreal.MaterialSamplerType.SAMPLERTYPE_COLOR if name == 'BaseColor' else unreal.MaterialSamplerType.SAMPLERTYPE_MASKS))
    return node


def parent(textures):
    path = DEST + '/M_DistrictSurface'
    if LIB.does_asset_exist(path):
        LIB.delete_asset(path)
    mat = TOOLS.create_asset('M_DistrictSurface', DEST, unreal.Material, unreal.MaterialFactoryNew())
    # The normal comes out of the three-sided mapping in world space already.
    mat.set_editor_property('tangent_space_normal', False)
    position = EDIT.create_material_expression(mat, unreal.MaterialExpressionWorldPosition, -1300, -700)
    normal = EDIT.create_material_expression(mat, unreal.MaterialExpressionVertexNormalWS, -1300, -600)
    size = scalar(mat, 'Size', 400.0, -500)
    macro_size = scalar(mat, 'MacroSize', 1700.0, -420)
    macro = scalar(mat, 'Macro', 0.22, -340)
    dirt_height = scalar(mat, 'DirtHeight', 90.0, -260)
    dirt_amount = scalar(mat, 'DirtAmount', 0.0, -180)
    strength = scalar(mat, 'NormalStrength', 1.0, -100)
    rough_scale = scalar(mat, 'RoughnessScale', 1.0, -20)
    tint = EDIT.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -1300, 80)
    tint.set_editor_property('parameter_name', 'Tint')
    tint.set_editor_property('default_value', unreal.LinearColor(1.0, 1.0, 1.0, 1.0))
    base_tex = texture_parameter(mat, 'BaseColor', textures['Concrete'][0], 300)
    normal_tex = texture_parameter(mat, 'Normal', textures['Concrete'][1], 500, True)
    rm_tex = texture_parameter(mat, 'RM', textures['Concrete'][2], 700)
    grunge_tex = texture_parameter(mat, 'Grunge', textures['Grunge'], 900)
    mono_tex = texture_parameter(mat, 'Monogram', textures['Monogram'], 1100)
    engrave_size = scalar(mat, 'EngraveSize', 56.0, 1300)
    engrave = scalar(mat, 'Engrave', 0.0, 1380)

    color = custom(mat, COLOR_CODE, ('WP', 'N', 'Size', 'Tex', 'MacroSize', 'Grunge', 'Macro', 'DirtHeight', 'DirtAmount', 'Tint', 'Mono', 'EngraveSize', 'Engrave'), -700, -400)
    for source, pin, name in ((position, '', 'WP'), (normal, '', 'N'), (size, '', 'Size'), (base_tex, '', 'Tex'), (macro_size, '', 'MacroSize'), (grunge_tex, '', 'Grunge'),
                              (macro, '', 'Macro'), (dirt_height, '', 'DirtHeight'), (dirt_amount, '', 'DirtAmount'), (tint, '', 'Tint'),
                              (mono_tex, '', 'Mono'), (engrave_size, '', 'EngraveSize'), (engrave, '', 'Engrave')):
        EDIT.connect_material_expressions(source, pin, color, name)
    EDIT.connect_material_property(color, '', unreal.MaterialProperty.MP_BASE_COLOR)

    bump = custom(mat, NORMAL_CODE, ('WP', 'N', 'Size', 'Tex', 'Strength', 'Mono', 'EngraveSize', 'Engrave'), -700, 100)
    for source, name in ((position, 'WP'), (normal, 'N'), (size, 'Size'), (normal_tex, 'Tex'), (strength, 'Strength'), (mono_tex, 'Mono'), (engrave_size, 'EngraveSize'), (engrave, 'Engrave')):
        EDIT.connect_material_expressions(source, '', bump, name)
    EDIT.connect_material_property(bump, '', unreal.MaterialProperty.MP_NORMAL)

    surface = custom(mat, RM_CODE, ('WP', 'N', 'Size', 'Tex'), -700, 500)
    for source, name in ((position, 'WP'), (normal, 'N'), (size, 'Size'), (rm_tex, 'Tex')):
        EDIT.connect_material_expressions(source, '', surface, name)
    red = EDIT.create_material_expression(mat, unreal.MaterialExpressionComponentMask, -400, 420)
    red.set_editor_property('r', True); red.set_editor_property('g', False); red.set_editor_property('b', False); red.set_editor_property('a', False)
    green = EDIT.create_material_expression(mat, unreal.MaterialExpressionComponentMask, -400, 560)
    green.set_editor_property('r', False); green.set_editor_property('g', True); green.set_editor_property('b', False); green.set_editor_property('a', False)
    blue = EDIT.create_material_expression(mat, unreal.MaterialExpressionComponentMask, -400, 700)
    blue.set_editor_property('r', False); blue.set_editor_property('g', False); blue.set_editor_property('b', True); blue.set_editor_property('a', False)
    for mask in (red, green, blue):
        EDIT.connect_material_expressions(surface, '', mask, '')
    rough = EDIT.create_material_expression(mat, unreal.MaterialExpressionMultiply, -200, 420)
    EDIT.connect_material_expressions(red, '', rough, 'A')
    EDIT.connect_material_expressions(rough_scale, '', rough, 'B')
    EDIT.connect_material_property(rough, '', unreal.MaterialProperty.MP_ROUGHNESS)
    EDIT.connect_material_property(green, '', unreal.MaterialProperty.MP_METALLIC)
    EDIT.connect_material_property(blue, '', unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)
    EDIT.recompile_material(mat)
    LIB.save_loaded_asset(mat)
    return mat


def instance(name, base, textures, surface, size, tint, macro=0.22, dirt=0.0, dirt_height=90.0, normal=1.0, rough=1.0, engrave=0.0):
    path = DEST + '/' + name
    if LIB.does_asset_exist(path):
        LIB.delete_asset(path)
    inst = TOOLS.create_asset(name, DEST, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    EDIT.set_material_instance_parent(inst, base)
    color_map, normal_map, rm_map = textures[surface]
    EDIT.set_material_instance_texture_parameter_value(inst, 'BaseColor', color_map)
    EDIT.set_material_instance_texture_parameter_value(inst, 'Normal', normal_map)
    EDIT.set_material_instance_texture_parameter_value(inst, 'RM', rm_map)
    for key, value in (('Size', size), ('Macro', macro), ('DirtAmount', dirt), ('DirtHeight', dirt_height), ('NormalStrength', normal), ('RoughnessScale', rough), ('Engrave', engrave)):
        EDIT.set_material_instance_scalar_parameter_value(inst, key, value)
    EDIT.set_material_instance_vector_parameter_value(inst, 'Tint', unreal.LinearColor(tint[0], tint[1], tint[2], 1.0))
    EDIT.update_material_instance(inst)
    LIB.save_loaded_asset(inst)
    return inst


def glow(name, color, strength):
    """A lit lens or strip: brightest seen straight on and dimmer towards its edge and at a slant, with fine
    ribs across it, so it reads as a diffuser in front of a lamp and not as a flat patch of colour."""
    path = DEST + '/' + name
    if LIB.does_asset_exist(path):
        LIB.delete_asset(path)
    mat = TOOLS.create_asset(name, DEST, unreal.Material, unreal.MaterialFactoryNew())
    base = EDIT.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector, -400, 0)
    base.set_editor_property('constant', unreal.LinearColor(color[0] * 0.2, color[1] * 0.2, color[2] * 0.2, 1.0))
    EDIT.connect_material_property(base, '', unreal.MaterialProperty.MP_BASE_COLOR)
    tone = EDIT.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector, -900, 200)
    tone.set_editor_property('constant', unreal.LinearColor(color[0] * strength, color[1] * strength, color[2] * strength, 1.0))
    normal = EDIT.create_material_expression(mat, unreal.MaterialExpressionVertexNormalWS, -900, 320)
    view = EDIT.create_material_expression(mat, unreal.MaterialExpressionCameraVectorWS, -900, 420)
    position = EDIT.create_material_expression(mat, unreal.MaterialExpressionWorldPosition, -900, 520)
    light = custom(mat, '''float d = saturate(abs(dot(normalize(N), normalize(V))));
float ribs = 0.86 + 0.14 * sin((WP.x + WP.y + WP.z) * 1.9);
return Color * (0.30 + 0.70 * d * d) * ribs;''', ('Color', 'N', 'V', 'WP'), -500, 300)
    for source, pin in ((tone, 'Color'), (normal, 'N'), (view, 'V'), (position, 'WP')):
        EDIT.connect_material_expressions(source, '', light, pin)
    EDIT.connect_material_property(light, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    rough = EDIT.create_material_expression(mat, unreal.MaterialExpressionConstant, -400, 520)
    rough.set_editor_property('r', 0.25)
    EDIT.connect_material_property(rough, '', unreal.MaterialProperty.MP_ROUGHNESS)
    EDIT.recompile_material(mat)
    LIB.save_loaded_asset(mat)
    return mat


def sample(mat, asset, y, kind):
    node = EDIT.create_material_expression(mat, unreal.MaterialExpressionTextureSample, -700, y)
    node.set_editor_property('texture', asset)
    node.set_editor_property('sampler_type', kind)
    return node


def mapped(name, color_map, normal_map, rm_map):
    """A surface with its own texture coordinates: the crate."""
    path = DEST + '/' + name
    if LIB.does_asset_exist(path):
        LIB.delete_asset(path)
    mat = TOOLS.create_asset(name, DEST, unreal.Material, unreal.MaterialFactoryNew())
    color = sample(mat, color_map, 0, unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    bump = sample(mat, normal_map, 260, unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    surface = sample(mat, rm_map, 520, unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
    EDIT.connect_material_property(color, 'RGB', unreal.MaterialProperty.MP_BASE_COLOR)
    EDIT.connect_material_property(bump, 'RGB', unreal.MaterialProperty.MP_NORMAL)
    EDIT.connect_material_property(surface, 'R', unreal.MaterialProperty.MP_ROUGHNESS)
    EDIT.connect_material_property(surface, 'G', unreal.MaterialProperty.MP_METALLIC)
    EDIT.connect_material_property(surface, 'B', unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)
    EDIT.recompile_material(mat)
    LIB.save_loaded_asset(mat)
    return mat


def foliage(name, leaf, leaf_normal, grunge):
    """Leaves: the leaf picture cut out along its outline, seen from both sides, with light coming through.
    Each leaf gets its own shade of green from where it hangs, so a curtain of them is not one flat colour."""
    path = DEST + '/' + name
    if LIB.does_asset_exist(path):
        LIB.delete_asset(path)
    mat = TOOLS.create_asset(name, DEST, unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property('two_sided', True)
    mat.set_editor_property('blend_mode', unreal.BlendMode.BLEND_MASKED)
    mat.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_TWO_SIDED_FOLIAGE)
    mat.set_editor_property('opacity_mask_clip_value', 0.4)
    color = sample(mat, leaf, 0, unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    bump = sample(mat, leaf_normal, 260, unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    position = EDIT.create_material_expression(mat, unreal.MaterialExpressionWorldPosition, -900, 520)
    mask = EDIT.create_material_expression(mat, unreal.MaterialExpressionTextureObject, -900, 640)
    mask.set_editor_property('texture', grunge)
    mask.set_editor_property('sampler_type', unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
    shade = custom(mat, '''float a = Texture2DSample(Tex, TexSampler, WP.yz / 41.0 + WP.x / 53.0).r;
float b = Texture2DSample(Tex, TexSampler, WP.xy / 13.0 + WP.z / 17.0).r;
float v = saturate(a * 0.6 + b * 0.6 - 0.1);
return Leaf * lerp(float3(0.55, 0.60, 0.50), float3(1.35, 1.25, 0.85), v);''', ('WP', 'Tex', 'Leaf'), -400, 400)
    EDIT.connect_material_expressions(position, '', shade, 'WP')
    EDIT.connect_material_expressions(mask, '', shade, 'Tex')
    EDIT.connect_material_expressions(color, 'RGB', shade, 'Leaf')
    EDIT.connect_material_property(shade, '', unreal.MaterialProperty.MP_BASE_COLOR)
    through = EDIT.create_material_expression(mat, unreal.MaterialExpressionMultiply, -200, 560)
    half = EDIT.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector, -400, 620)
    half.set_editor_property('constant', unreal.LinearColor(0.55, 0.75, 0.25, 1.0))
    EDIT.connect_material_expressions(shade, '', through, 'A')
    EDIT.connect_material_expressions(half, '', through, 'B')
    EDIT.connect_material_property(through, '', unreal.MaterialProperty.MP_SUBSURFACE_COLOR)
    EDIT.connect_material_property(color, 'A', unreal.MaterialProperty.MP_OPACITY_MASK)
    EDIT.connect_material_property(bump, 'RGB', unreal.MaterialProperty.MP_NORMAL)
    rough = EDIT.create_material_expression(mat, unreal.MaterialExpressionConstant, -400, 800)
    rough.set_editor_property('r', 0.45)
    EDIT.connect_material_property(rough, '', unreal.MaterialProperty.MP_ROUGHNESS)
    EDIT.recompile_material(mat)
    LIB.save_loaded_asset(mat)
    return mat


def plain(name, color, roughness):
    path = DEST + '/' + name
    if LIB.does_asset_exist(path):
        LIB.delete_asset(path)
    mat = TOOLS.create_asset(name, DEST, unreal.Material, unreal.MaterialFactoryNew())
    base = EDIT.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector, -400, 0)
    base.set_editor_property('constant', unreal.LinearColor(color[0], color[1], color[2], 1.0))
    EDIT.connect_material_property(base, '', unreal.MaterialProperty.MP_BASE_COLOR)
    rough = EDIT.create_material_expression(mat, unreal.MaterialExpressionConstant, -400, 200)
    rough.set_editor_property('r', roughness)
    EDIT.connect_material_property(rough, '', unreal.MaterialProperty.MP_ROUGHNESS)
    EDIT.recompile_material(mat)
    LIB.save_loaded_asset(mat)
    return mat


textures = {}
for surface in ('Concrete', 'Floor', 'Plaster', 'Tread', 'Deck', 'Steel', 'Wood'):
    textures[surface] = (texture('T_District_%s_BaseColor' % surface, True, False), texture('T_District_%s_Normal' % surface, False, True), texture('T_District_%s_RM' % surface, False, False))
textures['Grunge'] = texture('T_District_Grunge', False, False)
# Made by Tools/Art/build_district_details.py.
textures['Crate'] = (texture('T_District_Crate_BaseColor', True, False), texture('T_District_Crate_Normal', False, True), texture('T_District_Crate_RM', False, False))
textures['Leaf'] = (texture('T_District_Leaf', True, False), texture('T_District_Leaf_Normal', False, True))
textures['Monogram'] = texture('T_District_Monogram', False, False)
# The engraving is fine lines: stored without block compression, which would smear them.
textures['Monogram'].set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_VECTOR_DISPLACEMENTMAP)
LIB.save_loaded_asset(textures['Monogram'])
for stale in ('M_DistrictSoil',):
    if LIB.does_asset_exist(DEST + '/' + stale):
        LIB.delete_asset(DEST + '/' + stale)
base = parent(textures)
made = [
    # The look follows the reference picture: a dark hall with a wet, reflecting floor, light concrete cover
    # with dark steel caps, warm timber, a warm house, and orange markings; the light comes in warm pools.
    instance('MI_District_Floor', base, textures, 'Floor', 400.0, (0.50, 0.52, 0.56), macro=0.40, rough=0.42),
    instance('MI_District_Shell', base, textures, 'Concrete', 300.0, (0.20, 0.215, 0.25), macro=0.30, dirt=0.30, dirt_height=200.0, rough=0.9, engrave=1.0),
    instance('MI_District_Concrete', base, textures, 'Concrete', 400.0, (0.92, 0.90, 0.86), macro=0.28, dirt=0.30, dirt_height=70.0, engrave=1.0),
    instance('MI_District_House', base, textures, 'Plaster', 200.0, (0.86, 0.68, 0.50), macro=0.26, dirt=0.32, dirt_height=80.0, engrave=1.0),
    # The plate is baked dark; the tint lifts it to a mid steel blue so a deck does not read as a black hole.
    instance('MI_District_Tread', base, textures, 'Tread', 100.0, (2.2, 2.4, 2.8), macro=0.18, rough=1.0),
    instance('MI_District_Ceiling', base, textures, 'Deck', 200.0, (0.12, 0.13, 0.16), macro=0.15),
    instance('MI_District_SteelDark', base, textures, 'Steel', 150.0, (0.10, 0.11, 0.13), macro=0.20),
    instance('MI_District_SteelYellow', base, textures, 'Steel', 150.0, (1.0, 0.55, 0.06), macro=0.15),
    instance('MI_District_PaintWhite', base, textures, 'Steel', 150.0, (0.90, 0.88, 0.84), macro=0.30, rough=1.3),
    instance('MI_District_PaintOrange', base, textures, 'Steel', 150.0, (1.0, 0.50, 0.05), macro=0.35, rough=1.3),
    instance('MI_District_Wood', base, textures, 'Wood', 120.0, (1.0, 1.0, 1.0), macro=0.22),
    glow('M_DistrictLightPanel', (0.85, 0.92, 1.0), 9.0),
    glow('M_DistrictSconce', (1.0, 0.62, 0.26), 22.0),
    # The strips stay a colour: bright enough to bloom, not so bright that they burn out to white.
    glow('M_DistrictTeamCyan', (0.03, 0.62, 1.0), 6.0),
    glow('M_DistrictTeamViolet', (0.50, 0.10, 1.0), 6.0),
    foliage('M_DistrictLeaf', textures['Leaf'][0], textures['Leaf'][1], textures['Grunge']),
    plain('M_DistrictBark', (0.060, 0.042, 0.026), 0.85),
    mapped('M_DistrictCrate', *textures['Crate']),
]
unreal.log('DISTRICT MATERIALS %s' % [m.get_name() for m in made if m])
