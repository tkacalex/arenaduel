param([string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.8')
$ErrorActionPreference = 'Stop'
$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$sourceRoot = Join-Path $EngineRoot 'Templates/TemplateResources/High/Characters/Content'
# Preserve Epic package paths, including material/texture references. Never overwrite existing assets.
$files = Get-ChildItem (Join-Path $sourceRoot 'Mannequins') -Recurse -File | Where-Object {
    $_.FullName -match '\\(Meshes|Materials|Textures)\\' -or
    $_.Name -eq 'PA_Mannequin.uasset' -or
    $_.Name -in @('MF_Rifle_Idle_ADS.uasset','MF_Rifle_Walk_Fwd.uasset','MF_Rifle_Jog_Fwd.uasset','MM_Rifle_Jump_Fall_Loop.uasset','MM_Rifle_Reload.uasset')
}
foreach ($file in $files) {
    $relative = $file.FullName.Substring($sourceRoot.Length).TrimStart('\')
    $destination = Join-Path $projectRoot ('Content/Characters/' + $relative)
    if (!(Test-Path -LiteralPath $destination)) {
        New-Item -ItemType Directory -Path (Split-Path $destination) -Force | Out-Null
        Copy-Item -LiteralPath $file.FullName -Destination $destination
    }
}
Write-Output ('Epic template assets available: ' + $files.Count)
