"""Move crouch from C to Left Ctrl in IMC_Gameplay. Slide stays on Left Ctrl; the character decides which one a press means."""
import unreal

CONTEXT = '/Game/ArenaDuel/Input/IMC_Gameplay'
CROUCH = '/Game/ArenaDuel/Input/IA_Crouch'

def key(name):
    result = unreal.Key()
    result.set_editor_property('key_name', name)
    return result

context = unreal.load_asset(CONTEXT)
crouch = unreal.load_asset(CROUCH)
if not context or not crouch:
    raise RuntimeError('IMC_Gameplay or IA_Crouch is missing')
context.unmap_all_keys_from_action(crouch)
context.map_key(crouch, key('LeftControl'))
if not unreal.EditorAssetLibrary.save_asset(CONTEXT, only_if_is_dirty=False):
    raise RuntimeError('Could not save ' + CONTEXT)
unreal.log('CROUCH REMAP PASS')
