"""Import only the zombie clips built by Tools/Art/build_zombie_anims.py.

Tools/Editor/SetupZombieAssets.py imports them too, together with the bodies and materials; this is
the short way after changing nothing but the animation. Run with the editor closed.
"""
import os
import unreal

DEST = '/Game/ArenaDuel/Characters/Zombies'
ART = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'Art'))
LIB = unreal.EditorAssetLibrary
skeleton = unreal.load_asset('/Game/Characters/Mannequins/Meshes/SK_Mannequin')
clips = []
for clip in ('A_Zombie_Shamble', 'A_Zombie_Charge', 'A_Zombie_Slam', 'A_Zombie_DeathBack', 'A_Zombie_DeathFront'):
    source = os.path.join(ART, clip + '.fbx')
    if not os.path.isfile(source):
        continue
    task = unreal.AssetImportTask()
    task.set_editor_property('filename', source)
    task.set_editor_property('destination_path', DEST)
    task.set_editor_property('destination_name', clip)
    task.set_editor_property('automated', True)
    task.set_editor_property('save', True)
    task.set_editor_property('replace_existing', True)
    ui = unreal.FbxImportUI()
    ui.set_editor_property('import_mesh', False)
    ui.set_editor_property('import_as_skeletal', True)
    ui.set_editor_property('import_animations', True)
    ui.set_editor_property('import_materials', False)
    ui.set_editor_property('import_textures', False)
    ui.set_editor_property('mesh_type_to_import', unreal.FBXImportType.FBXIT_ANIMATION)
    ui.set_editor_property('skeleton', skeleton)
    task.set_editor_property('options', ui)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    asset = unreal.load_asset(DEST + '/' + clip)
    clips.append('%s=%s' % (clip, '%.2fs' % asset.get_play_length() if asset else 'missing'))
unreal.log('ZOMBIE CLIPS %s' % clips)
