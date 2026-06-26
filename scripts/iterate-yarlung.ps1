param(
    [ValidateSet("Auto", "Actor", "Material", "Terrain", "Full", "ScreenshotOnly")]
    [string]$Mode = "Auto",
    [ValidateSet("Quick", "Focus", "Standard", "Route", "Hero", "Final")]
    [string]$Preset = "Standard",
    [string]$NamePrefix = "",
    [int[]]$Times = @(),
    [int]$ResX = 0,
    [int]$ResY = 0,
    [int]$TimeoutSeconds = 0,
    [switch]$Build,
    [switch]$SkipCapture,
    [switch]$NoHandoff,
    [switch]$RestoreGeneratedMap,
    [string[]]$ExtraArgs = @()
)

$ErrorActionPreference = "Stop"

$RepoRoot = Split-Path -Parent $PSScriptRoot
$ConfigPath = Join-Path $RepoRoot "Config\yarlung-iteration.json"
if (-not (Test-Path -LiteralPath $ConfigPath)) {
    throw "Missing iteration config: $ConfigPath"
}
$Config = Get-Content -LiteralPath $ConfigPath -Raw | ConvertFrom-Json
if ($Mode -eq "Auto") {
    $Mode = [string]$Config.default_mode
}
$PresetConfig = $Config.presets.$Preset
if ($Times.Count -eq 0) {
    $Times = @($PresetConfig.times | ForEach-Object { [int]$_ })
}
if ($ResX -le 0) {
    $ResX = [int]$PresetConfig.resolution.x
}
if ($ResY -le 0) {
    $ResY = [int]$PresetConfig.resolution.y
}
if ($TimeoutSeconds -le 0) {
    $TimeoutSeconds = [int]$PresetConfig.timeout_seconds
}

$StartedAt = Get-Date
if ([string]::IsNullOrWhiteSpace($NamePrefix)) {
    $Stamp = $StartedAt.ToString("yyyyMMdd-HHmmss")
    $NamePrefix = "iter-$($Preset.ToLowerInvariant())-$($Mode.ToLowerInvariant())-$Stamp"
}

function Invoke-Step {
    param(
        [Parameter(Mandatory = $true)][string]$Name,
        [Parameter(Mandatory = $true)][scriptblock]$Script,
        [object[]]$StepArgs = @()
    )

    $Stopwatch = [System.Diagnostics.Stopwatch]::StartNew()
    Write-Host "[YARLUNG-ITER] start $Name"
    & $Script @StepArgs | ForEach-Object { Write-Host $_ }
    $Stopwatch.Stop()
    Write-Host ("[YARLUNG-ITER] end {0}: {1:n1}s" -f $Name, $Stopwatch.Elapsed.TotalSeconds)
    return [pscustomobject]@{
        name = $Name
        seconds = [math]::Round($Stopwatch.Elapsed.TotalSeconds, 3)
    }
}

function Test-GitPathDirty {
    param(
        [Parameter(Mandatory = $true)][string]$RepoRoot,
        [Parameter(Mandatory = $true)][string]$RelativePath
    )

    $Status = & git -C $RepoRoot status --porcelain -- $RelativePath
    return -not [string]::IsNullOrWhiteSpace(($Status | Out-String))
}

$Steps = @()
$GeneratedMapRelativePath = "Content/Generated/YarlungLandscape/YarlungLandscape_Level.umap"
$GeneratedMapState = [pscustomobject]@{
    path = $GeneratedMapRelativePath
    restore_requested = [bool]$RestoreGeneratedMap
    dirty_before = $false
    dirty_after = $false
    restored = $false
    restore_skipped_reason = ""
}
if ($RestoreGeneratedMap) {
    $GeneratedMapState.dirty_before = Test-GitPathDirty -RepoRoot $RepoRoot -RelativePath $GeneratedMapRelativePath
}

$ModeImportFlags = @{
    Actor = @(
        "SkipAssetGeneration",
        "SkipTrackGeneration",
        "SkipMaterials",
        "SkipTerrainMeshBuild"
    )
    Material = @(
        "SkipAssetGeneration",
        "SkipTrackGeneration",
        "ForceMaterials",
        "SkipTerrainMeshBuild"
    )
    Terrain = @(
        "SkipAssetGeneration",
        "SkipTrackGeneration",
        "ForceMaterials"
    )
    Full = @(
        "ForceAssetGeneration",
        "ForceTrackGeneration",
        "ForceMaterials"
    )
}

if ($Mode -ne "ScreenshotOnly") {
    $ImportParams = @{
        Verify = $true
    }
    if ($Build) {
        $ImportParams["Build"] = $true
    }

    foreach ($Flag in $ModeImportFlags[$Mode]) {
        $ImportParams[$Flag] = $true
    }

    $Steps += Invoke-Step -Name "import-$Mode" -StepArgs @($PSScriptRoot, $ImportParams) -Script {
        param([string]$ScriptRoot, [hashtable]$Params)
        & (Join-Path $ScriptRoot "import-yarlung-corridor.ps1") @Params
    }
} elseif ($Build) {
    $Steps += Invoke-Step -Name "build" -StepArgs @($RepoRoot) -Script {
        param([string]$Root)
        & "C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" CoasterSimEditor Win64 Development "-Project=$Root\CoasterSim.uproject" -WaitMutex -NoHotReloadFromIDE
        if ($LASTEXITCODE -ne 0) {
            throw "Unreal build failed with exit code $LASTEXITCODE"
        }
    }
}

$SurveyCsv = Join-Path $RepoRoot "Saved\Diagnostics\$NamePrefix.csv"
$SurveyJson = Join-Path $RepoRoot "Saved\Diagnostics\$NamePrefix.json"
$SurveySheet = Join-Path $RepoRoot "Saved\Diagnostics\$NamePrefix.png"
$HandoffPath = Join-Path $RepoRoot "Saved\Diagnostics\$NamePrefix-handoff.md"

if (-not $SkipCapture) {
    $SurveyParams = @{
        Times = $Times
        NamePrefix = $NamePrefix
        ResX = $ResX
        ResY = $ResY
        TimeoutSeconds = $TimeoutSeconds
        ExtraArgs = $ExtraArgs
    }
    $Steps += Invoke-Step -Name "visual-survey" -StepArgs @($PSScriptRoot, $SurveyParams) -Script {
        param([string]$ScriptRoot, [hashtable]$Params)
        & (Join-Path $ScriptRoot "visual-survey.ps1") @Params
    }
} else {
    Write-Host "[YARLUNG-ITER] skip visual-survey: requested"
}

if ($RestoreGeneratedMap) {
    $GeneratedMapState.dirty_after = Test-GitPathDirty -RepoRoot $RepoRoot -RelativePath $GeneratedMapRelativePath
    if ($GeneratedMapState.dirty_before) {
        $GeneratedMapState.restore_skipped_reason = "generated map was already dirty before iteration"
        Write-Host "[YARLUNG-ITER] generated map restore skipped: $($GeneratedMapState.restore_skipped_reason)"
    } elseif ($GeneratedMapState.dirty_after) {
        & git -C $RepoRoot restore -- $GeneratedMapRelativePath
        $GeneratedMapState.restored = $true
        Write-Host "[YARLUNG-ITER] restored generated map: $GeneratedMapRelativePath"
    } else {
        Write-Host "[YARLUNG-ITER] generated map stayed clean: $GeneratedMapRelativePath"
    }
}

$Worst = $null
if (Test-Path -LiteralPath $SurveyJson) {
    $Metrics = Get-Content -LiteralPath $SurveyJson -Raw | ConvertFrom-Json
    if ($Metrics.Count -gt 0) {
        $Worst = $Metrics | Sort-Object -Property visual_risk -Descending | Select-Object -First 1
    }
}

$RiskGate = "UNKNOWN"
if ($Worst) {
    $Risk = [double]$Worst.visual_risk
    if ($Risk -ge [double]$Config.risk_thresholds.visual_risk_fail) {
        $RiskGate = "FAIL"
    } elseif ($Risk -ge [double]$Config.risk_thresholds.visual_risk_warn) {
        $RiskGate = "WARN"
    } else {
        $RiskGate = "OK"
    }
}

$ManifestPath = Join-Path $RepoRoot "Saved\Diagnostics\$NamePrefix-run.json"
$Manifest = [pscustomobject]@{
    schema_version = 1
    name_prefix = $NamePrefix
    mode = $Mode
    preset = $Preset
    preset_purpose = [string]$PresetConfig.purpose
    started_at = $StartedAt.ToString("o")
    finished_at = (Get-Date).ToString("o")
    times = $Times
    resolution = [pscustomobject]@{ x = $ResX; y = $ResY }
    build = [bool]$Build
    skip_capture = [bool]$SkipCapture
    generated_map = $GeneratedMapState
    extra_args = $ExtraArgs
    steps = $Steps
    outputs = [pscustomobject]@{
        survey_csv = $SurveyCsv
        survey_json = $SurveyJson
        contact_sheet = $SurveySheet
        handoff = $HandoffPath
    }
    risk_gate = $RiskGate
    worst = $Worst
}
$Manifest | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $ManifestPath -Encoding UTF8

if (-not $NoHandoff) {
    $WorstFrameName = if ($Worst) { Split-Path -Leaf $Worst.path } else { "" }
    $WorstLine = if ($Worst) {
        "- Worst frame: $WorstFrameName risk=$($Worst.visual_risk) washed=$($Worst.washed_frac) green=$($Worst.forest_green_frac) flat=$($Worst.flat_block_frac) edge=$($Worst.edge_density)"
    } else {
        "- Worst frame: not available (capture skipped or analysis missing)"
    }
    $StartedIso = $StartedAt.ToString("o")
    $FinishedIso = (Get-Date).ToString("o")
    $TimesText = $Times -join ", "
    $Handoff = @(
        "# Yarlung Iteration Handoff",
        "",
        "- Name: $NamePrefix",
        "- Mode: $Mode",
        "- Preset: $Preset - $($PresetConfig.purpose)",
        "- Started: $StartedIso",
        "- Finished: $FinishedIso",
        "- Times: $TimesText",
        "- Resolution: ${ResX}x${ResY}",
        "- Risk gate: $RiskGate",
        $WorstLine,
        "- Generated map restore requested: $($GeneratedMapState.restore_requested); restored: $($GeneratedMapState.restored); dirty before: $($GeneratedMapState.dirty_before); dirty after: $($GeneratedMapState.dirty_after)",
        "",
        "## Outputs",
        "",
        "- Manifest: $ManifestPath",
        "- Contact sheet: $SurveySheet",
        "- Survey CSV: $SurveyCsv",
        "- Survey JSON: $SurveyJson",
        "",
        "## Required Follow-up",
        "",
        "1. Open the contact sheet as an image.",
        '2. Compare against `docs/refs/local/` L1-L3 and `docs/specs/photoreal-acceptance.md`.',
        '3. Update `docs/plans/photoreal-progress.md` with command, contact sheet, risk gate, visual verdict, and next step.',
        "4. If committing, stage only intentional source/docs/generated map assets; do not stage local reference images or unrelated dirty files."
    )
    $Handoff | Set-Content -LiteralPath $HandoffPath -Encoding UTF8
}

Write-Host "YarlungIterationManifest=$ManifestPath"
Write-Host "RiskGate=$RiskGate"
if ($Worst) {
    Write-Host ("WorstFrame={0} risk={1} washed={2} green={3} flat={4} edge={5}" -f `
        (Split-Path -Leaf $Worst.path), `
        $Worst.visual_risk, `
        $Worst.washed_frac, `
        $Worst.forest_green_frac, `
        $Worst.flat_block_frac, `
        $Worst.edge_density)
}
if (Test-Path -LiteralPath $SurveySheet) {
    Write-Host "ContactSheet=$SurveySheet"
}
if (Test-Path -LiteralPath $HandoffPath) {
    Write-Host "Handoff=$HandoffPath"
}
