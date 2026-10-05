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

function Invoke-Suite([string]$Suite, [string]$Name, [int]$ExpectedTests, [int]$Run) {
    $reportPath = Join-Path $ReportRoot "$Name-$Run"
    $logPath = Join-Path $LogRoot "Phase4-$Name-$Run.log"
    New-Item -ItemType Directory -Force -Path $reportPath | Out-Null
    & $EditorCmd $ProjectFile -unattended -nop4 -nullrhi -nosplash `
        "-ExecCmds=Automation RunTests $Suite; Quit" `
        '-TestExit=Automation Test Queue Empty' `
        "-ReportExportPath=$reportPath" `
        "-abslog=$logPath"
    if ($LASTEXITCODE -ne 0) { throw "$Suite run $Run failed with exit code $LASTEXITCODE" }
    $reportFile = Join-Path $reportPath 'index.json'
    if (-not (Test-Path -LiteralPath $reportFile)) { throw "Missing report: $reportFile" }
    $report = Get-Content -LiteralPath $reportFile -Raw | ConvertFrom-Json
    if ($report.failed -ne 0 -or $report.notRun -ne 0 -or $report.inProcess -ne 0) { throw "$Suite reported failures" }
    if (($report.succeeded + $report.succeededWithWarnings) -lt $ExpectedTests) { throw "$Suite did not execute the expected tests" }
}

for ($run = 1; $run -le 2; $run++) {
    Invoke-Suite 'ArenaDuel.Phase3' 'Phase3' 2 $run
    Invoke-Suite 'ArenaDuel.Phase4' 'Phase4' 13 $run
}

Write-Output 'PHASE4_VALIDATION_PASS'
