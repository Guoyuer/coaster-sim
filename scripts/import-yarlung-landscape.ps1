param(
    [switch]$Build,
    [switch]$SkipAssetGeneration,
    [switch]$ForceAssetGeneration,
    [switch]$SkipTrackGeneration,
    [switch]$ForceTrackGeneration,
    [switch]$SkipMaterials,
    [switch]$ForceMaterials,
    [switch]$SkipMapImport,
    [switch]$SkipTerrainMeshBuild,
    [switch]$Verify
)

$ErrorActionPreference = "Stop"

$RepoRoot = Split-Path -Parent $PSScriptRoot
$Project = Join-Path $RepoRoot "CoasterSim.uproject"
$EditorCmd = "C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
$BuildBat = "C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat"
$EditorCommonArgs = @("-DDC-ForceMemoryCache")
$LandscapeAssetScript = Join-Path $PSScriptRoot "generate-yarlung-landscape-assets.py"
$TrackScript = Join-Path $PSScriptRoot "generate-yarlung-track.py"
$TrackLibScript = Join-Path $PSScriptRoot "yarlung_track_lib.py"
$TrackVerifyScript = Join-Path $PSScriptRoot "verify-track-clearance.py"
$MaterialScript = Join-Path $PSScriptRoot "create-coaster-materials.py"
$InspectScript = Join-Path $PSScriptRoot "inspect-yarlung-map.py"
$MaterialSuccessMarker = Join-Path $RepoRoot "Saved\material-generation-ok.txt"
$MeshTerrainMaterialAsset = Join-Path $RepoRoot "Content\Generated\Materials\M_YarlungMeshTerrain.uasset"
$WaterRiverMaterialAsset = Join-Path $RepoRoot "Content\Generated\Materials\MI_YarlungWaterRiver.uasset"
$WaterSurfaceMaterialAsset = Join-Path $RepoRoot "Content\Generated\Materials\M_YarlungWaterSurface.uasset"
$HeightmapAsset = Join-Path $RepoRoot "Content\Generated\YarlungLandscape\YarlungTsangpo_1009.r16"
$TrackAsset = Join-Path $RepoRoot "Content\Generated\YarlungLandscape\YarlungTrack.csv"
$TrackOverlayAsset = Join-Path $RepoRoot "Saved\Diagnostics\yarlung-track-overlay.png"
$TrackClearanceCsv = Join-Path $RepoRoot "Saved\Diagnostics\track-clearance.csv"
$TrackClearancePlot = Join-Path $RepoRoot "Saved\Diagnostics\track-clearance.png"
$MacroTextureSources = @(
    (Join-Path $RepoRoot "Content\Generated\YarlungLandscape\YarlungTsangpo_masks.ppm"),
    (Join-Path $RepoRoot "Content\Generated\YarlungLandscape\YarlungTsangpo_preview.ppm"),
    (Join-Path $RepoRoot "Content\Generated\YarlungLandscape\manifest.json")
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

function Invoke-UnrealPythonScript {
    param(
        [Parameter(Mandatory = $true)][string]$ScriptPath,
        [switch]$NullRHI
    )

    $Args = @($Project, "-run=pythonscript", "-script=$ScriptPath", "-unattended", "-nop4", "-NoSplash")
    if ($NullRHI) {
        $Args += "-NullRHI"
    }
    $Args += $EditorCommonArgs
    Invoke-UnrealChecked -Args $Args -FailureLabel "Unreal Python script" -TargetLabel $ScriptPath -RequireSuccessSummary
}

function Invoke-UnrealChecked {
    param(
        [Parameter(Mandatory = $true)][string[]]$Args,
        [Parameter(Mandatory = $true)][string]$FailureLabel,
        [Parameter(Mandatory = $true)][string]$TargetLabel,
        [switch]$RequireSuccessSummary
    )

    $Output = @(& $EditorCmd @Args 2>&1)
    $Output | ForEach-Object { Write-Host $_ }
    $ExitCode = $LASTEXITCODE
    $Text = $Output -join [Environment]::NewLine
    if ($ExitCode -ne 0) {
        throw "$FailureLabel failed with exit code $ExitCode`: $TargetLabel"
    }
    if ($RequireSuccessSummary -and $Text -notmatch "Success - 0 error\(s\)") {
        throw "$FailureLabel did not report 'Success - 0 error(s)': $TargetLabel"
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

if (-not $SkipTrackGeneration -and ($ForceTrackGeneration -or $ForceAssetGeneration -or (Test-AnySourceNewerThanAnyOutput @($TrackScript, $TrackLibScript, $HeightmapAsset, (Join-Path $RepoRoot "Content\Generated\YarlungLandscape\manifest.json")) @($TrackAsset)))) {
    Invoke-TimedStep "generate track" {
        python $TrackScript --out $TrackAsset --overlay $TrackOverlayAsset
        if ($LASTEXITCODE -ne 0) {
            throw "Yarlung track generation failed with exit code $LASTEXITCODE"
        }
        if (-not (Test-Path -LiteralPath $TrackAsset)) {
            throw "Yarlung track generation did not produce $TrackAsset"
        }
    }
} elseif (-not $SkipTrackGeneration) {
    Write-Host "[YARLUNG-TIME] skip generate track: output is fresh"
    if (-not (Test-Path -LiteralPath $TrackAsset)) {
        throw "Missing required generated track: $TrackAsset"
    }
}

if (-not $SkipTrackGeneration) {
    Invoke-TimedStep "verify generated track" {
        python $TrackVerifyScript --track $TrackAsset --out-csv $TrackClearanceCsv --out-png $TrackClearancePlot
        if ($LASTEXITCODE -ne 0) {
            throw "Yarlung track verification failed with exit code $LASTEXITCODE"
        }
    }
}

if (-not $SkipMaterials -and ($ForceMaterials -or (Test-AnySourceNewerThanAnyOutput @($MaterialScript) @($MeshTerrainMaterialAsset, $WaterRiverMaterialAsset, $WaterSurfaceMaterialAsset)))) {
    Invoke-TimedStep "import materials" {
        Remove-Item -LiteralPath $MaterialSuccessMarker -ErrorAction SilentlyContinue

        Invoke-UnrealPythonScript -ScriptPath $MaterialScript -NullRHI
        if (-not (Test-Path -LiteralPath $MaterialSuccessMarker)) {
            throw "Coaster material generation did not complete successfully; missing marker $MaterialSuccessMarker"
        }
        if (-not (Test-Path -LiteralPath $MeshTerrainMaterialAsset)) {
            throw "Coaster material generation did not produce $MeshTerrainMaterialAsset"
        }
        if (-not (Test-Path -LiteralPath $WaterRiverMaterialAsset)) {
            throw "Coaster material generation did not produce $WaterRiverMaterialAsset"
        }
        if (-not (Test-Path -LiteralPath $WaterSurfaceMaterialAsset)) {
            throw "Coaster material generation did not produce $WaterSurfaceMaterialAsset"
        }
    }
} elseif (-not $SkipMaterials) {
    Write-Host "[YARLUNG-TIME] skip import materials: assets are fresh"
    if (-not (Test-Path -LiteralPath $MeshTerrainMaterialAsset)) {
        throw "Missing required mesh terrain material asset: $MeshTerrainMaterialAsset"
    }
    if (-not (Test-Path -LiteralPath $WaterRiverMaterialAsset)) {
        throw "Missing required UE Water river material instance: $WaterRiverMaterialAsset"
    }
    if (-not (Test-Path -LiteralPath $WaterSurfaceMaterialAsset)) {
        throw "Missing required UE Water surface material: $WaterSurfaceMaterialAsset"
    }
}

if (-not $SkipMapImport) {
    Invoke-TimedStep "import landscape map" {
        $MapImportArgs = @($Project, "-run=YarlungLandscapeImport", "-unattended", "-nop4", "-NullRHI", "-NoSplash") + $EditorCommonArgs
        if ($SkipTerrainMeshBuild) {
            $MapImportArgs += "-SkipTerrainMeshBuild"
        }
        Invoke-UnrealChecked -Args $MapImportArgs -FailureLabel "Yarlung landscape import" -TargetLabel "YarlungLandscapeImport" -RequireSuccessSummary
    }
} else {
    Write-Host "[YARLUNG-TIME] skip import landscape map: requested"
}

if ($Verify) {
    if ($SkipMapImport) {
        Write-Host "[YARLUNG-TIME] verify existing map without reimport"
    }
    Invoke-TimedStep "verify map" {
        Invoke-UnrealPythonScript -ScriptPath $InspectScript
    }
}
