"""Build L_ZombieArena, the Zombie Survival map, its materials and sounds, and L_MainMenu, the menu map.

The arena is a closed hall, 84 x 84 m. A perimeter wall runs 8 m inside the shell; the corridor between
the two is where zombies spawn, out of sight, and they come in through twelve gates, three per side.
Inside the perimeter there are five areas: an open plaza in the middle where the player starts, a container
yard with narrow lanes in the north, a building with rooms and doorways in the east, a raised platform with
two ramps in the south and a hall of pillars in the west. A navigation bounds volume covers all of it; the
navigation mesh itself is generated when the map starts.

The sounds are synthesised here, written as WAV files and imported, so the project needs no audio sources.
"""
import math
import os
import random
import struct
import wave

import unreal

ARENA = '/Game/ArenaDuel/Maps/L_ZombieArena'
MENU = '/Game/ArenaDuel/Maps/L_MainMenu'
ASSETS = '/Game/ArenaDuel/Characters/Common'
AUDIO = '/Game/ArenaDuel/Audio'
LIB = unreal.EditorAssetLibrary
PREFIX = 'Survival_'
RATE = 22050
count = {'pieces': 0, 'spawns': 0, 'lights': 0}


def make_material(name, color, metallic, roughness, emissive=None):
    path = ASSETS + '/' + name
    if LIB.does_asset_exist(path):
        return unreal.load_asset(path)
    mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, ASSETS, unreal.Material, unreal.MaterialFactoryNew())
    edit = unreal.MaterialEditingLibrary
    base = edit.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector)
    base.set_editor_property('constant', unreal.LinearColor(*color))
    edit.connect_material_property(base, '', unreal.MaterialProperty.MP_BASE_COLOR)
    metal = edit.create_material_expression(mat, unreal.MaterialExpressionConstant)
    metal.set_editor_property('r', metallic)
    edit.connect_material_property(metal, '', unreal.MaterialProperty.MP_METALLIC)
    rough = edit.create_material_expression(mat, unreal.MaterialExpressionConstant)
    rough.set_editor_property('r', roughness)
    edit.connect_material_property(rough, '', unreal.MaterialProperty.MP_ROUGHNESS)
    if emissive:
        glow = edit.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector)
        glow.set_editor_property('constant', unreal.LinearColor(*emissive))
        edit.connect_material_property(glow, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    edit.recompile_material(mat)
    LIB.save_loaded_asset(mat)
    return mat


def box(label, centre, size, material, pitch=0.0, yaw=0.0):
    actor = unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(*centre), unreal.Rotator(roll=0.0, pitch=pitch, yaw=yaw))
    actor.set_actor_label('%s%s_%03d' % (PREFIX, label, count['pieces']))
    component = actor.static_mesh_component
    component.set_static_mesh(unreal.load_object(None, '/Engine/BasicShapes/Cube.Cube'))
    component.set_material(0, material)
    component.set_mobility(unreal.ComponentMobility.STATIC)
    component.set_collision_profile_name('BlockAll')
    actor.set_actor_scale3d(unreal.Vector(size[0] / 100.0, size[1] / 100.0, size[2] / 100.0))
    count['pieces'] += 1


def cube(label, x0, x1, y0, y1, z0, z1, material):
    box(label, ((x0 + x1) * 0.5, (y0 + y1) * 0.5, (z0 + z1) * 0.5), (x1 - x0, y1 - y0, z1 - z0), material)


def wall_with_gaps(label, axis, at, a0, a1, height, material, gaps, thickness=40.0):
    """A wall along one axis with openings. axis 'x' runs along x at y = at; gaps are (centre, width)."""
    cursor = a0
    for centre, width in sorted(gaps):
        end = centre - width * 0.5
        if end > cursor:
            piece(label, axis, at, cursor, end, height, material, thickness)
        cursor = centre + width * 0.5
    if a1 > cursor:
        piece(label, axis, at, cursor, a1, height, material, thickness)


def piece(label, axis, at, a0, a1, height, material, thickness):
    half = thickness * 0.5
    if axis == 'x':
        cube(label, a0, a1, at - half, at + half, 0, height, material)
    else:
        cube(label, at - half, at + half, a0, a1, 0, height, material)


def spawn_point(x, y):
    point = unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.TargetPoint, unreal.Vector(x, y, 20.0), unreal.Rotator(roll=0.0, pitch=0.0, yaw=0.0))
    point.set_actor_label('%sZombieSpawn_%02d' % (PREFIX, count['spawns']))
    point.set_editor_property('tags', [unreal.Name('ZombieSpawn')])
    count['spawns'] += 1


def light(x, y, z, intensity, radius, r, g, b):
    actor = unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.PointLight, unreal.Vector(x, y, z), unreal.Rotator())
    actor.set_actor_label('%sLight_%02d' % (PREFIX, count['lights']))
    component = actor.get_component_by_class(unreal.PointLightComponent)
    component.set_mobility(unreal.ComponentMobility.MOVABLE)
    component.set_cast_shadows(False)
    component.set_editor_property('intensity', intensity)
    component.set_editor_property('attenuation_radius', radius)
    component.set_editor_property('light_color', unreal.Color(r=r, g=g, b=b, a=255))
    component.set_editor_property('use_inverse_squared_falloff', True)
    count['lights'] += 1


def clear(prefix):
    for actor in unreal.EditorLevelLibrary.get_all_level_actors():
        if actor.get_actor_label().startswith(prefix):
            unreal.EditorLevelLibrary.destroy_actor(actor)


def set_game_mode(class_path):
    world = unreal.EditorLevelLibrary.get_editor_world()
    world.get_world_settings().set_editor_property('default_game_mode', unreal.load_class(None, class_path))
    return world


def build_zombie_materials():
    materials = [
        make_material('M_SurvivalZombie', (0.05, 0.16, 0.045, 1), 0.0, 0.7, (0.003, 0.016, 0.003, 1)),
        make_material('M_SurvivalRunner', (0.26, 0.22, 0.04, 1), 0.0, 0.6, (0.02, 0.016, 0.002, 1)),
        make_material('M_SurvivalArmoured', (0.16, 0.18, 0.21, 1), 0.9, 0.35, (0.001, 0.005, 0.008, 1)),
        make_material('M_SurvivalBrute', (0.34, 0.12, 0.03, 1), 0.1, 0.5, (0.05, 0.014, 0.002, 1)),
        make_material('M_SurvivalAbomination', (0.32, 0.03, 0.03, 1), 0.1, 0.45, (0.09, 0.005, 0.004, 1)),
    ]
    # They are drawn on a skinned mesh. Without this flag the engine falls back to the default material in game.
    for material in materials:
        if not material.get_editor_property('used_with_skeletal_mesh'):
            material.set_editor_property('used_with_skeletal_mesh', True)
            unreal.MaterialEditingLibrary.recompile_material(material)
            LIB.save_loaded_asset(material)
    # The droplets of a hit are instances of one sphere.
    blood = make_material('M_SurvivalBlood', (0.20, 0.004, 0.004, 1), 0.0, 0.25, (0.03, 0.0, 0.0, 1))
    if not blood.get_editor_property('used_with_instanced_static_meshes'):
        blood.set_editor_property('used_with_instanced_static_meshes', True)
        unreal.MaterialEditingLibrary.recompile_material(blood)
        LIB.save_loaded_asset(blood)


# --- Sounds -------------------------------------------------------------------------------------------

def lowpass(samples, strength):
    out, state = [], 0.0
    for sample in samples:
        state += strength * (sample - state)
        out.append(state)
    return out


def envelope(index, total, attack, release):
    t = index / float(total)
    if t < attack:
        return t / attack
    if t > 1.0 - release:
        return max(0.0, (1.0 - t) / release)
    return 1.0


def growl(seconds, f0, f1, flutter, noise_share, attack, release, seed):
    rng = random.Random(seed)
    total = int(seconds * RATE)
    noise = lowpass([rng.uniform(-1.0, 1.0) for _ in range(total)], 0.12)
    out, phase = [], 0.0
    for index in range(total):
        t = index / float(total)
        freq = f0 + (f1 - f0) * t
        phase += freq / RATE
        saw = 2.0 * (phase % 1.0) - 1.0
        rough = 0.6 + 0.4 * math.sin(2.0 * math.pi * flutter * index / RATE)
        body = saw * 0.5 + math.sin(2.0 * math.pi * phase * 0.5) * 0.5
        out.append((body * rough * (1.0 - noise_share) + noise[index] * 3.0 * noise_share) * envelope(index, total, attack, release))
    return lowpass(out, 0.35)


def whoosh(seconds, seed):
    rng = random.Random(seed)
    total = int(seconds * RATE)
    out, state = [], 0.0
    for index in range(total):
        t = index / float(total)
        strength = 0.05 + 0.5 * math.sin(math.pi * t)
        state += strength * (rng.uniform(-1.0, 1.0) - state)
        out.append(state * math.sin(math.pi * t) * 1.8)
    low = growl(seconds, 120.0, 75.0, 30.0, 0.2, 0.1, 0.5, seed + 1)
    return [a + 0.6 * b for a, b in zip(out, low)]


def thud(seconds, seed):
    rng = random.Random(seed)
    total = int(seconds * RATE)
    out, phase = [], 0.0
    for index in range(total):
        t = index / float(total)
        phase += (150.0 - 95.0 * t) / RATE
        decay = math.exp(-6.0 * t)
        click = rng.uniform(-1.0, 1.0) * math.exp(-40.0 * t) * 0.6
        out.append((math.sin(2.0 * math.pi * phase) + click) * decay)
    return out


def ambience(seconds, seed):
    """A low drone with slow wind. Every part completes whole cycles, so the loop has no seam."""
    rng = random.Random(seed)
    total = int(seconds * RATE)
    noise = lowpass(lowpass([rng.uniform(-1.0, 1.0) for _ in range(total)], 0.03), 0.03)
    fade = int(0.5 * RATE)
    for index in range(fade):
        share = index / float(fade)
        noise[index] = noise[index] * share + noise[total - fade + index] * (1.0 - share)
    out = []
    for index in range(total - fade):
        t = index / float(RATE)
        drone = 0.5 * math.sin(2.0 * math.pi * 48.0 * t) + 0.35 * math.sin(2.0 * math.pi * 72.25 * t) + 0.2 * math.sin(2.0 * math.pi * 96.5 * t)
        swell = 0.6 + 0.4 * math.sin(2.0 * math.pi * t / ((total - fade) / float(RATE)))
        out.append(drone * 0.35 * swell + noise[index] * 9.0 * (1.2 - swell))
    return out


def horn(seconds):
    total = int(seconds * RATE)
    out = []
    for index in range(total):
        t = index / float(RATE)
        freq = 82.4 if t < seconds * 0.5 else 61.7
        tone = sum(math.sin(2.0 * math.pi * freq * harmonic * t) / harmonic for harmonic in (1, 2, 3, 4, 5))
        local = (t % (seconds * 0.5)) / (seconds * 0.5)
        out.append(tone * min(1.0, local * 12.0) * (1.0 - local) ** 0.6)
    return lowpass(out, 0.25)


def write_wav(folder, name, samples):
    peak = max(0.001, max(abs(sample) for sample in samples))
    path = os.path.join(folder, name + '.wav')
    with wave.open(path, 'wb') as out:
        out.setnchannels(1)
        out.setsampwidth(2)
        out.setframerate(RATE)
        out.writeframes(b''.join(struct.pack('<h', int(max(-1.0, min(1.0, sample / peak * 0.85)) * 32767)) for sample in samples))
    return path


def build_sounds():
    folder = os.path.join(unreal.Paths.project_saved_dir(), 'SurvivalAudio')
    if not os.path.isdir(folder):
        os.makedirs(folder)
    sounds = {
        'S_ZombieGrowl': growl(0.9, 78.0, 58.0, 23.0, 0.25, 0.15, 0.4, 11),
        'S_ZombieAttack': whoosh(0.4, 21),
        'S_ZombieHit': thud(0.14, 31),
        'S_ZombieDeath': growl(1.2, 96.0, 38.0, 17.0, 0.35, 0.05, 0.7, 41),
        'S_SurvivalAmbience': ambience(8.5, 51),
        'S_SurvivalWaveStart': horn(1.4),
    }
    tasks = []
    for name, samples in sounds.items():
        if LIB.does_asset_exist(AUDIO + '/' + name):
            continue
        task = unreal.AssetImportTask()
        task.set_editor_property('filename', write_wav(folder, name, samples))
        task.set_editor_property('destination_path', AUDIO)
        task.set_editor_property('automated', True)
        task.set_editor_property('save', True)
        task.set_editor_property('replace_existing', True)
        tasks.append(task)
    if tasks:
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
    loop = unreal.load_asset(AUDIO + '/S_SurvivalAmbience')
    if loop and not loop.get_editor_property('looping'):
        loop.set_editor_property('looping', True)
        LIB.save_loaded_asset(loop)
    found = sum(1 for name in sounds if LIB.does_asset_exist(AUDIO + '/' + name))
    unreal.log('SURVIVAL SOUNDS PASS %d of %d' % (found, len(sounds)))
    return loop


# --- Arena --------------------------------------------------------------------------------------------

def build_arena(ambience_sound):
    if LIB.does_asset_exist(ARENA):
        unreal.EditorLevelLibrary.load_level(ARENA)
        clear(PREFIX)
    else:
        unreal.EditorLevelLibrary.new_level(ARENA)

    floor = make_material('M_ArenaCoreFloor', (0.055, 0.075, 0.105, 1), 0.35, 0.78)
    shell = make_material('M_ArenaCoreWall', (0.018, 0.026, 0.046, 1), 0.42, 0.68)
    ceiling = make_material('M_ArenaCoreCeiling', (0.009, 0.014, 0.026, 1), 0.28, 0.8)
    concrete = make_material('M_DistrictDarkConcrete', (0.030, 0.034, 0.042, 1), 0.10, 0.80)
    crate = make_material('M_DistrictDarkCrate', (0.055, 0.034, 0.020, 1), 0.15, 0.65)
    metal = make_material('M_DistrictDarkMetal', (0.016, 0.019, 0.026, 1), 0.85, 0.35)
    container = make_material('M_SurvivalContainer', (0.070, 0.026, 0.020, 1), 0.55, 0.55)
    deck = make_material('M_SurvivalDeck', (0.040, 0.046, 0.050, 1), 0.60, 0.50)

    half, inner = 4200, 3400

    # Shell: 8400 x 8400, floor at z = 0, ceiling at 1400.
    cube('Floor', -half, half, -half, half, -100, 0, floor)
    cube('Ceiling', -half, half, -half, half, 1400, 1500, ceiling)
    cube('Shell', -half, half, half, half + 100, 0, 1400, shell)
    cube('Shell', -half, half, -half - 100, -half, 0, 1400, shell)
    cube('Shell', -half - 100, -half, -half, half, 0, 1400, shell)
    cube('Shell', half, half + 100, -half, half, 0, 1400, shell)

    # Perimeter wall with three gates per side. The corridor behind it is where zombies appear.
    gates = [(-2000, 320), (0, 320), (2000, 320)]
    for side in (1, -1):
        wall_with_gaps('Perimeter', 'x', side * inner, -inner, inner, 340, concrete, gates)
        wall_with_gaps('Perimeter', 'y', side * inner, -inner, inner, 340, concrete, gates)
        for centre, _ in gates:
            light(centre, side * (inner + 60), 300, 55.0, 650.0, 255, 40, 25)
            light(side * (inner + 60), centre, 300, 55.0, 650.0, 255, 40, 25)

    # Spawn points in the corridor, beside the gates rather than behind them, plus the four corners.
    for side in (1, -1):
        for offset in (-3000, -1000, 1000, 3000):
            spawn_point(offset, side * 3800)
            spawn_point(side * 3800, offset)
    for sx in (1, -1):
        for sy in (1, -1):
            spawn_point(sx * 3800, sy * 3800)

    # Plaza, the open middle: a block to circle, four low walls to vault and four crates.
    cube('Centre', -70, 70, -70, 70, 0, 110, metal)
    for sx in (1, -1):
        for sy in (1, -1):
            cube('PlazaLow', sx * 620 - 150, sx * 620 + 150, sy * 620 - 20, sy * 620 + 20, 0, 80, concrete)
    for x, y in ((950, 0), (-950, 0), (0, 950), (0, -1000)):
        cube('PlazaCrate', x - 70, x + 70, y - 70, y + 70, 0, 140, crate)

    # North: the container yard. Two rows of containers with lanes between them and gaps to cut through.
    for x0, x1 in ((-2700, -1800), (-1300, -400), (400, 1300), (1800, 2700)):
        cube('Container', x0, x1, 1500, 1740, 0, 260, container)
    for x0, x1 in ((-2000, -1100), (-450, 450), (1100, 2000)):
        cube('Container', x0, x1, 2350, 2590, 0, 260, container)
    cube('Container', -3000, -2760, 1900, 2800, 0, 260, container)
    cube('Container', 2760, 3000, 1900, 2800, 0, 260, container)
    for x in (-800, 800):
        cube('YardCrate', x - 70, x + 70, 2000, 2140, 0, 140, crate)

    # East: the building. Rooms and doorways, the tightest part of the map.
    wall_with_gaps('Building', 'y', 1750, -2700, 1000, 320, concrete, [(-1900, 220), (-300, 220), (600, 220)])
    wall_with_gaps('Building', 'y', 3050, -2700, 1000, 320, concrete, [(-1100, 220), (300, 220)])
    wall_with_gaps('Building', 'x', -2700, 1750, 3050, 320, concrete, [(2400, 220)])
    wall_with_gaps('Building', 'x', 1000, 1750, 3050, 320, concrete, [(2100, 220), (2800, 220)])
    wall_with_gaps('Building', 'x', -1000, 1750, 3050, 320, concrete, [(2850, 260)])
    wall_with_gaps('Building', 'x', 0, 1750, 3050, 320, concrete, [(2000, 260)])
    cube('RoomCrate', 2350, 2490, -1900, -1760, 0, 140, crate)
    cube('RoomCrate', 2450, 2590, 400, 540, 0, 140, crate)

    # South: a raised deck that is reached over two ramps or by climbing its front edge.
    cube('Deck', -1000, 1000, -2800, -1800, 0, 130, deck)
    cube('DeckRail', -1000, -300, -1820, -1800, 130, 200, metal)
    cube('DeckRail', 300, 1000, -1820, -1800, 130, 200, metal)
    cube('DeckCover', -120, 120, -2500, -2400, 130, 250, crate)
    angle = math.degrees(math.atan2(130.0, 520.0))
    length = math.hypot(520.0, 130.0)
    nx, nz = -math.sin(math.radians(angle)), math.cos(math.radians(angle))
    for side in (1, -1):
        box('Ramp', (side * (-1260 - nx * 10.0), -2300, 65.0 - nz * 10.0), (length, 420.0, 20.0), deck, pitch=side * angle)
    cube('SouthCrate', -1900, -1760, -2300, -2160, 0, 140, crate)
    cube('SouthCrate', 1100, 1240, -1500, -1360, 0, 140, crate)

    # West: a hall of pillars for quick peeks, with two head-high blocks.
    for x in (-1900, -2450, -3000):
        for y in (-2300, -1500, -700, 100, 900):
            cube('Pillar', x - 60, x + 60, y - 60, y + 60, 0, 320, metal)
    cube('WestBlock', -2150, -1950, -1150, -1050, 0, 210, concrete)
    cube('WestBlock', -2750, -2550, 450, 550, 0, 210, concrete)

    # The game mode looks for these two tags. The second start is there for a later co-op run.
    for x, name in ((-160, 'P1'), (160, 'P2')):
        start = unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(x, -260, 100), unreal.Rotator(roll=0.0, pitch=0.0, yaw=90.0))
        start.set_actor_label(PREFIX + 'PlayerStart_' + name)
        start.set_editor_property('player_start_tag', 'ArenaCore_' + name)

    # Light: cold and dim everywhere, a warmer tone over the yard, red at the gates.
    for x in (-3000, -1000, 1000, 3000):
        for y in (-3000, -1000, 1000, 3000):
            if y > 1000:
                light(x, y, 1000, 150.0, 2600.0, 235, 205, 170)
            else:
                light(x, y, 1000, 150.0, 2600.0, 195, 208, 230)

    fog = unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.ExponentialHeightFog, unreal.Vector(0, 0, 100), unreal.Rotator())
    fog.set_actor_label(PREFIX + 'Fog')
    fog_component = fog.get_component_by_class(unreal.ExponentialHeightFogComponent)
    fog_component.set_editor_property('fog_density', 0.012)
    fog_component.set_editor_property('fog_height_falloff', 0.05)
    fog_component.set_editor_property('fog_inscattering_luminance', unreal.LinearColor(0.010, 0.016, 0.020, 1.0))

    if ambience_sound:
        ambient = unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.AmbientSound, unreal.Vector(0, 0, 400), unreal.Rotator())
        ambient.set_actor_label(PREFIX + 'Ambience')
        audio = ambient.get_component_by_class(unreal.AudioComponent)
        audio.set_editor_property('sound', ambience_sound)
        audio.set_editor_property('volume_multiplier', 0.45)
        audio.set_editor_property('is_ui_sound', True)

    # Navigation: one bounds volume over the whole floor. The mesh is generated at runtime (DefaultEngine.ini).
    volume = unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.NavMeshBoundsVolume, unreal.Vector(0, 0, 300), unreal.Rotator())
    volume.set_actor_label(PREFIX + 'NavBounds')
    volume.set_actor_scale3d(unreal.Vector(43.0, 43.0, 5.0))

    world = set_game_mode('/Script/ArenaDuel.ArenaDuelZombieGameMode')
    if not unreal.EditorLoadingAndSavingUtils.save_map(world, ARENA):
        raise RuntimeError('Failed to save ' + ARENA)
    unreal.log('SURVIVAL ARENA PASS pieces=%d spawns=%d lights=%d' % (count['pieces'], count['spawns'], count['lights']))


def build_menu():
    if LIB.does_asset_exist(MENU):
        unreal.EditorLevelLibrary.load_level(MENU)
    else:
        unreal.EditorLevelLibrary.new_level(MENU)
    world = set_game_mode('/Script/ArenaDuel.ArenaDuelMenuGameMode')
    if not unreal.EditorLoadingAndSavingUtils.save_map(world, MENU):
        raise RuntimeError('Failed to save ' + MENU)
    unreal.log('MAIN MENU MAP PASS')


build_zombie_materials()
build_arena(build_sounds())
build_menu()
