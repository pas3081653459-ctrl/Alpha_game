#!/usr/bin/env python3
"""Repository consistency checks only; this does not compile or run Unreal."""
import json
import hashlib
from pathlib import Path
import re
import struct

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
        'Source/AlphaGame/World/AlphaStoryNPC.h',
        'Source/AlphaGame/UI/AlphaDialogueWidget.h',
        'Source/AlphaGame/Visuals/AlphaChapterEnvironment.h',
        'Source/AlphaGame/Rules/StoryRules.h',
        'Source/AlphaGame/Tests/AlphaStoryTests.cpp',
        'Source/AlphaGame/Tests/AlphaStoryNPCTests.cpp',
        'Scripts/run_story_tests.py',
        'Scripts/test_story_rules.cpp',
        'Source/AlphaGame/Rules/EnemyTacticsRules.h',
        'Source/AlphaGame/Rules/ProfileRules.h',
        'Source/AlphaGame/World/AlphaProfileSave.h',
        'Source/AlphaGame/Tests/AlphaProfileTests.cpp',
        'Scripts/run_profile_tests.py',
        'Scripts/test_profile_rules.cpp',
        'Source/AlphaGame/Tests/AlphaEnemyTacticsTests.cpp',
        'Source/AlphaGame/Visuals/AlphaCombatAudio.h',
        'Scripts/run_enemy_tactics_tests.py',
        'Scripts/test_enemy_tactics.cpp',
        'Scripts/create_audio_assets.py',
        'Scripts/generate_audio_sources.py',
        'Scripts/validate_audio_report.py',
        'SourceAudio/manifest.json',
        'Source/AlphaGame/Rules/LevelLayoutRules.h',
        'Source/AlphaGame/World/AlphaRouteGeometry.h',
        'Source/AlphaGame/Tests/AlphaLevelLayoutTests.cpp',
        'Scripts/run_level_layout_tests.py',
        'Scripts/test_level_layout.cpp',
        'Scripts/Build-Mac.sh',
        'Content/Python/init_unreal.py',
        'SourceArt/Anime/T_AnimeSurfaceAtlas.png',
        'SourceArt/Anime/manifest.json',
        'Source/AlphaGame/World/AlphaBreakableProp.h',
        'Source/AlphaGame/Tests/AlphaEnvironmentTests.cpp',
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
    required.extend([
        'Source/AlphaGame/Visuals/AlphaScenicBackdrop.h',
        'Scripts/create_visual_assets.py', 'Scripts/create_backdrop_assets.py',
        'Scripts/Editor/import_character_meshes.py', 'Scripts/generate_character_meshes.py',
        'Scripts/validate_visual_report.py', 'SourceArt/Characters/manifest.json',
        'SourceArt/Characters/References/AnimeHeroTurnarounds.png',
        'SourceArt/Backdrops/T_ChapterPanoramas.png', 'SourceArt/Backdrops/manifest.json',
    ])
    required.extend(f'SourceArt/Characters/SM_AlphaHero{hero}_{part}.fbx'
                    for hero in range(3)
                    for part in ('Body','Head','Hair','LeftArm','RightArm','LeftLeg','RightLeg'))
    required.extend([
        'Scripts/generate_monster_meshes.py', 'Scripts/Editor/import_monster_meshes.py',
        'Scripts/create_monster_effect_assets.py', 'SourceArt/Monsters/manifest.json',
        'SourceArt/Monsters/References/AnimeMonsterTurnarounds.png',
        'SourceArt/MonsterEffects/T_MonsterEffects.png', 'SourceArt/MonsterEffects/manifest.json',
    ])
    required.extend(f'SourceArt/Monsters/SM_AlphaMonster{role}_{part}.fbx'
                    for role in range(4)
                    for part in ('Body','Head','Hair','LeftArm','RightArm','LeftLeg','RightLeg','Weapon'))
    required.append('SourceArt/Monsters/SM_AlphaMonster1_Shield.fbx')
    required.extend(f'SourceAudio/S_Alpha{name}.wav' for name in ('Slash','Hit','Poise','Roll','Skill','Ultimate'))
    for path in required:
        assert (ROOT / path).is_file(), f'Missing: {path}'
    art_source = ROOT / 'SourceArt/Anime/T_AnimeSurfaceAtlas.png'
    art_bytes = art_source.read_bytes()
    art_manifest = json.loads((art_source.parent / 'manifest.json').read_text(encoding='utf-8'))
    assert art_bytes[:8] == b'\x89PNG\r\n\x1a\n', 'Invalid anime atlas PNG signature'
    assert hashlib.sha256(art_bytes).hexdigest() == art_manifest['sha256'], 'Anime atlas source differs from provenance manifest'
    assert struct.unpack('>II', art_bytes[16:24]) == (art_manifest['width'], art_manifest['height']), 'Anime atlas dimensions mismatch'
    assert art_manifest['width'] == art_manifest['height'] and art_manifest['width'] % 2 == 0, 'Atlas requires an even square 2x2 layout'
    for folder, filename in [('SourceArt/Characters/References', 'AnimeHeroTurnarounds.png'),
                             ('SourceArt/Backdrops', 'T_ChapterPanoramas.png'),
                             ('SourceArt/Monsters/References', 'AnimeMonsterTurnarounds.png'),
                             ('SourceArt/MonsterEffects', 'T_MonsterEffects.png')]:
        image = ROOT / folder / filename
        data = image.read_bytes()
        manifest = json.loads((image.parent / 'manifest.json').read_text(encoding='utf-8'))
        assert data[:8] == b'\x89PNG\r\n\x1a\n', f'Invalid generated PNG: {image}'
        assert hashlib.sha256(data).hexdigest() == manifest['sha256'], f'Source image integrity failed: {image}'
        assert struct.unpack('>II', data[16:24]) == (manifest['width'], manifest['height'])
    character_manifest = json.loads((ROOT / 'SourceArt/Characters/manifest.json').read_text())
    assert len(character_manifest['meshes']) == 21, 'Missing authored character parts'
    for mesh in character_manifest['meshes']:
        source = ROOT / 'SourceArt/Characters' / (mesh['name'] + '.fbx')
        assert source.read_bytes().startswith(b'Kaydara FBX Binary'), f'FBX source is missing or an LFS pointer: {source}'
        assert mesh['vertices'] > 0 and mesh['triangles'] > 0, 'Empty character mesh'
    monster_manifest = json.loads((ROOT / 'SourceArt/Monsters/manifest.json').read_text())
    assert len(monster_manifest['meshes']) == 33, 'Missing monster model parts'
    for mesh in monster_manifest['meshes']:
        source = ROOT / 'SourceArt/Monsters' / (mesh['name'] + '.fbx')
        assert source.read_bytes().startswith(b'Kaydara FBX Binary'), f'Missing monster FBX source: {source}'
        assert mesh['vertices'] > 0 and mesh['triangles'] > 0, 'Empty monster mesh'
    vfx = (ROOT / 'SourceArt/MonsterEffects/T_MonsterEffects.png').read_bytes()
    vfx_width, vfx_height = struct.unpack('>II', vfx[16:24])
    assert vfx[25] == 6 and vfx_width == vfx_height and vfx_width % 2 == 0, 'VFX atlas needs an even square RGBA layout'
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
