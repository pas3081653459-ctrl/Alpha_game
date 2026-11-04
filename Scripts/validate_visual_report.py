"""Validate current invocation's UE asset save report; does not validate rendering."""
import json
from pathlib import Path
import sys

root, report_path = map(Path, sys.argv[1:])
report = json.loads(report_path.read_text(encoding='utf-8'))
assert report.get('status') == 'saved' and report.get('revision') == 1, 'No current visual save report'
expected = {f'/Game/Art/Characters/Generated/SM_AlphaHero{hero}_{part}'
            for hero in range(3)
            for part in ('Body', 'Head', 'Hair', 'LeftArm', 'RightArm', 'LeftLeg', 'RightLeg')}
expected.update(f'/Game/Art/Backdrops/M_Chapter{chapter}' for chapter in range(3))
expected.add('/Game/Art/Backdrops/T_ChapterPanoramas')
assert expected == set(report.get('assets', [])), 'Incomplete visual asset report'
for asset in expected:
    assert (root / 'Content' / (asset.removeprefix('/Game/') + '.uasset')).is_file(), f'Missing saved package: {asset}'
print(f'Visual asset save report verified: {report_path}. UE rendering still needs verification.')
