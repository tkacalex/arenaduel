"""Author the zombie clips in Blender on the mannequin armature and export each as FBX.

    ART_DIR = r"<repo>/Tools/Art"; exec(open(ART_DIR + "/build_zombie_anims.py").read())

All at 30 fps, in place. Every pose is written as turns in character space (X to the character's left,
-Y forward, Z up) about each bone's own joint, parents first, then keyed. A positive turn about X tips
whatever points up forward and whatever hangs down backward.

- A_Zombie_Shamble, 40 frame loop: short stiff steps with the left leg, the right leg dragged with its
  foot turned out, hips swaying, the trunk stooped and rocking against the hips, the head lolling.
- A_Zombie_Charge, 22 frame loop: the bosses' rush, trunk thrown forward, long heavy strides, arms pumping.
- A_Zombie_Slam, 33 frames: both arms hauled up over an arched back, then the whole body thrown down
  into a crouch; the hands land on frame 14, which is where the game expects the impact (42 %).
- A_Zombie_DeathBack and A_Zombie_DeathFront, 20 frames each: the knees give and the body goes over
  backwards, or folds forward. The game plays the first third and lets the ragdoll take the rest.

Output: A_Zombie_<Clip>.fbx and zombie_anims.blend.
"""
import math
import os

import bpy
from mathutils import Matrix, Vector

X, Y, Z = Vector((1, 0, 0)), Vector((0, 1, 0)), Vector((0, 0, 1))
KEYED = ['pelvis', 'spine_02', 'spine_04', 'neck_01', 'head', 'thigh_l', 'calf_l', 'foot_l', 'thigh_r', 'calf_r', 'foot_r',
         'clavicle_l', 'clavicle_r', 'upperarm_l', 'lowerarm_l', 'hand_l', 'upperarm_r', 'lowerarm_r', 'hand_r']


def shamble(frame, frames):
    p = 2.0 * math.pi * frame / frames
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
    return turns, Vector((2.5 * s, 0.0, -3.0 + 1.6 * math.cos(2 * p)))


def charge(frame, frames):
    p = 2.0 * math.pi * frame / frames
    s, c = math.sin(p), math.cos(p)
    left_swing, right_swing = -44.0 * s - 8.0, 44.0 * s - 8.0
    left_knee, right_knee = 62.0 * max(0.0, c) + 16.0, 62.0 * max(0.0, -c) + 16.0
    turns = {
        'pelvis': [(X, 10.0), (Z, 9.0 * s), (Y, 4.0 * s)],
        'spine_02': [(X, 20.0 + 3.0 * math.sin(2 * p)), (Z, -8.0 * s)],
        'spine_04': [(X, 12.0), (Z, -6.0 * s)],
        'neck_01': [(X, -14.0)],
        'head': [(X, -20.0), (Y, 4.0 * s)],
        'thigh_l': [(X, left_swing)],
        'calf_l': [(X, left_knee)],
        'foot_l': [(X, -(left_swing + left_knee) * 0.5)],
        'thigh_r': [(X, right_swing)],
        'calf_r': [(X, right_knee)],
        'foot_r': [(X, -(right_swing + right_knee) * 0.5)],
        # The arms pump against the legs, elbows bent, hands like hooks.
        'upperarm_l': [(X, -18.0 + 34.0 * s)],
        'lowerarm_l': [(X, -78.0)],
        'hand_l': [(X, -20.0)],
        'upperarm_r': [(X, -18.0 - 34.0 * s)],
        'lowerarm_r': [(X, -78.0)],
        'hand_r': [(X, -20.0)],
    }
    return turns, Vector((2.0 * s, 0.0, -9.0 + 4.0 * math.cos(2 * p)))


def blend(a, b, t):
    """Blend two poses given as bone -> {axis index: degrees} and two lifts."""
    pose_a, lift_a = a
    pose_b, lift_b = b
    out = {}
    for name in set(pose_a) | set(pose_b):
        ta, tb = pose_a.get(name, {}), pose_b.get(name, {})
        out[name] = {axis: ta.get(axis, 0.0) + (tb.get(axis, 0.0) - ta.get(axis, 0.0)) * t for axis in set(ta) | set(tb)}
    return out, lift_a.lerp(lift_b, t)


def as_turns(pose):
    axes = {'x': X, 'y': Y, 'z': Z}
    return {name: [(axes[axis], degrees) for axis, degrees in sorted(values.items())] for name, values in pose.items()}


def ease(t):
    t = max(0.0, min(1.0, t))
    return t * t * (3.0 - 2.0 * t)


def both(pose, bone, values):
    pose[bone + '_l'] = dict(values)
    pose[bone + '_r'] = dict(values)


def stooped():
    pose = {'spine_02': {'x': 8.0}, 'spine_04': {'x': 4.0}, 'head': {'x': -6.0}}
    both(pose, 'upperarm', {'x': -32.0})
    both(pose, 'lowerarm', {'x': -22.0})
    return pose, Vector((0.0, 0.0, -2.0))


def slam(frame, frames):
    raised = {'pelvis': {'x': -6.0}, 'spine_02': {'x': -16.0}, 'spine_04': {'x': -10.0}, 'neck_01': {'x': -6.0}, 'head': {'x': -8.0}}
    both(raised, 'clavicle', {'x': -18.0})
    both(raised, 'upperarm', {'x': -158.0})
    both(raised, 'lowerarm', {'x': -28.0})
    both(raised, 'thigh', {'x': -10.0})
    both(raised, 'calf', {'x': 20.0})
    both(raised, 'foot', {'x': -10.0})
    down = {'pelvis': {'x': 14.0}, 'spine_02': {'x': 34.0}, 'spine_04': {'x': 20.0}, 'neck_01': {'x': -12.0}, 'head': {'x': -16.0}}
    both(down, 'upperarm', {'x': -64.0})
    both(down, 'lowerarm', {'x': -6.0})
    both(down, 'hand', {'x': 20.0})
    both(down, 'thigh', {'x': -62.0})
    both(down, 'calf', {'x': 84.0})
    both(down, 'foot', {'x': -30.0})
    raised, down = (raised, Vector((0.0, 3.0, -5.0))), (down, Vector((0.0, -6.0, -30.0)))
    if frame <= 11:
        result = blend(stooped(), raised, ease(frame / 11.0))
    elif frame <= 14:
        t = (frame - 11) / 3.0
        result = blend(raised, down, t * t)          # accelerating into the ground
    elif frame <= 19:
        result = down
    else:
        result = blend(down, stooped(), ease((frame - 19) / float(frames - 19)))
    return as_turns(result[0]), result[1]


def death_back(frame, frames):
    buckle = {'pelvis': {'x': -22.0}, 'spine_02': {'x': -24.0, 'z': 8.0}, 'spine_04': {'x': -14.0}, 'neck_01': {'x': -10.0}, 'head': {'x': -22.0, 'y': 10.0},
              'upperarm_l': {'x': 14.0, 'y': 26.0}, 'upperarm_r': {'x': 30.0, 'y': -34.0}, 'lowerarm_l': {'x': -30.0}, 'lowerarm_r': {'x': -46.0},
              'thigh_l': {'x': -34.0}, 'thigh_r': {'x': -14.0}, 'calf_l': {'x': 66.0}, 'calf_r': {'x': 58.0}, 'foot_l': {'x': -10.0}, 'foot_r': {'x': 10.0}}
    floor = {'pelvis': {'x': -78.0}, 'spine_02': {'x': -12.0}, 'spine_04': {'x': -6.0}, 'head': {'x': -10.0, 'y': 18.0},
             'upperarm_l': {'x': 30.0, 'y': 40.0}, 'upperarm_r': {'x': 40.0, 'y': -44.0}, 'lowerarm_l': {'x': -20.0}, 'lowerarm_r': {'x': -30.0},
             'thigh_l': {'x': -60.0}, 'thigh_r': {'x': -30.0}, 'calf_l': {'x': 70.0}, 'calf_r': {'x': 40.0}}
    buckle, floor = (buckle, Vector((0.0, 14.0, -30.0))), (floor, Vector((0.0, 46.0, -78.0)))
    half = frames * 0.55
    result = blend(stooped(), buckle, ease(frame / half)) if frame <= half else blend(buckle, floor, ((frame - half) / (frames - half)) ** 1.5)
    return as_turns(result[0]), result[1]


def death_front(frame, frames):
    knees = {'pelvis': {'x': 16.0}, 'spine_02': {'x': 26.0, 'z': -8.0}, 'spine_04': {'x': 16.0}, 'neck_01': {'x': 10.0}, 'head': {'x': 26.0, 'y': -10.0},
             'upperarm_l': {'x': -16.0}, 'upperarm_r': {'x': -6.0}, 'lowerarm_l': {'x': -12.0}, 'lowerarm_r': {'x': -8.0},
             'thigh_l': {'x': -40.0}, 'thigh_r': {'x': -28.0}, 'calf_l': {'x': 104.0}, 'calf_r': {'x': 96.0}, 'foot_l': {'x': 20.0}, 'foot_r': {'x': 20.0}}
    floor = {'pelvis': {'x': 74.0}, 'spine_02': {'x': 10.0}, 'spine_04': {'x': 6.0}, 'head': {'x': 12.0, 'y': -24.0},
             'upperarm_l': {'x': -40.0, 'y': 20.0}, 'upperarm_r': {'x': -20.0, 'y': -24.0}, 'lowerarm_l': {'x': -50.0}, 'lowerarm_r': {'x': -20.0},
             'thigh_l': {'x': -20.0}, 'thigh_r': {'x': -8.0}, 'calf_l': {'x': 50.0}, 'calf_r': {'x': 30.0}, 'foot_l': {'x': 30.0}, 'foot_r': {'x': 30.0}}
    knees, floor = (knees, Vector((0.0, -8.0, -44.0))), (floor, Vector((0.0, -40.0, -80.0)))
    half = frames * 0.55
    result = blend(stooped(), knees, ease(frame / half)) if frame <= half else blend(knees, floor, ((frame - half) / (frames - half)) ** 1.5)
    return as_turns(result[0]), result[1]


CLIPS = (('Shamble', 40, shamble), ('Charge', 22, charge), ('Slam', 33, slam), ('DeathBack', 20, death_back), ('DeathFront', 20, death_front))


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
                matrix = Matrix.Translation(to_rig @ (lift * 0.01)) @ matrix
            if name in KEYED:
                bone.matrix = matrix
                bpy.context.view_layer.update()

    made = []
    for clip, frames, pose_at in CLIPS:
        if rig.animation_data:
            rig.animation_data.action = None
        for bone in rig.pose.bones:
            bone.matrix_basis = Matrix.Identity(4)
        scene.frame_start, scene.frame_end = 0, frames
        for frame in range(frames + 1):
            turns, lift = pose_at(frame, frames)
            pose(turns, lift)
            for name in KEYED:
                bone = rig.pose.bones[name]
                bone.keyframe_insert('rotation_quaternion', frame=frame)
                if name == 'pelvis':
                    bone.keyframe_insert('location', frame=frame)
        action = rig.animation_data.action
        action.name = 'A_Zombie_' + clip
        action.use_fake_user = True
        bpy.ops.object.mode_set(mode='OBJECT')
        bpy.ops.object.select_all(action='DESELECT')
        rig.select_set(True)
        bpy.context.view_layer.objects.active = rig
        bpy.ops.export_scene.fbx(filepath=os.path.join(art_dir, 'A_Zombie_%s.fbx' % clip), use_selection=True, object_types={'ARMATURE'}, add_leaf_bones=False,
                                 bake_anim=True, bake_anim_use_all_bones=True, bake_anim_use_nla_strips=False, bake_anim_use_all_actions=False, bake_anim_force_startend_keying=True,
                                 bake_anim_simplify_factor=0.0, primary_bone_axis='Y', secondary_bone_axis='X', armature_nodetype='NULL', use_armature_deform_only=False)
        bpy.ops.object.mode_set(mode='POSE')
        made.append('%s:%d' % (clip, frames))
    bpy.ops.object.mode_set(mode='OBJECT')
    bpy.ops.wm.save_as_mainfile(filepath=os.path.join(art_dir, 'zombie_anims.blend'))
    print('ZOMBIE ANIMS', made)


if not globals().get('DEFER'):
    build(ART_DIR)
