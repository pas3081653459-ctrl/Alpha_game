"""Run in UE5.6 Editor Python: import owned original prototype sound sources.

Saved assets/report are not evidence of playback or platform compression quality.
"""
import hashlib
import json
import os
from pathlib import Path

import unreal

ROOT = Path(__file__).resolve().parents[1]
TAG = 'AlphaGame.GeneratedBy'
GENERATOR = 'AlphaGame.Audio.v1'
AUDIO_REVISION = 1
AUDIO_PATH = '/Game/Audio/Generated'
SOUND_NAMES = ('S_AlphaSlash', 'S_AlphaHit', 'S_AlphaPoise', 'S_AlphaRoll', 'S_AlphaSkill', 'S_AlphaUltimate')
LIB = unreal.EditorAssetLibrary


def source_fingerprint():
    digest = hashlib.sha256()
    for source in [Path(__file__), ROOT / 'Scripts/generate_audio_sources.py', ROOT / 'SourceAudio/manifest.json',
                   *(ROOT / 'SourceAudio' / (name + '.wav') for name in SOUND_NAMES)]:
        digest.update(str(source.relative_to(ROOT)).encode('utf-8'))
        digest.update(source.read_bytes())
    return digest.hexdigest()


def main():
    LIB.make_directory(AUDIO_PATH)
    assets = []
    for name in SOUND_NAMES:
        path = f'{AUDIO_PATH}/{name}'
        if LIB.does_asset_exist(path):
            existing = LIB.load_asset(path)
            if LIB.get_metadata_tag(existing, TAG) != GENERATOR:
                raise RuntimeError(f'Refusing to overwrite an unowned audio asset: {path}')
        source = ROOT / 'SourceAudio' / (name + '.wav')
        if not source.is_file():
            raise RuntimeError(f'Missing original sound source: {source}')
        if source.read_bytes().startswith(b'version https://git-lfs.github.com/spec/'):
            raise RuntimeError('Audio sources are Git LFS pointers. Install Git LFS, then run git lfs pull in the project directory.')
        task = unreal.AssetImportTask()
        task.set_editor_property('filename', str(source))
        task.set_editor_property('destination_path', AUDIO_PATH)
        task.set_editor_property('destination_name', name)
        task.set_editor_property('automated', True)
        task.set_editor_property('replace_existing', True)
        task.set_editor_property('save', False)
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
        sound = LIB.load_asset(path)
        if not sound or not isinstance(sound, unreal.SoundWave):
            raise RuntimeError(f'Expected imported SoundWave at {path}')
        sound.set_editor_property('looping', False)
        LIB.set_metadata_tag(sound, TAG, GENERATOR)
        if not LIB.save_loaded_asset(sound, False):
            raise RuntimeError(f'Sound asset save failed: {path}')
        assets.append(path)
    report = {
        'generator': GENERATOR, 'revision': AUDIO_REVISION, 'status': 'saved',
        'source_fingerprint': source_fingerprint(), 'assets': assets,
        'note': 'Asset import/save is not playback, compression quality or packaged-audio verification.',
    }
    path = Path(os.environ.get('ALPHA_AUDIO_REPORT', str(ROOT / 'Saved/AudioBuild.json')))
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    unreal.log(f'AlphaGame audio sources imported; save report: {path}')


if __name__ == '__main__':
    main()
