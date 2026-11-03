#!/usr/bin/env python3
"""Repository consistency checks only; this does not compile or run Unreal."""
import json
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]


def main():
    project = json.loads((ROOT / 'AlphaGame.uproject').read_text(encoding='utf-8'))
    assert project['Modules'][0]['Name'] == 'AlphaGame'
    assert project['EngineAssociation'] == '5.6'
    required = [
        'Source/AlphaGame.Target.cs', 'Source/AlphaGameEditor.Target.cs',
        'Source/AlphaGame/AlphaGame.Build.cs',
        'Source/AlphaGame/Combat/AlphaCombatComponent.h',
        'Source/AlphaGame/Characters/AlphaCharacter.h',
        'Source/AlphaGame/Characters/AlphaEnemy.h',
        'Source/AlphaGame/Characters/AlphaCompanion.h',
        'Source/AlphaGame/Rules/CompanionRules.h',
        'Source/AlphaGame/Rules/PlayerControlRules.h',
        'Source/AlphaGame/Tests/AlphaComboTests.cpp',
        'Source/AlphaGame/Tests/AlphaMovementTests.cpp',
        'Scripts/run_player_control_tests.py',
        'Scripts/test_player_controls.cpp',
        'Source/AlphaGame/Tests/AlphaCompanionTests.cpp',
        'Source/AlphaGame/Tests/AlphaPartyTests.cpp',
        'Scripts/run_companion_tests.py',
        'Scripts/test_companion_rules.cpp',
        'Source/AlphaGame/World/AlphaGameMode.h',
        'Source/AlphaGame/World/AlphaGameState.h',
        'Source/AlphaGame/World/AlphaPlayerController.h',
        'Source/AlphaGame/World/AlphaGameInstance.h',
        'Source/AlphaGame/UI/AlphaHUD.h',
        'Source/AlphaGame/Tests/AlphaCombatTests.cpp',
        'Source/AlphaGame/Tests/AlphaCoopTests.cpp',
        'Source/AlphaGame/Tests/AlphaAbilityTests.cpp',
        'Source/AlphaGame/Tests/AlphaEncounterTests.cpp',
        'Source/AlphaGame/Coop/AlphaReviveComponent.h',
        'Source/AlphaGame/Abilities/AlphaAbilityComponent.h',
        'Source/AlphaGame/Characters/Boss/AlphaForgeBoss.h',
        'Source/AlphaGame/UI/AlphaLobbyWidget.h',
        'Source/AlphaGame/Rules/CombatRules.h',
        'Source/AlphaGame/Rules/PresentationRules.h',
        'Source/AlphaGame/Visuals/AlphaArtLibrary.h',
        'Source/AlphaGame/Visuals/AlphaForgeEnvironment.h',
        'Source/AlphaGame/Visuals/AlphaCharacterVisualComponent.h',
        'Source/AlphaGame/Visuals/AlphaCombatEffect.h',
        'Source/AlphaGame/Visuals/AlphaCombatCamera.h',
        'Scripts/create_art_assets.py',
        'SourceArt/Surfaces/T_ForgeDetail.png',
        'SourceArt/Surfaces/T_ForgeNormal_N.png',
    ]
    for path in required:
        assert (ROOT / path).is_file(), f'Missing: {path}'
    config = (ROOT / 'Config/DefaultEngine.ini').read_text(encoding='utf-8')
    assert 'GameInstanceClass=/Script/AlphaGame.AlphaGameInstance' in config
    assert 'GlobalDefaultGameMode=/Script/AlphaGame.AlphaGameMode' in config
    plugins = {plugin['Name']: plugin for plugin in project.get('Plugins', [])}
    for plugin in ('PythonScriptPlugin', 'EditorScriptingUtilities'):
        assert plugins.get(plugin, {}).get('Enabled'), f'Missing art-generation plugin: {plugin}'
        assert plugins[plugin].get('TargetAllowList') == ['Editor'], f'Editor-only plugin target: {plugin}'
    headers = list((ROOT / 'Source/AlphaGame').rglob('*.h'))
    for path in headers:
        text = path.read_text(encoding='utf-8')
        includes = re.findall(r'^#include [<"]([^">]+)[">]', text, re.M)
        if 'GENERATED_BODY' in text:
            assert includes[-1] == path.stem + '.generated.h', f'Generated include order: {path}'
    settings = (ROOT / 'Config/DefaultInput.ini').read_text(encoding='utf-8')
    for path in (ROOT / 'Source/AlphaGame').rglob('*.cpp'):
        for binding in re.findall(r'Bind(?:Axis|Action)\(TEXT\("([^"]+)"\)', path.read_text(encoding='utf-8')):
            assert f'Name="{binding}"' in settings, f'Unconfigured input: {binding}'
    # Local quoted includes must resolve; engine headers are checked by UBT on Windows.
    source_root = ROOT / 'Source/AlphaGame'
    folders = {'Characters', 'Combat', 'World', 'UI', 'Abilities', 'Coop', 'Rules', 'Visuals'}
    for path in source_root.rglob('*'):
        if path.suffix not in {'.h', '.cpp'}:
            continue
        for include in re.findall(r'^#include "([^"]+)"', path.read_text(encoding='utf-8'), re.M):
            if include.split('/')[0] in folders:
                assert (source_root / include).is_file(), f'Unresolved local include: {path}: {include}'
    print(f'PASS: project JSON, {len(required)} required files, {len(headers)} headers, input mappings, local includes')
    names = []
    for path in (source_root / 'Tests').glob('*.cpp'):
        names.extend(re.findall(r'"(AlphaGame\.[A-Za-z0-9_.]+)"', path.read_text(encoding='utf-8')))
    assert names and len(names) == len(set(names)), 'Missing or duplicate automation test names'
    print(f'Found {len(names)} distinct Unreal automation tests in source (not executed).')
    print('Unreal compilation, automation, rendering, and LAN play remain unverified by this check.')


if __name__ == '__main__':
    main()
