param(
    [Parameter(Mandatory=$true)][string]$EngineRoot,
    [ValidateSet('Build','Art','Test','Package')][string]$Action = 'Build'
)
$ErrorActionPreference = 'Stop'
$ProjectRoot = Split-Path -Parent $PSScriptRoot
$Project = Join-Path $ProjectRoot 'AlphaGame.uproject'
$Build = Join-Path $EngineRoot 'Engine/Build/BatchFiles/Build.bat'
$Editor = Join-Path $EngineRoot 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
$UAT = Join-Path $EngineRoot 'Engine/Build/BatchFiles/RunUAT.bat'
if (!(Test-Path $Build)) { throw 'EngineRoot must point to an installed Unreal Engine 5.6 directory.' }
& $Build AlphaGameEditor Win64 Development "-Project=$Project" -WaitMutex
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
# Every entry prepares art, including Build/Test on a clean checkout.
if ($Action -in @('Build', 'Art', 'Test', 'Package')) {
    $ArtReport = Join-Path $ProjectRoot ('Artifacts/Art/' + [guid]::NewGuid().ToString() + '.json')
    $PreviousArtReport = $env:ALPHA_ART_REPORT
    try {
        $env:ALPHA_ART_REPORT = $ArtReport
        $ArtScript = Join-Path $ProjectRoot 'Scripts/create_art_assets.py'
        & $Editor $Project -run=pythonscript "-script=$ArtScript" -unattended -nop4 -NullRHI -stdout
        $ArtExit = $LASTEXITCODE
    } finally {
        if ($null -eq $PreviousArtReport) { Remove-Item Env:ALPHA_ART_REPORT -ErrorAction SilentlyContinue }
        else { $env:ALPHA_ART_REPORT = $PreviousArtReport }
    }
    if ($ArtExit -ne 0) { exit $ArtExit }
    if (!(Test-Path $ArtReport)) { throw 'Art commandlet produced no current save report.' }
    $ArtResult = Get-Content -Raw $ArtReport | ConvertFrom-Json
    if ($ArtResult.status -ne 'saved' -or $ArtResult.generator -ne 'AlphaGame.Art.v1' -or
        $ArtResult.material -ne '/Game/Art/Materials/M_AlphaSurface' -or
        $ArtResult.effects_material -ne '/Game/Art/Materials/M_AlphaEffect' -or
        $ArtResult.revision -ne 3 -or $ArtResult.parameter_contract -ne 'checked') {
        throw "Art asset generation did not finish: $ArtReport"
    }
    foreach ($ArtPackage in @('Materials/M_AlphaSurface', 'Materials/M_AlphaEffect', 'Textures/T_ForgeDetail', 'Textures/T_ForgeNormal_N', 'Textures/T_AnimeSurfaceAtlas')) {
        if (!(Test-Path (Join-Path $ProjectRoot ("Content/Art/$ArtPackage.uasset")))) {
            throw "Art package is missing after save: $ArtPackage"
        }
    }
    Write-Host "Art save report: $ArtReport (visual rendering still requires validation)."
}
$AudioReport = Join-Path $ProjectRoot ('Artifacts/Audio/' + [guid]::NewGuid().ToString() + '.json')
$PreviousAudioReport = $env:ALPHA_AUDIO_REPORT
try {
    $env:ALPHA_AUDIO_REPORT = $AudioReport
    $AudioScript = Join-Path $ProjectRoot 'Scripts/create_audio_assets.py'
    & $Editor $Project -run=pythonscript "-script=$AudioScript" -unattended -nop4 -NullRHI -stdout
    $AudioExit = $LASTEXITCODE
} finally {
    if ($null -eq $PreviousAudioReport) { Remove-Item Env:ALPHA_AUDIO_REPORT -ErrorAction SilentlyContinue }
    else { $env:ALPHA_AUDIO_REPORT = $PreviousAudioReport }
}
if ($AudioExit -ne 0) { exit $AudioExit }
if (!(Test-Path $AudioReport)) { throw 'No current audio save report.' }
$AudioResult = Get-Content -Raw $AudioReport | ConvertFrom-Json
if ($AudioResult.status -ne 'saved' -or $AudioResult.generator -ne 'AlphaGame.Audio.v1' -or $AudioResult.revision -ne 1) {
    throw 'Audio import/save did not complete.'
}
$AudioNames = @('S_AlphaSlash','S_AlphaHit','S_AlphaPoise','S_AlphaRoll','S_AlphaSkill','S_AlphaUltimate')
if (@($AudioResult.assets).Count -ne $AudioNames.Count) { throw 'Audio asset report count mismatch.' }
foreach ($AudioName in $AudioNames) {
    if ($AudioResult.assets -notcontains "/Game/Audio/Generated/$AudioName" -or
        !(Test-Path (Join-Path $ProjectRoot "Content/Audio/Generated/$AudioName.uasset"))) {
        throw "Audio package missing after save: $AudioName"
    }
}
Write-Host "Audio save report: $AudioReport (playback still requires validation)."
$VisualReport = Join-Path $ProjectRoot ('Artifacts/Visuals/' + [guid]::NewGuid().ToString() + '.json')
$PreviousVisualReport = $env:ALPHA_VISUAL_REPORT
try {
    $env:ALPHA_VISUAL_REPORT = $VisualReport
    $VisualScript = Join-Path $ProjectRoot 'Scripts/create_visual_assets.py'
    & $Editor $Project -run=pythonscript "-script=$VisualScript" -unattended -nop4 -NullRHI -stdout
    $VisualExit = $LASTEXITCODE
} finally {
    if ($null -eq $PreviousVisualReport) { Remove-Item Env:ALPHA_VISUAL_REPORT -ErrorAction SilentlyContinue }
    else { $env:ALPHA_VISUAL_REPORT = $PreviousVisualReport }
}
if ($VisualExit -ne 0) { exit $VisualExit }
if (!(Test-Path $VisualReport)) { throw 'No current modeled character/backdrop save report.' }
$VisualResult = Get-Content -Raw $VisualReport | ConvertFrom-Json
if ($VisualResult.status -ne 'saved' -or $VisualResult.revision -ne 2) { throw 'Visual import did not finish.' }
$ExpectedVisuals = @('/Game/Art/Backdrops/T_ChapterPanoramas')
foreach ($Hero in 0..2) {
    foreach ($Part in @('Body','Head','Hair','LeftArm','RightArm','LeftLeg','RightLeg')) {
        $ExpectedVisuals += "/Game/Art/Characters/Generated/SM_AlphaHero${Hero}_$Part"
    }
}
foreach ($Role in 0..3) {
    foreach ($Part in @('Body','Head','Hair','LeftArm','RightArm','LeftLeg','RightLeg','Weapon')) {
        $ExpectedVisuals += "/Game/Art/Monsters/Generated/SM_AlphaMonster${Role}_$Part"
    }
}
$ExpectedVisuals += '/Game/Art/Monsters/Generated/SM_AlphaMonster1_Shield'
$ExpectedVisuals += '/Game/Art/MonsterEffects/T_MonsterEffects'
foreach ($Name in @('Slash','Impact','Fire','Smoke')) { $ExpectedVisuals += "/Game/Art/MonsterEffects/M_$Name" }
foreach ($Chapter in 0..2) { $ExpectedVisuals += "/Game/Art/Backdrops/M_Chapter$Chapter" }
if (@($VisualResult.assets).Count -ne $ExpectedVisuals.Count) { throw 'Visual asset count mismatch.' }
foreach ($Asset in $ExpectedVisuals) {
    $Relative = $Asset.Substring('/Game/'.Length)
    if ($VisualResult.assets -notcontains $Asset -or !(Test-Path (Join-Path $ProjectRoot "Content/$Relative.uasset"))) {
        throw "Missing visual asset: $Asset"
    }
}
Write-Host "Modeled characters and background assets saved: $VisualReport (rendering still requires validation)."
if ($Action -eq 'Test') {
    $Expected = @(Get-ChildItem (Join-Path $ProjectRoot 'Source/AlphaGame/Tests') -Filter '*.cpp' | ForEach-Object {
        $Source = Get-Content -Raw $_.FullName
        foreach ($Match in [regex]::Matches($Source, '"(AlphaGame\.[A-Za-z0-9_.]+)"')) {
            $Match.Groups[1].Value
        }
    } | Sort-Object -Unique)
    if ($Expected.Count -eq 0) { throw 'No AlphaGame automation tests found in source.' }
    $Report = Join-Path $ProjectRoot ('Artifacts/Tests/' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
    & $Editor $Project -unattended -nop4 -NullRHI '-ExecCmds=Automation RunTests AlphaGame.' '-TestExit=Automation Test Queue Empty' "-ReportExportPath=$Report" -log
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    $Index = Join-Path $Report 'index.json'
    if (!(Test-Path $Index)) { throw 'Automation did not produce a current report; tests are not verified.' }
    $Result = Get-Content -Raw $Index | ConvertFrom-Json
    $Completed = [int]$Result.succeeded + [int]$Result.succeededWithWarnings
    if ($Completed -ne $Expected.Count -or $Result.failed -gt 0 -or $Result.notRun -gt 0 -or $Result.inProcess -gt 0) {
        throw "Automation did not complete successfully. Inspect $Index"
    }
    foreach ($TestName in $Expected) {
        $Entries = @($Result.tests | Where-Object { $_.fullTestPath -eq $TestName })
        if ($Entries.Count -ne 1 -or $Entries[0].state -notin @('Success', 'SuccessWithWarnings')) {
            throw "Required test missing or unsuccessful: $TestName. Inspect $Index"
        }
    }
    Write-Host "Automation report: $Index"
}
if ($Action -eq 'Package') {
    $Output = Join-Path $ProjectRoot 'Artifacts/Windows'
    & $UAT BuildCookRun "-project=$Project" -noP4 -platform=Win64 -clientconfig=Development -build -cook '-map=/Engine/Maps/Entry' -stage -pak -archive "-archivedirectory=$Output" -utf8output
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}
