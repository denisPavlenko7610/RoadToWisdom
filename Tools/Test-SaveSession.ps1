param([string]$EngineRoot = 'C:\Program Files\Epic Games\U_5.8\UE_5.8')
$ErrorActionPreference = 'Stop'
$ProjectRoot = Split-Path $PSScriptRoot -Parent
$Report = Join-Path $ProjectRoot 'Saved\Automation\MultiSave'
$Log = Join-Path $ProjectRoot 'Saved\Logs\MultiSaveTests.log'
$Started = Get-Date
& (Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe') (Join-Path $ProjectRoot 'RoadToWisdom.uproject') /Game/Tests/L_SaveSession -NullRHI -unattended -nosplash -QuietRoadAutomation '-ExecCmds=Automation RunTests QuietRoad.Session.Persistence+QuietRoad.Session.RadioHold' '-TestExit=Automation Test Queue Empty' "-ReportExportPath=$Report" "-abslog=$Log" '-ini:EditorPerProjectUserSettings:[/Script/ModelContextProtocolEngine.ModelContextProtocolSettings]:ServerPortNumber=8017'
if ($LASTEXITCODE -ne 0) { throw "Unreal failed ($LASTEXITCODE). See $Log" }
$ReportFile = Get-Item (Join-Path $Report 'index.json')
if ($ReportFile.LastWriteTime -lt $Started) { throw 'No fresh Automation report.' }
$Result = Get-Content $ReportFile.FullName -Raw | ConvertFrom-Json
if ($Result.failed -gt 0 -or $Result.notRun -gt 0 -or ($Result.succeeded + $Result.succeededWithWarnings) -ne 2) {
    throw "Expected two passing session tests. See $Report"
}
Write-Output "Passed two session tests. Report: $Report"
