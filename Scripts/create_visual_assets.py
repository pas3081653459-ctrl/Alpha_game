"""UE editor-only: prepare original modeled heroes and painted chapter backdrops."""
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import unreal

ROOT = Path(__file__).resolve().parents[1]
REVISION = 2
GENERATORS = ('Scripts/Editor/import_character_meshes.py', 'Scripts/create_backdrop_assets.py',
              'Scripts/Editor/import_monster_meshes.py', 'Scripts/create_monster_effect_assets.py')


def modules():
    result = []
    for index, relative in enumerate(GENERATORS):
        spec = importlib.util.spec_from_file_location(f'alpha_visual_generator_{index}', ROOT / relative)
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        result.append(module)
    return result


def source_fingerprint():
    digest = hashlib.sha256(Path(__file__).read_bytes())
    for relative, module in zip(GENERATORS, modules()):
        digest.update((ROOT / relative).read_bytes())
        digest.update(module.source_fingerprint().encode())
    return digest.hexdigest()


def expected_assets():
    return ([f'/Game/Art/Characters/Generated/SM_AlphaHero{hero}_{part}'
             for hero in range(3)
             for part in ('Body', 'Head', 'Hair', 'LeftArm', 'RightArm', 'LeftLeg', 'RightLeg')]
            + [f'/Game/Art/Monsters/Generated/SM_AlphaMonster{role}_{part}'
               for role in range(4)
               for part in ('Body', 'Head', 'Hair', 'LeftArm', 'RightArm', 'LeftLeg', 'RightLeg', 'Weapon')]
            + ['/Game/Art/Monsters/Generated/SM_AlphaMonster1_Shield']
            + ['/Game/Art/MonsterEffects/T_MonsterEffects']
            + [f'/Game/Art/MonsterEffects/M_{name}' for name in ('Slash', 'Impact', 'Fire', 'Smoke')]
            + ['/Game/Art/Backdrops/T_ChapterPanoramas']
            + [f'/Game/Art/Backdrops/M_Chapter{chapter}' for chapter in range(3)])


def main():
    path = Path(os.environ.get('ALPHA_VISUAL_REPORT', str(ROOT / 'Saved/VisualBuild.json')))
    # A failed commandlet must never leave a previous success at this invocation's report path.
    path.parent.mkdir(parents=True, exist_ok=True)
    path.unlink(missing_ok=True)
    for module in modules():
        module.main()
    assets = expected_assets()
    for asset in assets:
        if not unreal.EditorAssetLibrary.does_asset_exist(asset):
            raise RuntimeError(f'Visual generation did not create required asset: {asset}')
    report = {'status': 'saved', 'revision': REVISION,
              'source_fingerprint': source_fingerprint(), 'assets': assets}
    path.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    unreal.log(f'AlphaGame modeled characters and backdrops saved: {path}; rendering remains unverified.')


if __name__ == '__main__':
    main()
