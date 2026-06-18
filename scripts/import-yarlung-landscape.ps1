param(
    [switch]$Build,
    [switch]$SkipMaterials,
    [switch]$Verify
)

$ErrorActionPreference = "Stop"

$RepoRoot = Split-Path -Parent $PSScriptRoot
$Project = Join-Path $RepoRoot "CoasterSim.uproject"
$EditorCmd = "C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
$BuildBat = "C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat"
$MaterialScript = Join-Path $PSScriptRoot "create-coaster-materials.py"
$InspectScript = Join-Path $PSScriptRoot "inspect-yarlung-map.py"

if ($Build) {
    & $BuildBat CoasterSimEditor Win64 Development "-Project=$Project" -WaitMutex -NoHotReloadFromIDE
    if ($LASTEXITCODE -ne 0) {
        throw "Unreal build failed with exit code $LASTEXITCODE"
    }
}

if (-not $SkipMaterials) {
    & $EditorCmd $Project "-ExecutePythonScript=$MaterialScript" -unattended -nop4 -NullRHI -NoSplash
    if ($LASTEXITCODE -ne 0) {
        throw "Coaster material generation failed with exit code $LASTEXITCODE"
    }
}

& $EditorCmd $Project -run=YarlungLandscapeImport -unattended -nop4 -NullRHI -NoSplash
if ($LASTEXITCODE -ne 0) {
    throw "Yarlung landscape import failed with exit code $LASTEXITCODE"
}

if ($Verify) {
    & $EditorCmd $Project "-ExecutePythonScript=$InspectScript" -unattended -nop4 -NullRHI -NoSplash
    if ($LASTEXITCODE -ne 0) {
        throw "Yarlung landscape verification failed with exit code $LASTEXITCODE"
    }
}
