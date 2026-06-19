param(
    [switch]$Build,
    [switch]$SkipAssetGeneration,
    [switch]$ForceAssetGeneration,
    [switch]$SkipMaterials,
    [switch]$ForceMaterials,
    [switch]$SkipModels,
    [switch]$ForceModels,
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
$HeightmapAsset = Join-Path $RepoRoot "Content\Generated\YarlungLandscape\YarlungTsangpo_1009.r16"
$MacroTextureSources = @(
    (Join-Path $RepoRoot "SourceAssets\Generated\YarlungLandscape\YarlungTsangpo_macro_albedo.tga"),
    (Join-Path $RepoRoot "SourceAssets\Generated\YarlungLandscape\YarlungTsangpo_macro_masks.tga"),
    (Join-Path $RepoRoot "SourceAssets\Generated\YarlungLandscape\YarlungTsangpo_macro_roughness.tga")
)
$MacroTextureAssets = @(
    (Join-Path $RepoRoot "Content\Generated\Materials\YarlungMacro\T_YarlungMacroAlbedo.uasset"),
    (Join-Path $RepoRoot "Content\Generated\Materials\YarlungMacro\T_YarlungMacroMasks.uasset"),
    (Join-Path $RepoRoot "Content\Generated\Materials\YarlungMacro\T_YarlungMacroRoughness.uasset")
)

function Invoke-TimedStep {
    param(
        [Parameter(Mandatory = $true)][string]$Name,
        [Parameter(Mandatory = $true)][scriptblock]$Script
    )

    $Stopwatch = [System.Diagnostics.Stopwatch]::StartNew()
    Write-Host "[YARLUNG-TIME] start $Name"
    try {
        & $Script
    } finally {
        $Stopwatch.Stop()
        Write-Host ("[YARLUNG-TIME] end {0}: {1:n1}s" -f $Name, $Stopwatch.Elapsed.TotalSeconds)
    }
}

function Test-AllPathsExist {
    param([Parameter(Mandatory = $true)][string[]]$Paths)

    foreach ($Path in $Paths) {
        if (-not (Test-Path -LiteralPath $Path)) {
            return $false
        }
    }
    return $true
}

function Test-AnySourceNewerThanAnyOutput {
    param(
        [Parameter(Mandatory = $true)][string[]]$Sources,
        [Parameter(Mandatory = $true)][string[]]$Outputs
    )

    if (-not (Test-AllPathsExist $Outputs)) {
        return $true
    }

    $NewestSource = ($Sources | Where-Object { Test-Path -LiteralPath $_ } | ForEach-Object {
        (Get-Item -LiteralPath $_).LastWriteTimeUtc
    } | Measure-Object -Maximum).Maximum
    $OldestOutput = ($Outputs | ForEach-Object {
        (Get-Item -LiteralPath $_).LastWriteTimeUtc
    } | Measure-Object -Minimum).Minimum

    return $NewestSource -gt $OldestOutput
}

if ($Build) {
    Invoke-TimedStep "build" {
        & $BuildBat CoasterSimEditor Win64 Development "-Project=$Project" -WaitMutex -NoHotReloadFromIDE
        if ($LASTEXITCODE -ne 0) {
            throw "Unreal build failed with exit code $LASTEXITCODE"
        }
    }
}

if (-not $SkipAssetGeneration -and ($ForceAssetGeneration -or (Test-AnySourceNewerThanAnyOutput @($LandscapeAssetScript) (@($HeightmapAsset) + $MacroTextureSources)))) {
    Invoke-TimedStep "generate landscape source assets" {
        python $LandscapeAssetScript
        if ($LASTEXITCODE -ne 0) {
            throw "Yarlung landscape asset generation failed with exit code $LASTEXITCODE"
        }
    }
} elseif (-not $SkipAssetGeneration) {
    Write-Host "[YARLUNG-TIME] skip generate landscape source assets: outputs are fresh"
}

if (-not $SkipMaterials -and ($ForceMaterials -or (Test-AnySourceNewerThanAnyOutput (@($MaterialScript) + $MacroTextureSources) (@($LandscapeMaterialAsset) + $MacroTextureAssets)))) {
    Invoke-TimedStep "import materials" {
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
    }
} elseif (-not $SkipMaterials) {
    Write-Host "[YARLUNG-TIME] skip import materials: assets are fresh"
    if (-not (Test-Path -LiteralPath $LandscapeMaterialAsset)) {
        throw "Missing required landscape material asset: $LandscapeMaterialAsset"
    }
}

if (-not $SkipModels -and ($ForceModels -or -not (Test-Path -LiteralPath $BoulderAsset))) {
    Invoke-TimedStep "import models" {
        & $EditorCmd $Project "-ExecutePythonScript=$ModelScript" -unattended -nop4 -NullRHI -NoSplash
        if ($LASTEXITCODE -ne 0) {
            throw "Poly Haven model import failed with exit code $LASTEXITCODE"
        }
        if (-not (Test-Path -LiteralPath $BoulderAsset)) {
            throw "Poly Haven model import did not produce $BoulderAsset"
        }
    }
} elseif (-not $SkipModels) {
    Write-Host "[YARLUNG-TIME] skip import models: existing asset $BoulderAsset"
}

Invoke-TimedStep "import landscape map" {
    & $EditorCmd $Project -run=YarlungLandscapeImport -unattended -nop4 -NullRHI -NoSplash
    if ($LASTEXITCODE -ne 0) {
        throw "Yarlung landscape import failed with exit code $LASTEXITCODE"
    }
}

if ($Verify) {
    Invoke-TimedStep "verify map" {
        & $EditorCmd $Project "-ExecutePythonScript=$InspectScript" -unattended -nop4 -NullRHI -NoSplash
        if ($LASTEXITCODE -ne 0) {
            throw "Yarlung landscape verification failed with exit code $LASTEXITCODE"
        }
    }
}
