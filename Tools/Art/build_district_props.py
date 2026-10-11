"""Model the set dressing of the 1v1 map in Blender and export it as FBX.

    ART_DIR = r"<repo>/Tools/Art"; exec(open(ART_DIR + "/build_district_props.py").read())

Sizes in centimetres. Material slots are named for what they are (D_Metal, D_Glow, D_Bark, D_Leaf, D_Crate);
Tools/Editor/SetupDistrictProps.py gives them the map's materials.

- SM_WallLamp: a shaded wall lamp on a curved arm. Origin on the wall, +X out of it; the shade opens downwards,
  so its light belongs below it and nowhere else.
- SM_CeilingLamp: a linear hall light on two rods. Origin on the ceiling, lens down.
- SM_Ivy: ivy hanging from a wall top. Origin on the wall at the top edge, +X out of the wall, it hangs down -Z.
  Every leaf is a folded card with the leaf of T_District_Leaf on it (build_district_details.py): stalk at v = 0.
- SM_DistrictCrate: a timber crate, exactly 140 cm each way: twelve frame beams, six panels set back 3 cm, a
  brace across every panel. Every face is mapped straight on, one face of T_District_Crate per side, so the
  painted boards lie on the modelled ones.
Output: District/SM_*.fbx and district_props.blend.
"""
import math
import os
import random

import bmesh
import bpy
from mathutils import Matrix, Vector

SLOTS = ('D_Metal', 'D_Glow', 'D_Bark', 'D_Leaf', 'D_Crate')


class Prop:
    def __init__(self, name):
        self.name = name
        self.bm = bmesh.new()
        self.color = self.bm.loops.layers.color.new('Col')
        self.uv = self.bm.loops.layers.uv.new('UVMap')

    def paint(self, faces, slot, smooth=False, color=(1.0, 1.0, 1.0, 1.0)):
        for face in faces:
            face.material_index = SLOTS.index(slot)
            face.smooth = smooth
            for loop in face.loops:
                loop[self.color] = color

    def box(self, centre, size, slot, bevel=0.0, turn=None):
        matrix = Matrix.Translation(Vector(centre)) @ (turn or Matrix.Identity(4)) @ Matrix.Diagonal((size[0], size[1], size[2], 1.0))
        made = bmesh.ops.create_cube(self.bm, size=1.0, matrix=matrix)
        verts = made['verts']
        if bevel > 0.0:
            result = bmesh.ops.bevel(self.bm, geom=list({e for v in verts for e in v.link_edges}), offset=min(bevel, 0.45 * min(size)), segments=2, affect='EDGES', profile=0.5)
            verts = result['verts'] + [v for v in verts if v.is_valid]
        self.paint({f for v in verts if v.is_valid for f in v.link_faces}, slot)

    def cone(self, centre, radius_bottom, radius_top, height, slot, segments=12, color=(1.0, 1.0, 1.0, 1.0), smooth=True):
        made = bmesh.ops.create_cone(self.bm, cap_ends=True, segments=segments, radius1=radius_bottom, radius2=radius_top, depth=height, matrix=Matrix.Translation(Vector(centre)))
        self.paint({f for v in made['verts'] for f in v.link_faces}, slot, smooth, color)
        return made['verts']

    def tube(self, start, end, radius, slot, segments=8):
        """A round bar from one point to another."""
        start, end = Vector(start), Vector(end)
        axis = end - start
        turn = axis.to_track_quat('Z', 'Y').to_matrix().to_4x4()
        made = bmesh.ops.create_cone(self.bm, cap_ends=True, segments=segments, radius1=radius, radius2=radius, depth=axis.length, matrix=Matrix.Translation((start + end) * 0.5) @ turn)
        self.paint({f for v in made['verts'] for f in v.link_faces}, slot, True)

    def card(self, corners, uvs, slot):
        """A face with its own texture coordinates."""
        face = self.bm.faces.new([self.bm.verts.new(c) for c in corners])
        face.material_index = SLOTS.index(slot)
        face.smooth = True
        for loop, uv in zip(face.loops, uvs):
            loop[self.uv].uv = uv
        return face

    def export(self, art_dir, collection):
        mesh = bpy.data.meshes.new(self.name)
        if self.name != 'SM_Ivy':   # leaf cards are seen from both sides and keep the side they were built with
            bmesh.ops.recalc_face_normals(self.bm, faces=self.bm.faces)
        self.bm.to_mesh(mesh)
        self.bm.free()
        for slot in SLOTS:
            mesh.materials.append(bpy.data.materials.get(slot) or bpy.data.materials.new(slot))
        obj = bpy.data.objects.new(self.name, mesh)
        collection.objects.link(obj)
        mesh.transform(Matrix.Scale(0.01, 4))
        bpy.ops.object.select_all(action='DESELECT')
        obj.select_set(True)
        bpy.context.view_layer.objects.active = obj
        folder = os.path.join(art_dir, 'District')
        os.makedirs(folder, exist_ok=True)
        bpy.ops.export_scene.fbx(filepath=os.path.join(folder, self.name + '.fbx'), use_selection=True, object_types={'MESH'}, mesh_smooth_type='FACE', bake_anim=False, colors_type='SRGB')
        return obj


def wall_lamp():
    prop = Prop('SM_WallLamp')
    prop.box((1.2, 0, 2), (2.4, 11, 20), 'D_Metal', bevel=1.0)               # wall plate
    for z in (-5, 9):
        prop.cone((2.7, 0, z), 0.9, 0.9, 0.8, 'D_Metal', segments=8)          # screws (heads up, close enough at this size)
    prop.box((3.4, 0, 2), (2.0, 5, 6), 'D_Metal', bevel=0.8)                  # boss the arm grows out of
    # The arm: up and out of the plate in a curve, down into the top of the shade.
    points = [Vector((3.5, 0, 2)), Vector((6.5, 0, 6.5)), Vector((10.5, 0, 9.5)), Vector((14.5, 0, 10.0)), Vector((17.3, 0, 8.3)), Vector((18.0, 0, 5.0))]
    for a, b in zip(points, points[1:]):
        prop.tube(a, b, 1.25, 'D_Metal')
    prop.cone((18, 0, 3.6), 3.4, 2.6, 3.2, 'D_Metal', segments=16)            # neck
    prop.cone((18, 0, -3.5), 11.5, 3.6, 11.0, 'D_Metal', segments=24)         # shade, opening downwards
    prop.cone((18, 0, -9.3), 12.2, 11.5, 0.9, 'D_Metal', segments=24)         # rolled rim
    prop.cone((18, 0, -9.6), 9.6, 9.6, 0.8, 'D_Glow', segments=24, smooth=False)  # the lit opening
    return prop


def ceiling_lamp():
    prop = Prop('SM_CeilingLamp')
    for x in (-110, 110):
        prop.cone((x, 0, -22), 1.2, 1.2, 44, 'D_Metal', segments=8)          # rods
        prop.box((x, 0, -1.5), (14, 14, 3), 'D_Metal', bevel=0.6)            # ceiling plates
    prop.box((0, 0, -50), (350, 64, 12), 'D_Metal', bevel=2.0)               # housing
    prop.box((0, 0, -57), (330, 46, 3), 'D_Glow', bevel=0.8)                 # lens
    for x in (-110, 0, 110):
        prop.box((x, 0, -57.5), (2, 50, 3.5), 'D_Metal')                     # lens bars
    for side in (-1, 1):
        prop.box((side * 176, 0, -50), (4, 68, 15), 'D_Metal', bevel=1.0)    # end caps
    return prop


def ivy():
    rng = random.Random(9)
    prop = Prop('SM_Ivy')
    width = 230.0

    def leaf(stalk, tip_direction, facing, size):
        """A leaf card folded a little along its middle vein: stalk at v = 0, tip at v = 1."""
        tip_direction = tip_direction.normalized()
        side = tip_direction.cross(facing).normalized()
        facing = side.cross(tip_direction).normalized()
        half = side * size * 0.5
        fold = facing * size * 0.10
        tip = stalk + tip_direction * size
        prop.card((stalk - half + fold, stalk, tip, tip - half + fold), ((0.0, 0.0), (0.5, 0.0), (0.5, 1.0), (0.0, 1.0)), 'D_Leaf')
        prop.card((stalk, stalk + half + fold, tip + half + fold, tip), ((0.5, 0.0), (1.0, 0.0), (1.0, 1.0), (0.5, 1.0)), 'D_Leaf')

    def leaves_at(point, count, size_scale):
        for _ in range(count):
            # Tips point down and a little sideways, faces look out of the wall, each one turned its own way.
            tip_direction = Vector((rng.uniform(0.0, 0.5), rng.uniform(-0.75, 0.75), -1.0 + rng.uniform(0.0, 0.5)))
            facing = Vector((1.0, rng.uniform(-0.6, 0.6), rng.uniform(-0.1, 0.6)))
            offset = Vector((rng.uniform(0.3, 5.5), rng.uniform(-6, 6), rng.uniform(-4, 4)))
            leaf(point + offset, tip_direction, facing, rng.uniform(8.5, 14.0) * size_scale)

    # A thick mat along the top edge, spilling over it.
    for _ in range(150):
        point = Vector((rng.uniform(-9, 3), rng.uniform(-width * 0.5, width * 0.5), rng.uniform(-10, 9)))
        if point.x < 0:
            point.z = rng.uniform(2, 8)          # on top of the wall
        leaves_at(point, 1, 1.0)
    # Runners hanging down from it, long in the middle and short towards the sides.
    for strand in range(34):
        y = rng.uniform(-width * 0.5, width * 0.5)
        length = rng.uniform(35, 185) * (1.0 - 0.55 * abs(y) / (width * 0.5))
        x, z, step = rng.uniform(1.0, 3.5), 4.0, 0.0
        drift = rng.uniform(-0.22, 0.22)
        previous = Vector((x, y, z))
        while step < length:
            advance = rng.uniform(5, 9)
            step += advance
            y += drift * advance + rng.uniform(-2.0, 2.0)
            z -= advance
            x = max(0.8, x + rng.uniform(-0.6, 0.8))
            here = Vector((x, y, z))
            prop.tube(previous, here, 0.35, 'D_Bark', segments=3)
            previous = here
            leaves_at(here, rng.randint(1, 3), 1.0 - 0.3 * step / max(length, 1.0))
    return prop


def crate():
    prop = Prop('SM_DistrictCrate')
    beam, half = 14.0, 70.0
    mid = half - beam * 0.5
    for a in (-mid, mid):
        for b in (-mid, mid):
            prop.box((0, a, b), (140, beam, beam), 'D_Crate', bevel=0.7)
            prop.box((a, 0, b), (beam, 140 - 2 * beam, beam), 'D_Crate', bevel=0.7)
            prop.box((a, b, 0), (beam, beam, 140 - 2 * beam), 'D_Crate', bevel=0.7)
    prop.box((0, 0, 0), (134, 134, 134), 'D_Crate')
    # The mapping of every side: the two axes that lie in it. The brace follows the same two axes, from the low
    # corner of both to the high corner, which is where the texture has it.
    sides = {0: (Vector((0, 1, 0)), Vector((0, 0, 1))), 1: (Vector((1, 0, 0)), Vector((0, 0, 1))), 2: (Vector((1, 0, 0)), Vector((0, 1, 0)))}
    for axis, (u_axis, v_axis) in sides.items():
        normal = Vector([1.0 if i == axis else 0.0 for i in range(3)])
        along, across = (u_axis + v_axis).normalized(), (v_axis - u_axis).normalized()
        for sign in (-1.0, 1.0):
            turn = Matrix((along, across, normal)).transposed().to_4x4()
            prop.box(normal * sign * 68.1, ((140 - 2 * beam) * math.sqrt(2.0) + 4.0, 0.085 * 140, 2.2), 'D_Crate', bevel=0.4, turn=turn)
    for face in prop.bm.faces:
        axis = max(range(3), key=lambda i: abs(face.normal[i]))
        u_axis, v_axis = sides[axis]
        for loop in face.loops:
            loop[prop.uv].uv = ((loop.vert.co.dot(u_axis) + half) / 140.0, (loop.vert.co.dot(v_axis) + half) / 140.0)
    return prop


def build_all(art_dir):
    for obj in list(bpy.data.objects):
        bpy.data.objects.remove(obj, do_unlink=True)
    collection = bpy.context.scene.collection
    made = []
    for index, factory in enumerate((wall_lamp, ceiling_lamp, ivy, crate)):
        obj = factory().export(art_dir, collection)
        obj.location.x = index * 4.5
        made.append('%s:%d' % (obj.name, len(obj.data.polygons)))
    for name, color in (('D_Metal', (0.05, 0.055, 0.065, 1)), ('D_Glow', (1.0, 0.7, 0.35, 1)), ('D_Bark', (0.2, 0.13, 0.08, 1)), ('D_Leaf', (0.1, 0.35, 0.08, 1)), ('D_Crate', (0.5, 0.38, 0.24, 1))):
        bpy.data.materials[name].diffuse_color = color
    bpy.ops.wm.save_as_mainfile(filepath=os.path.join(art_dir, 'district_props.blend'))
    print('DISTRICT PROPS', made)


if not globals().get('DEFER'):
    build_all(ART_DIR)
