"""The two sounds a shooter hears when the server confirms a hit: S_HitHead and S_HitBody in /Game/ArenaDuel/Audio.

Generated here from tones and noise, like the knife and flashbang sounds, so the project needs no recordings.
Played by the HUD without a place in the world (UArenaDuelHUDWidget::ShowHitMarker). Run with the editor closed;
an existing sound is replaced.

- S_HitHead: a bright, short metallic ring, a few inharmonic partials struck at once with a click in front. It
  has to cut through gunfire and be unmistakable, so it sits high, where the shots are not.
- S_HitBody: a dull, very short tick, there to be felt more than heard.
"""
import math
import os
import random
import struct
import wave

import unreal

AUDIO = '/Game/ArenaDuel/Audio'
LIB = unreal.EditorAssetLibrary
RATE = 44100


def head(seconds=0.42):
    count = int(seconds * RATE)
    rng = random.Random(11)
    out = []
    # A struck bar: partials that are not whole multiples of each other, the high ones dying first.
    partials = ((1760.0, 9.0, 1.0), (2637.0, 12.0, 0.7), (3951.0, 17.0, 0.45), (5274.0, 24.0, 0.3), (880.0, 14.0, 0.35))
    for index in range(count):
        t = index / float(RATE)
        tone = sum(level * math.sin(2.0 * math.pi * frequency * t) * math.exp(-decay * t) for frequency, decay, level in partials)
        click = rng.uniform(-1.0, 1.0) * math.exp(-900.0 * t) * 0.9
        attack = min(1.0, t / 0.0015)
        out.append((tone * 0.4 + click) * attack)
    return out


def body(seconds=0.09):
    count = int(seconds * RATE)
    rng = random.Random(12)
    out, state, phase = [], 0.0, 0.0
    for index in range(count):
        t = index / float(RATE)
        state += 0.18 * (rng.uniform(-1.0, 1.0) - state)
        phase += (520.0 - 2600.0 * t) / RATE
        out.append(math.sin(2.0 * math.pi * phase) * math.exp(-55.0 * t) * 0.8 + state * 1.6 * math.exp(-80.0 * t))
    return out


def write_wav(folder, name, samples):
    peak = max(0.001, max(abs(sample) for sample in samples))
    path = os.path.join(folder, name + '.wav')
    with wave.open(path, 'wb') as out:
        out.setnchannels(1)
        out.setsampwidth(2)
        out.setframerate(RATE)
        out.writeframes(b''.join(struct.pack('<h', int(max(-1.0, min(1.0, sample / peak * 0.9)) * 32767)) for sample in samples))
    return path


folder = os.path.join(unreal.Paths.project_saved_dir(), 'HitAudio')
if not os.path.isdir(folder):
    os.makedirs(folder)
made = []
for name, samples in (('S_HitHead', head()), ('S_HitBody', body())):
    task = unreal.AssetImportTask()
    task.set_editor_property('filename', write_wav(folder, name, samples))
    task.set_editor_property('destination_path', AUDIO)
    task.set_editor_property('destination_name', name)
    task.set_editor_property('automated', True)
    task.set_editor_property('save', True)
    task.set_editor_property('replace_existing', True)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    made.append('%s=%s' % (name, LIB.does_asset_exist(AUDIO + '/' + name)))
unreal.log('HIT SOUNDS %s' % made)
