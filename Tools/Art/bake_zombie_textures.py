"""Bake the zombie textures in Blender: base colour (with a skin mask in alpha), normal and roughness.

Run inside Blender after build_zombie.py, with zombie_normal.blend's objects in the scene:

    ART_DIR = r"<repo>/Tools/Art"; exec(open(ART_DIR + "/bake_zombie_textures.py").read())

All five variants share the base mesh's UV layout, so one set serves them all. The skin is baked pale and
neutral; the game tints it per variant where the alpha mask says skin. Shirt, trousers and shoes carry
their own colours. Everything is procedural: noise for blotches and dirt, cell edges for veins and
creases, a fine weave for cloth. Output: T_Zombie_BaseColor.png, T_Zombie_Normal.png, T_Zombie_Roughness.png.
"""
import os

import bpy

SIZE = 2048


def nodes_for(mat, kind):
    mat.use_nodes = True
    tree = mat.node_tree
    tree.nodes.clear()
    out = tree.nodes.new('ShaderNodeOutputMaterial')
    bsdf = tree.nodes.new('ShaderNodeBsdfPrincipled')
    tree.links.new(bsdf.outputs['BSDF'], out.inputs['Surface'])
    coord = tree.nodes.new('ShaderNodeTexCoord')
    # The mesh is stored in centimetres; the patterns below are sized in metres.
    metres = tree.nodes.new('ShaderNodeVectorMath')
    metres.operation = 'SCALE'
    metres.inputs['Scale'].default_value = 0.01
    tree.links.new(coord.outputs['Object'], metres.inputs[0])

    def noise(scale, detail=4.0, rough=0.55):
        node = tree.nodes.new('ShaderNodeTexNoise')
        node.inputs['Scale'].default_value = scale
        node.inputs['Detail'].default_value = detail
        node.inputs['Roughness'].default_value = rough
        tree.links.new(metres.outputs['Vector'], node.inputs['Vector'])
        return node

    def ramp(source, stops):
        node = tree.nodes.new('ShaderNodeValToRGB')
        elements = node.color_ramp.elements
        while len(elements) < len(stops):
            elements.new(0.5)
        for element, (position, color) in zip(elements, stops):
            element.position = position
            element.color = color
        tree.links.new(source, node.inputs['Fac'])
        return node

    def mix(a, b, factor_socket=None, factor=0.5, mode='MIX'):
        node = tree.nodes.new('ShaderNodeMix')
        node.data_type = 'RGBA'
        node.blend_type = mode
        node.inputs[0].default_value = factor
        if factor_socket is not None:
            tree.links.new(factor_socket, node.inputs[0])
        for socket, value in ((node.inputs[6], a), (node.inputs[7], b)):
            if isinstance(value, tuple):
                socket.default_value = value
            else:
                tree.links.new(value, socket)
        return node.outputs[2]

    bump_height = tree.nodes.new('ShaderNodeMath')
    bump_height.operation = 'ADD'
    bump = tree.nodes.new('ShaderNodeBump')
    tree.links.new(bump_height.outputs[0], bump.inputs['Height'])
    tree.links.new(bump.outputs['Normal'], bsdf.inputs['Normal'])

    if kind == 'skin':
        blotch = ramp(noise(7.0).outputs['Fac'], [(0.35, (0.62, 0.63, 0.58, 1)), (0.62, (0.36, 0.37, 0.36, 1)), (0.80, (0.22, 0.20, 0.22, 1))])
        veins_tex = tree.nodes.new('ShaderNodeTexVoronoi')
        veins_tex.feature = 'DISTANCE_TO_EDGE'
        veins_tex.inputs['Scale'].default_value = 16.0
        tree.links.new(metres.outputs['Vector'], veins_tex.inputs['Vector'])
        veins = ramp(veins_tex.outputs['Distance'], [(0.0, (1, 1, 1, 1)), (0.035, (0, 0, 0, 1))])
        veined = mix(blotch.outputs['Color'], (0.16, 0.13, 0.20, 1), veins.outputs['Color'], mode='MIX')
        sores = ramp(noise(3.2, 2.0).outputs['Fac'], [(0.66, (0, 0, 0, 1)), (0.72, (1, 1, 1, 1))])
        color = mix(veined, (0.30, 0.05, 0.04, 1), sores.outputs['Color'])
        pores = noise(140.0, 2.0)
        tree.links.new(pores.outputs['Fac'], bump_height.inputs[0])
        tree.links.new(veins.outputs['Color'], bump_height.inputs[1])
        bump.inputs['Strength'].default_value = 0.35
        rough = ramp(noise(20.0).outputs['Fac'], [(0.3, (0.50, 0.50, 0.50, 1)), (0.7, (0.78, 0.78, 0.78, 1))])
    else:
        base = {'shirt': (0.060, 0.066, 0.078, 1), 'trousers': (0.032, 0.038, 0.055, 1), 'shoes': (0.020, 0.018, 0.016, 1)}[kind]
        dirt = ramp(noise(5.0).outputs['Fac'], [(0.40, (0, 0, 0, 1)), (0.75, (1, 1, 1, 1))])
        dirty = mix(base, (0.080, 0.062, 0.045, 1), dirt.outputs['Color'], mode='MIX')
        stain = ramp(noise(2.4, 3.0).outputs['Fac'], [(0.62, (0, 0, 0, 1)), (0.70, (1, 1, 1, 1))])
        color = mix(dirty, (0.11, 0.012, 0.010, 1), stain.outputs['Color']) if kind != 'shoes' else dirty
        if kind == 'shoes':
            grain = noise(60.0, 3.0)
            tree.links.new(grain.outputs['Fac'], bump_height.inputs[0])
            bump.inputs['Strength'].default_value = 0.25
            rough = ramp(noise(12.0).outputs['Fac'], [(0.3, (0.45, 0.45, 0.45, 1)), (0.7, (0.70, 0.70, 0.70, 1))])
        else:
            weave = tree.nodes.new('ShaderNodeTexWave')
            weave.inputs['Scale'].default_value = 260.0
            weave.inputs['Distortion'].default_value = 1.5
            tree.links.new(metres.outputs['Vector'], weave.inputs['Vector'])
            folds_tex = tree.nodes.new('ShaderNodeTexVoronoi')
            folds_tex.feature = 'DISTANCE_TO_EDGE'
            folds_tex.inputs['Scale'].default_value = 9.0
            tree.links.new(metres.outputs['Vector'], folds_tex.inputs['Vector'])
            folds = ramp(folds_tex.outputs['Distance'], [(0.0, (0, 0, 0, 1)), (0.12, (1, 1, 1, 1))])
            tree.links.new(weave.outputs['Fac'], bump_height.inputs[0])
            tree.links.new(folds.outputs['Color'], bump_height.inputs[1])
            bump.inputs['Strength'].default_value = 0.5
            rough = ramp(noise(10.0).outputs['Fac'], [(0.3, (0.80, 0.80, 0.80, 1)), (0.7, (0.96, 0.96, 0.96, 1))])
    if isinstance(color, tuple):
        bsdf.inputs['Base Color'].default_value = color
    else:
        tree.links.new(color, bsdf.inputs['Base Color'])
    tree.links.new(rough.outputs['Color'], bsdf.inputs['Roughness'])
    # Emission carries the skin mask for its own bake.
    bsdf.inputs['Emission Color'].default_value = (1, 1, 1, 1) if kind == 'skin' else (0, 0, 0, 1)
    bsdf.inputs['Emission Strength'].default_value = 1.0
    target = tree.nodes.new('ShaderNodeTexImage')
    target.name = 'BakeTarget'
    tree.nodes.active = target
    return target


def bake(art_dir):
    body = next(o for o in bpy.data.objects if o.type == 'MESH' and o.name.startswith('Zombie_'))
    kinds = {'Z_Skin': 'skin', 'Z_Shirt': 'shirt', 'Z_Trousers': 'trousers', 'Z_Shoes': 'shoes', 'Z_Eyes': 'shoes'}
    targets = [nodes_for(slot.material, kinds[slot.material.name.split('.')[0]]) for slot in body.material_slots]
    scene = bpy.context.scene
    scene.render.engine = 'CYCLES'
    scene.cycles.samples = 4
    scene.render.bake.margin = 12
    bpy.ops.object.select_all(action='DESELECT')
    body.hide_set(False)
    body.select_set(True)
    bpy.context.view_layer.objects.active = body
    images = {}
    for name, kind, colorspace in (('BaseColor', 'DIFFUSE', 'sRGB'), ('Mask', 'EMIT', 'Non-Color'), ('Normal', 'NORMAL', 'Non-Color'), ('Roughness', 'ROUGHNESS', 'Non-Color')):
        image = bpy.data.images.new('T_Zombie_' + name, SIZE, SIZE, alpha=True)
        image.colorspace_settings.name = colorspace
        for target in targets:
            target.image = image
        if kind == 'DIFFUSE':
            bpy.ops.object.bake(type=kind, pass_filter={'COLOR'})
        else:
            bpy.ops.object.bake(type=kind)
        images[name] = image
    # The skin mask goes into the alpha of the base colour.
    color = list(images['BaseColor'].pixels[:])
    mask = images['Mask'].pixels[:]
    color[3::4] = mask[0::4]
    images['BaseColor'].pixels[:] = color
    for name in ('BaseColor', 'Normal', 'Roughness'):
        image = images[name]
        image.filepath_raw = os.path.join(art_dir, 'T_Zombie_%s.png' % name)
        image.file_format = 'PNG'
        image.save()
    print('ZOMBIE TEXTURES', [os.path.basename(images[n].filepath_raw) for n in ('BaseColor', 'Normal', 'Roughness')])


bake(ART_DIR)
