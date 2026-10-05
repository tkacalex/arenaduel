$ErrorActionPreference = 'Stop'

$ProjectRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$ProjectFile = Join-Path $ProjectRoot 'ArenaDuel.uproject'
$EngineRoot = 'C:\Program Files\Epic Games\UE_5.8'
$BuildTool = Join-Path $EngineRoot 'Engine\Build\BatchFiles\Build.bat'
$EditorCmd = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$ReportRoot = Join-Path $ProjectRoot 'Saved\Automation\Phase3Validation'
$LogRoot = Join-Path $ProjectRoot 'Saved\Logs'

if (-not (Test-Path -LiteralPath $ProjectFile)) { throw "Missing project: $ProjectFile" }
if (-not (Test-Path -LiteralPath $BuildTool)) { throw "Missing Unreal build tool: $BuildTool" }
if (-not (Test-Path -LiteralPath $EditorCmd)) { throw "Missing Unreal command line editor: $EditorCmd" }

& $BuildTool ArenaDuelEditor Win64 Development "-Project=$ProjectFile" -WaitMutex -FromMsBuild
if ($LASTEXITCODE -ne 0) { throw "ArenaDuelEditor build failed with exit code $LASTEXITCODE" }

for ($run = 1; $run -le 2; $run++) {
    $reportPath = Join-Path $ReportRoot "Run$run"
    $logPath = Join-Path $LogRoot "Phase3Validation$run.log"
    New-Item -ItemType Directory -Force -Path $reportPath | Out-Null

    & $EditorCmd $ProjectFile -unattended -nop4 -nullrhi -nosplash -stdout -FullStdOutLogOutput `
        '-ExecCmds=Automation RunTests ArenaDuel.Phase3; Quit' `
        '-TestExit=Automation Test Queue Empty' `
        "-ReportExportPath=$reportPath" `
        "-abslog=$logPath"
    if ($LASTEXITCODE -ne 0) { throw "Phase 3 automation run $run failed with exit code $LASTEXITCODE" }

    $reportFile = Join-Path $reportPath 'index.json'
    if (-not (Test-Path -LiteralPath $reportFile)) { throw "Missing automation report: $reportFile" }
    $report = Get-Content -LiteralPath $reportFile -Raw | ConvertFrom-Json
    if ($report.failed -ne 0 -or $report.notRun -ne 0 -or $report.inProcess -ne 0) {
        throw "Phase 3 automation run $run reported failed, not-run, or in-process tests"
    }
    if (($report.succeeded + $report.succeededWithWarnings) -lt 2) { throw "Phase 3 automation run $run did not execute the expected tests" }
}

Write-Output 'PHASE3_VALIDATION_PASS'
