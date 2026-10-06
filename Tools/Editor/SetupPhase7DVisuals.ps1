param([string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.8')
$ErrorActionPreference = 'Stop'
$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$project = Join-Path $projectRoot 'ArenaDuel.uproject'
$editor = Join-Path $EngineRoot 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
if (Get-Process UnrealEditor -ErrorAction SilentlyContinue) { throw 'Close only Unreal Editor before rebuilding reflected code.' }
& (Join-Path $PSScriptRoot 'InstallPhase7DTemplateAssets.ps1') -EngineRoot $EngineRoot
& (Join-Path $EngineRoot 'Engine/Build/BatchFiles/Build.bat') ArenaDuelEditor Win64 Development "-Project=$project" -WaitMutex -NoHotReloadFromIDE
if ($LASTEXITCODE -ne 0) { throw 'Editor build failed.' }
$reportPath = Join-Path $projectRoot 'Saved/Automation/Phase7DAssetSetup'
& $editor $project -unattended -nop4 -nullrhi -nosplash -nosound '-ExecCmds=Automation RunTests ArenaDuel.VisualAssetSetup; Quit' '-TestExit=Automation Test Queue Empty' "-ReportExportPath=$reportPath"
if ($LASTEXITCODE -ne 0) { throw 'Unreal arm generation failed.' }
$report = Get-Content -Raw (Join-Path $reportPath 'index.json') | ConvertFrom-Json
if ($report.failed -ne 0 -or $report.notRun -ne 0 -or $report.inProcess -ne 0 -or !($report.tests | Where-Object { $_.fullTestPath -eq 'ArenaDuel.VisualAssetSetup.Arms' -and $_.state -eq 'Success' })) { throw 'Arm generation did not pass.' }
$log = Join-Path $projectRoot 'Saved/Logs/Phase7DVisualAssetSetup.log'
& $editor $project -unattended -nop4 -nullrhi -nosplash -nosound '-EnablePlugins=PythonScriptPlugin,EditorScriptingUtilities' "-ExecutePythonScript=$(Join-Path $PSScriptRoot 'SetupPhase7DVisuals.py')" "-abslog=$log"
if ($LASTEXITCODE -ne 0 -or !(Select-String -LiteralPath $log -SimpleMatch 'PHASE7D VISUAL ASSETS PASS') -or (Select-String -LiteralPath $log -SimpleMatch 'LogPython: Error:')) { throw 'Visual asset generation/load validation failed.' }
Write-Output 'Phase 7D assets ready. No project plugin entries or project maps were changed.'
