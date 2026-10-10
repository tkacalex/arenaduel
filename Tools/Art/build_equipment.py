"""Build the knife and the flashbang in Blender and export them as FBX.

    ART_DIR = r"<repo>/Tools/Art"; exec(open(ART_DIR + "/build_equipment.py").read())

Real sizes in centimetres, with the conventions of the code-built models they replace.
Knife: +X to the tip, +Z to the spine, origin in the middle of the grip; slots blade, grip, guard.
Flashbang: +Z up, origin in the middle of the body, lever on one side and pin on the other;
slots body, bands, lever. Blender's +Y becomes Unreal's -Y, which does not matter for either.
Output: Equipment/SM_Knife.fbx, Equipment/SM_Flashbang.fbx and equipment.blend.
"""
import math
import os

import bmesh
import bpy
from mathutils import Matrix, Vector


class Item:
    def __init__(self, name, slots):
        self.name, self.slots = name, slots
        self.bm = bmesh.new()

    def paint(self, verts, slot, smooth):
        for face in {f for v in verts if v.is_valid for f in v.link_faces}:
            face.material_index = slot
            face.smooth = smooth

    def box(self, centre, size, slot, bevel=0.1, turn=None):
        matrix = Matrix.Translation(Vector(centre)) @ (turn or Matrix.Identity(4)) @ Matrix.Diagonal((size[0], size[1], size[2], 1.0))
        made = bmesh.ops.create_cube(self.bm, size=1.0, matrix=matrix)
        verts = made['verts']
        if bevel > 0.0:
            result = bmesh.ops.bevel(self.bm, geom=list({e for v in verts for e in v.link_edges}), offset=min(bevel, 0.45 * min(size)), segments=2, affect='EDGES', profile=0.5)
            verts = result['verts'] + [v for v in verts if v.is_valid]
        self.paint(verts, slot, False)

    def round(self, centre, radius, height, slot, segments=20, axis='Z', radius_top=None, squash=1.0):
        turn = Matrix.Identity(4) if axis == 'Z' else Matrix.Rotation(math.radians(90.0), 4, 'Y' if axis == 'X' else 'X')
        shape = Matrix.Diagonal((1.0, squash, 1.0, 1.0)) if axis == 'X' else Matrix.Identity(4)
        made = bmesh.ops.create_cone(self.bm, cap_ends=True, segments=segments, radius1=radius, radius2=radius if radius_top is None else radius_top, depth=height,
                                     matrix=Matrix.Translation(Vector(centre)) @ shape @ turn)
        self.paint(made['verts'], slot, segments >= 10)

    def export(self, art_dir, collection):
        mesh = bpy.data.meshes.new('SM_' + self.name)
        bmesh.ops.remove_doubles(self.bm, verts=self.bm.verts, dist=0.0005)
        bmesh.ops.recalc_face_normals(self.bm, faces=self.bm.faces)
        self.bm.to_mesh(mesh)
        self.bm.free()
        for slot in self.slots:
            mesh.materials.append(bpy.data.materials.get(slot) or bpy.data.materials.new(slot))
        obj = bpy.data.objects.new('SM_' + self.name, mesh)
        collection.objects.link(obj)
        mesh.transform(Matrix.Scale(0.01, 4))
        bpy.ops.object.select_all(action='DESELECT')
        obj.select_set(True)
        bpy.context.view_layer.objects.active = obj
        folder = os.path.join(art_dir, 'Equipment')
        os.makedirs(folder, exist_ok=True)
        bpy.ops.export_scene.fbx(filepath=os.path.join(folder, 'SM_%s.fbx' % self.name), use_selection=True, object_types={'MESH'}, mesh_smooth_type='FACE', bake_anim=False)
        return obj


def knife():
    item = Item('Knife', ('E_Blade', 'E_Grip', 'E_Guard'))
    blade, grip, guard = 0, 1, 2
    # Grip: an oval handle with five ribs for the fingers, a pommel with a lanyard ring.
    item.round((-0.2, 0, 0), 1.45, 10.4, grip, segments=16, axis='X', squash=0.72)
    for x in (-4.2, -2.2, -0.2, 1.8, 3.8):
        item.round((x, 0, 0), 1.62, 0.9, grip, segments=16, axis='X', squash=0.74)
    item.round((-6.0, 0, 0), 1.7, 1.3, guard, segments=16, axis='X', squash=0.75)
    item.round((-7.0, 0, 0), 0.75, 0.5, guard, segments=12, axis='Y')
    # Guard: longer on the edge side.
    item.box((5.45, 0, -0.4), (0.9, 1.9, 5.6), guard, bevel=0.25)
    # Blade: a clip point with a flat spine, a ground bevel towards the edge and a fuller.
    profile = [(5.9, 1.55, 0.22), (15.0, 1.55, 0.20), (18.6, 1.15, 0.14), (23.2, -0.25, 0.0), (20.6, -1.25, 0.0), (17.0, -1.72, 0.0), (8.0, -1.78, 0.0), (5.9, -1.35, 0.10)]
    made = []
    for side in (1.0, -1.0):
        ridge = [item.bm.verts.new((x, side * (0.25 if 6.5 < x < 20.0 else t), z * 0.25 + 0.35)) for x, z, t in profile]
        rim = [item.bm.verts.new((x, side * t, z)) for x, z, t in profile]
        made += ridge + rim
        count = len(profile)
        for index in range(count):
            nxt = (index + 1) % count
            item.bm.faces.new((rim[index], rim[nxt], ridge[nxt], ridge[index]))
        item.bm.faces.new(ridge)
    item.paint(made, blade, False)
    # Spine and tang flat, where the blade has thickness, close up through remove_doubles and the two flanks.
    item.box((11.0, 0, 0.55), (8.5, 0.56, 0.5), guard, bevel=0.12)      # fuller, in the accent colour
    return item


def flashbang():
    item = Item('Flashbang', ('E_Body', 'E_Band', 'E_Lever'))
    body, band, lever = 0, 1, 2
    item.round((0, 0, -0.5), 2.55, 7.4, body, segments=24)
    # Hexagonal end caps with a row of vent holes each, as on a stun grenade.
    for z in (-4.6, 3.6):
        item.round((0, 0, z), 2.95, 0.9, band, segments=6)
        for index in range(6):
            angle = math.radians(60.0 * index + 30.0)
            item.round((math.cos(angle) * 2.56, math.sin(angle) * 2.56, z), 0.34, 0.5, lever, segments=8, axis='X' if index % 3 == 0 else 'Y')
    for z in (-2.2, 1.2):
        item.round((0, 0, z), 2.62, 0.35, band, segments=24)                 # identification bands
    item.round((0, 0, 4.5), 2.3, 0.9, band, segments=24, radius_top=1.3)
    item.round((0, 0, 5.8), 1.05, 1.7, band, segments=12)                    # fuse head
    item.box((0, -1.6, 6.25), (1.5, 3.4, 0.28), lever, bevel=0.08)           # safety lever over the top
    item.box((0, -3.15, 2.6), (1.4, 0.26, 7.4), lever, bevel=0.08)           # and down the side
    item.round((0, 1.9, 5.7), 0.14, 2.2, lever, segments=8, axis='Y')        # pin
    ring = bmesh.ops.create_circle(item.bm, cap_ends=False, segments=16, radius=1.05, matrix=Matrix.Translation(Vector((0, 3.7, 5.7))) @ Matrix.Rotation(math.radians(90.0), 4, 'Y'))
    solid = bmesh.ops.extrude_edge_only(item.bm, edges=list({e for v in ring['verts'] for e in v.link_edges}))
    grown = [g for g in solid['geom'] if isinstance(g, bmesh.types.BMVert)]
    bmesh.ops.scale(item.bm, vec=(1.0, 0.82, 0.82), space=Matrix.Translation(Vector((0, -3.7, -5.7))), verts=grown)
    bmesh.ops.translate(item.bm, vec=(0.22, 0, 0), verts=grown)
    item.paint(ring['verts'] + grown, lever, True)
    return item


def build_all(art_dir):
    for obj in list(bpy.data.objects):
        bpy.data.objects.remove(obj, do_unlink=True)
    collection = bpy.context.scene.collection
    made = []
    for index, factory in enumerate((knife, flashbang)):
        obj = factory().export(art_dir, collection)
        obj.location.y = index * 0.25
        made.append('%s:%d' % (obj.name, len(obj.data.vertices)))
    for name, color in (('E_Blade', (0.7, 0.72, 0.75, 1)), ('E_Grip', (0.03, 0.03, 0.035, 1)), ('E_Guard', (0.2, 0.6, 0.8, 1)),
                        ('E_Body', (0.12, 0.14, 0.12, 1)), ('E_Band', (0.55, 0.62, 0.68, 1)), ('E_Lever', (0.3, 0.3, 0.32, 1))):
        bpy.data.materials[name].diffuse_color = color
    bpy.ops.wm.save_as_mainfile(filepath=os.path.join(art_dir, 'equipment.blend'))
    print('EQUIPMENT', made)


build_all(ART_DIR)
