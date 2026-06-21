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

function Get-GitStatusPath {
    param([Parameter(Mandatory = $true)][string]$Line)

    if ($Line.Length -le 3) {
        return ""
    }

    $Path = $Line.Substring(3).Trim()
    if ($Path -like "* -> *") {
        $Path = @($Path -split " -> ")[-1].Trim()
    }
    if ($Path.StartsWith('"') -and $Path.EndsWith('"')) {
        $Path = $Path.Substring(1, $Path.Length - 2)
    }
    return ($Path -replace "\\", "/")
}

function Test-PathPrefix {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [object[]]$Prefixes
    )

    foreach ($PrefixValue in @($Prefixes)) {
        $Prefix = [string]$PrefixValue
        if ([string]::IsNullOrWhiteSpace($Prefix)) {
            continue
        }
        $Normalized = $Prefix -replace "\\", "/"
        if ($Path -eq $Normalized -or $Path.StartsWith($Normalized)) {
            return $true
        }
    }
    return $false
}

function Split-DirtyStatus {
    param(
        [string[]]$StatusLines,
        [object]$Config
    )

    $Groups = [ordered]@{
        generated_tracked = @()
        local_only = @()
        source_docs = @()
        other = @()
    }

    $DirtyConfig = if ($Config) { $Config.dirty_classification } else { $null }
    $GeneratedPrefixes = if ($DirtyConfig) { @($DirtyConfig.generated_tracked_prefixes) } else { @("Content/Generated/", "SourceAssets/Generated/") }
    $LocalPrefixes = if ($DirtyConfig) { @($DirtyConfig.local_only_prefixes) } else { @("docs/refs/local/") }
    $SourceDocPrefixes = if ($DirtyConfig) { @($DirtyConfig.source_doc_prefixes) } else { @("AGENTS.md", "Config/", "Source/", "scripts/", "docs/") }
    $RootReferenceNames = if ($DirtyConfig) { @($DirtyConfig.root_reference_names) } else { @() }

    foreach ($Line in $StatusLines) {
        $Path = Get-GitStatusPath $Line
        $Leaf = Split-Path -Leaf $Path

        if ((Test-PathPrefix $Path $LocalPrefixes) -or ($RootReferenceNames -contains $Leaf)) {
            $Groups.local_only += $Line
        } elseif (Test-PathPrefix $Path $GeneratedPrefixes) {
            $Groups.generated_tracked += $Line
        } elseif (Test-PathPrefix $Path $SourceDocPrefixes) {
            $Groups.source_docs += $Line
        } else {
            $Groups.other += $Line
        }
    }

    return [pscustomobject]$Groups
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
$DirtyGroups = Split-DirtyStatus -StatusLines $StatusLines -Config $Config
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
    dirty_groups = $DirtyGroups
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
    Write-Host "Dirty groups:"
    $GroupLabels = [ordered]@{
        generated_tracked = "generated tracked outputs"
        source_docs = "source/docs/config"
        local_only = "local-only refs/clutter"
        other = "other"
    }
    foreach ($Key in $GroupLabels.Keys) {
        $Items = @($DirtyGroups.$Key)
        Write-Host ("  {0}: {1}" -f $GroupLabels[$Key], $Items.Count)
        foreach ($Item in $Items) {
            Write-Host "    $Item"
        }
    }
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
