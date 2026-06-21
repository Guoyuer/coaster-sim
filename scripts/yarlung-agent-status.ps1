param(
    [switch]$Json,
    [int]$ProgressLines = 18,
    [int]$LatestRuns = 5
)

$ErrorActionPreference = "Stop"

$RepoRoot = Split-Path -Parent $PSScriptRoot
$ConfigPath = Join-Path $RepoRoot "Config\yarlung-iteration.json"
$ProgressPath = Join-Path $RepoRoot "docs\plans\photoreal-progress.md"
$DiagnosticsDir = Join-Path $RepoRoot "Saved\Diagnostics"

function Read-JsonFile {
    param([Parameter(Mandatory = $true)][string]$Path)
    if (-not (Test-Path -LiteralPath $Path)) {
        return $null
    }
    return Get-Content -LiteralPath $Path -Raw | ConvertFrom-Json
}

Push-Location $RepoRoot
try {
    $Branch = (& git branch --show-current).Trim()
    $Head = (& git log -1 --oneline).Trim()
    $StatusLines = @(& git status --short | ForEach-Object { [string]$_ })
} finally {
    Pop-Location
}

$Config = Read-JsonFile $ConfigPath
$ProgressTop = @()
if (Test-Path -LiteralPath $ProgressPath) {
    $ProgressTop = Get-Content -LiteralPath $ProgressPath -TotalCount $ProgressLines
}

$RunFiles = @()
if (Test-Path -LiteralPath $DiagnosticsDir) {
    $RunFiles = Get-ChildItem -LiteralPath $DiagnosticsDir -Filter "*-run.json" -File |
        Sort-Object LastWriteTimeUtc -Descending |
        Select-Object -First $LatestRuns
}

$Runs = @()
foreach ($RunFile in $RunFiles) {
    $Run = Read-JsonFile $RunFile.FullName
    if ($Run) {
        $Runs += [pscustomobject]@{
            file = $RunFile.FullName
            name_prefix = $Run.name_prefix
            mode = $Run.mode
            preset = $Run.preset
            risk_gate = $Run.risk_gate
            worst = if ($Run.worst) { Split-Path -Leaf $Run.worst.path } else { $null }
            contact_sheet = $Run.outputs.contact_sheet
            finished_at = $Run.finished_at
        }
    }
}

$Recommended = ".\scripts\iterate-yarlung.ps1 -Mode $($Config.default_mode) -Preset Standard -Build -NamePrefix <short-name>"
$Summary = [ordered]@{
    branch = $Branch
    head = $Head
    dirty_count = $StatusLines.Count
    dirty = @($StatusLines)
    config = $ConfigPath
    progress = $ProgressPath
    recommended_command = $Recommended
    latest_runs = @($Runs)
    progress_top_text = ($ProgressTop -join [Environment]::NewLine)
}

if ($Json) {
    [pscustomobject]$Summary | ConvertTo-Json -Depth 6 -Compress
    exit 0
}

Write-Host "Yarlung Codex Agent Status"
Write-Host "Branch: $Branch"
Write-Host "HEAD:   $Head"
Write-Host "Dirty:  $($StatusLines.Count) file(s)"
if ($StatusLines.Count -gt 0) {
    Write-Host ""
    Write-Host "Dirty files:"
    $StatusLines | ForEach-Object { Write-Host "  $_" }
}

Write-Host ""
Write-Host "Recommended next command:"
Write-Host "  $Recommended"

if ($Runs.Count -gt 0) {
    Write-Host ""
    Write-Host "Latest runs:"
    foreach ($Run in $Runs) {
        Write-Host ("  {0} mode={1} preset={2} risk={3} contact={4}" -f $Run.name_prefix, $Run.mode, $Run.preset, $Run.risk_gate, $Run.contact_sheet)
    }
}

Write-Host ""
Write-Host "Progress top:"
$ProgressTop | ForEach-Object { Write-Host $_ }
