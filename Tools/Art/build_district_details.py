"""Detail textures of the 1v1 map, made in Blender: an ivy leaf, one face of a timber crate, and the fine
monogram engraved into the walls.

    ART_DIR = r"<repo>/Tools/Art"; exec(open(ART_DIR + "/build_district_details.py").read())

Output in Tools/Art/District:
- T_District_Leaf (colour with the leaf's outline in alpha) and T_District_Leaf_Normal. The stalk is at the
  bottom centre, the tip at the top.
- T_District_Crate_BaseColor / _Normal / _RM: one 140 cm face of the crate of build_district_props.py, drawn
  where the model has its boards, so paint and relief agree. Row 0 of the arrays is the bottom of the face.
- T_District_Monogram: red is the engraved line, green and blue its slope (0.5 is flat). The letters are
  Blender text objects, rendered straight from above.
Everything is generated here; nothing is taken from a picture.
"""
import math
import os

import bpy
import numpy as np

_helpers = {'DEFER': True, '__name__': 'district_textures'}
exec(open(os.path.join(ART_DIR, 'build_district_textures.py')).read(), _helpers)
fbm, stretched, smoothstep, normal_from_height, save = (_helpers[k] for k in ('fbm', 'stretched', 'smoothstep', 'normal_from_height', 'save'))


def save_rgba(folder, name, rgba):
    rgba = np.clip(rgba, 0.0, 1.0).astype(np.float32)
    n = rgba.shape[0]
    if name in bpy.data.images:
        bpy.data.images.remove(bpy.data.images[name])
    image = bpy.data.images.new(name, n, n, alpha=True)
    image.alpha_mode = 'STRAIGHT'
    image.pixels.foreach_set(rgba.ravel())
    image.filepath_raw = os.path.join(folder, name + '.png')
    image.file_format = 'PNG'
    image.save()
    bpy.data.images.remove(image)


def blur(a, radius):
    """A soft blur, three passes of a wrapped box filter."""
    for _ in range(3):
        for axis in (0, 1):
            total = np.zeros_like(a)
            for shift in range(-radius, radius + 1):
                total += np.roll(a, shift, axis=axis)
            a = total / (2 * radius + 1)
    return a


def leaf(folder):
    """An ivy leaf: five pointed lobes from one base, a vein down each, side veins, a darker rim."""
    n = 512
    rng = np.random.default_rng(31)
    v, u = np.mgrid[0:n, 0:n] / float(n)
    base = np.array((0.5, 0.20))
    shape = np.zeros((n, n))
    vein = np.zeros((n, n))
    side = np.zeros((n, n))
    for angle, length, width in ((90, 0.74, 0.20), (38, 0.56, 0.17), (142, 0.56, 0.17), (-12, 0.36, 0.13), (192, 0.36, 0.13)):
        a = math.radians(angle)
        du, dv = u - base[0], v - base[1]
        along = du * math.cos(a) + dv * math.sin(a)
        across = -du * math.sin(a) + dv * math.cos(a)
        t = np.clip(along / length, 0.0, 1.0)
        half = width * np.power(t, 0.45) * np.power(1.0 - t, 0.75) * 1.9
        inside = (along > -0.02) & (along < length)
        lobe = np.where(inside, smoothstep(0.0, 0.012, half - np.abs(across)), 0.0)
        shape = np.maximum(shape, lobe)
        vein = np.maximum(vein, np.where(inside, 1.0 - smoothstep(0.0, 0.004 + 0.006 * (1.0 - t), np.abs(across)), 0.0) * lobe)
        # Side veins leave the middle vein at an angle, every so often.
        rib = np.abs(((along - np.abs(across) * 0.9) * 13.0) % 1.0 - 0.5)
        side = np.maximum(side, (1.0 - smoothstep(0.0, 0.07, rib)) * lobe * smoothstep(0.0, 0.02, np.abs(across)) * 0.55)
    # A round body where the lobes meet, and the stalk down to the bottom edge.
    body = 1.0 - smoothstep(0.17, 0.19, np.sqrt((u - 0.5) ** 2 + (v - 0.36) ** 2))
    shape = np.maximum(shape, body)
    stalk = (1.0 - smoothstep(0.006, 0.011, np.abs(u - 0.5))) * (v < 0.22)
    alpha = np.maximum(shape, stalk)
    veins = np.clip(vein + side, 0.0, 1.0)
    mottle = fbm(n, 9, 4, rng)
    fine = fbm(n, 70, 3, rng)
    rim = 1.0 - smoothstep(0.0, 0.5, blur(shape, 6) * shape)
    value = (0.62 + 0.5 * mottle) * (0.9 + 0.2 * fine) * (1.0 - 0.35 * rim)
    green = np.stack((0.085 * value + 0.03 * mottle, 0.26 * value, 0.055 * value), axis=-1)
    pale = np.array((0.30, 0.44, 0.16))
    color = green * (1.0 - veins[..., None] * 0.75) + pale[None, None, :] * veins[..., None] * 0.75
    color = np.where(stalk[..., None] > shape[..., None], np.array((0.16, 0.12, 0.06))[None, None, :], color)
    # Outside the outline the colour continues, so the edge does not turn dark where the mask is filtered.
    color = np.where(alpha[..., None] > 0.02, color, np.array((0.07, 0.20, 0.05))[None, None, :])
    save_rgba(folder, 'T_District_Leaf', np.concatenate((color, alpha[..., None]), axis=-1))
    height = blur(shape, 5) * 0.5 - veins * 0.35 + fine * 0.05 + mottle * 0.1
    save(folder, 'T_District_Leaf_Normal', normal_from_height(height * shape, 6.0), False)


def crate(folder):
    """One face of a timber crate: a frame of four boards, upright planks behind it, a brace from corner to
    corner, nails, and the dirt and wear of something that has been stacked and dragged."""
    n = 2048
    rng = np.random.default_rng(57)
    v, u = np.mgrid[0:n, 0:n] / float(n)
    frame_w, brace_w = 0.10, 0.085
    upright_grain = stretched(n, 300, 9, rng) * 0.65 + stretched(n, 90, 5, rng) * 0.35
    level_grain = upright_grain.T.copy()
    # Grain along the brace: the upright grain read in coordinates turned by 45 degrees.
    turned_u = (((u - v) * 0.7071 * n).astype(int)) % n
    turned_v = (((u + v) * 0.7071 * n).astype(int)) % n
    brace_grain = upright_grain[turned_v, turned_u]
    fine = fbm(n, 300, 3, rng)
    blotch = fbm(n, 5, 4, rng)

    in_frame = (u < frame_w) | (u > 1 - frame_w) | (v < frame_w) | (v > 1 - frame_w)
    level_board = (v < frame_w) | (v > 1 - frame_w)
    brace_axis = np.abs(u - v) * 0.7071
    in_brace = (brace_axis < brace_w * 0.5) & ~in_frame
    planks = 6
    inner = (u - frame_w) / (1 - 2 * frame_w)
    plank = np.clip((inner * planks).astype(int), 0, planks - 1)
    plank_edge = np.abs((inner * planks) % 1.0 - 0.5)
    gap = smoothstep(0.468, 0.492, plank_edge) * (~in_frame) * (~in_brace)
    tone_plank = (0.84 + 0.30 * rng.random(planks))[plank]
    tone = np.where(in_frame, np.where(level_board, 0.90, 0.97), np.where(in_brace, 1.04, tone_plank))
    grain = np.where(in_frame, np.where(level_board, level_grain, upright_grain), np.where(in_brace, brace_grain, upright_grain))
    # Butt joints of the frame: the upright boards end against the level ones.
    joint = (1.0 - smoothstep(0.0, 0.0022, np.minimum(np.abs(v - frame_w), np.abs(v - (1 - frame_w))))) * ((u < frame_w) | (u > 1 - frame_w))
    raised = in_frame.astype(float) + in_brace.astype(float) * 0.75
    shade = np.clip(blur(raised, 9) - raised, 0.0, 1.0) * 2.4          # what the frame and the brace shade on the planks
    edge_wear = np.clip(raised - blur(raised, 3), 0.0, 1.0) * 3.0       # their own edges, rubbed pale
    knots = smoothstep(0.83, 0.88, fbm(n, 11, 2, rng))
    # Nails: two at each corner of the frame, two at each end of the brace, one per plank top and bottom.
    nail = np.zeros((n, n))

    def nails(points, radius=0.0052):
        nonlocal nail
        for pu, pv in points:
            nail = np.maximum(nail, 1.0 - smoothstep(radius * 0.7, radius, np.sqrt((u - pu) ** 2 + (v - pv) ** 2)))
    half = frame_w * 0.5
    nails([(a, b) for a in (half * 0.6, half * 1.4, 1 - half * 0.6, 1 - half * 1.4) for b in (half, 1 - half)])
    nails([(frame_w + (i + 0.5) * (1 - 2 * frame_w) / planks, b) for i in range(planks) for b in (half, 1 - half)])
    nails([(0.17, 0.15), (0.15, 0.17), (0.83, 0.85), (0.85, 0.83), (0.5, 0.5)])
    dirt = smoothstep(0.30, 0.0, v) * (0.35 + 0.65 * blotch) + smoothstep(0.55, 0.85, fbm(n, 7, 3, rng)) * 0.25
    drips = stretched(n, 160, 4, rng)
    value = tone * (0.72 + 0.40 * grain) * (0.93 + 0.14 * fine) * (0.88 + 0.24 * blotch)
    value = value * (1.0 - 0.55 * gap) * (1.0 - 0.45 * shade) * (1.0 - 0.4 * knots) * (1.0 - 0.5 * joint) * (1.0 - 0.42 * dirt * (0.6 + 0.4 * drips))
    value = value + edge_wear * 0.10
    warm = np.array((0.62, 0.47, 0.30))
    grey = np.array((0.50, 0.47, 0.42))
    weathered = (0.35 + 0.5 * blotch)[..., None]
    color = value[..., None] * (warm[None, None, :] * (1.0 - weathered) + grey[None, None, :] * weathered)
    color = np.where(in_frame[..., None], color * np.array((0.86, 0.80, 0.74))[None, None, :], color)
    steel = np.array((0.16, 0.15, 0.14))
    rust = np.array((0.30, 0.15, 0.07))
    color = color * (1.0 - nail[..., None]) + (steel * (1.0 - blotch[..., None] * 0.5) + rust * blotch[..., None] * 0.5) * nail[..., None]
    height = raised * 1.0 + grain * 0.07 + fine * 0.02 - gap * 0.5 - joint * 0.3 - knots * 0.06 + nail * 0.10
    rough = np.clip(0.70 + 0.14 * (grain - 0.5) + 0.12 * dirt - 0.30 * nail, 0.0, 1.0)
    cavity = np.clip(1.0 - gap * 0.7 - shade * 0.5 - joint * 0.5, 0.35, 1.0)
    save(folder, 'T_District_Crate_BaseColor', color, True)
    save(folder, 'T_District_Crate_Normal', normal_from_height(blur(height, 1), 5.0), False)
    save(folder, 'T_District_Crate_RM', np.stack((rough, nail * 0.9, cavity), axis=-1), False)


def monogram(folder):
    """The wall engraving: the letters L and V, interlocked, and two small ornaments, on a diagonal grid.
    The letters are Blender text objects in Blender's own font; a camera straight above renders their outline."""
    n = 1024
    scene = bpy.data.scenes.new('MonogramBake')
    made = []

    def text(body, x, y, size, shear=0.0):
        curve = bpy.data.curves.new('Mono_' + body, 'FONT')
        curve.body = body
        curve.size = size
        curve.shear = shear
        curve.align_x = 'CENTER'
        curve.align_y = 'CENTER'
        obj = bpy.data.objects.new('Mono_' + body, curve)
        obj.location = (x, y, 0.0)
        scene.collection.objects.link(obj)
        made.append(obj)

    def ring_mesh(name, rings, x, y):
        """Flat outlines: each ring is (points, width); built as thin quads."""
        verts, faces = [], []
        for points, width in rings:
            count = len(points)
            for i in range(count):
                (ax, ay), (bx, by) = points[i], points[(i + 1) % count]
                dx, dy = bx - ax, by - ay
                length = math.hypot(dx, dy) or 1.0
                ox, oy = -dy / length * width * 0.5, dx / length * width * 0.5
                start = len(verts)
                verts += [(ax - ox, ay - oy, 0), (ax + ox, ay + oy, 0), (bx + ox, by + oy, 0), (bx - ox, by - oy, 0)]
                faces.append((start, start + 1, start + 2, start + 3))
        mesh = bpy.data.meshes.new(name)
        mesh.from_pydata(verts, [], faces)
        obj = bpy.data.objects.new(name, mesh)
        obj.location = (x, y, 0.0)
        scene.collection.objects.link(obj)
        made.append(obj)

    def circle(radius, count=40):
        return [(math.cos(i / count * math.tau) * radius, math.sin(i / count * math.tau) * radius) for i in range(count)]

    def star(outer, inner, points=4):
        return [(math.cos(i / (points * 2) * math.tau) * (outer if i % 2 == 0 else inner), math.sin(i / (points * 2) * math.tau) * (outer if i % 2 == 0 else inner)) for i in range(points * 2)]

    def petals(radius, count=4, steps=14):
        out = []
        for p in range(count):
            a = p / count * math.tau
            cx, cy = math.cos(a) * radius * 0.5, math.sin(a) * radius * 0.5
            out.append(([(cx + math.cos(i / steps * math.tau) * radius * 0.45 * (abs(math.cos(a)) * 0.5 + 0.5), cy + math.sin(i / steps * math.tau) * radius * 0.45 * (abs(math.sin(a)) * 0.5 + 0.5)) for i in range(steps)], 0.012))
        return out

    # The tile is one unit wide and repeats: the letters at two places, the ornaments at the two others.
    for cx, cy in ((0.25, 0.25), (0.75, 0.75)):
        text('L', cx - 0.035, cy + 0.012, 0.21, 0.18)
        text('V', cx + 0.045, cy - 0.028, 0.21, 0.18)
    ring_mesh('Mono_Star', [(star(0.085, 0.030), 0.012), (circle(0.016, 16), 0.012)], 0.75, 0.25)
    ring_mesh('Mono_Flower', [(circle(0.098), 0.012)] + petals(0.085) + [(circle(0.014, 16), 0.012)], 0.25, 0.75)

    camera_data = bpy.data.cameras.new('MonoCam')
    camera_data.type = 'ORTHO'
    camera_data.ortho_scale = 1.0
    camera = bpy.data.objects.new('MonoCam', camera_data)
    camera.location = (0.5, 0.5, 2.0)
    scene.collection.objects.link(camera)
    scene.camera = camera
    engines = [item.identifier for item in bpy.types.RenderSettings.bl_rna.properties['engine'].enum_items]
    scene.render.engine = 'BLENDER_WORKBENCH' if 'BLENDER_WORKBENCH' in engines else scene.render.engine
    scene.render.resolution_x = scene.render.resolution_y = n
    scene.render.resolution_percentage = 100
    scene.render.film_transparent = True
    scene.render.image_settings.file_format = 'PNG'
    scene.render.image_settings.color_mode = 'RGBA'
    path = os.path.join(bpy.app.tempdir, 'district_monogram_raw.png')
    scene.render.filepath = path
    bpy.ops.render.render(scene=scene.name, write_still=True)
    image = bpy.data.images.load(path)
    pixels = np.empty(n * n * 4, dtype=np.float32)
    image.pixels.foreach_get(pixels)
    mask = pixels.reshape(n, n, 4)[..., 3].astype(float)
    bpy.data.images.remove(image)
    for obj in made + [camera]:
        data = obj.data
        bpy.data.objects.remove(obj, do_unlink=True)
        if isinstance(data, bpy.types.Mesh):
            bpy.data.meshes.remove(data)
        elif isinstance(data, bpy.types.Camera):
            bpy.data.cameras.remove(data)
        else:
            bpy.data.curves.remove(data)
    bpy.data.scenes.remove(scene)
    coverage = float(mask.mean())
    line = blur(mask, 1)
    slope = normal_from_height(-blur(mask, 2), 9.0)
    # Rows are stored bottom first; the material reads the picture top first, as Unreal imports it.
    save(folder, 'T_District_Monogram', np.stack((line, slope[..., 0], slope[..., 1]), axis=-1), False)
    return coverage


def build_details(art_dir):
    folder = os.path.join(art_dir, 'District')
    os.makedirs(folder, exist_ok=True)
    leaf(folder)
    crate(folder)
    coverage = monogram(folder)
    print('DISTRICT DETAILS monogram coverage %.3f' % coverage)
    return coverage


if not globals().get('DEFER_DETAILS'):
    build_details(ART_DIR)
