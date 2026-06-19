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
$LandscapeAssetScript = Join-Path $PSScriptRoot "generate-yarlung-landscape-assets.py"
$MaterialScript = Join-Path $PSScriptRoot "create-coaster-materials.py"
$ModelScript = Join-Path $PSScriptRoot "import-polyhaven-models.py"
$InspectScript = Join-Path $PSScriptRoot "inspect-yarlung-map.py"
$MaterialSuccessMarker = Join-Path $RepoRoot "Saved\material-generation-ok.txt"
$LandscapeMaterialAsset = Join-Path $RepoRoot "Content\Generated\Materials\M_YarlungLandscapeGround.uasset"
$BoulderAsset = Join-Path $RepoRoot "Content\Generated\Models\Boulder01\boulder_01_1k.uasset"

if ($Build) {
    & $BuildBat CoasterSimEditor Win64 Development "-Project=$Project" -WaitMutex -NoHotReloadFromIDE
    if ($LASTEXITCODE -ne 0) {
        throw "Unreal build failed with exit code $LASTEXITCODE"
    }
}

python $LandscapeAssetScript
if ($LASTEXITCODE -ne 0) {
    throw "Yarlung landscape asset generation failed with exit code $LASTEXITCODE"
}

if (-not $SkipMaterials) {
    Remove-Item -LiteralPath $MaterialSuccessMarker -ErrorAction SilentlyContinue

    & $EditorCmd $Project "-ExecutePythonScript=$MaterialScript" -unattended -nop4 -NullRHI -NoSplash
    if ($LASTEXITCODE -ne 0) {
        throw "Coaster material generation failed with exit code $LASTEXITCODE"
    }
    if (-not (Test-Path -LiteralPath $MaterialSuccessMarker)) {
        throw "Coaster material generation did not complete successfully; missing marker $MaterialSuccessMarker"
    }
    if (-not (Test-Path -LiteralPath $LandscapeMaterialAsset)) {
        throw "Coaster material generation did not produce $LandscapeMaterialAsset"
    }

    & $EditorCmd $Project "-ExecutePythonScript=$ModelScript" -unattended -nop4 -NullRHI -NoSplash
    if ($LASTEXITCODE -ne 0) {
        throw "Poly Haven model import failed with exit code $LASTEXITCODE"
    }
    if (-not (Test-Path -LiteralPath $BoulderAsset)) {
        throw "Poly Haven model import did not produce $BoulderAsset"
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
