"""Build the four firearms in Blender and export them as FBX.

    ART_DIR = r"<repo>/Tools/Art"; exec(open(ART_DIR + "/build_weapons.py").read())

The game's conventions are kept, so nothing in code has to move: the origin is the pistol grip, the
barrel points down +X at a height of 13, sizes are in centimetres of the oversized game mesh (the game
scales first person weapons to about 0.4), the muzzle lies at x = length + 3, slot 0 is the body and
slot 1 the accent the game recolours per archetype. Output: Weapons/SM_<Name>.fbx and weapons.blend.
"""
import math
import os

import bmesh
import bpy
from mathutils import Matrix, Vector

BODY, ACCENT = 0, 1
AXIS = 13.0


class Gun:
    def __init__(self, name):
        self.name = name
        self.bm = bmesh.new()

    def _finish(self, made, slot, smooth):
        faces = {f for v in made['verts'] for f in v.link_faces}
        for face in faces:
            face.material_index = slot
            face.smooth = smooth

    def box(self, x0, x1, z0, z1, width, slot=BODY, bevel=0.5, pitch=0.0, y=0.0):
        centre = Vector(((x0 + x1) * 0.5, y, (z0 + z1) * 0.5))
        matrix = Matrix.Translation(centre) @ Matrix.Rotation(math.radians(pitch), 4, 'Y') @ Matrix.Diagonal((x1 - x0, width, z1 - z0, 1.0))
        made = bmesh.ops.create_cube(self.bm, size=1.0, matrix=matrix)
        if bevel > 0.0:
            edges = list({e for v in made['verts'] for e in v.link_edges})
            limit = 0.45 * min(x1 - x0, width, z1 - z0)
            result = bmesh.ops.bevel(self.bm, geom=edges, offset=min(bevel, limit), segments=2, affect='EDGES', profile=0.5)
            made = {'verts': result['verts'] + [v for v in made['verts'] if v.is_valid]}
        self._finish(made, slot, False)

    def tube(self, x0, x1, radius, z=AXIS, slot=BODY, segments=16, y=0.0, radius_end=None):
        matrix = Matrix.Translation(Vector(((x0 + x1) * 0.5, y, z))) @ Matrix.Rotation(math.radians(90.0), 4, 'Y')
        made = bmesh.ops.create_cone(self.bm, cap_ends=True, segments=segments, radius1=radius, radius2=radius if radius_end is None else radius_end, depth=x1 - x0, matrix=matrix)
        self._finish(made, slot, segments >= 12)

    def export(self, art_dir, collection):
        mesh = bpy.data.meshes.new('SM_' + self.name)
        bmesh.ops.recalc_face_normals(self.bm, faces=self.bm.faces)
        self.bm.to_mesh(mesh)
        self.bm.free()
        for slot in ('W_Body', 'W_Accent'):
            mesh.materials.append(bpy.data.materials.get(slot) or bpy.data.materials.new(slot))
        obj = bpy.data.objects.new('SM_' + self.name, mesh)
        collection.objects.link(obj)
        # Modelled in centimetres, stored in metres.
        mesh.transform(Matrix.Scale(0.01, 4))
        bpy.ops.object.select_all(action='DESELECT')
        obj.select_set(True)
        bpy.context.view_layer.objects.active = obj
        folder = os.path.join(art_dir, 'Weapons')
        os.makedirs(folder, exist_ok=True)
        bpy.ops.export_scene.fbx(filepath=os.path.join(folder, 'SM_%s.fbx' % self.name), use_selection=True, object_types={'MESH'}, mesh_smooth_type='FACE', bake_anim=False)
        return obj


def common(gun, receiver_end, width, mag):
    """Receiver, grip, trigger guard and magazine: what every one of them has around the hand."""
    gun.box(-6, receiver_end, 9, 19, width, bevel=0.9)                 # upper receiver
    gun.box(-5, receiver_end - 6, 4.5, 9.5, width * 0.85, bevel=0.6)   # lower receiver
    gun.box(-4.2, 2.6, -13, 6, 5.2, bevel=1.3, pitch=-14)              # pistol grip, through the origin
    gun.box(2.5, 9.5, 0.6, 1.6, 2.2, bevel=0.3)                        # trigger guard, bottom
    gun.box(9.0, 10.2, 0.6, 5.0, 2.2, bevel=0.3)                       # trigger guard, front
    gun.box(4.6, 5.6, 1.8, 5.0, 1.0, bevel=0.2, pitch=12)              # trigger
    gun.box(1.0, 6.5, 15.2, 16.4, width + 0.5, ACCENT, bevel=0.2)      # selector strip on both sides
    if mag:
        x0, x1, bottom, curve = mag
        steps = 4
        for step in range(steps):
            top = 5.0 - (5.0 - bottom) * step / steps
            low = 5.0 - (5.0 - bottom) * (step + 1) / steps
            shift = curve * (step / (steps - 1)) ** 2
            gun.box(x0 + shift, x1 + shift, low - 0.4, top, width * 0.62, bevel=0.5, pitch=-curve * 2.2 * step / steps)
        gun.box(x0 + curve - 0.4, x1 + curve + 0.4, bottom - 1.2, bottom, width * 0.7, ACCENT, bevel=0.3)


def sights(gun, rear_x, front_x, base=20.6):
    gun.box(rear_x - 1.2, rear_x + 1.2, base, base + 4.2, 5.0, bevel=0.4)
    gun.box(rear_x - 0.8, rear_x + 0.8, base + 2.0, base + 4.6, 1.6, ACCENT, bevel=0.0)
    gun.box(front_x - 1.0, front_x + 1.0, base, base + 3.0, 3.6, bevel=0.4)
    gun.box(front_x - 0.5, front_x + 0.5, base + 3.0, base + 4.6, 0.9, ACCENT, bevel=0.0)


def arc_rifle():
    gun = Gun('ArcRifle')
    common(gun, 22, 7.4, (10.5, 16.5, -17.0, 3.0))
    gun.tube(22, 48, 4.7, segments=8)                                   # handguard
    for side in (-1, 1):
        for x in (26, 32, 38, 44):
            gun.box(x - 1.8, x + 1.8, 12.2, 13.8, 0.8, ACCENT, bevel=0.0, y=side * 4.3)   # vents
    gun.box(-4, 46, 19, 20.6, 3.4, bevel=0.3)                           # top rail
    gun.tube(48, 67, 1.7)                                               # barrel
    gun.box(49, 53, 13.5, 18.5, 3.2, bevel=0.5)                         # gas block
    gun.tube(66, 73, 2.5, segments=12)                                  # muzzle brake
    gun.tube(71.5, 73.2, 1.9, slot=ACCENT, segments=12)
    gun.tube(-28, -6, 2.1)                                              # buffer tube
    gun.box(-32, -13, 5.0, 17.5, 5.2, bevel=1.2)                        # stock
    gun.box(-34, -32, 3.0, 19.0, 5.8, bevel=0.6)                        # butt pad
    gun.box(-22, -15, 8.5, 12.0, 5.6, ACCENT, bevel=0.3)                # stock inlay
    gun.box(12, 17, 17.2, 18.6, 8.2, bevel=0.3)                         # ejection port cover
    sights(gun, -1.5, 44)
    return gun


def shade_smg():
    gun = Gun('ShadeSMG')
    common(gun, 20, 6.8, (10.0, 14.5, -20.0, 0.6))
    gun.tube(20, 37, 3.7, segments=10)                                  # barrel shroud
    for side in (-1, 1):
        for x in (23, 27.5, 32):
            gun.box(x - 1.3, x + 1.3, 12.3, 13.7, 0.8, ACCENT, bevel=0.0, y=side * 3.4)
    gun.tube(36, 45, 2.7, segments=14)                                  # suppressor
    gun.tube(44.2, 45.2, 1.3, slot=ACCENT, segments=12)
    gun.box(-4, 19, 19, 20.5, 3.2, bevel=0.3)                           # rail
    for z in (16.5, 9.5):                                               # wire stock
        gun.tube(-19, -6, 0.75, z=z, segments=8)
    gun.box(-21, -19, 6.5, 19.0, 4.6, bevel=0.6)
    gun.box(24, 28, 0.5, 9.5, 3.6, bevel=1.0, pitch=8)                  # stubby foregrip
    sights(gun, -1.0, 17, base=20.5)
    return gun


def rune_dmr():
    gun = Gun('RuneDMR')
    common(gun, 22, 6.4, (10.0, 16.0, -9.0, 0.4))
    gun.tube(22, 52, 3.9, segments=10)                                  # slim handguard
    gun.box(24, 50, 16.6, 18.0, 2.8, ACCENT, bevel=0.2)
    gun.tube(52, 87, 1.6)                                               # long barrel
    gun.tube(85, 93, 2.3, segments=12)                                  # muzzle brake
    gun.tube(91.6, 93.2, 1.8, slot=ACCENT, segments=12)
    gun.box(-36, -7, 5.0, 17.5, 5.0, bevel=1.4)                         # fixed stock
    gun.box(-30, -12, 17.0, 20.0, 4.2, bevel=0.9)                       # cheek rest
    gun.box(-38, -36, 3.0, 19.0, 5.6, bevel=0.6)
    gun.box(-4, 30, 19, 20.4, 3.2, bevel=0.3)                           # rail under the scope
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
    return gun


def hex_shotgun():
    gun = Gun('HexShotgun')
    common(gun, 24, 7.8, None)
    gun.tube(24, 62, 2.5)                                               # barrel
    gun.tube(24, 55, 2.2, z=8.4)                                        # magazine tube
    gun.box(53, 56, 7.0, 15.0, 4.0, bevel=0.5)                          # barrel band
    gun.tube(31, 47, 4.3, z=9.2, segments=10)                           # pump
    for x in (34, 38, 42):
        gun.tube(x - 0.5, x + 0.5, 4.6, z=9.2, slot=ACCENT, segments=10)
    gun.box(-33, -6, 5.5, 17.0, 5.4, bevel=1.5, pitch=-4)               # stock
    gun.box(-36, -33, 2.5, 18.0, 6.0, bevel=0.7)                        # butt pad
    gun.box(8, 17, 15.0, 17.6, 8.4, ACCENT, bevel=0.3)                  # ejection port
    gun.box(-4, 22, 19, 20.2, 3.0, bevel=0.3)                           # rib
    gun.box(60.0, 61.2, 15.4, 17.4, 1.2, ACCENT, bevel=0.2)             # bead sight
    gun.box(-1.5, 1.0, 20.2, 23.2, 4.6, bevel=0.4)                      # rear notch
    return gun


def build_all(art_dir):
    for obj in list(bpy.data.objects):
        bpy.data.objects.remove(obj, do_unlink=True)
    collection = bpy.context.scene.collection
    made = []
    for index, factory in enumerate((arc_rifle, shade_smg, rune_dmr, hex_shotgun)):
        obj = factory().export(art_dir, collection)
        obj.location.y = index * 0.4
        made.append('%s:%d' % (obj.name, len(obj.data.vertices)))
    for name, color in (('W_Body', (0.05, 0.055, 0.065, 1)), ('W_Accent', (0.1, 0.7, 0.9, 1))):
        bpy.data.materials[name].diffuse_color = color
    bpy.ops.wm.save_as_mainfile(filepath=os.path.join(art_dir, 'weapons.blend'))
    print('WEAPONS', made)


build_all(ART_DIR)
