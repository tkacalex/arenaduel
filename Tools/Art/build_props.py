"""Build the arena props and pickups in Blender and export them as FBX.

    ART_DIR = r"<repo>/Tools/Art"; exec(open(ART_DIR + "/build_props.py").read())

Every prop is centred on its origin and has exactly the size of the block it replaces, so cover heights,
gaps and collision in the maps stay what they were. Slot 0 is the main surface, slot 1 the trim.
Output: Props/SM_<Name>.fbx and props.blend.
"""
import math
import os

import bmesh
import bpy
from mathutils import Matrix, Vector

MAIN, TRIM, PANEL = 0, 1, 2


class Prop:
    def __init__(self, name):
        self.name = name
        self.bm = bmesh.new()
        self.panel = False

    def _paint(self, verts, slot, smooth=False):
        for face in {f for v in verts if v.is_valid for f in v.link_faces}:
            face.material_index = slot
            face.smooth = smooth

    def box(self, centre, size, slot=MAIN, bevel=1.0, turn=None):
        matrix = Matrix.Translation(Vector(centre)) @ (turn or Matrix.Identity(4)) @ Matrix.Diagonal((size[0], size[1], size[2], 1.0))
        made = bmesh.ops.create_cube(self.bm, size=1.0, matrix=matrix)
        verts = made['verts']
        if bevel > 0.0:
            edges = list({e for v in verts for e in v.link_edges})
            result = bmesh.ops.bevel(self.bm, geom=edges, offset=min(bevel, 0.45 * min(size)), segments=2, affect='EDGES', profile=0.5)
            verts = result['verts'] + [v for v in verts if v.is_valid]
        self._paint(verts, slot)

    def disc(self, centre, radius, height, slot=MAIN, segments=24, axis='Z'):
        turn = Matrix.Identity(4) if axis == 'Z' else Matrix.Rotation(math.radians(90.0), 4, 'Y' if axis == 'X' else 'X')
        made = bmesh.ops.create_cone(self.bm, cap_ends=True, segments=segments, radius1=radius, radius2=radius, depth=height, matrix=Matrix.Translation(Vector(centre)) @ turn)
        self._paint(made['verts'], slot, smooth=segments >= 12)

    def cone(self, centre, radius_bottom, radius_top, height, slot=MAIN, segments=96):
        """A round piece whose top is a little narrower than its foot: a chamfered edge."""
        made = bmesh.ops.create_cone(self.bm, cap_ends=True, segments=segments, radius1=radius_bottom, radius2=radius_top, depth=height, matrix=Matrix.Translation(Vector(centre)))
        self._paint(made['verts'], slot, smooth=True)

    def ring(self, inner, outer, z0, z1, slot=MAIN, segments=96):
        """A flat ring (an annulus) between two radii: top, bottom, inner and outer wall, smooth shaded."""
        bm = self.bm
        rows = []
        for radius, z in ((outer, z0), (outer, z1), (inner, z1), (inner, z0)):
            rows.append([bm.verts.new((math.cos(2.0 * math.pi * i / segments) * radius, math.sin(2.0 * math.pi * i / segments) * radius, z)) for i in range(segments)])
        made = []
        for row in range(4):
            a, b = rows[row], rows[(row + 1) % 4]
            for i in range(segments):
                j = (i + 1) % segments
                made.append(bm.faces.new((a[i], a[j], b[j], b[i])))
        for face in made:
            face.material_index = slot
            face.smooth = True

    def export(self, art_dir, collection):
        mesh = bpy.data.meshes.new('SM_' + self.name)
        bmesh.ops.recalc_face_normals(self.bm, faces=self.bm.faces)
        self.bm.to_mesh(mesh)
        self.bm.free()
        for slot in ('P_Main', 'P_Trim') + (('P_Panel',) if self.panel else ()):
            mesh.materials.append(bpy.data.materials.get(slot) or bpy.data.materials.new(slot))
        obj = bpy.data.objects.new('SM_' + self.name, mesh)
        collection.objects.link(obj)
        mesh.transform(Matrix.Scale(0.01, 4))
        bpy.ops.object.select_all(action='DESELECT')
        obj.select_set(True)
        bpy.context.view_layer.objects.active = obj
        folder = os.path.join(art_dir, 'Props')
        os.makedirs(folder, exist_ok=True)
        bpy.ops.export_scene.fbx(filepath=os.path.join(folder, 'SM_%s.fbx' % self.name), use_selection=True, object_types={'MESH'}, mesh_smooth_type='FACE', bake_anim=False)
        return obj


def crate():
    """A 140 cm wooden crate: boards inside a frame, a brace across each side."""
    prop = Prop('Crate')
    half, beam = 70.0, 9.0
    prop.box((0, 0, 0), (140 - beam, 140 - beam, 140 - beam), bevel=0.8)
    for a in (-1, 1):
        for b in (-1, 1):
            edge = half - beam * 0.5
            prop.box((a * edge, b * edge, 0), (beam, beam, 140), TRIM, bevel=1.0)
            prop.box((a * edge, 0, b * edge), (beam, 140, beam), TRIM, bevel=1.0)
            prop.box((0, a * edge, b * edge), (140, beam, beam), TRIM, bevel=1.0)
    brace = math.radians(45.0)
    for side in (-1, 1):
        prop.box((side * (half - 3.0), 0, 0), (4.0, 170.0, 9.0), TRIM, bevel=0.6, turn=Matrix.Rotation(brace * side, 4, 'X'))
        prop.box((0, side * (half - 3.0), 0), (170.0, 4.0, 9.0), TRIM, bevel=0.6, turn=Matrix.Rotation(brace * side, 4, 'Y'))
    return prop


def container():
    """A 900 x 240 x 260 cm shipping container: corrugated sides, corner posts, doors at one end."""
    prop = Prop('Container')
    prop.box((0, 0, 0), (892, 232, 252), bevel=1.5)
    for sx in (-1, 1):
        for sy in (-1, 1):
            prop.box((sx * 444, sy * 114, 0), (12, 12, 260), TRIM, bevel=1.5)
        for sz in (-1, 1):
            prop.box((sx * 444, 0, sz * 124), (12, 240, 12), TRIM, bevel=1.5)
    for sy in (-1, 1):
        for sz in (-1, 1):
            prop.box((0, sy * 114, sz * 124), (900, 12, 12), TRIM, bevel=1.5)
        x = -420.0
        while x <= 420.0:
            prop.box((x, sy * 117.5, 0), (14, 5, 232), bevel=1.2)        # corrugation
            x += 30.0
    y = -90.0
    while y <= 90.0:
        prop.box((0, y, 127.5), (872, 14, 5), bevel=1.2)                 # roof ribs
        y += 30.0
    # Doors at the +X end: two leaves, four lock rods with handles.
    for sy in (-1, 1):
        prop.box((447.5, sy * 55, 0), (5, 104, 236), bevel=1.0)
        for offset in (25, 80):
            prop.disc((451.5, sy * offset, 0), 1.8, 232, TRIM, segments=10)
            prop.box((453.0, sy * offset, -10), (3, 16, 5), TRIM, bevel=0.6)
    return prop


def ammo_box():
    """42 x 28 x 22 cm: a lidded steel box with latches, side handles and a bright band."""
    prop = Prop('AmmoBox')
    prop.box((0, 0, -3.5), (42, 28, 15), bevel=1.2)
    prop.box((0, 0, 7.5), (43, 29, 7), bevel=1.4)                         # lid
    prop.box((0, 0, 3.6), (43.4, 29.4, 1.2), TRIM, bevel=0.2)             # seam
    for side in (-1, 1):
        prop.box((side * 12, -14.8, 3.5), (4, 1.4, 7), TRIM, bevel=0.4)   # latches
        prop.disc((side * 21.8, 0, 2.0), 1.1, 14, TRIM, segments=8, axis='Y')   # handles
        prop.box((side * 21.4, -6.5, 0.5), (1.4, 1.6, 4), TRIM, bevel=0.3)
        prop.box((side * 21.4, 6.5, 0.5), (1.4, 1.6, 4), TRIM, bevel=0.3)
    prop.box((0, 0, -4.0), (42.6, 28.6, 4.0), TRIM, bevel=0.2)            # band all the way round
    for x in (-9, -3, 3, 9):                                              # four rounds on the lid
        prop.disc((x, 0, 11.4), 1.6, 1.0, TRIM, segments=10)
        prop.box((x, 5.0, 11.2), (3.2, 9.0, 0.8), TRIM, bevel=0.3)
    return prop


def heal_pad():
    """A round medical floor pad, 126 cm across and 5.5 cm high, built like a real one: a brushed steel plinth
    with a chamfered edge, a raised rim held by twelve bolts, a recessed light ring, and a frosted panel with
    a raised cross under it.

    Slot 0 is the light ring and the cross (the placed pads carry their green there), slot 1 the steel, slot 2
    the frosted panel.
    """
    prop = Prop('HealPad')
    prop.panel = True
    prop.cone((0, 0, 1.5), 63.0, 61.0, 3.0, TRIM)                 # plinth with a chamfered edge
    prop.ring(55.0, 61.0, 3.0, 5.5, TRIM)                          # raised rim
    prop.ring(54.2, 55.0, 3.0, 5.5, TRIM)                          # inner wall of the rim, a step down into the light
    prop.ring(51.0, 54.2, 3.0, 4.4, MAIN)                          # recessed light ring
    prop.ring(49.0, 51.0, 3.0, 5.0, TRIM)                          # frame of the panel
    prop.cone((0, 0, 3.7), 49.0, 49.0, 1.4, PANEL)                 # frosted panel, 4.4 cm high, below the frame
    prop.box((0, 0, 4.7), (60, 15, 0.8), MAIN, bevel=0.35)         # the cross, raised on the panel
    prop.box((0, 0, 4.7), (15, 60, 0.8), MAIN, bevel=0.35)
    for index in range(12):                                        # bolts on the rim
        angle = 2.0 * math.pi * (index + 0.5) / 12.0
        prop.cone((math.cos(angle) * 58.0, math.sin(angle) * 58.0, 5.8), 1.3, 1.1, 0.6, TRIM, segments=12)
    return prop


def build_all(art_dir):
    for obj in list(bpy.data.objects):
        bpy.data.objects.remove(obj, do_unlink=True)
    collection = bpy.context.scene.collection
    made = []
    only = [name for name in os.environ.get('ARENADUEL_PROPS', '').split(',') if name]
    for index, factory in enumerate((crate, container, ammo_box, heal_pad)):
        prop = factory()
        if only and prop.name not in only:
            prop.bm.free()
            continue
        obj = prop.export(art_dir, collection)
        obj.location.y = (-3.0, 3.0, -1.0, 0.5)[index]
        made.append('%s:%d' % (obj.name, len(obj.data.vertices)))
    for name, color in (('P_Main', (0.25, 0.2, 0.16, 1)), ('P_Trim', (0.08, 0.09, 0.1, 1)), ('P_Panel', (0.8, 0.85, 0.8, 1))):
        if name in bpy.data.materials:
            bpy.data.materials[name].diffuse_color = color
    if not only:
        bpy.ops.wm.save_as_mainfile(filepath=os.path.join(art_dir, 'props.blend'))
    print('PROPS', made)


build_all(ART_DIR)
