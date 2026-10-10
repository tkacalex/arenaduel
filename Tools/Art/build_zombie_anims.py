"""Author the zombie shamble in Blender on the mannequin armature and export it as FBX.

    ART_DIR = r"<repo>/Tools/Art"; exec(open(ART_DIR + "/build_zombie_anims.py").read())

A 40 frame loop at 30 fps, in place: short stiff steps with the left leg, the right leg dragged with its
foot turned out, hips swaying, the trunk stooped and rocking against the hips, the head lolling. Every
pose is written as turns in character space (X to the character's left, -Y forward, Z up) about each
bone's own joint, parents first, then keyed. Output: A_Zombie_Shamble.fbx and zombie_anims.blend.
"""
import math
import os

import bpy
from mathutils import Matrix, Vector

FRAMES = 40
X, Y, Z = Vector((1, 0, 0)), Vector((0, 1, 0)), Vector((0, 0, 1))


def build(art_dir):
    for obj in list(bpy.data.objects):
        bpy.data.objects.remove(obj, do_unlink=True)
    for action in list(bpy.data.actions):
        bpy.data.actions.remove(action)
    bpy.ops.import_scene.fbx(filepath=os.path.join(art_dir, 'manny.fbx'), automatic_bone_orientation=False, ignore_leaf_bones=False)
    rig = next(o for o in bpy.data.objects if o.type == 'ARMATURE')
    rig.name = 'root'
    scene = bpy.context.scene
    scene.render.fps = 30
    scene.frame_start, scene.frame_end = 0, FRAMES
    bpy.ops.object.select_all(action='DESELECT')
    rig.select_set(True)
    bpy.context.view_layer.objects.active = rig
    bpy.ops.object.mode_set(mode='POSE')
    for bone in rig.pose.bones:
        bone.rotation_mode = 'QUATERNION'
    # The character space the turns are given in, expressed in the armature's own space.
    to_rig = rig.matrix_world.inverted().to_3x3()
    order = [b.name for b in rig.data.bones]   # parents come before children

    def pose(turns, lift):
        """turns: bone -> list of (axis, degrees). lift: pelvis offset in character space, centimetres."""
        for name in order:
            bone = rig.pose.bones[name]
            if bone.parent:
                inherited = bone.parent.matrix @ bone.parent.bone.matrix_local.inverted() @ bone.bone.matrix_local
            else:
                inherited = bone.bone.matrix_local.copy()
            matrix = inherited
            head = inherited.translation.copy()
            for axis, degrees in turns.get(name, ()):
                spin = Matrix.Rotation(math.radians(degrees), 4, (to_rig @ axis).normalized())
                matrix = Matrix.Translation(head) @ spin @ Matrix.Translation(-head) @ matrix
            if name == 'pelvis':
                matrix = Matrix.Translation(to_rig @ (lift * 0.01) / rig.matrix_world.to_scale().x * rig.matrix_world.to_scale().x) @ matrix
            if name in turns or name == 'pelvis':
                bone.matrix = matrix
                bpy.context.view_layer.update()

    keyed = ['pelvis', 'spine_02', 'spine_04', 'neck_01', 'head', 'thigh_l', 'calf_l', 'foot_l', 'thigh_r', 'calf_r', 'foot_r',
             'upperarm_l', 'lowerarm_l', 'upperarm_r', 'lowerarm_r']
    for frame in range(FRAMES + 1):
        p = 2.0 * math.pi * frame / FRAMES
        s, c = math.sin(p), math.cos(p)
        left_swing = -20.0 * s                       # forward is a negative turn about X
        left_knee = 34.0 * max(0.0, c) + 6.0         # bends as the leg comes through
        right_swing = 9.0 * s - 6.0                  # the dragged leg trails behind
        right_knee = 10.0 * max(0.0, -c) + 8.0
        turns = {
            'pelvis': [(Y, 5.0 * s), (Z, 7.0 * s)],
            'spine_02': [(X, 6.0 + 2.0 * math.sin(2 * p)), (Y, -6.0 * s)],
            'spine_04': [(X, 3.0), (Y, -3.0 * s)],
            'neck_01': [(X, -4.0)],
            'head': [(X, -4.0), (Y, 9.0 + 4.0 * math.sin(p + 1.0)), (Z, 5.0 * math.sin(p + 0.5))],
            'thigh_l': [(X, left_swing)],
            'calf_l': [(X, left_knee)],
            'foot_l': [(X, -(left_swing + left_knee) * 0.8)],
            'thigh_r': [(X, right_swing), (Z, -10.0)],
            'calf_r': [(X, right_knee)],
            'foot_r': [(X, -(right_swing + right_knee) * 0.6), (Z, -16.0)],
            'upperarm_l': [(X, -38.0 + 6.0 * s)],
            'lowerarm_l': [(X, -24.0)],
            'upperarm_r': [(X, -10.0 - 8.0 * s)],
            'lowerarm_r': [(X, -14.0)],
        }
        # Hips drop on each step and shift over the standing leg.
        pose(turns, Vector((2.5 * s, 0.0, -3.0 + 1.6 * math.cos(2 * p))))
        for name in keyed:
            bone = rig.pose.bones[name]
            bone.keyframe_insert('rotation_quaternion', frame=frame)
            if name == 'pelvis':
                bone.keyframe_insert('location', frame=frame)
    bpy.ops.object.mode_set(mode='OBJECT')
    action = rig.animation_data.action
    action.name = 'A_Zombie_Shamble'
    bpy.ops.wm.save_as_mainfile(filepath=os.path.join(art_dir, 'zombie_anims.blend'))
    bpy.ops.object.select_all(action='DESELECT')
    rig.select_set(True)
    bpy.context.view_layer.objects.active = rig
    bpy.ops.export_scene.fbx(filepath=os.path.join(art_dir, 'A_Zombie_Shamble.fbx'), use_selection=True, object_types={'ARMATURE'}, add_leaf_bones=False,
                             bake_anim=True, bake_anim_use_all_bones=True, bake_anim_use_nla_strips=False, bake_anim_use_all_actions=False, bake_anim_force_startend_keying=True,
                             bake_anim_simplify_factor=0.0, primary_bone_axis='Y', secondary_bone_axis='X', armature_nodetype='NULL', use_armature_deform_only=False)
    print('ZOMBIE ANIM frames=%d bones=%d' % (FRAMES, len(keyed)))


build(ART_DIR)
