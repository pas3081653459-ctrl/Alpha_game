"""Validate current invocation's UE asset save report; does not validate rendering."""
import json
from pathlib import Path
import sys

root, report_path = map(Path, sys.argv[1:])
report = json.loads(report_path.read_text(encoding='utf-8'))
assert report.get('status') == 'saved' and report.get('revision') == 2, 'No current visual save report'
expected = {f'/Game/Art/Characters/Generated/SM_AlphaHero{hero}_{part}'
            for hero in range(3)
            for part in ('Body', 'Head', 'Hair', 'LeftArm', 'RightArm', 'LeftLeg', 'RightLeg')}
expected.update(f'/Game/Art/Backdrops/M_Chapter{chapter}' for chapter in range(3))
expected.add('/Game/Art/Backdrops/T_ChapterPanoramas')
expected.update(f'/Game/Art/Monsters/Generated/SM_AlphaMonster{role}_{part}'
                for role in range(4)
                for part in ('Body', 'Head', 'Hair', 'LeftArm', 'RightArm', 'LeftLeg', 'RightLeg', 'Weapon'))
expected.add('/Game/Art/Monsters/Generated/SM_AlphaMonster1_Shield')
expected.add('/Game/Art/MonsterEffects/T_MonsterEffects')
expected.update(f'/Game/Art/MonsterEffects/M_{name}' for name in ('Slash', 'Impact', 'Fire', 'Smoke'))
assert expected == set(report.get('assets', [])), 'Incomplete visual asset report'
for asset in expected:
    assert (root / 'Content' / (asset.removeprefix('/Game/') + '.uasset')).is_file(), f'Missing saved package: {asset}'
print(f'Visual asset save report verified: {report_path}. UE rendering still needs verification.')
