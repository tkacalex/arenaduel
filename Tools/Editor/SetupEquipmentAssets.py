"""Knife and flashbang: import the models, make their materials and synthesise their sounds.

Two editor runs with the editor closed. As it is: sounds, materials and the import of
Tools/Art/Equipment/SM_Knife.fbx and SM_Flashbang.fbx to /Game/ArenaDuel/Weapons/Equipment.
With ARENADUEL_EQUIPMENT_FINISH=1: the materials onto the mesh slots, then the save (the importer
finishes a mesh after the script, so both cannot happen in one run).

The sounds are generated here, tones and noise shaped by envelopes, so the project needs no recordings.
"""
import math
import os
import random
import struct
import wave

import unreal

DEST = '/Game/ArenaDuel/Weapons/Equipment'
AUDIO = '/Game/ArenaDuel/Audio'
ART = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'Art', 'Equipment'))
LIB = unreal.EditorAssetLibrary
EDIT = unreal.MaterialEditingLibrary
RATE = 44100
FINISH = os.environ.get('ARENADUEL_EQUIPMENT_FINISH') == '1'
# Mesh, then the material of each slot in order.
MESHES = (('Knife', ('M_KnifeSteel', 'M_KnifeGrip', 'M_KnifeFittings')), ('Flashbang', ('M_FlashBody', 'M_FlashBand', 'M_FlashLever')))


def material(name, color, roughness, metallic):
    path = DEST + '/' + name
    if LIB.does_asset_exist(path):
        LIB.delete_asset(path)
    mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, DEST, unreal.Material, unreal.MaterialFactoryNew())
    base = EDIT.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector, -500, 0)
    base.set_editor_property('constant', unreal.LinearColor(color[0], color[1], color[2], 1.0))
    EDIT.connect_material_property(base, '', unreal.MaterialProperty.MP_BASE_COLOR)
    for value, target, y in ((roughness, unreal.MaterialProperty.MP_ROUGHNESS, 200), (metallic, unreal.MaterialProperty.MP_METALLIC, 350)):
        node = EDIT.create_material_expression(mat, unreal.MaterialExpressionConstant, -500, y)
        node.set_editor_property('r', value)
        EDIT.connect_material_property(node, '', target)
    EDIT.recompile_material(mat)
    LIB.save_loaded_asset(mat)
    return mat


def lowpass(samples, strength):
    out, state = [], 0.0
    for sample in samples:
        state += strength * (sample - state)
        out.append(state)
    return out


def noise(count, seed):
    rng = random.Random(seed)
    return [rng.uniform(-1.0, 1.0) for _ in range(count)]


def swing(seconds, seed):
    """Air cut by a blade: noise through a filter that opens and closes."""
    count = int(seconds * RATE)
    raw, out, state = noise(count, seed), [], 0.0
    for index in range(count):
        t = index / float(count)
        state += (0.03 + 0.35 * math.sin(math.pi * t) ** 2) * (raw[index] - state)
        out.append(state * math.sin(math.pi * t) ** 1.5)
    return out


def knife_hit(seconds, seed):
    """A dull, wet thud with a short metallic tick on top."""
    count = int(seconds * RATE)
    raw = lowpass(noise(count, seed), 0.08)
    out, phase = [], 0.0
    for index in range(count):
        t = index / float(count)
        phase += (120.0 - 70.0 * t) / RATE
        out.append(math.sin(2.0 * math.pi * phase) * math.exp(-9.0 * t) + raw[index] * 3.0 * math.exp(-14.0 * t) + 0.25 * math.sin(2.0 * math.pi * 3100.0 * index / RATE) * math.exp(-60.0 * t))
    return out


def clink(seconds, seed):
    """Metal on concrete: a few inharmonic partials that die quickly."""
    count = int(seconds * RATE)
    raw = noise(count, seed)
    out = []
    for index in range(count):
        t = index / float(RATE)
        tone = sum(math.sin(2.0 * math.pi * f * t) * math.exp(-d * t) for f, d in ((1850.0, 28.0), (2740.0, 36.0), (4120.0, 48.0), (640.0, 22.0)))
        out.append(tone * 0.25 + raw[index] * 0.5 * math.exp(-90.0 * t))
    return out


def pin(seconds, seed):
    """The pin pulled and the lever flying off: two ticks and a short ring."""
    count = int(seconds * RATE)
    raw = noise(count, seed)
    out = []
    for index in range(count):
        t = index / float(RATE)
        later = max(0.0, t - 0.09)
        out.append(raw[index] * 0.6 * math.exp(-120.0 * t) + (raw[index] * 0.5 * math.exp(-80.0 * later) + 0.3 * math.sin(2.0 * math.pi * 2300.0 * later) * math.exp(-30.0 * later)) * (1.0 if t >= 0.09 else 0.0))
    return out


def bang(seconds, seed):
    """The burst: a hard crack, a low body and a noisy tail."""
    count = int(seconds * RATE)
    raw = noise(count, seed)
    low = lowpass(raw, 0.02)
    out, phase = [], 0.0
    for index in range(count):
        t = index / float(RATE)
        phase += (95.0 * math.exp(-5.0 * t) + 38.0) / RATE
        out.append(raw[index] * math.exp(-45.0 * t) * 1.2 + math.sin(2.0 * math.pi * phase) * math.exp(-5.5 * t) + low[index] * 6.0 * math.exp(-3.2 * t))
    return out


def ringing(seconds):
    """The ears afterwards: a high tone with a slow beat that fades away."""
    count = int(seconds * RATE)
    out = []
    for index in range(count):
        t = index / float(RATE)
        fade = min(1.0, t * 30.0) * (1.0 - t / seconds) ** 1.6
        out.append((math.sin(2.0 * math.pi * 3950.0 * t) + 0.6 * math.sin(2.0 * math.pi * 3990.0 * t) + 0.25 * math.sin(2.0 * math.pi * 7900.0 * t)) * fade)
    return out


def write_wav(folder, name, samples):
    peak = max(0.001, max(abs(sample) for sample in samples))
    path = os.path.join(folder, name + '.wav')
    with wave.open(path, 'wb') as out:
        out.setnchannels(1)
        out.setsampwidth(2)
        out.setframerate(RATE)
        out.writeframes(b''.join(struct.pack('<h', int(max(-1.0, min(1.0, sample / peak * 0.9)) * 32767)) for sample in samples))
    return path


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


report = []
if not FINISH:
    for name, color, roughness, metallic in (('M_KnifeSteel', (0.66, 0.68, 0.72), 0.30, 0.55), ('M_KnifeGrip', (0.012, 0.012, 0.014), 0.72, 0.0), ('M_KnifeFittings', (0.10, 0.11, 0.12), 0.38, 1.0),
                                             ('M_FlashBody', (0.045, 0.060, 0.048), 0.55, 0.6), ('M_FlashBand', (0.42, 0.50, 0.56), 0.5, 0.3), ('M_FlashLever', (0.20, 0.21, 0.22), 0.34, 1.0)):
        material(name, color, roughness, metallic)
    folder = os.path.join(unreal.Paths.project_saved_dir(), 'EquipmentAudio')
    if not os.path.isdir(folder):
        os.makedirs(folder)
    sounds = {'S_KnifeSwing': swing(0.26, 3), 'S_KnifeStab': swing(0.38, 4), 'S_KnifeHit': knife_hit(0.3, 5), 'S_FlashPin': pin(0.35, 6),
              'S_FlashBounce': clink(0.35, 7), 'S_FlashBang': bang(1.4, 8), 'S_FlashRing': ringing(3.5)}
    for name, samples in sounds.items():
        if not LIB.does_asset_exist(AUDIO + '/' + name):
            import_file(write_wav(folder, name, samples), AUDIO, name)
    report.append('sounds=%d' % sum(1 for name in sounds if LIB.does_asset_exist(AUDIO + '/' + name)))
    for name, _ in MESHES:
        source = os.path.join(ART, 'SM_%s.fbx' % name)
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
        import_file(source, DEST, 'SM_' + name, ui)
        report.append('%s imported=%s' % (name, LIB.does_asset_exist(DEST + '/SM_' + name)))
else:
    for name, slots in MESHES:
        mesh = unreal.load_asset(DEST + '/SM_' + name)
        if not mesh:
            continue
        for index, slot in enumerate(slots):
            asset = unreal.load_asset(DEST + '/' + slot)
            if asset and index < len(mesh.get_editor_property('static_materials')):
                mesh.set_material(index, asset)
        saved = LIB.save_loaded_asset(mesh, only_if_is_dirty=False)
        bounds = mesh.get_bounds()
        report.append('%s saved=%s slots=%d size=(%.1f, %.1f, %.1f)' % (name, saved, len(mesh.get_editor_property('static_materials')), bounds.box_extent.x * 2, bounds.box_extent.y * 2, bounds.box_extent.z * 2))
unreal.log('EQUIPMENT %s %s' % ('FINISH' if FINISH else 'IMPORT', report))
