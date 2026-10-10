"""Build a zombie body in Blender for ArenaDuel and export it as FBX.

Run inside Blender 5.x with the MPFB extension enabled (the MakeHuman add-on, assets CC0):

    ART_DIR = r"<repo>/Tools/Art"; VARIANT = "Normal"; exec(open(ART_DIR + "/build_zombie.py").read())

What it does: makes an MPFB human with the variant's build, gives it MPFB's game engine rig, poses that
rig joint by joint onto the bind pose of the Unreal mannequin (manny.fbx, exported from the project),
bakes the result, marks skin, shirt, trousers, shoes and eyes as material slots, binds the mesh to the
mannequin armature and exports SKM_Zombie_<Variant>.fbx. Sharing the mannequin skeleton means every
mannequin clip, socket and physics body fits without retargeting.
"""
import os

import bmesh
import bpy
from mathutils import Matrix, Vector

VARIANTS = {
    # gender, muscle, weight, limb thickness, torso thickness
    'Normal': ('male', 'averagemuscle', 'minweight', 1.00, 1.00),
    'Runner': ('male', 'minmuscle', 'minweight', 0.90, 0.92),
    'Armoured': ('male', 'maxmuscle', 'averageweight', 1.08, 1.10),
    'Brute': ('male', 'maxmuscle', 'maxweight', 1.18, 1.22),
    'Abomination': ('male', 'maxmuscle', 'maxweight', 1.30, 1.34),
}
FINGERS = ('thumb', 'index', 'middle', 'ring', 'pinky')


def build(art_dir, variant):
    gender, muscle, weight, limb, torso = VARIANTS[variant]
    for obj in list(bpy.data.objects):
        bpy.data.objects.remove(obj, do_unlink=True)
    scene = bpy.context.scene
    scene.MPFB_NH_phenotype_gender = gender
    scene.MPFB_NH_phenotype_muscle = muscle
    scene.MPFB_NH_phenotype_weight = weight
    scene.MPFB_NH_add_phenotype = True
    bpy.ops.mpfb.create_human()
    human = next(o for o in bpy.data.objects if o.type == 'MESH')
    bpy.context.view_layer.objects.active = human
    human.select_set(True)
    scene.MPFB_ADR_standard_rig = 'game_engine'
    scene.MPFB_ADR_import_weights = True
    bpy.ops.mpfb.add_standard_rig()
    rig = next(o for o in bpy.data.objects if o.type == 'ARMATURE')
    bpy.ops.import_scene.fbx(filepath=os.path.join(art_dir, 'manny.fbx'), automatic_bone_orientation=False, ignore_leaf_bones=False)
    manny = next(o for o in bpy.data.objects if o.type == 'ARMATURE' and o is not rig)
    for obj in [o for o in bpy.data.objects if o.type == 'MESH' and o is not human]:
        bpy.data.objects.remove(obj, do_unlink=True)

    # Eyes: two small spheres in the sockets, carried by the head.
    gi = {g.index: g.name for g in human.vertex_groups}
    eyes = []
    for name in ('joint-l-eye', 'joint-r-eye'):
        index = human.vertex_groups[name].index
        points = [v.co for v in human.data.vertices if any(g.group == index for g in v.groups)]
        eyes.append(sum(points, Vector()) / len(points))

    # --- Fit the rig to the mannequin's joints -----------------------------------------------------
    bpy.ops.object.select_all(action='DESELECT')
    bpy.context.view_layer.objects.active = rig
    rig.select_set(True)
    bpy.ops.object.mode_set(mode='EDIT')
    for bone in rig.data.edit_bones:
        bone.use_connect = False
        bone.inherit_scale = 'NONE'
    bpy.ops.object.mode_set(mode='POSE')
    to_rig = rig.matrix_world.inverted()

    def mj(name):
        return to_rig @ (manny.matrix_world @ manny.data.bones[name].head_local)

    def bj(name):
        return rig.data.bones[name].head_local.copy()

    # The game engine rig has three spine bones, the mannequin five. The chest goes to the mannequin's last
    # spine bone, the one the shoulders hang from, so chest and shoulders bend together.
    target = {b.name: b.name for b in rig.data.bones if b.name in manny.data.bones}
    target.update({'spine_01': 'spine_01', 'spine_02': 'spine_03', 'spine_03': 'spine_05'})

    def place(name, child, delta, width, override=None, keep=False):
        bone, pose = rig.data.bones[name], rig.pose.bones[name]
        rest = bone.matrix_local.to_3x3()
        length = 1.0
        if override is not None:
            turn = override
        elif keep or child is None:
            turn = delta
        else:
            before = delta @ (bj(child) - bj(name))
            after = mj(target[child]) - mj(target[name])
            turn = before.rotation_difference(after).to_matrix() @ delta
            length = after.length / max((bj(child) - bj(name)).length, 1e-6)
        matrix = (turn @ rest).to_4x4() @ Matrix.Diagonal((width, length if override is None else width, width, 1.0))
        matrix.translation = mj(target[name])
        pose.matrix = matrix
        bpy.context.view_layer.update()
        return turn, length

    def frame(origin, a, b):
        x = (a - origin).normalized()
        z = x.cross(b - origin).normalized()
        return Matrix((x, z.cross(x), z)).transposed()

    one = Matrix.Identity(3)
    d, _ = place('pelvis', None, one, torso)
    s1, _ = place('spine_01', 'spine_02', one, torso)
    s2, _ = place('spine_02', 'spine_03', s1, torso)
    s3, _ = place('spine_03', 'neck_01', s2, torso)
    n1, _ = place('neck_01', 'head', s3, 1.0)
    place('head', None, n1, 1.0)
    for side in ('_l', '_r'):
        d, _ = place('clavicle' + side, 'upperarm' + side, s3, torso)
        d, _ = place('upperarm' + side, 'lowerarm' + side, d, 1.15 * limb)
        d, _ = place('lowerarm' + side, 'hand' + side, d, 1.15 * limb)
        palm = frame(mj('hand' + side), mj('middle_01' + side), mj('index_01' + side)) @ frame(bj('hand' + side), bj('middle_01' + side), bj('index_01' + side)).inverted()
        place('hand' + side, None, None, 1.30, override=palm)
        for finger in FINGERS:
            a, _ = place('%s_01%s' % (finger, side), '%s_02%s' % (finger, side), palm, 1.30)
            b, _ = place('%s_02%s' % (finger, side), '%s_03%s' % (finger, side), a, 1.30)
            place('%s_03%s' % (finger, side), None, b, 1.30)
        d, _ = place('thigh' + side, 'calf' + side, one, limb)
        d, _ = place('calf' + side, 'foot' + side, d, limb)
        d, _ = place('foot' + side, 'ball' + side, d, 1.0)
        place('ball' + side, None, d, 1.0)
    bpy.ops.object.mode_set(mode='OBJECT')

    # --- Bake the fitted shape ---------------------------------------------------------------------
    bpy.ops.object.select_all(action='DESELECT')
    bpy.context.view_layer.objects.active = human
    human.select_set(True)
    if human.data.shape_keys:
        bpy.ops.object.shape_key_remove(all=True, apply_mix=True)
    # The eyes join the mesh before the bake so they move with the head.
    bm = bmesh.new()
    bm.from_mesh(human.data)
    deform = bm.verts.layers.deform.verify()
    head_group = human.vertex_groups['head'].index
    body_group = human.vertex_groups['body'].index
    eye_marker = human.vertex_groups.new(name='zombie_eyes').index
    for centre in eyes:
        made = bmesh.ops.create_uvsphere(bm, u_segments=12, v_segments=8, radius=0.0125, matrix=Matrix.Translation(centre))
        for vert in made['verts']:
            vert[deform][head_group] = 1.0
            vert[deform][body_group] = 1.0
            vert[deform][eye_marker] = 1.0
    bm.to_mesh(human.data)
    bm.free()
    for modifier in list(human.modifiers):
        if modifier.type != 'ARMATURE':
            human.modifiers.remove(modifier)
    bpy.context.view_layer.update()
    evaluated = human.evaluated_get(bpy.context.evaluated_depsgraph_get())
    baked = evaluated.to_mesh()
    coords = [v.co.copy() for v in baked.vertices]
    evaluated.to_mesh_clear()
    for modifier in list(human.modifiers):
        human.modifiers.remove(modifier)
    for vert, co in zip(human.data.vertices, coords):
        vert.co = co
    human.parent = None

    # --- Keep the body, mark the regions ------------------------------------------------------------
    gi = {g.index: g.name for g in human.vertex_groups}
    bm = bmesh.new()
    bm.from_mesh(human.data)
    deform = bm.verts.layers.deform.verify()
    bmesh.ops.delete(bm, geom=[v for v in bm.verts if body_group not in v[deform]], context='VERTS')
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    bm.to_mesh(human.data)
    bm.free()
    mesh = human.data
    slots = ['Z_Skin', 'Z_Shirt', 'Z_Trousers', 'Z_Shoes', 'Z_Eyes']
    for slot in slots:
        mesh.materials.append(bpy.data.materials.get(slot) or bpy.data.materials.new(slot))

    def J(name):
        return manny.matrix_world @ manny.data.bones[name].head_local

    region = []
    for vert in mesh.vertices:
        weights = {gi[g.group]: g.weight for g in vert.groups}
        if weights.get('zombie_eyes', 0.0) > 0.5:
            region.append(4)
            continue
        best = max((k for k in weights if k in rig.data.bones), key=lambda k: weights[k], default='head')
        side = '_l' if vert.co.x > 0 else '_r'
        if best.startswith(('foot', 'ball')):
            region.append(3)
        elif best.startswith(('thigh', 'pelvis')):
            region.append(2)
        elif best.startswith('calf'):
            # ragged trouser legs end above the ankle
            ankle, knee = J('foot' + side), J('calf' + side)
            region.append(2 if (vert.co - ankle).dot((knee - ankle).normalized()) > 0.10 else 0)
        elif best.startswith(('spine', 'clavicle')):
            region.append(1)
        elif best.startswith('upperarm'):
            # short, torn sleeves
            shoulder, elbow = J('upperarm' + side), J('lowerarm' + side)
            region.append(1 if (vert.co - shoulder).dot((elbow - shoulder).normalized()) < 0.16 else 0)
        else:
            region.append(0)
    push = (0.0, 0.006, 0.005, 0.006, 0.0)
    for vert in mesh.vertices:
        vert.co = vert.co + vert.normal * push[region[vert.index]]
    for poly in mesh.polygons:
        votes = [region[i] for i in poly.vertices]
        poly.material_index = max(set(votes), key=votes.count)
        poly.use_smooth = True

    # --- Weights onto the mannequin's bone names, then bind and export --------------------------------
    for group in list(human.vertex_groups):
        if group.name not in rig.data.bones:
            human.vertex_groups.remove(group)
    for group in human.vertex_groups:
        if target.get(group.name, group.name) != group.name:
            group.name = '__' + target[group.name]
    for group in human.vertex_groups:
        if group.name.startswith('__'):
            group.name = group.name[2:]
    bpy.ops.object.select_all(action='DESELECT')
    bpy.context.view_layer.objects.active = human
    human.select_set(True)
    bpy.ops.object.vertex_group_normalize_all(lock_active=False)
    bpy.data.objects.remove(rig, do_unlink=True)
    manny.name = 'root'
    human.name = 'Zombie_' + variant
    human.data.transform(manny.matrix_world.inverted() @ human.matrix_world)
    human.parent = manny
    human.matrix_parent_inverse.identity()
    human.matrix_basis.identity()
    human.modifiers.new('Armature', 'ARMATURE').object = manny
    bpy.context.view_layer.update()
    bpy.ops.wm.save_as_mainfile(filepath=os.path.join(art_dir, 'zombie_%s.blend' % variant.lower()))
    bpy.ops.object.select_all(action='DESELECT')
    for obj in (manny, human):
        obj.hide_set(False)
        obj.select_set(True)
    bpy.context.view_layer.objects.active = manny
    out = os.path.join(art_dir, 'SKM_Zombie_%s.fbx' % variant)
    bpy.ops.export_scene.fbx(filepath=out, use_selection=True, object_types={'ARMATURE', 'MESH'}, add_leaf_bones=False, bake_anim=False,
                             use_armature_deform_only=False, mesh_smooth_type='FACE', primary_bone_axis='Y', secondary_bone_axis='X',
                             armature_nodetype='NULL', apply_unit_scale=True, global_scale=1.0)
    print('ZOMBIE %s verts=%d faces=%d spine=%s file=%s' % (variant, len(mesh.vertices), len(mesh.polygons), {k: v for k, v in target.items() if k.startswith('spine')}, out))
    return human


build(ART_DIR, VARIANT)
