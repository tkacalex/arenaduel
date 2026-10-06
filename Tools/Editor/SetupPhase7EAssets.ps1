param(
    [string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.8'
)
$ErrorActionPreference = 'Stop'
$project = (Resolve-Path (Join-Path $PSScriptRoot '..\..\ArenaDuel.uproject')).Path
$logPath = Join-Path (Split-Path $project) 'Saved\Logs\Phase7EAssetSetup.log'
$editor = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$script = Join-Path $PSScriptRoot 'SetupPhase7EAssets.py'
$guiEditor = Get-Process UnrealEditor -ErrorAction SilentlyContinue
if ($guiEditor) { throw 'Close only the Unreal Editor before running Phase 7E asset setup.' }
& $editor $project -unattended -nop4 -nosplash -nosound "-ExecutePythonScript=$script" '-EnablePlugins=PythonScriptPlugin,EditorScriptingUtilities' '-ExecCmds=Quit' "-abslog=$logPath"
if ($LASTEXITCODE -ne 0) { throw "Unreal Editor Python setup failed with exit code $LASTEXITCODE" }
$log = Get-Content $logPath -Raw
if ($log -notmatch 'PHASE7E ARENA PASS') { throw 'Arena validation marker missing. Inspect Saved/Logs/Phase7EAssetSetup.log.' }
