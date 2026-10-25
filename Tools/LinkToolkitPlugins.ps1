# Links this project's Plugins/ folder to the sibling UnrealTemplates repository
# via NTFS directory junctions, so both projects share one copy of the UT plugin
# sources (no duplication; edits in either place are the same files).
#
# Usage (from anywhere):
#   powershell -NoProfile -ExecutionPolicy Bypass -File Tools/LinkToolkitPlugins.ps1
#   powershell ... -ToolkitRoot 'D:\somewhere\UnrealTemplates'
#
# The junctions are not versioned (see .gitignore); after a fresh clone run this
# script once before opening the editor or building.

param(
    [string]$ToolkitRoot = (Join-Path $PSScriptRoot '..\..\UnrealTemplates\Plugins')
)

$ErrorActionPreference = 'Stop'

# The adopted Adventure Kit closure. UTCombat is UTWorldMechanics' mandatory
# dependency (see Docs/ToolkitIntegrationMatrix.md). Keep in sync with that file.
$Plugins = @(
    'UTFoundation',
    'UTInteraction',
    'UTSave',
    'UTItems',
    'UTProgression',
    'UTWorldMechanics',
    'UTCombat',
    'UTGameTime'
)

$ToolkitRoot = (Resolve-Path $ToolkitRoot).Path
$ProjectPlugins = Join-Path $PSScriptRoot '..\Plugins'

Write-Host "Toolkit plugins: $ToolkitRoot"
Write-Host "Project plugins: $ProjectPlugins"

if (-not (Test-Path (Join-Path $ToolkitRoot 'UTFoundation\UTFoundation.uplugin'))) {
    throw "UnrealTemplates not found at '$ToolkitRoot'. Pass -ToolkitRoot explicitly."
}

foreach ($Name in $Plugins) {
    $linkPath = Join-Path $ProjectPlugins $Name
    $targetPath = Join-Path $ToolkitRoot $Name

    if (-not (Test-Path $targetPath)) {
        throw "Missing plugin in toolkit repo: $targetPath"
    }

    $item = Get-Item $linkPath -ErrorAction SilentlyContinue
    if ($item -and $item.LinkType -eq 'Junction') {
        Write-Host "OK (junction exists): $Name -> $($item.Target)"
        continue
    }
    if ($item) {
        throw "'$linkPath' exists and is not a junction; refusing to overwrite. Remove it first."
    }

    New-Item -ItemType Junction -Path $linkPath -Target $targetPath | Out-Null
    Write-Host "Linked: $Name -> $targetPath"
}

Write-Host 'Done. Build the editor target or open the project.'
