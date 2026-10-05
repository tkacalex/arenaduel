$ErrorActionPreference = 'Stop'

$ProjectRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$ProjectFile = Join-Path $ProjectRoot 'ArenaDuel.uproject'
$EngineRoot = 'C:\Program Files\Epic Games\UE_5.8'
$BuildTool = Join-Path $EngineRoot 'Engine\Build\BatchFiles\Build.bat'
$EditorCmd = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$ReportRoot = Join-Path $ProjectRoot 'Saved\Automation\Phase4Validation'
$LogRoot = Join-Path $ProjectRoot 'Saved\Logs'

if (-not (Test-Path -LiteralPath $ProjectFile)) { throw "Missing project: $ProjectFile" }
& $BuildTool ArenaDuelEditor Win64 Development "-Project=$ProjectFile" -WaitMutex -FromMsBuild
if ($LASTEXITCODE -ne 0) { throw "ArenaDuelEditor build failed with exit code $LASTEXITCODE" }

function Remove-GeneratedSecurityToken {
    # UE may regenerate the Win64-irrelevant Android File Server section during editor startup.
    # Remove only that generated section after the process exits; never print or stage its value.
    $engineConfig = Join-Path $ProjectRoot 'Config\DefaultEngine.ini'
    if (-not (Test-Path -LiteralPath $engineConfig)) { return }
    $contents = [IO.File]::ReadAllText($engineConfig)
    $cleaned = [regex]::Replace($contents, '(?ms)^\[/Script/AndroidFileServerEditor\.AndroidFileServerRuntimeSettings\].*?(?=^\[|\z)', '')
    $cleaned = $cleaned.TrimEnd("`r", "`n") + "`r`n"
    if ($cleaned -ne $contents) {
        [IO.File]::WriteAllText($engineConfig, $cleaned, (New-Object Text.UTF8Encoding($false)))
    }
}

function Invoke-Suite([string]$Suite, [string]$Name, [string[]]$RequiredTests, [int]$Run) {
    $reportPath = Join-Path $ReportRoot "$Name-$Run"
    $logPath = Join-Path $LogRoot "Phase4-$Name-$Run.log"
    New-Item -ItemType Directory -Force -Path $reportPath | Out-Null
    & $EditorCmd $ProjectFile -unattended -nop4 -nullrhi -nosplash `
        "-ExecCmds=Automation RunTests $Suite; Quit" `
        '-TestExit=Automation Test Queue Empty' `
        "-ReportExportPath=$reportPath" `
        "-abslog=$logPath"
    Remove-GeneratedSecurityToken
    if ($LASTEXITCODE -ne 0) { throw "$Suite run $Run failed with exit code $LASTEXITCODE" }
    $reportFile = Join-Path $reportPath 'index.json'
    if (-not (Test-Path -LiteralPath $reportFile)) { throw "Missing report: $reportFile" }
    $report = Get-Content -LiteralPath $reportFile -Raw | ConvertFrom-Json
    if ($report.failed -ne 0 -or $report.notRun -ne 0 -or $report.inProcess -ne 0) { throw "$Suite reported failures" }
    $executed = @($report.tests | ForEach-Object { $_.fullTestPath })
    foreach ($required in $RequiredTests) {
        if (-not ($executed -contains $required)) { throw "$Suite did not execute required test: $required" }
    }
}

$Phase3Tests = @(
    'ArenaDuel.Phase3.Network.FArenaDuelPhase3NetworkTest.FrameworkAndInputOwnership',
    'ArenaDuel.Phase3.MapRuntime'
)
$Phase4Tests = @(
    'ArenaDuel.Phase4.MapAndAssets',
    'ArenaDuel.Phase4.MovementState',
    'ArenaDuel.Phase4.Sprint',
    'ArenaDuel.Phase4.Crouch',
    'ArenaDuel.Phase4.Slide',
    'ArenaDuel.Phase4.SlideJump',
    'ArenaDuel.Phase4.AirControlTrajectory',
    'ArenaDuel.Phase4.StaminaLifecycle',
    'ArenaDuel.Phase4.WallRunEntry',
    'ArenaDuel.Phase4.WallRunInvalidCases',
    'ArenaDuel.Phase4.WallRunExit',
    'ArenaDuel.Phase4.WallRunReattach',
    'ArenaDuel.Phase4.WallJumpBehavior',
    'ArenaDuel.Phase4.VaultProgression',
    'ArenaDuel.Phase4.VaultInvalidCases',
    'ArenaDuel.Phase4.MantleProgression',
    'ArenaDuel.Phase4.MantleInvalidCases',
    'ArenaDuel.Phase4.TraversalCollisionSafety',
    'ArenaDuel.Phase4.Network.FArenaDuelPhase4NetworkTest.SprintAndCrouchIntent',
    'ArenaDuel.Phase4.Network.FArenaDuelPhase4NetworkTest.Slide',
    'ArenaDuel.Phase4.Network.FArenaDuelPhase4NetworkTest.SlideJump',
    'ArenaDuel.Phase4.Network.FArenaDuelPhase4NetworkTest.WallRun',
    'ArenaDuel.Phase4.Network.FArenaDuelPhase4NetworkTest.WallJump',
    'ArenaDuel.Phase4.Network.FArenaDuelPhase4NetworkTest.Stamina',
    'ArenaDuel.Phase4.Network.FArenaDuelPhase4NetworkTest.Traversal',
    'ArenaDuel.Phase4.Network.FArenaDuelPhase4NetworkTest.ClientServerConvergence',
    'ArenaDuel.Phase4.Network.FArenaDuelPhase4NetworkTest.CrossControlIsolation'
)

for ($run = 1; $run -le 2; $run++) {
    Invoke-Suite 'ArenaDuel.Phase3' 'Phase3' $Phase3Tests $run
    Invoke-Suite 'ArenaDuel.Phase4' 'Phase4' $Phase4Tests $run
}

Write-Output 'PHASE4_VALIDATION_PASS'
