param(
    [string[]]$Times = @("0", "6", "12", "18", "24", "30"),
    [string]$NamePrefix = "visual-survey",
    [int]$ResX = 1920,
    [int]$ResY = 1080,
    [int]$TimeoutSeconds = 240,
    [switch]$Build,
    [switch]$SkipCapture,
    [switch]$LegacyPerShot,
    [string[]]$ExtraArgs = @()
)

$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "yarlung-shot-lib.ps1")

$RepoRoot = Split-Path -Parent $PSScriptRoot
$Images = @()

$Times = Convert-YarlungToSecondList -Values $Times -ParameterName "Times"
$CaptureStartedAt = Get-Date

if (-not $SkipCapture -and -not $LegacyPerShot) {
    $ShotParams = @{
        BatchJumpSeconds = $Times
        BatchNamePrefix = $NamePrefix
        ResX = $ResX
        ResY = $ResY
        TimeoutSeconds = $TimeoutSeconds
        ExtraArgs = $ExtraArgs
    }
    if ($Build) {
        $ShotParams["Build"] = $true
    }
    & (Join-Path $PSScriptRoot "offscreen-shot.ps1") @ShotParams
}

foreach ($Time in $Times) {
    $Name = "$NamePrefix-t$Time"
    $Path = Join-Path $RepoRoot "Saved\OffscreenShots\$Name.png"
    if (-not $SkipCapture -and $LegacyPerShot) {
        $ShotParams = @{
            Name = $Name
            JumpSeconds = $Time
            ResX = $ResX
            ResY = $ResY
            TimeoutSeconds = $TimeoutSeconds
            ExtraArgs = $ExtraArgs
        }
        if ($Build) {
            $ShotParams["Build"] = $true
            $Build = $false
        }
        & (Join-Path $PSScriptRoot "offscreen-shot.ps1") @ShotParams
    }
    if (-not (Test-Path -LiteralPath $Path)) {
        throw "Missing expected screenshot: $Path"
    }
    if (-not $SkipCapture) {
        Assert-YarlungFreshNonEmptyFile -Path $Path -StartedAt $CaptureStartedAt
    }
    $Images += $Path
}

$Csv = Join-Path $RepoRoot "Saved\Diagnostics\$NamePrefix.csv"
$Json = Join-Path $RepoRoot "Saved\Diagnostics\$NamePrefix.json"
$Sheet = Join-Path $RepoRoot "Saved\Diagnostics\$NamePrefix.png"
Remove-Item -LiteralPath $Csv, $Json, $Sheet -Force -ErrorAction SilentlyContinue
$AnalysisStartedAt = Get-Date
& python (Join-Path $PSScriptRoot "analyze-offscreen-shots.py") @Images --out-csv $Csv --out-json $Json --contact-sheet $Sheet
if ($LASTEXITCODE -ne 0) {
    throw "visual survey analysis failed"
}
Assert-YarlungFreshNonEmptyFile -Path $Csv -StartedAt $AnalysisStartedAt
Assert-YarlungFreshNonEmptyFile -Path $Json -StartedAt $AnalysisStartedAt
Assert-YarlungFreshNonEmptyFile -Path $Sheet -StartedAt $AnalysisStartedAt
$Metrics = Get-Content -LiteralPath $Json -Raw | ConvertFrom-Json
$MetricCount = @($Metrics).Count
if ($MetricCount -ne $Images.Count) {
    throw "visual survey JSON row count mismatch: expected $($Images.Count), got $MetricCount. See $Json"
}

Write-Host "VisualSurveyCsv=$Csv"
Write-Host "VisualSurveyJson=$Json"
Write-Host "VisualSurveySheet=$Sheet"
