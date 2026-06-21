param(
    [ValidateSet("Actor", "Material", "Terrain", "Full", "ScreenshotOnly")]
    [string]$Mode = "Actor",
    [string]$NamePrefix = "",
    [int[]]$Times = @(30, 90, 150),
    [int]$ResX = 1280,
    [int]$ResY = 720,
    [int]$TimeoutSeconds = 240,
    [switch]$Build,
    [switch]$SkipCapture,
    [string[]]$ExtraArgs = @()
)

$ErrorActionPreference = "Stop"

$RepoRoot = Split-Path -Parent $PSScriptRoot
$StartedAt = Get-Date
if ([string]::IsNullOrWhiteSpace($NamePrefix)) {
    $Stamp = $StartedAt.ToString("yyyyMMdd-HHmmss")
    $NamePrefix = "iter-$($Mode.ToLowerInvariant())-$Stamp"
}

function Invoke-Step {
    param(
        [Parameter(Mandatory = $true)][string]$Name,
        [Parameter(Mandatory = $true)][scriptblock]$Script,
        [object[]]$ArgumentList = @()
    )

    $Stopwatch = [System.Diagnostics.Stopwatch]::StartNew()
    Write-Host "[YARLUNG-ITER] start $Name"
    & $Script @ArgumentList | ForEach-Object { Write-Host $_ }
    $Stopwatch.Stop()
    Write-Host ("[YARLUNG-ITER] end {0}: {1:n1}s" -f $Name, $Stopwatch.Elapsed.TotalSeconds)
    return [pscustomobject]@{
        name = $Name
        seconds = [math]::Round($Stopwatch.Elapsed.TotalSeconds, 3)
    }
}

$Steps = @()

if ($Mode -ne "ScreenshotOnly") {
    $ImportParams = @{
        Verify = $true
    }
    if ($Build) {
        $ImportParams["Build"] = $true
    }

    switch ($Mode) {
        "Actor" {
            $ImportParams["SkipAssetGeneration"] = $true
            $ImportParams["SkipMaterials"] = $true
            $ImportParams["SkipModels"] = $true
            $ImportParams["SkipTerrainMeshBuild"] = $true
        }
        "Material" {
            $ImportParams["SkipAssetGeneration"] = $true
            $ImportParams["ForceMaterials"] = $true
            $ImportParams["SkipModels"] = $true
            $ImportParams["SkipTerrainMeshBuild"] = $true
        }
        "Terrain" {
            $ImportParams["SkipAssetGeneration"] = $true
            $ImportParams["SkipMaterials"] = $true
            $ImportParams["SkipModels"] = $true
        }
        "Full" {
            $ImportParams["ForceAssetGeneration"] = $true
            $ImportParams["ForceMaterials"] = $true
            $ImportParams["ForceModels"] = $true
        }
    }

    $Steps += Invoke-Step -Name "import-$Mode" -ArgumentList @($PSScriptRoot, $ImportParams) -Script {
        param([string]$ScriptRoot, [hashtable]$Params)
        & (Join-Path $ScriptRoot "import-yarlung-landscape.ps1") @Params
    }
} elseif ($Build) {
    throw "-Build is only valid when Mode is not ScreenshotOnly; build with Mode Actor/Material/Terrain/Full."
}

$SurveyCsv = Join-Path $RepoRoot "Saved\Diagnostics\$NamePrefix.csv"
$SurveyJson = Join-Path $RepoRoot "Saved\Diagnostics\$NamePrefix.json"
$SurveySheet = Join-Path $RepoRoot "Saved\Diagnostics\$NamePrefix.png"

if (-not $SkipCapture) {
    $SurveyParams = @{
        Times = $Times
        NamePrefix = $NamePrefix
        ResX = $ResX
        ResY = $ResY
        TimeoutSeconds = $TimeoutSeconds
        ExtraArgs = $ExtraArgs
    }
    $Steps += Invoke-Step -Name "visual-survey" -ArgumentList @($PSScriptRoot, $SurveyParams) -Script {
        param([string]$ScriptRoot, [hashtable]$Params)
        & (Join-Path $ScriptRoot "visual-survey.ps1") @Params
    }
} else {
    Write-Host "[YARLUNG-ITER] skip visual-survey: requested"
}

$Worst = $null
if (Test-Path -LiteralPath $SurveyJson) {
    $Metrics = Get-Content -LiteralPath $SurveyJson -Raw | ConvertFrom-Json
    if ($Metrics.Count -gt 0) {
        $Worst = $Metrics | Sort-Object -Property visual_risk -Descending | Select-Object -First 1
    }
}

$ManifestPath = Join-Path $RepoRoot "Saved\Diagnostics\$NamePrefix-run.json"
$Manifest = [pscustomobject]@{
    name_prefix = $NamePrefix
    mode = $Mode
    started_at = $StartedAt.ToString("o")
    finished_at = (Get-Date).ToString("o")
    times = $Times
    resolution = [pscustomobject]@{ x = $ResX; y = $ResY }
    build = [bool]$Build
    skip_capture = [bool]$SkipCapture
    extra_args = $ExtraArgs
    steps = $Steps
    outputs = [pscustomobject]@{
        survey_csv = $SurveyCsv
        survey_json = $SurveyJson
        contact_sheet = $SurveySheet
    }
    worst = $Worst
}
$Manifest | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $ManifestPath -Encoding UTF8

Write-Host "YarlungIterationManifest=$ManifestPath"
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
