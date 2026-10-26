"""Build L_ArenaDistrict, a 1v1 map: spawn cover, a windowed centre house with a roof, a parkour lane and a cover lane.

The layout is mirrored across X = 0, so both players get the same geometry. Sizes follow the movement
component: 80 cm cover can be vaulted and crouched behind, steps of at most 140 cm can be mantled,
gaps of 350 cm are a sprint jump. Walls, decks and pillars are engine cubes, like L_ArenaCore; the
crates are the modelled crate from Tools/Editor/SetupProps.py, scaled to the size of the block they stand for.

Look: the surfaces use the textured materials of Tools/Editor/SetupDistrictMaterials.py (run that first):
cast concrete, sealed floor, painted render on the house, tread plate on decks and bridge, a steel deck
ceiling. Everything that is only there to be looked at (lamps, wall caps, painted lines, edge trims, the
neon, ivy) is placed by decor() and prop() and has no collision, so cover, routes and traces are what the
layout says and nothing else.

Light follows the lamps: every light in the hall sits at a lamp that can be seen, every lamp carries a light,
and a shaded lamp only lights what is below its shade.
"""
import unreal

MAP = '/Game/ArenaDuel/Maps/L_ArenaDistrict'
ASSETS = '/Game/ArenaDuel/Characters/Common'
CRATE = '/Game/ArenaDuel/Props/SM_Crate'
DISTRICT = '/Game/ArenaDuel/Maps/District'
PROPS = DISTRICT + '/Props'
LOOK = {}
LIB = unreal.EditorAssetLibrary
PREFIX = 'District_'
T = 40.0  # wall thickness
# Brightness of the hall: the fill light of each panel and the shadow-casting spots, in the engine's light
# units, and the fixed exposure the picture is developed with.
FILL_INTENSITY = 700.0
KEY_INTENSITY = 4600.0
SCONCE_INTENSITY = 55.0
HALL_LAMP_INTENSITY = 260.0
NEON_INTENSITY = 26.0
AMBIENT_INTENSITY = 4.0
EXPOSURE = 1.0
# A rectangular LED bar spreads its light over a half space, a shaded lamp's cone over far less: the same
# lamp values times this give about the same brightness on the floor in front of the bar.
LED_SCALE = 2.4
count = {'pieces': 0, 'windows': 0, 'decor': 0, 'lights': 0}


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


def cube(label, x0, x1, y0, y1, z0, z1, material, yaw=0.0):
    """Axis aligned box given by its extents, optionally turned about its centre."""
    if x1 - x0 <= 0.5 or y1 - y0 <= 0.5 or z1 - z0 <= 0.5:
        return
    if material and material.get_name() == 'M_DistrictDarkCrate' and LIB.does_asset_exist(PROPS + '/SM_DistrictCrate'):
        # The timber crate of Tools/Art/build_district_props.py, scaled to the block's exact size: its collision
        # is one box of that size, so cover heights and gaps stay what they were. A tall block is a stack.
        stack = max(1, int(round((z1 - z0) / max(x1 - x0, y1 - y0))))
        height = (z1 - z0) / stack
        for level in range(stack):
            centre = unreal.Vector((x0 + x1) * 0.5, (y0 + y1) * 0.5, z0 + height * (level + 0.5))
            # Every second crate of a stack is turned a quarter, so the braces do not all run the same way.
            turned = (level + count['pieces']) % 2 == 1 and abs(yaw) < 0.5
            actor = unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.StaticMeshActor, centre, unreal.Rotator(roll=0.0, pitch=0.0, yaw=yaw + (90.0 if turned else 0.0)))
            actor.set_actor_label('%s%s_%03d' % (PREFIX, label, count['pieces']))
            component = actor.static_mesh_component
            component.set_static_mesh(unreal.load_asset(PROPS + '/SM_DistrictCrate'))
            component.set_mobility(unreal.ComponentMobility.STATIC)
            component.set_collision_profile_name('BlockAll')
            size = ((y1 - y0), (x1 - x0)) if turned else ((x1 - x0), (y1 - y0))
            actor.set_actor_scale3d(unreal.Vector(size[0] / 140.0, size[1] / 140.0, height / 140.0))
            count['pieces'] += 1
        return
    if material and material.get_name() == 'M_DistrictDarkCrate' and LIB.does_asset_exist(CRATE):
        # Every crate-coloured block becomes the modelled crate, scaled to the block's exact size, so cover
        # heights, gaps and collision stay what they were. A block about twice as tall as wide is two crates.
        stack = max(1, int(round((z1 - z0) / max(x1 - x0, y1 - y0))))
        height = (z1 - z0) / stack
        for level in range(stack):
            centre = unreal.Vector((x0 + x1) * 0.5, (y0 + y1) * 0.5, z0 + height * (level + 0.5))
            actor = unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.StaticMeshActor, centre, unreal.Rotator(roll=0.0, pitch=0.0, yaw=yaw))
            actor.set_actor_label('%s%s_%03d' % (PREFIX, label, count['pieces']))
            component = actor.static_mesh_component
            component.set_static_mesh(unreal.load_asset(CRATE))
            # Timber panels in a dark steel frame.
            if LOOK.get('wood'):
                component.set_material(0, LOOK['wood'])
            if LOOK.get('cap'):
                component.set_material(1, LOOK['cap'])
            component.set_mobility(unreal.ComponentMobility.STATIC)
            component.set_collision_profile_name('BlockAll')
            actor.set_actor_scale3d(unreal.Vector((x1 - x0) / 140.0, (y1 - y0) / 140.0, height / 140.0))
            count['pieces'] += 1
        return
    centre = unreal.Vector((x0 + x1) * 0.5, (y0 + y1) * 0.5, (z0 + z1) * 0.5)
    actor = unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.StaticMeshActor, centre, unreal.Rotator(roll=0.0, pitch=0.0, yaw=yaw))
    actor.set_actor_label('%s%s_%03d' % (PREFIX, label, count['pieces']))
    component = actor.static_mesh_component
    component.set_static_mesh(unreal.load_object(None, '/Engine/BasicShapes/Cube.Cube'))
    component.set_material(0, material)
    component.set_mobility(unreal.ComponentMobility.STATIC)
    component.set_collision_profile_name('BlockAll')
    actor.set_actor_scale3d(unreal.Vector((x1 - x0) / 100.0, (y1 - y0) / 100.0, (z1 - z0) / 100.0))
    count['pieces'] += 1


def decor(label, x0, x1, y0, y1, z0, z1, material, shadow=True, mirror=False, roll=0.0, pitch=0.0):
    """A box that is only looked at: no collision, so it changes no cover, no route and no trace."""
    if not material or x1 - x0 <= 0.05 or y1 - y0 <= 0.05 or z1 - z0 <= 0.05:
        return
    centre = unreal.Vector((x0 + x1) * 0.5, (y0 + y1) * 0.5, (z0 + z1) * 0.5)
    actor = unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.StaticMeshActor, centre, unreal.Rotator(roll=roll, pitch=pitch, yaw=0.0))
    actor.set_actor_label('%sDecor_%s_%03d' % (PREFIX, label, count['decor']))
    component = actor.static_mesh_component
    component.set_static_mesh(unreal.load_object(None, '/Engine/BasicShapes/Cube.Cube'))
    component.set_material(0, material)
    component.set_mobility(unreal.ComponentMobility.STATIC)
    component.set_collision_profile_name('NoCollision')
    component.set_cast_shadow(shadow)
    actor.set_actor_scale3d(unreal.Vector((x1 - x0) / 100.0, (y1 - y0) / 100.0, (z1 - z0) / 100.0))
    count['decor'] += 1
    if mirror and not abs(x0 + x1) < 0.5:
        decor(label, -x1, -x0, y0, y1, z0, z1, material, shadow, False, -roll, -pitch)


def prop(name, x, y, z, yaw=0.0, scale=1.0, collide=False, mirror=False, shadow=True):
    """A modelled piece from Tools/Art/build_district_props.py. Without it (a checkout that has not imported
    the props) nothing is placed and the caller falls back to boxes. Mirrored pieces turn to face the other way."""
    path = PROPS + '/' + name
    if not LIB.does_asset_exist(path):
        return False
    actor = unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(x, y, z), unreal.Rotator(roll=0.0, pitch=0.0, yaw=yaw))
    actor.set_actor_label('%sProp_%s_%03d' % (PREFIX, name, count['decor']))
    component = actor.static_mesh_component
    component.set_static_mesh(unreal.load_asset(path))
    component.set_mobility(unreal.ComponentMobility.STATIC)
    component.set_collision_profile_name('BlockAll' if collide else 'NoCollision')
    component.set_cast_shadow(shadow)
    actor.set_actor_scale3d(unreal.Vector(scale, scale, scale))
    count['decor'] += 1
    if mirror and abs(x) > 0.5:
        prop(name, -x, y, z, 180.0 - yaw, scale, collide, False, shadow)
    return True


def tube_light(label, x, y, z, color, intensity, radius, length):
    """The light of a neon strip: one light as long as its piece of the strip, so the wall and the floor are
    lit evenly along it instead of in one spot. The strip runs along Y."""
    light = unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.PointLight, unreal.Vector(x, y, z), unreal.Rotator(roll=90.0, pitch=0.0, yaw=0.0))
    light.set_actor_label('%s%s_%03d' % (PREFIX, label, count['lights']))
    component = light.get_component_by_class(unreal.PointLightComponent)
    component.set_mobility(unreal.ComponentMobility.MOVABLE)
    component.set_cast_shadows(False)
    component.set_editor_property('intensity', intensity)
    component.set_editor_property('attenuation_radius', radius)
    component.set_editor_property('source_radius', 4.0)
    component.set_editor_property('source_length', length)
    component.set_editor_property('light_color', unreal.Color(r=color[0], g=color[1], b=color[2], a=255))
    component.set_editor_property('use_inverse_squared_falloff', True)
    count['lights'] += 1


def down_light(label, x, y, z, color, intensity, radius, mirror=False):
    """The light of a shaded lamp: a cone straight down from under the shade. Nothing above the lamp is lit."""
    light = unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.SpotLight, unreal.Vector(x, y, z), unreal.Rotator(roll=0.0, pitch=-90.0, yaw=0.0))
    light.set_actor_label('%s%s_%03d' % (PREFIX, label, count['lights']))
    component = light.get_component_by_class(unreal.SpotLightComponent)
    component.set_mobility(unreal.ComponentMobility.MOVABLE)
    component.set_cast_shadows(False)
    component.set_editor_property('intensity', intensity)
    component.set_editor_property('attenuation_radius', radius)
    component.set_editor_property('outer_cone_angle', 66.0)
    component.set_editor_property('inner_cone_angle', 26.0)
    component.set_editor_property('source_radius', 6.0)
    component.set_editor_property('light_color', unreal.Color(r=color[0], g=color[1], b=color[2], a=255))
    component.set_editor_property('use_inverse_squared_falloff', True)
    count['lights'] += 1
    if mirror and abs(x) > 0.5:
        down_light(label, -x, y, z, color, intensity, radius, False)


def led_light(label, x, y, z, nx, ny, width, height, intensity, radius):
    """The light of a recessed LED bar: a rectangular light as large as the bar, sitting on its face and tilted
    down, so it washes the wall below the bar and the floor in front of it like a real wall-mounted LED."""
    import math
    yaw = math.degrees(math.atan2(ny, nx))
    light = unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.RectLight, unreal.Vector(x + nx * 3.0, y + ny * 3.0, z), unreal.Rotator(roll=0.0, pitch=-38.0, yaw=yaw))
    light.set_actor_label('%s%s_%03d' % (PREFIX, label, count['lights']))
    component = light.get_component_by_class(unreal.RectLightComponent)
    component.set_mobility(unreal.ComponentMobility.MOVABLE)
    component.set_cast_shadows(False)
    component.set_editor_property('intensity', intensity)
    component.set_editor_property('attenuation_radius', radius)
    component.set_editor_property('source_width', width)
    component.set_editor_property('source_height', height)
    component.set_editor_property('barn_door_angle', 70.0)
    component.set_editor_property('barn_door_length', 6.0)
    component.set_editor_property('light_color', unreal.Color(r=255, g=214, b=168, a=255))
    count['lights'] += 1


def sconce(x, y, nx, ny, z=255.0, mirror=True, scale=1.0, intensity=None, radius=560.0):
    """A recessed LED bar set into the wall face at (x, y), facing along (nx, ny): a dark aluminium frame flush
    with the wall, a warm diffuser sunk a few millimetres into it, and its light. No collision, like all decor.

    scale 1 is the 120 cm bar of the cover walls and the house, scale 2 the 260 cm bar of the hall walls."""
    length = 120.0 if scale < 1.5 else 260.0
    tall = 7.0 if scale < 1.5 else 10.0
    frame, lens = 1.6, 1.0
    # Along the wall: the axis that is not the facing axis.
    along_x = abs(ny) > abs(nx)
    for (sx, sy) in ((x, y), (-x, y)) if mirror and abs(x) > 0.5 else ((x, y),):
        fx = nx if (sx, sy) == (x, y) else -nx
        half = length * 0.5
        def slab(label, depth, extra, height, material):
            # A box from the wall face out to depth, extra wider than the bar on every side.
            if along_x:
                y0, y1 = (sy, sy + ny * depth) if ny > 0 else (sy + ny * depth, sy)
                decor(label, sx - half - extra, sx + half + extra, y0, y1, z - height * 0.5 - extra, z + height * 0.5 + extra, material, shadow=False)
            else:
                x0, x1 = (sx, sx + fx * depth) if fx > 0 else (sx + fx * depth, sx)
                decor(label, x0, x1, sy - half - extra, sy + half + extra, z - height * 0.5 - extra, z + height * 0.5 + extra, material, shadow=False)
        slab('LedFrame', frame, 1.8, tall, LOOK['cap'])
        slab('LedLens', frame + 0.2, 0.0, tall, LOOK['led'])
        led_light('LedLight', sx, sy, z, fx, ny, length, tall, (intensity or SCONCE_INTENSITY) * LED_SCALE, radius)


def both(label, x0, x1, y0, y1, z0, z1, material, yaw=0.0):
    """The piece and its mirror image across X = 0."""
    cube(label, x0, x1, y0, y1, z0, z1, material, yaw)
    if not (abs(x0 + x1) < 0.5 and abs(yaw) < 0.5):
        cube(label, -x1, -x0, y0, y1, z0, z1, material, -yaw)


def wall(label, axis, at, a0, a1, height, material, openings, mirror=True):
    """A wall with doors and windows. axis 'y': the wall stands at x = at and runs along y; axis 'x' the other way.

    openings: (from, to, bottom, top) along the run. A door starts at 0, a window has a sill.
    """
    place = both if mirror else cube

    def part(b0, b1, z0, z1):
        if axis == 'y':
            place(label, at - T * 0.5, at + T * 0.5, b0, b1, z0, z1, material)
        else:
            place(label, b0, b1, at - T * 0.5, at + T * 0.5, z0, z1, material)

    cursor = a0
    for (o0, o1, bottom, top) in sorted(openings):
        part(cursor, o0, 0.0, height)
        part(o0, o1, 0.0, bottom)
        part(o0, o1, top, height)
        if bottom > 0.0:
            count['windows'] += 2 if mirror and not (axis == 'x' and abs(o0 + o1) < 0.5) else 1
        cursor = o1
    part(cursor, a1, 0.0, height)
    # A dark steel cap along the top edge, a little wider than the wall.
    cap = LOOK.get('cap')
    if axis == 'y':
        decor(label + 'Cap', at - T * 0.5 - 5, at + T * 0.5 + 5, a0 - 5, a1 + 5, height, height + 10, cap, mirror=mirror)
    else:
        decor(label + 'Cap', a0 - 5, a1 + 5, at - T * 0.5 - 5, at + T * 0.5 + 5, height, height + 10, cap, mirror=mirror)


def window(a, width=220.0):
    return (a - width * 0.5, a + width * 0.5, 110.0, 235.0)


def door(a, width=200.0):
    return (a - width * 0.5, a + width * 0.5, 0.0, 245.0)


def spawn_lights():
    """Rows of linear lights under the ceiling. Each one carries a shadowless fill light, which
    keeps the hall evenly readable; a few shadow-casting spots over the main areas give objects a footing."""
    index = 0
    for x in (-2800, -1400, 0, 1400, 2800):
        for y in (-1700, 0, 1700):
            if not prop('SM_CeilingLamp', x, y, 1500.0, shadow=False):
                decor('LightPanel', x - 170, x + 170, y - 38, y + 38, 1488, 1499, LOOK.get('panel'), shadow=False)
                decor('LightFrame', x - 180, x + 180, y - 48, y + 48, 1494, 1500, LOOK.get('cap'), shadow=False)
            light = unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.PointLight, unreal.Vector(x, y, 1380), unreal.Rotator())
            light.set_actor_label('%sFillLight_%02d' % (PREFIX, index))
            component = light.get_component_by_class(unreal.PointLightComponent)
            component.set_mobility(unreal.ComponentMobility.MOVABLE)
            component.set_cast_shadows(False)
            component.set_editor_property('intensity', FILL_INTENSITY)
            component.set_editor_property('attenuation_radius', 2300.0)
            component.set_editor_property('source_radius', 60.0)
            # unreal.Color takes b, g, r, a by position, so name the channels. Cool, like daylight from high windows;
            # the warmth comes from the wall lamps.
            component.set_editor_property('light_color', unreal.Color(r=214, g=228, b=255, a=255))
            component.set_editor_property('use_inverse_squared_falloff', True)
            index += 1
    # Shadow casters: straight down over both halves of the field and the house. Three, not more: every one of
    # them is rendered into shadow maps each frame.
    # Each hangs at a ceiling light of its own, turned across the rows so it reads as a different fitting.
    for number, (x, y) in enumerate(((-2000, 0), (2000, 0), (0, 0))):
        if abs(x) > 0.5:
            prop('SM_CeilingLamp', x, y, 1500.0, yaw=90.0, shadow=False)
        spot = unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.SpotLight, unreal.Vector(x, y, 1470), unreal.Rotator(roll=0.0, pitch=-90.0, yaw=0.0))
        spot.set_actor_label('%sKeyLight_%02d' % (PREFIX, number))
        component = spot.get_component_by_class(unreal.SpotLightComponent)
        component.set_mobility(unreal.ComponentMobility.MOVABLE)
        component.set_cast_shadows(True)
        component.set_editor_property('intensity', KEY_INTENSITY)
        component.set_editor_property('attenuation_radius', 3400.0)
        component.set_editor_property('outer_cone_angle', 72.0)
        component.set_editor_property('inner_cone_angle', 38.0)
        component.set_editor_property('source_radius', 40.0)
        component.set_editor_property('light_color', unreal.Color(r=255, g=250, b=240, a=255))
        component.set_editor_property('use_inverse_squared_falloff', True)


def spawn_ambient():
    """A sky light that captures the hall itself: with bounced light switched off, this is what keeps a face
    that looks away from every lamp from going black. It casts no shadow and costs next to nothing."""
    sky = unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0, 0, 700), unreal.Rotator())
    sky.set_actor_label(PREFIX + 'Ambient')
    component = sky.get_component_by_class(unreal.SkyLightComponent)
    component.set_mobility(unreal.ComponentMobility.MOVABLE)
    component.set_editor_property('source_type', unreal.SkyLightSourceType.SLS_CAPTURED_SCENE)
    component.set_editor_property('real_time_capture', False)
    component.set_editor_property('intensity', AMBIENT_INTENSITY)
    component.set_cast_shadows(False)


def spawn_post_process():
    """One unbound volume: a fixed exposure so the picture does not pump when looking from dark to light, a
    little bloom on the light panels, contact shading, and nothing that hides a player."""
    volume = unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.PostProcessVolume, unreal.Vector(0, 0, 400), unreal.Rotator())
    volume.set_actor_label(PREFIX + 'PostProcess')
    volume.set_editor_property('unbound', True)
    settings = volume.get_editor_property('settings')
    for key, value in (('auto_exposure_min_brightness', EXPOSURE), ('auto_exposure_max_brightness', EXPOSURE), ('auto_exposure_bias', 0.0),
                       ('bloom_intensity', 0.7), ('vignette_intensity', 0.25), ('ambient_occlusion_intensity', 0.6), ('ambient_occlusion_radius', 120.0),
                       ('motion_blur_amount', 0.0), ('lens_flare_intensity', 0.0), ('film_grain_intensity', 0.0),
                       # Reflections on the wet floor at reduced quality.
                       ('lumen_reflection_quality', 0.5)):
        try:
            settings.set_editor_property('override_' + key, True)
            settings.set_editor_property(key, value)
        except Exception as error:
            unreal.log_warning('Post process setting %s not applied: %s' % (key, error))
    # Bounced light is switched off for this map. Measured in a two player editor session it was a quarter of
    # the whole picture's cost, and a hall lit by this many lamps and fills holds up without it.
    try:
        settings.set_editor_property('override_dynamic_global_illumination_method', True)
        settings.set_editor_property('dynamic_global_illumination_method', unreal.DynamicGlobalIlluminationMethod.NONE)
    except Exception as error:
        unreal.log_warning('Post process setting dynamic_global_illumination_method not applied: %s' % error)
    volume.set_editor_property('settings', settings)


def build():
    if LIB.does_asset_exist(MAP):
        unreal.EditorLevelLibrary.load_level(MAP)
        for actor in unreal.EditorLevelLibrary.get_all_level_actors():
            if actor.get_actor_label().startswith(PREFIX):
                unreal.EditorLevelLibrary.destroy_actor(actor)
    else:
        unreal.EditorLevelLibrary.new_level(MAP)

    def look(name, fallback):
        """A textured material of SetupDistrictMaterials.py; the plain one only in a checkout without them."""
        asset = unreal.load_asset(DISTRICT + '/' + name) if LIB.does_asset_exist(DISTRICT + '/' + name) else None
        return asset or fallback()

    # Light, neutral surfaces: concrete walls, a mid grey sealed floor, warm render on the house. A dark figure
    # stands out against all of them, and the two team colours are the only saturated things in the hall.
    floor = look('MI_District_Floor', lambda: make_material('M_ArenaCoreFloor', (0.055, 0.075, 0.105, 1), 0.35, 0.78))
    shell = look('MI_District_Shell', lambda: make_material('M_ArenaCoreWall', (0.018, 0.026, 0.046, 1), 0.42, 0.68))
    ceiling = look('MI_District_Ceiling', lambda: make_material('M_ArenaCoreCeiling', (0.009, 0.014, 0.026, 1), 0.28, 0.8))
    concrete = look('MI_District_Concrete', lambda: make_material('M_DistrictDarkConcrete', (0.030, 0.034, 0.042, 1), 0.10, 0.80))
    house = look('MI_District_House', lambda: make_material('M_DistrictDarkHouse', (0.040, 0.030, 0.028, 1), 0.10, 0.75))
    # The crate material is only the marker cube() replaces with the modelled crate.
    crate = make_material('M_DistrictDarkCrate', (0.055, 0.034, 0.020, 1), 0.15, 0.65)
    metal = look('MI_District_Tread', lambda: make_material('M_DistrictDarkMetal', (0.016, 0.019, 0.026, 1), 0.85, 0.35))
    cyan = look('M_DistrictTeamCyan', lambda: make_material('M_DistrictDimCyan', (0.004, 0.03, 0.045, 1), 0.3, 0.5, (0.002, 0.02, 0.032, 1)))
    violet = look('M_DistrictTeamViolet', lambda: make_material('M_DistrictDimViolet', (0.028, 0.006, 0.045, 1), 0.3, 0.5, (0.016, 0.003, 0.032, 1)))
    LOOK['cap'] = look('MI_District_SteelDark', lambda: None)
    LOOK['panel'] = look('M_DistrictLightPanel', lambda: None)
    yellow = look('MI_District_SteelYellow', lambda: None)
    paint = look('MI_District_PaintWhite', lambda: None)
    orange = look('MI_District_PaintOrange', lambda: None)
    LOOK['sconce'] = look('M_DistrictSconce', lambda: None)
    # Warm white LED diffuser: bright enough to bloom a little, not so bright that it burns out to white.
    LOOK['led'] = make_material('M_DistrictLedStrip', (0.9, 0.78, 0.62, 1), 0.0, 0.4, (9.0, 6.6, 4.4, 1))
    LOOK['wood'] = look('MI_District_Wood', lambda: None)
    wood = LOOK['wood'] or concrete

    # Shell: 7000 x 5000 interior, floor at z = 0, ceiling at z = 1500.
    cube('Floor', -3500, 3500, -2500, 2500, -100, 0, floor)
    cube('Ceiling', -3500, 3500, -2500, 2500, 1500, 1600, ceiling)
    cube('Shell_North', -3500, 3500, 2500, 2600, 0, 1500, shell)
    cube('Shell_South', -3500, 3500, -2600, -2500, 0, 1500, shell)
    cube('Shell_West', -3600, -3500, -2500, 2500, 0, 1500, shell)
    cube('Shell_East', 3500, 3600, -2500, 2500, 0, 1500, shell)

    # --- Spawn cover: no line of sight from spawn to spawn. A window in the middle, a door on each side.
    wall('SpawnWall', 'y', -2500, -950, 950, 330, concrete, [door(-600), window(0), door(600)])
    # Team colour strips on the spawn walls, cyan for player one, violet for player two.
    cube('SpawnStrip', -2522, -2478, -950, 950, 330, 336, cyan)
    cube('SpawnStrip', 2478, 2522, -950, 950, 330, 336, violet)
    # Wing walls so the doors are peeked, not walked through in the open.
    wall('SpawnWing', 'x', 950, -2900, -2500, 330, concrete, [])
    wall('SpawnWing', 'x', -950, -2900, -2500, 330, concrete, [])

    # --- Centre house: windows towards both spawns, doors north and south, a roof with parapets.
    wall('HouseWest', 'y', -600, -450, 450, 330, house, [window(-230), window(230)])
    wall('HouseNorth', 'x', 450, -600, 600, 330, house, [window(-380), door(0), window(380)], mirror=False)
    wall('HouseSouth', 'x', -450, -600, 600, 330, house, [window(-380), door(0), window(380)], mirror=False)
    cube('HouseRoof', -620, 620, -470, 470, 330, 350, metal)
    both('RoofParapet', -620, -590, -470, -120, 350, 435, wood)
    both('RoofParapet', -620, -590, 120, 470, 350, 435, wood)
    cube('RoofBlock', -90, 90, -90, 90, 350, 500, crate)
    # Inside: a pillar to dance around and a crate under each west and east window.
    cube('HousePillar', -60, 60, -60, 60, 0, 330, concrete)
    both('HouseCrate', -540, -420, -300, -160, 0, 80, crate)
    # Way up to the roof at the north-west and north-east corners: 135, 270, then the roof edge at 350.
    both('RoofStep', -800, -640, 520, 680, 0, 135, crate)
    both('RoofStep', -800, -640, 340, 500, 0, 270, crate)

    # --- Field between spawn and house: pillars, head-high boxes and low walls for quick peeks.
    for (x, y) in ((-2050, -330), (-2050, 330), (-1500, 0), (-1250, -700), (-1250, 700)):
        both('Pillar', x - 55, x + 55, y - 55, y + 55, 0, 330, concrete)
    both('HeadBox', -1850, -1650, -620, -500, 0, 140, crate)
    both('HeadBox', -1850, -1650, 500, 620, 0, 140, crate)
    both('LowWall', -1020, -980, -330, -80, 0, 80, wood)
    both('LowWall', -1020, -980, 80, 330, 0, 80, wood)
    both('AngleWall', -1750, -1350, 930, 970, 0, 300, concrete, yaw=40.0)
    both('AngleWall', -1750, -1350, -970, -930, 0, 300, concrete, yaw=-40.0)
    # A free-standing wall with a window on each flank of the house.
    wall('PeekWall', 'y', -900, 620, 1150, 300, concrete, [window(885, 200)])
    wall('PeekWall', 'y', -900, -1150, -620, 300, concrete, [window(-885, 200)])

    # --- North lane: parkour. Mantle up, sprint jump the gaps, wall run the long wall, cross the bridge.
    wall('NorthDivider', 'x', 1280, -2450, -1950, 330, concrete, [window(-2200, 200)])
    both('ParkourStep', -2600, -2400, 1800, 2000, 0, 140, crate)
    both('ParkourDeck', -2350, -1850, 1760, 2160, 0, 280, metal)
    both('ParkourDeck', -1500, -1100, 1760, 2160, 0, 280, metal)
    both('ParkourDeck', -750, -350, 1820, 2160, 0, 300, metal)
    # The wall run wall stands beside both gaps, 700 high.
    both('WallRun', -2000, -950, 1700, 1740, 0, 700, concrete)
    # Bridge over the lane centre; walk over it or run under it.
    cube('Bridge', -350, 350, 1860, 2120, 270, 300, metal)
    both('BridgePost', -330, -290, 1860, 1900, 0, 270, metal)
    both('BridgePost', -330, -290, 2080, 2120, 0, 270, metal)
    cube('BridgeRail', -200, 200, 1860, 1880, 300, 385, metal)
    # Ground cover under the decks so the lane also works on foot.
    both('LaneBox', -1750, -1600, 2250, 2400, 0, 140, crate)
    both('LaneLow', -980, -940, 2180, 2460, 0, 80, wood)

    # --- South lane: tight cover. A divider with two windows and a door, staggered boxes and pillars inside.
    wall('SouthDivider', 'x', -1280, -2350, -480, 330, concrete, [window(-1950, 200), door(-1420), window(-880, 200)])
    both('Crate', -2080, -1930, -1780, -1630, 0, 150, crate)
    both('CrateStack', -1580, -1430, -2200, -2050, 0, 300, crate)
    both('Crate', -1580, -1430, -2050, -1900, 0, 150, crate)
    both('LaneLow', -1020, -980, -2000, -1620, 0, 80, wood)
    both('HeadBox', -520, -320, -1700, -1580, 0, 140, crate)
    both('Pillar', -755, -645, -2255, -2145, 0, 330, concrete)
    # Centre block of the south lane with a window slot through it.
    cube('SouthBlock', -260, 260, -2050, -2010, 0, 110, concrete)
    cube('SouthBlock', -260, 260, -2050, -2010, 235, 330, concrete)
    cube('SouthBlock', -260, -110, -2050, -2010, 110, 235, concrete)
    cube('SouthBlock', 110, 260, -2050, -2010, 110, 235, concrete)
    count['windows'] += 1

    # --- Heal pad: a small green plate in the open north of the house, on the centre line. Both peek-wall
    # windows look straight at it, so healing means standing in a crossfire.
    green = make_material('M_DistrictHealGreen', (0.02, 0.45, 0.10, 1), 0.0, 0.5, (0.03, 0.85, 0.16, 1))
    pad_class = unreal.load_class(None, '/Script/ArenaDuel.ArenaDuelHealPad')
    pad = unreal.EditorLevelLibrary.spawn_actor_from_class(pad_class, unreal.Vector(0, 900, 2), unreal.Rotator(roll=0.0, pitch=0.0, yaw=0.0))
    pad.set_actor_label(PREFIX + 'HealPad')
    pad.get_component_by_class(unreal.StaticMeshComponent).set_material(0, green)
    glow = unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.PointLight, unreal.Vector(0, 900, 9), unreal.Rotator())
    glow.set_actor_label(PREFIX + 'HealPadGlow')
    component = glow.get_component_by_class(unreal.PointLightComponent)
    component.set_mobility(unreal.ComponentMobility.MOVABLE)
    component.set_cast_shadows(False)
    component.set_editor_property('intensity', 4.0)
    component.set_editor_property('attenuation_radius', 200.0)
    component.set_editor_property('source_radius', 12.0)
    component.set_editor_property('light_color', unreal.Color(r=70, g=255, b=120, a=255))
    component.set_editor_property('use_inverse_squared_falloff', True)

    # The game mode looks for these two tags.
    for x, yaw, name in ((-3050, 0, 'P1'), (3050, 180, 'P2')):
        start = unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(x, 0, 100), unreal.Rotator(roll=0.0, pitch=0.0, yaw=yaw))
        start.set_actor_label(PREFIX + 'PlayerStart_' + name)
        start.set_editor_property('player_start_tag', 'ArenaCore_' + name)
    # --- Only to look at; none of this has collision.
    # Neon along the hall behind each spawn, in the team colour, in a dark channel. Its light is as long as
    # the strip, so the wall glows along all of it.
    for x, material, color, side in ((-3500, cyan, (30, 170, 255), 1.0), (3500, violet, (150, 50, 255), -1.0)):
        decor('NeonChannel', min(x, x + side * 5), max(x, x + side * 5), -2420, 2420, 424, 452, LOOK['cap'], shadow=False)
        decor('Neon', min(x + side * 5, x + side * 9), max(x + side * 5, x + side * 9), -2400, 2400, 432, 444, material, shadow=False)
        for y in (-1600, 0, 1600):
            tube_light('NeonLight', x + side * 70, y, 438, color, NEON_INTENSITY, 1150.0, 1600.0)
    # Orange diagonals on the long walls, two pairs each, leaning towards the middle.
    for y, face in ((2497.0, -1.0), (-2497.0, 1.0)):
        for x, lean in ((-2300, 34.0), (-1500, -34.0)):
            decor('Stripe', x - 430, x + 430, y - 1.5, y + 1.5, 700, 760, orange, shadow=False, mirror=True, pitch=lean)
    # Large shaded lamps along the hall walls, each with its own light: they light the two lanes and the wall
    # below them.
    for x in (-2800, -2000, -1200, -400):
        sconce(x, 2500.0, 0.0, -1.0, z=350.0, scale=2.0, intensity=HALL_LAMP_INTENSITY, radius=900.0)
        sconce(x, -2500.0, 0.0, 1.0, z=350.0, scale=2.0, intensity=HALL_LAMP_INTENSITY, radius=900.0)
    for y in (-1700, 1700):
        sconce(-3500.0, y, 1.0, 0.0, z=330.0, scale=2.0, intensity=HALL_LAMP_INTENSITY, radius=900.0)
    # Wall lamps on the cover walls and the house.
    for y in (-330, 330):
        sconce(-2500 - T * 0.5, y, -1.0, 0.0)
        sconce(-2500 + T * 0.5, y, 1.0, 0.0)
    sconce(-600 - T * 0.5, 0.0, -1.0, 0.0)
    for x in (-175, 175):
        sconce(x, 450 + T * 0.5, 0.0, 1.0, mirror=False)
        sconce(x, -450 - T * 0.5, 0.0, -1.0, mirror=False)
    for y in (1090, -1090):
        sconce(-900 - T * 0.5, y, -1.0, 0.0, z=235.0)
    for x in (-2270, -1700, -1100, -700):
        sconce(x, -1280 + T * 0.5, 0.0, 1.0)
    sconce(-2380, 1280 - T * 0.5, 0.0, -1.0)
    sconce(-2020, 1280 - T * 0.5, 0.0, -1.0)
    # The parkour lane: lamps on both faces of the wall run wall and under the decks, so nobody jumps into the dark.
    for x in (-1800, -1150):
        sconce(x, 1700.0, 0.0, -1.0, z=300.0)
        sconce(x, 1740.0, 0.0, 1.0, z=420.0)
    # Green: ivy over the tops of the cover walls, away from windows and doors.
    for y in (-820, 820):
        prop('SM_Ivy', -2500 + T * 0.5 + 5, y, 338.0, 0.0, mirror=True)
        prop('SM_Ivy', -2500 - T * 0.5 - 5, y, 338.0, 180.0, mirror=True)
    prop('SM_Ivy', -900 - T * 0.5 - 5, 700.0, 308.0, 180.0, mirror=True)
    prop('SM_Ivy', -900 - T * 0.5 - 5, -700.0, 308.0, 180.0, mirror=True)
    prop('SM_Ivy', -1500.0, -1280 + T * 0.5 + 5, 338.0, 90.0, mirror=True)
    prop('SM_Ivy', -620.0, -1280 + T * 0.5 + 5, 338.0, 90.0, mirror=True)
    prop('SM_Ivy', -600 - T * 0.5 - 5, 0.0, 338.0, 180.0, mirror=True, scale=0.9)
    # Orange floor markings: the edges of the two lanes, a box around each spawn, the approaches to the house doors.
    for y in (-1500, 1500):
        decor('Line', -3400, 3400, y - 6, y + 6, 0.0, 0.6, orange, shadow=False)
    for (x0, x1) in ((-3260, -2840), (2840, 3260)):
        decor('SpawnBox', x0, x1, -226, -214, 0.0, 0.6, paint, shadow=False)
        decor('SpawnBox', x0, x1, 214, 226, 0.0, 0.6, paint, shadow=False)
        decor('SpawnBox', x0, x0 + 12, -226, 226, 0.0, 0.6, paint, shadow=False)
        decor('SpawnBox', x1 - 12, x1, -226, 226, 0.0, 0.6, paint, shadow=False)
    for y0, y1 in ((480, 760), (-760, -480)):
        decor('DoorMark', -130, -118, y0, y1, 0.0, 0.6, orange, shadow=False)
        decor('DoorMark', 118, 130, y0, y1, 0.0, 0.6, orange, shadow=False)
    # Yellow edges on everything that can be stood on above the floor: the three decks, the bridge and the roof.
    for (x0, x1, y0, y1, top) in ((-2350, -1850, 1760, 2160, 280), (-1500, -1100, 1760, 2160, 280), (-750, -350, 1820, 2160, 300)):
        decor('DeckEdge', x0 - 2, x1 + 2, y0 - 2, y0 + 6, top - 6, top + 1, yellow, mirror=True)
        decor('DeckEdge', x0 - 2, x0 + 6, y0, y1, top - 6, top + 1, yellow, mirror=True)
        decor('DeckEdge', x1 - 6, x1 + 2, y0, y1, top - 6, top + 1, yellow, mirror=True)
    decor('BridgeEdge', -350, 350, 2114, 2122, 294, 301, yellow)
    decor('RoofEdge', -626, 626, -476, -464, 340, 352, LOOK['cap'])
    decor('RoofEdge', -626, 626, 464, 476, 340, 352, LOOK['cap'])
    decor('RoofEdge', -626, -614, -476, 476, 340, 352, LOOK['cap'], mirror=True)
    spawn_lights()
    spawn_ambient()
    spawn_post_process()

    world = unreal.EditorLevelLibrary.get_editor_world()
    if not unreal.EditorLoadingAndSavingUtils.save_map(world, MAP):
        raise RuntimeError('Failed to save ' + MAP)
    starts = [a for a in unreal.EditorLevelLibrary.get_all_level_actors() if isinstance(a, unreal.PlayerStart)]
    assert len(starts) == 2, 'Expected exactly two PlayerStarts'
    unreal.log('ARENA DISTRICT PASS pieces=%d windows=%d decor=%d lights=%d' % (count['pieces'], count['windows'], count['decor'], count['lights']))


build()
