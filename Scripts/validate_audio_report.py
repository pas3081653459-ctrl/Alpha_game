"""Validate a current UE import/save report; this does not verify sound playback."""
import json
from pathlib import Path
import sys


def validate(root: Path, report: Path):
    result = json.loads(report.read_text(encoding='utf-8'))
    if (result.get('status') != 'saved' or result.get('generator') != 'AlphaGame.Audio.v1'
            or result.get('revision') != 1):
        raise RuntimeError('No current audio import/save report')
    names = ('S_AlphaSlash', 'S_AlphaHit', 'S_AlphaPoise', 'S_AlphaRoll', 'S_AlphaSkill', 'S_AlphaUltimate')
    expected = {'/Game/Audio/Generated/' + name for name in names}
    assets = result.get('assets', [])
    if len(assets) != len(expected) or set(assets) != expected:
        raise RuntimeError('Audio asset report is missing/duplicating expected cues')
    for name in names:
        if not (root / f'Content/Audio/Generated/{name}.uasset').is_file():
            raise RuntimeError(f'Missing imported sound: {name}')
    print(f'Audio save report valid: {report}. UE playback/packaging remain unverified.')


if __name__ == '__main__':
    if len(sys.argv) != 3:
        raise SystemExit('Usage: validate_audio_report.py PROJECT_ROOT CURRENT_REPORT')
    validate(Path(sys.argv[1]), Path(sys.argv[2]))
