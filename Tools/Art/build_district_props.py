"""Model the set dressing of the 1v1 map in Blender and export it as FBX.

    ART_DIR = r"<repo>/Tools/Art"; exec(open(ART_DIR + "/build_district_props.py").read())

Sizes in centimetres. Material slots are named for what they are (D_Metal, D_Glow, D_Concrete, D_Soil,
D_Bark, D_Leaf); Tools/Editor/SetupDistrictProps.py gives them the map's materials. Leaves carry a
vertex colour so one flat material shows many greens.

- SM_WallLamp: a wall lamp. Origin on the wall, +X out of it, the light opening looks down.
- SM_CeilingLamp: a linear hall light on two rods. Origin on the ceiling, lens down.
- SM_Planter: a concrete tub with a small conifer. Origin at the floor, in the middle.
- SM_Ivy: ivy hanging from a wall top. Origin on the wall at the top edge, +X out of the wall, it hangs down -Z.
Output: District/SM_*.fbx and district_props.blend.
"""
import math
import os
import random

import bmesh
import bpy
from mathutils import Matrix, Vector

SLOTS = ('D_Metal', 'D_Glow', 'D_Concrete', 'D_Soil', 'D_Bark', 'D_Leaf')


class Prop:
    def __init__(self, name):
        self.name = name
        self.bm = bmesh.new()
        self.color = self.bm.loops.layers.color.new('Col')

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

    def export(self, art_dir, collection):
        mesh = bpy.data.meshes.new(self.name)
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


def green(rng, dark=0.0):
    shade = 0.55 + 0.45 * rng.random() - dark
    return (0.10 * shade + 0.04 * rng.random(), 0.34 * shade, 0.07 * shade, 1.0)


def wall_lamp():
    prop = Prop('SM_WallLamp')
    prop.box((1.5, 0, 0), (3, 22, 34), 'D_Metal', bevel=0.8)                 # back plate
    prop.box((7.5, 0, 9), (12, 20, 5), 'D_Metal', bevel=0.8)                 # hood
    prop.box((12.5, 0, 1), (2, 20, 14), 'D_Metal', bevel=0.5)                # front lip
    for side in (-1, 1):
        prop.box((7.5, side * 10.5, 2), (12, 1.2, 16), 'D_Metal', bevel=0.3)  # cheeks
    prop.box((7.0, 0, 0), (8, 18, 11), 'D_Glow', bevel=1.0)                  # the lit body behind the lip
    for z in (-3, 1, 5):
        prop.box((12.6, 0, z), (1.2, 19, 0.8), 'D_Metal')                    # louvres
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


def planter():
    rng = random.Random(5)
    prop = Prop('SM_Planter')
    prop.box((0, 0, 32), (120, 120, 64), 'D_Concrete', bevel=3.0)
    prop.box((0, 0, 66), (128, 128, 8), 'D_Concrete', bevel=2.0)             # rim
    prop.box((0, 0, 70.5), (104, 104, 2), 'D_Soil')
    prop.cone((0, 0, 100), 6, 4, 60, 'D_Bark', segments=8, color=(0.20, 0.13, 0.08, 1.0))
    # A small conifer: tiers of ragged skirts, wider at the bottom, each with its own greens.
    tiers = 7
    for tier in range(tiers):
        t = tier / (tiers - 1)
        radius = 58 * (1.0 - t) + 10
        height = 46 - 14 * t
        z = 118 + tier * 26
        verts = prop.cone((0, 0, z), radius, radius * 0.18, height, 'D_Leaf', segments=14, smooth=False)
        for vert in verts:
            if vert.co.z < z:                                                # the hem: pushed in and out, up and down
                spread = 0.72 + 0.5 * rng.random()
                vert.co.x *= spread
                vert.co.y *= spread
                vert.co.z += rng.uniform(-9, 5)
        for face in {f for v in verts for f in v.link_faces}:
            shade = green(rng, dark=0.12 * (1.0 - t))
            for loop in face.loops:
                loop[prop.color] = shade
    # Low growth around the trunk.
    for _ in range(40):
        angle, reach = rng.random() * math.tau, rng.uniform(14, 46)
        centre = Vector((math.cos(angle) * reach, math.sin(angle) * reach, 74 + rng.random() * 8))
        turn = Matrix.Rotation(rng.random() * math.tau, 4, 'Z') @ Matrix.Rotation(rng.uniform(0.3, 1.2), 4, 'Y')
        made = bmesh.ops.create_grid(prop.bm, x_segments=1, y_segments=1, size=rng.uniform(5, 9), matrix=Matrix.Translation(centre) @ turn)
        prop.paint({f for v in made['verts'] for f in v.link_faces}, 'D_Leaf', color=green(rng))
    return prop


def ivy():
    rng = random.Random(9)
    prop = Prop('SM_Ivy')
    width = 220.0
    for strand in range(26):
        y = rng.uniform(-width * 0.5, width * 0.5)
        length = rng.uniform(40, 170) * (1.0 - 0.5 * abs(y) / (width * 0.5))
        x, z, step = rng.uniform(1.5, 4.0), 6.0, 0.0
        drift = rng.uniform(-0.25, 0.25)
        previous = Vector((x, y, z))
        while step < length:
            advance = rng.uniform(6, 11)
            step += advance
            y += drift * advance + rng.uniform(-2.5, 2.5)
            z -= advance
            x = max(1.0, x + rng.uniform(-0.8, 1.0))
            here = Vector((x, y, z))
            # The stem, a thin strip.
            side = Vector((0, 0.5, 0))
            face = prop.bm.faces.new([prop.bm.verts.new(previous - side), prop.bm.verts.new(previous + side), prop.bm.verts.new(here + side), prop.bm.verts.new(here - side)])
            prop.paint((face,), 'D_Leaf', color=(0.07, 0.10, 0.04, 1.0))
            previous = here
            for _ in range(rng.randint(1, 3)):
                size = rng.uniform(4.5, 8.5) * (1.0 - 0.35 * step / max(length, 1.0))
                centre = here + Vector((rng.uniform(0.5, 4.0), rng.uniform(-7, 7), rng.uniform(-4, 4)))
                turn = Matrix.Rotation(rng.uniform(-0.9, 0.9), 4, 'X') @ Matrix.Rotation(math.radians(90) + rng.uniform(-0.6, 0.6), 4, 'Y') @ Matrix.Rotation(rng.random() * math.tau, 4, 'Z')
                made = bmesh.ops.create_grid(prop.bm, x_segments=1, y_segments=1, size=size, matrix=Matrix.Translation(centre) @ turn)
                # A leaf shape instead of a square: pull one corner out, the opposite one in.
                corners = made['verts']
                corners[0].co += (corners[0].co - centre) * 0.35
                corners[3].co -= (corners[3].co - centre) * 0.30
                prop.paint({f for v in corners for f in v.link_faces}, 'D_Leaf', color=green(rng, dark=0.15 * rng.random()))
    return prop


def build_all(art_dir):
    for obj in list(bpy.data.objects):
        bpy.data.objects.remove(obj, do_unlink=True)
    collection = bpy.context.scene.collection
    made = []
    for index, factory in enumerate((wall_lamp, ceiling_lamp, planter, ivy)):
        obj = factory().export(art_dir, collection)
        obj.location.x = index * 4.5
        made.append('%s:%d' % (obj.name, len(obj.data.polygons)))
    for name, color in (('D_Metal', (0.05, 0.055, 0.065, 1)), ('D_Glow', (1.0, 0.7, 0.35, 1)), ('D_Concrete', (0.55, 0.54, 0.52, 1)), ('D_Soil', (0.08, 0.06, 0.04, 1)), ('D_Bark', (0.2, 0.13, 0.08, 1)), ('D_Leaf', (0.1, 0.35, 0.08, 1))):
        bpy.data.materials[name].diffuse_color = color
    bpy.ops.wm.save_as_mainfile(filepath=os.path.join(art_dir, 'district_props.blend'))
    print('DISTRICT PROPS', made)


if not globals().get('DEFER'):
    build_all(ART_DIR)
