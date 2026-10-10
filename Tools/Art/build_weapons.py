"""Build the four firearms in Blender, bake their textures and export them as FBX.

    ART_DIR = r"<repo>/Tools/Art"; exec(open(ART_DIR + "/build_weapons.py").read())

The game's conventions are kept, so nothing in code has to move: the origin is the pistol grip, the
barrel points down +X at a height of 13, sizes are in centimetres of the oversized game mesh (the game
scales first person weapons to about 0.4), the muzzle lies at x = length + 3, slot 0 is the body and
slot 1 the accent the game recolours per archetype.

Each weapon is three meshes with the same origin, so the game can move two of them: SM_<Name> (the
body), SM_<Name>_Bolt (bolt and charging handle; the pump on the shotgun) and SM_<Name>_Mag (the
magazine; the shotgun has none). All three share one texture set, baked here from a procedural
material: T_<Name>_BaseColor, T_<Name>_Normal and T_<Name>_RM (red roughness, green metallic).
Output: Weapons/*.fbx, Weapons/T_*.png and weapons.blend.

Set DEFER = True before running to only define the functions; build_one(ART_DIR, index) then builds a
single weapon, which keeps each call short when Blender is driven from outside.
"""
import math
import os

import bmesh
import bpy
from mathutils import Matrix, Vector

BODY, ACCENT = 0, 1
METAL, POLYMER = 0, 1
FRAME, BOLT, MAG = 0, 1, 2
AXIS = 13.0
SIZE = 2048


class Gun:
    def __init__(self, name):
        self.name = name
        self.bm = bmesh.new()
        self.part = FRAME
        self.layers = [self.bm.faces.layers.int.new(n) for n in ('w_part', 'w_kind', 'w_accent')]

    def _finish(self, made, slot, smooth, kind):
        faces = {f for v in made['verts'] for f in v.link_faces}
        for face in faces:
            face.material_index = slot
            face.smooth = smooth
            face[self.layers[0]] = self.part
            face[self.layers[1]] = kind
            face[self.layers[2]] = slot

    def box(self, x0, x1, z0, z1, width, slot=BODY, bevel=0.5, pitch=0.0, y=0.0, kind=METAL):
        centre = Vector(((x0 + x1) * 0.5, y, (z0 + z1) * 0.5))
        matrix = Matrix.Translation(centre) @ Matrix.Rotation(math.radians(pitch), 4, 'Y') @ Matrix.Diagonal((x1 - x0, width, z1 - z0, 1.0))
        made = bmesh.ops.create_cube(self.bm, size=1.0, matrix=matrix)
        if bevel > 0.0:
            edges = list({e for v in made['verts'] for e in v.link_edges})
            limit = 0.45 * min(x1 - x0, width, z1 - z0)
            result = bmesh.ops.bevel(self.bm, geom=edges, offset=min(bevel, limit), segments=2, affect='EDGES', profile=0.5)
            made = {'verts': result['verts'] + [v for v in made['verts'] if v.is_valid]}
        self._finish(made, slot, False, kind)

    def tube(self, x0, x1, radius, z=AXIS, slot=BODY, segments=16, y=0.0, radius_end=None, kind=METAL):
        matrix = Matrix.Translation(Vector(((x0 + x1) * 0.5, y, z))) @ Matrix.Rotation(math.radians(90.0), 4, 'Y')
        made = bmesh.ops.create_cone(self.bm, cap_ends=True, segments=segments, radius1=radius, radius2=radius if radius_end is None else radius_end, depth=x1 - x0, matrix=matrix)
        self._finish(made, slot, segments >= 12, kind)

    def to_object(self, collection):
        mesh = bpy.data.meshes.new('WB_' + self.name)
        bmesh.ops.recalc_face_normals(self.bm, faces=self.bm.faces)
        self.bm.to_mesh(mesh)
        self.bm.free()
        for slot in ('W_Body', 'W_Accent'):
            mesh.materials.append(bpy.data.materials.get(slot) or bpy.data.materials.new(slot))
        obj = bpy.data.objects.new('WB_' + self.name, mesh)
        collection.objects.link(obj)
        return obj


def common(gun, receiver_end, width, mag):
    """Receiver, grip, trigger guard and magazine: what every one of them has around the hand."""
    gun.box(-6, receiver_end, 9, 19, width, bevel=0.9)                 # upper receiver
    gun.box(-5, receiver_end - 6, 4.5, 9.5, width * 0.85, bevel=0.6)   # lower receiver
    gun.box(-4.2, 2.6, -13, 6, 5.2, bevel=1.3, pitch=-14, kind=POLYMER)  # pistol grip, through the origin
    gun.box(2.5, 9.5, 0.6, 1.6, 2.2, bevel=0.3)                        # trigger guard, bottom
    gun.box(9.0, 10.2, 0.6, 5.0, 2.2, bevel=0.3)                       # trigger guard, front
    gun.box(4.6, 5.6, 1.8, 5.0, 1.0, bevel=0.2, pitch=12)              # trigger
    gun.box(1.0, 6.5, 15.2, 16.4, width + 0.5, ACCENT, bevel=0.2)      # selector strip on both sides
    for x in (-3.5, receiver_end - 9.0):                               # takedown pins
        for side in (-1, 1):
            gun.box(x, x + 1.4, 7.3, 8.7, 0.5, bevel=0.2, y=side * width * 0.43)
    if mag:
        gun.part = MAG
        x0, x1, bottom, curve = mag
        steps = 4
        for step in range(steps):
            top = 5.0 - (5.0 - bottom) * step / steps
            low = 5.0 - (5.0 - bottom) * (step + 1) / steps
            shift = curve * (step / (steps - 1)) ** 2
            gun.box(x0 + shift, x1 + shift, low - 0.4, top + (3.0 if step == 0 else 0.0), width * 0.62, bevel=0.5, pitch=-curve * 2.2 * step / steps)
        gun.box(x0 + curve - 0.4, x1 + curve + 0.4, bottom - 1.2, bottom, width * 0.7, ACCENT, bevel=0.3)
        gun.part = FRAME


def bolt(gun, x0, x1, width, handle_x):
    """Bolt carrier showing in the ejection port, with the charging handle on one side."""
    gun.part = BOLT
    gun.box(x0, x1, 16.3, 18.3, width, bevel=0.3)
    gun.box(handle_x, handle_x + 1.8, 16.7, 17.9, 2.4, bevel=0.4, y=width * 0.5 + 0.9)
    gun.tube(handle_x + 0.2, handle_x + 1.6, 1.0, z=17.3, segments=10, y=width * 0.5 + 2.2)
    gun.part = FRAME


def sights(gun, rear_x, front_x, base=20.6):
    gun.box(rear_x - 1.2, rear_x + 1.2, base, base + 4.2, 5.0, bevel=0.4)
    gun.box(rear_x - 0.8, rear_x + 0.8, base + 2.0, base + 4.6, 1.6, ACCENT, bevel=0.0)
    gun.box(front_x - 1.0, front_x + 1.0, base, base + 3.0, 3.6, bevel=0.4)
    gun.box(front_x - 0.5, front_x + 0.5, base + 3.0, base + 4.6, 0.9, ACCENT, bevel=0.0)


def rail(gun, x0, x1, z0, z1, width):
    """A top rail with its cross slots."""
    gun.box(x0, x1, z0, z1, width, bevel=0.3)
    x = x0 + 1.5
    while x + 1.0 < x1 - 1.0:
        gun.box(x, x + 1.0, z1 - 0.1, z1 + 0.5, width + 0.7, bevel=0.12)
        x += 2.0


def arc_rifle():
    gun = Gun('ArcRifle')
    common(gun, 22, 7.4, (10.5, 16.5, -17.0, 3.0))
    gun.tube(22, 48, 4.7, segments=8, kind=POLYMER)                     # handguard
    for side in (-1, 1):
        for x in (26, 32, 38, 44):
            gun.box(x - 1.8, x + 1.8, 12.2, 13.8, 0.8, ACCENT, bevel=0.0, y=side * 4.3)   # vents
    rail(gun, -4, 46, 19, 20.6, 3.4)                                    # top rail
    gun.tube(48, 67, 1.7)                                               # barrel
    gun.box(49, 53, 13.5, 18.5, 3.2, bevel=0.5)                         # gas block
    gun.tube(66, 73, 2.5, segments=12)                                  # muzzle brake
    gun.tube(71.5, 73.2, 1.9, slot=ACCENT, segments=12)
    gun.tube(-28, -6, 2.1)                                              # buffer tube
    gun.box(-32, -13, 5.0, 17.5, 5.2, bevel=1.2, kind=POLYMER)          # stock
    gun.box(-34, -32, 3.0, 19.0, 5.8, bevel=0.6, kind=POLYMER)          # butt pad
    gun.box(-22, -15, 8.5, 12.0, 5.6, ACCENT, bevel=0.3)                # stock inlay
    bolt(gun, 11.5, 17.5, 7.9, 15.0)
    sights(gun, -1.5, 44)
    return gun


def shade_smg():
    gun = Gun('ShadeSMG')
    common(gun, 20, 6.8, (10.0, 14.5, -20.0, 0.6))
    gun.tube(20, 37, 3.7, segments=10, kind=POLYMER)                    # barrel shroud
    for side in (-1, 1):
        for x in (23, 27.5, 32):
            gun.box(x - 1.3, x + 1.3, 12.3, 13.7, 0.8, ACCENT, bevel=0.0, y=side * 3.4)
    gun.tube(36, 45, 2.7, segments=14)                                  # suppressor
    gun.tube(44.2, 45.2, 1.3, slot=ACCENT, segments=12)
    rail(gun, -4, 19, 19, 20.5, 3.2)                                    # rail
    for z in (16.5, 9.5):                                               # wire stock
        gun.tube(-19, -6, 0.75, z=z, segments=8)
    gun.box(-21, -19, 6.5, 19.0, 4.6, bevel=0.6, kind=POLYMER)
    gun.box(24, 28, 0.5, 9.5, 3.6, bevel=1.0, pitch=8, kind=POLYMER)    # stubby foregrip
    bolt(gun, 9.0, 14.0, 7.3, 12.0)
    sights(gun, -1.0, 17, base=20.5)
    return gun


def rune_dmr():
    gun = Gun('RuneDMR')
    common(gun, 22, 6.4, (10.0, 16.0, -9.0, 0.4))
    gun.tube(22, 52, 3.9, segments=10, kind=POLYMER)                    # slim handguard
    gun.box(24, 50, 16.6, 18.0, 2.8, ACCENT, bevel=0.2)
    gun.tube(52, 87, 1.6)                                               # long barrel
    gun.tube(85, 93, 2.3, segments=12)                                  # muzzle brake
    gun.tube(91.6, 93.2, 1.8, slot=ACCENT, segments=12)
    gun.box(-36, -7, 5.0, 17.5, 5.0, bevel=1.4, kind=POLYMER)           # fixed stock
    gun.box(-30, -12, 17.0, 20.0, 4.2, bevel=0.9, kind=POLYMER)         # cheek rest
    gun.box(-38, -36, 3.0, 19.0, 5.6, bevel=0.6, kind=POLYMER)
    rail(gun, -4, 30, 19, 20.4, 3.2)                                    # rail under the scope
    for x in (5, 22):                                                   # scope mounts
        gun.box(x - 1.5, x + 1.5, 20.4, 23.4, 4.2, bevel=0.4)
    gun.tube(1, 29, 3.0, z=26.2)                                        # scope body
    gun.tube(26, 34, 3.0, z=26.2, radius_end=4.3)                       # objective bell
    gun.tube(-3, 2, 3.7, z=26.2)                                        # eyepiece
    gun.tube(33.6, 34.2, 3.9, z=26.2, slot=ACCENT)                      # front lens
    gun.tube(-3.2, -2.8, 3.2, z=26.2, slot=ACCENT)                      # rear lens
    gun.tube(12, 16, 1.5, z=29.6, segments=10)                          # elevation turret
    for side in (-1, 1):                                                # folded bipod
        gun.tube(30, 50, 0.7, z=8.4, segments=8, y=side * 2.2)
    bolt(gun, 10.0, 17.0, 6.9, 14.5)
    return gun


def hex_shotgun():
    gun = Gun('HexShotgun')
    common(gun, 24, 7.8, None)
    gun.tube(24, 62, 2.5)                                               # barrel
    gun.tube(24, 55, 2.2, z=8.4)                                        # magazine tube
    gun.box(53, 56, 7.0, 15.0, 4.0, bevel=0.5)                          # barrel band
    gun.part = BOLT                                                     # the pump is what moves
    gun.tube(31, 47, 4.3, z=9.2, segments=10, kind=POLYMER)
    for x in (34, 38, 42):
        gun.tube(x - 0.5, x + 0.5, 4.6, z=9.2, slot=ACCENT, segments=10)
    for side in (-1, 1):                                                # action bars back to the receiver
        gun.box(22, 32, 9.6, 10.6, 0.6, bevel=0.15, y=side * 2.9)
    gun.part = FRAME
    gun.box(-33, -6, 5.5, 17.0, 5.4, bevel=1.5, pitch=-4, kind=POLYMER)  # stock
    gun.box(-36, -33, 2.5, 18.0, 6.0, bevel=0.7, kind=POLYMER)          # butt pad
    gun.box(8, 17, 15.0, 17.6, 8.4, ACCENT, bevel=0.3)                  # ejection port
    rail(gun, -4, 22, 19, 20.2, 3.0)                                    # rib
    gun.box(60.0, 61.2, 15.4, 17.4, 1.2, ACCENT, bevel=0.2)             # bead sight
    gun.box(-1.5, 1.0, 20.2, 23.2, 4.6, bevel=0.4)                      # rear notch
    return gun


def bake_material():
    """Gunmetal and polymer, worn bright on the edges, scratched along the barrel, dusty in the large."""
    mat = bpy.data.materials.get('W_Bake') or bpy.data.materials.new('W_Bake')
    mat.use_nodes = True
    tree = mat.node_tree
    tree.nodes.clear()
    new, link = tree.nodes.new, tree.links.new
    out = new('ShaderNodeOutputMaterial')
    bsdf = new('ShaderNodeBsdfPrincipled')
    link(bsdf.outputs['BSDF'], out.inputs['Surface'])
    coord = new('ShaderNodeTexCoord')

    def attribute(name):
        node = new('ShaderNodeAttribute')
        node.attribute_name = name
        return node.outputs['Fac']

    def noise(scale, detail=4.0, stretch=None):
        node = new('ShaderNodeTexNoise')
        node.inputs['Scale'].default_value = scale
        node.inputs['Detail'].default_value = detail
        source = coord.outputs['Object']
        if stretch:
            mapping = new('ShaderNodeMapping')
            mapping.inputs['Scale'].default_value = stretch
            link(source, mapping.inputs['Vector'])
            source = mapping.outputs['Vector']
        link(source, node.inputs['Vector'])
        return node.outputs['Fac']

    def ramp(source, low, high):
        node = new('ShaderNodeMapRange')
        node.inputs['From Min'].default_value = low
        node.inputs['From Max'].default_value = high
        link(source, node.inputs['Value'])
        return node.outputs['Result']

    def math_node(op, a, b):
        node = new('ShaderNodeMath')
        node.operation = op
        node.use_clamp = True
        for socket, value in zip(node.inputs, (a, b)):
            if isinstance(value, float):
                socket.default_value = value
            else:
                link(value, socket)
        return node.outputs[0]

    def mix(a, b, factor, mode='MIX'):
        node = new('ShaderNodeMix')
        node.data_type = 'RGBA'
        node.blend_type = mode
        link(factor, node.inputs[0])
        for socket, value in ((node.inputs[6], a), (node.inputs[7], b)):
            if isinstance(value, tuple):
                socket.default_value = value
            else:
                link(value, socket)
        return node.outputs[2]

    polymer = attribute('w_kind')
    accent = attribute('w_accent')
    geometry = new('ShaderNodeNewGeometry')
    # Worn edges: where the surface is convex, broken up by noise so it is not an even outline.
    # A rounded normal differs from the real one only near an edge; pointiness is no use on separate boxes.
    rounded = new('ShaderNodeBevel')
    rounded.samples = 8
    rounded.inputs['Radius'].default_value = 0.45
    turn = new('ShaderNodeVectorMath')
    turn.operation = 'DOT_PRODUCT'
    link(rounded.outputs['Normal'], turn.inputs[0])
    link(geometry.outputs['Normal'], turn.inputs[1])
    edge = math_node('MULTIPLY', ramp(math_node('SUBTRACT', 1.0, turn.outputs['Value']), 0.01, 0.10), ramp(noise(0.9, 3.0), 0.38, 0.62))
    scratch = math_node('MULTIPLY', ramp(noise(2.2, 2.0, (0.06, 1.0, 1.0)), 0.63, 0.66), ramp(noise(0.25), 0.4, 0.6))
    wear = math_node('MAXIMUM', edge, math_node('MULTIPLY', scratch, 0.6))
    dust = ramp(noise(0.12, 5.0), 0.45, 0.8)
    metal_color = mix((0.036, 0.040, 0.047, 1), (0.060, 0.064, 0.072, 1), ramp(noise(0.35), 0.3, 0.7))
    polymer_color = mix((0.016, 0.016, 0.018, 1), (0.028, 0.027, 0.026, 1), ramp(noise(0.5), 0.3, 0.7))
    base = mix(metal_color, polymer_color, polymer)
    base = mix(base, (0.085, 0.075, 0.062, 1), math_node('MULTIPLY', dust, 0.35))
    worn = mix((0.50, 0.50, 0.52, 1), (0.075, 0.075, 0.08, 1), polymer)
    base = mix(base, worn, wear)
    # The accent faces are recoloured by the game; in the texture they are a neutral light grey.
    base = mix(base, (0.6, 0.6, 0.6, 1), accent)
    link(base, bsdf.inputs['Base Color'])
    rough = mix((0.40, 0.40, 0.40, 1), (0.66, 0.66, 0.66, 1), polymer)
    rough = mix(rough, (0.85, 0.85, 0.85, 1), math_node('MULTIPLY', dust, 0.5))
    rough = mix(rough, mix((0.20, 0.20, 0.20, 1), (0.45, 0.45, 0.45, 1), polymer), wear)
    link(rough, bsdf.inputs['Roughness'])
    # Emission carries the metallic value for its own bake.
    metallic = math_node('SUBTRACT', 1.0, polymer)
    link(metallic, bsdf.inputs['Emission Color'])
    bsdf.inputs['Emission Strength'].default_value = 1.0
    # Polymer is stippled, metal has a faint machined grain, scratches are cut in.
    height = math_node('ADD', math_node('MULTIPLY', noise(9.0, 1.0), math_node('ADD', math_node('MULTIPLY', polymer, 0.8), 0.12)), math_node('MULTIPLY', scratch, -0.6))
    bump = new('ShaderNodeBump')
    bump.inputs['Strength'].default_value = 0.35
    bump.inputs['Distance'].default_value = 0.1
    link(height, bump.inputs['Height'])
    link(bump.outputs['Normal'], bsdf.inputs['Normal'])
    target = new('ShaderNodeTexImage')
    target.name = 'BakeTarget'
    tree.nodes.active = target
    return mat, target


def unwrap_and_bake(obj, name, folder):
    mesh = obj.data
    bpy.ops.object.select_all(action='DESELECT')
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    bpy.ops.object.mode_set(mode='EDIT')
    bpy.ops.mesh.select_all(action='SELECT')
    bpy.ops.uv.smart_project(angle_limit=math.radians(66.0), island_margin=0.004)
    bpy.ops.object.mode_set(mode='OBJECT')
    mat, target = bake_material()
    slots = [slot.material for slot in obj.material_slots]
    for slot in obj.material_slots:
        slot.material = mat
    scene = bpy.context.scene
    scene.render.engine = 'CYCLES'
    scene.cycles.samples = 4
    scene.render.bake.margin = 8
    images = {}
    for key, kind, colorspace in (('BaseColor', 'DIFFUSE', 'sRGB'), ('Metal', 'EMIT', 'Non-Color'), ('Normal', 'NORMAL', 'Non-Color'), ('Rough', 'ROUGHNESS', 'Non-Color')):
        image = bpy.data.images.new('T_%s_%s' % (name, key), SIZE, SIZE, alpha=False)
        image.colorspace_settings.name = colorspace
        target.image = image
        if kind == 'DIFFUSE':
            bpy.ops.object.bake(type=kind, pass_filter={'COLOR'})
        else:
            bpy.ops.object.bake(type=kind)
        images[key] = image
    packed = list(images['Rough'].pixels[:])
    packed[1::4] = images['Metal'].pixels[0::4]
    packed[2::4] = [0.0] * (SIZE * SIZE)
    rm = bpy.data.images.new('T_%s_RM' % name, SIZE, SIZE, alpha=False)
    rm.colorspace_settings.name = 'Non-Color'
    rm.pixels[:] = packed
    images['RM'] = rm
    for key in ('BaseColor', 'Normal', 'RM'):
        image = images[key]
        image.filepath_raw = os.path.join(folder, 'T_%s_%s.png' % (name, key))
        image.file_format = 'PNG'
        image.save()
    for key in ('Metal', 'Rough'):
        bpy.data.images.remove(images[key])
    for slot, material in zip(obj.material_slots, slots):
        slot.material = material


def split_and_export(obj, name, folder, collection):
    """One object per moving part, all with the weapon's origin."""
    made = []
    for part, suffix in ((FRAME, ''), (BOLT, '_Bolt'), (MAG, '_Mag')):
        bm = bmesh.new()
        bm.from_mesh(obj.data)
        layer = bm.faces.layers.int.get('w_part')
        bmesh.ops.delete(bm, geom=[f for f in bm.faces if f[layer] != part], context='FACES')
        if not bm.faces:
            bm.free()
            continue
        piece_name = 'SM_%s%s' % (name, suffix)
        if piece_name in bpy.data.meshes:
            bpy.data.meshes.remove(bpy.data.meshes[piece_name])
        mesh = bpy.data.meshes.new(piece_name)
        bm.to_mesh(mesh)
        bm.free()
        for material in obj.data.materials:
            mesh.materials.append(material)
        # Modelled in centimetres, stored in metres.
        mesh.transform(Matrix.Scale(0.01, 4))
        piece = bpy.data.objects.new(piece_name, mesh)
        collection.objects.link(piece)
        bpy.ops.object.select_all(action='DESELECT')
        piece.select_set(True)
        bpy.context.view_layer.objects.active = piece
        bpy.ops.export_scene.fbx(filepath=os.path.join(folder, piece_name + '.fbx'), use_selection=True, object_types={'MESH'}, mesh_smooth_type='FACE', bake_anim=False)
        made.append(piece)
    bpy.data.objects.remove(obj, do_unlink=True)
    return made


FACTORIES = (arc_rifle, shade_smg, rune_dmr, hex_shotgun)


def build_one(art_dir, index):
    folder = os.path.join(art_dir, 'Weapons')
    os.makedirs(folder, exist_ok=True)
    collection = bpy.context.scene.collection
    gun = FACTORIES[index]()
    name = gun.name
    for old in [o for o in bpy.data.objects if o.name.split('.')[0] in ('SM_' + name, 'SM_%s_Bolt' % name, 'SM_%s_Mag' % name, 'WB_' + name)]:
        bpy.data.objects.remove(old, do_unlink=True)
    for image in [i for i in bpy.data.images if i.name.startswith('T_%s_' % name)]:
        bpy.data.images.remove(image)
    obj = gun.to_object(collection)
    unwrap_and_bake(obj, name, folder)
    pieces = split_and_export(obj, name, folder, collection)
    for piece in pieces:
        piece.location.y = index * 0.4
    for slot, color in (('W_Body', (0.05, 0.055, 0.065, 1)), ('W_Accent', (0.1, 0.7, 0.9, 1))):
        bpy.data.materials[slot].diffuse_color = color
    return ['%s:%d' % (piece.name, len(piece.data.vertices)) for piece in pieces]


def build_all(art_dir):
    for obj in list(bpy.data.objects):
        bpy.data.objects.remove(obj, do_unlink=True)
    made = []
    for index in range(len(FACTORIES)):
        made += build_one(art_dir, index)
    bpy.ops.wm.save_as_mainfile(filepath=os.path.join(art_dir, 'weapons.blend'))
    print('WEAPONS', made)


if not globals().get('DEFER'):
    build_all(ART_DIR)
