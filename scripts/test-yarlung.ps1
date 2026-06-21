param(
    [string]$TestFilter = "CoasterSim.Yarlung",
    [switch]$Build
)

$ErrorActionPreference = "Stop"

$RepoRoot = Split-Path -Parent $PSScriptRoot
$Project = Join-Path $RepoRoot "CoasterSim.uproject"
$EditorCmd = "C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
$BuildBat = "C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat"
$LogName = "yarlung-automation.log"
$RequestedLogPath = "Saved\Logs\$LogName"
$ObservedLogPath = Join-Path $RepoRoot "Saved\Logs\Saved\Logs\$LogName"

if (-not (Test-Path -LiteralPath $Project)) {
    throw "Missing Unreal project: $Project"
}
if (-not (Test-Path -LiteralPath $EditorCmd)) {
    throw "Missing UnrealEditor-Cmd.exe: $EditorCmd"
}
if ($Build -and -not (Test-Path -LiteralPath $BuildBat)) {
    throw "Missing Unreal Build.bat: $BuildBat"
}

New-Item -ItemType Directory -Force -Path (Split-Path -Parent $ObservedLogPath) | Out-Null
Remove-Item -LiteralPath $ObservedLogPath -ErrorAction SilentlyContinue

if ($Build) {
    & $BuildBat CoasterSimEditor Win64 Development "-Project=$Project" -WaitMutex -NoHotReloadFromIDE
    if ($LASTEXITCODE -ne 0) {
        throw "Unreal build failed with exit code $LASTEXITCODE"
    }
}

$Args = @(
    $Project,
    "-unattended",
    "-nop4",
    "-NullRHI",
    "-NoSplash",
    "-ExecCmds=Automation RunTests $TestFilter; Quit",
    "-TestExit=Automation Test Queue Empty",
    "-log=$RequestedLogPath",
    "-DDC-ForceMemoryCache"
)

& $EditorCmd @Args
if ($LASTEXITCODE -ne 0) {
    throw "Automation command failed with exit code $LASTEXITCODE`: $TestFilter"
}
if (-not (Test-Path -LiteralPath $ObservedLogPath)) {
    throw "Automation did not produce log: $ObservedLogPath"
}

$LogText = Get-Content -LiteralPath $ObservedLogPath -Raw
if ($LogText -notmatch "Found\s+(\d+)\s+automation tests based on '$([regex]::Escape($TestFilter))'") {
    throw "Automation log did not confirm discovered tests for '$TestFilter': $ObservedLogPath"
}
$Discovered = [int]$Matches[1]
if ($Discovered -le 0) {
    throw "Automation discovered zero tests for '$TestFilter': $ObservedLogPath"
}
if ($LogText -match "Result=\{Fail\}|Error:|Fatal error") {
    throw "Automation log contains failures or errors: $ObservedLogPath"
}
if ($LogText -notmatch "\*\*\*\* TEST COMPLETE\. EXIT CODE: 0 \*\*\*\*") {
    throw "Automation log is missing success marker: $ObservedLogPath"
}

Write-Host "Yarlung automation passed: $Discovered test(s), log=$ObservedLogPath"
