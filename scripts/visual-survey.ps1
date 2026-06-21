param(
    [int[]]$Times = @(0, 6, 12, 18, 24, 30),
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

$RepoRoot = Split-Path -Parent $PSScriptRoot
$Images = @()

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
    $Images += $Path
}

$Csv = Join-Path $RepoRoot "Saved\Diagnostics\$NamePrefix.csv"
$Json = Join-Path $RepoRoot "Saved\Diagnostics\$NamePrefix.json"
$Sheet = Join-Path $RepoRoot "Saved\Diagnostics\$NamePrefix.png"
& python (Join-Path $PSScriptRoot "analyze-offscreen-shots.py") @Images --out-csv $Csv --out-json $Json --contact-sheet $Sheet
if ($LASTEXITCODE -ne 0) {
    throw "visual survey analysis failed"
}

Write-Host "VisualSurveyCsv=$Csv"
Write-Host "VisualSurveyJson=$Json"
Write-Host "VisualSurveySheet=$Sheet"
