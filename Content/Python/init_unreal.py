"""Unreal Editor startup hook: build this project's generated art when sources change.

Uses only tagged generated assets. Commandlets use their explicit build step,
so they never accidentally generate twice or accept a stale saved report.
"""
import importlib.util
import json
from pathlib import Path

import unreal


def ensure_generated_art():
    command_line = unreal.SystemLibrary.get_command_line().lower()
    if '-run=' in command_line or '-executepythonscript=' in command_line:
        return
    root = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
    script = root / 'Scripts/create_art_assets.py'
    if not script.is_file():
        unreal.log_error(f'AlphaGame art generator is missing: {script}')
        return
    spec = importlib.util.spec_from_file_location('alpha_project_art_generator', script)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    expected = [module.MATERIAL_PATH, module.EFFECT_PATH,
                module.TEXTURE_PATH + '/T_ForgeDetail', module.TEXTURE_PATH + '/T_ForgeNormal_N',
                module.TEXTURE_PATH + '/T_AnimeSurfaceAtlas']
    report_path = root / 'Saved/ArtBuild.json'
    try:
        report = json.loads(report_path.read_text(encoding='utf-8'))
    except (OSError, ValueError):
        report = {}
    current = module.source_fingerprint()
    if (report.get('source_fingerprint') == current
            and report.get('revision') == module.ART_REVISION
            and all(unreal.EditorAssetLibrary.does_asset_exist(path) for path in expected)):
        unreal.log('AlphaGame generated art matches source fingerprint. Runtime check: AlphaArtCheck.')
        return
    unreal.log('AlphaGame: generating updated anime art assets before play...')
    module.main()
    unreal.log('AlphaGame art assets saved. Check material shader compilation before Play.')


try:
    ensure_generated_art()
except Exception as error:
    # Never pretend a failed auto-generation created usable colors. The runtime
    # also displays a warning if these assets/parameters cannot be loaded.
    unreal.log_error(f'AlphaGame art generation FAILED: {error}. Run Scripts/create_art_assets.py and inspect the full Python log.')


def ensure_generated_audio():
    command_line = unreal.SystemLibrary.get_command_line().lower()
    if '-run=' in command_line or '-executepythonscript=' in command_line:
        return
    root = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
    script = root / 'Scripts/create_audio_assets.py'
    spec = importlib.util.spec_from_file_location('alpha_project_audio_generator', script)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    try:
        report = json.loads((root / 'Saved/AudioBuild.json').read_text(encoding='utf-8'))
    except (OSError, ValueError):
        report = {}
    expected = [module.AUDIO_PATH + '/' + name for name in module.SOUND_NAMES]
    if (report.get('source_fingerprint') == module.source_fingerprint()
            and report.get('revision') == module.AUDIO_REVISION
            and all(unreal.EditorAssetLibrary.does_asset_exist(path) for path in expected)):
        return
    module.main()


try:
    ensure_generated_audio()
except Exception as error:
    unreal.log_error(f'AlphaGame audio generation FAILED: {error}. Run Scripts/create_audio_assets.py and inspect the Python log.')


def ensure_generated_visuals():
    command_line = unreal.SystemLibrary.get_command_line().lower()
    if '-run=' in command_line or '-executepythonscript=' in command_line:
        return
    root = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
    script = root / 'Scripts/create_visual_assets.py'
    spec = importlib.util.spec_from_file_location('alpha_project_visual_generator', script)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    try:
        report = json.loads((root / 'Saved/VisualBuild.json').read_text(encoding='utf-8'))
    except (OSError, ValueError):
        report = {}
    if (report.get('source_fingerprint') == module.source_fingerprint()
            and report.get('revision') == module.REVISION
            and all(unreal.EditorAssetLibrary.does_asset_exist(path) for path in module.expected_assets())):
        return
    module.main()


try:
    ensure_generated_visuals()
except Exception as error:
    unreal.log_error(f'AlphaGame modeled hero/backdrop import FAILED: {error}. Run Scripts/create_visual_assets.py and inspect the Python log.')
