#!/usr/bin/env bash
# Build the editor AND prepare generated art, so a clean checkout is not gray.
set -euo pipefail

if [[ $# -lt 1 || $# -gt 2 ]]; then
    echo 'Usage: Scripts/Build-Mac.sh "/Users/Shared/Epic Games/UE_5.6" [Build|Art|Test]' >&2
    exit 2
fi
engine_root="$1"
action="${2:-Build}"
case "$action" in Build|Art|Test) ;; *) echo 'Action must be Build, Art or Test.' >&2; exit 2 ;; esac
script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
project_root="$(cd -- "$script_dir/.." && pwd)"
project="$project_root/AlphaGame.uproject"
build="$engine_root/Engine/Build/BatchFiles/Mac/Build.sh"
editor="$engine_root/Engine/Binaries/Mac/UnrealEditor-Cmd"
if [[ ! -x "$editor" ]]; then
    editor="$engine_root/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor"
fi
if [[ ! -f "$build" || ! -x "$editor" ]]; then
    echo 'EngineRoot must contain the UE5.6 Mac build script and UnrealEditor executable.' >&2
    exit 2
fi
if ! command -v python3 >/dev/null 2>&1; then
    echo 'python3 is required to verify the current art/test reports.' >&2
    exit 2
fi
bash "$build" AlphaGameEditor Mac Development "-Project=$project" -WaitMutex
mkdir -p "$project_root/Artifacts/Art"
art_report="$(mktemp "$project_root/Artifacts/Art/mac-art.XXXXXX")"
ALPHA_ART_REPORT="$art_report" "$editor" "$project" -run=pythonscript \
    "-script=$script_dir/create_art_assets.py" -unattended -nop4 -NullRHI -stdout
python3 - "$project_root" "$art_report" <<'PY'
import json
from pathlib import Path
import sys
root, report = map(Path, sys.argv[1:])
result = json.loads(report.read_text(encoding='utf-8'))
assert result.get('status') == 'saved' and result.get('revision') == 3, 'No current art revision save report'
assert result.get('parameter_contract') == 'checked', 'Material parameter validation did not complete'
for package in ['Materials/M_AlphaSurface', 'Materials/M_AlphaEffect', 'Textures/T_ForgeDetail',
                'Textures/T_ForgeNormal_N', 'Textures/T_AnimeSurfaceAtlas']:
    assert (root / f'Content/Art/{package}.uasset').is_file(), f'Missing art asset: {package}'
print(f'Art save/parameter report: {report}. Rendering and shader errors still need UE verification.')
PY
if [[ "$action" == Test ]]; then
    mkdir -p "$project_root/Artifacts/Tests"
    test_report="$(mktemp -d "$project_root/Artifacts/Tests/mac-tests.XXXXXX")"
    "$editor" "$project" -unattended -nop4 -NullRHI \
        '-ExecCmds=Automation RunTests AlphaGame.' '-TestExit=Automation Test Queue Empty' \
        "-ReportExportPath=$test_report" -log
    python3 - "$project_root" "$test_report/index.json" <<'PY'
import json
from pathlib import Path
import re
import sys
root, report = map(Path, sys.argv[1:])
expected = set()
for source in (root / 'Source/AlphaGame/Tests').glob('*.cpp'):
    expected.update(re.findall(r'"(AlphaGame\.[A-Za-z0-9_.]+)"', source.read_text(encoding='utf-8')))
assert expected, 'No expected automation tests in source'
result = json.loads(report.read_text(encoding='utf-8'))
assert not any(result.get(field, 0) for field in ['failed', 'notRun', 'inProcess']), 'Unsuccessful/incomplete UE tests'
entries = result.get('tests', [])
for name in expected:
    found = [test for test in entries if test.get('fullTestPath') == name]
    assert len(found) == 1 and found[0].get('state') in ['Success', 'SuccessWithWarnings'], f'Missing/unsuccessful {name}'
assert result.get('succeeded', 0) + result.get('succeededWithWarnings', 0) == len(expected), 'Unexpected completed UE test count'
print(f'All {len(expected)} required UE tests completed: {report}')
PY
fi
