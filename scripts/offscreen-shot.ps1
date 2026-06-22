param(
    [switch]$Build,
    [Nullable[int]]$JumpSeconds = $null,
    [int]$ResX = 2560,
    [int]$ResY = 1440,
    [string]$Name = "offscreen-latest",
    [int]$TimeoutSeconds = 240,
    [int]$CaptureFps = 1,
    [int]$CaptureSeconds = 1,
    [double]$StartRatio = 0.34,
    [double]$StartSpeedMps = 18.0,
    [string[]]$BatchJumpSeconds = @(),
    [string]$BatchNamePrefix = "",
    [int]$BatchSettleFrames = 4,
    [int]$BatchPostFrames = 8,
    [string[]]$ExtraArgs = @(),
    [ValidateSet("MovieFrames", "ImmediateHighResShot")]
    [string]$CaptureMode = "MovieFrames"
)

$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "yarlung-shot-lib.ps1")

$RepoRoot = Split-Path -Parent $PSScriptRoot
$Project = Join-Path $RepoRoot "CoasterSim.uproject"
$EditorCmd = "C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
$BuildBat = "C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat"
$EditorCommonArgs = @("-DDC-ForceMemoryCache")
$OutputDir = Join-Path $RepoRoot "Saved\OffscreenShots"
$LogPath = Join-Path $RepoRoot "Saved\Logs\offscreen-shot.log"
$StdoutPath = Join-Path $RepoRoot "Saved\Logs\offscreen-shot.stdout.log"
$StderrPath = Join-Path $RepoRoot "Saved\Logs\offscreen-shot.stderr.log"

New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $LogPath) | Out-Null

function Assert-OffscreenRunNoAssetSubstitution {
    param([Parameter(Mandatory = $true)][string]$Context)

    $TextParts = @()
    foreach ($Path in @($LogPath, $StdoutPath, $StderrPath)) {
        if (Test-Path -LiteralPath $Path) {
            $TextParts += Get-Content -LiteralPath $Path -Raw
        }
    }
    if ($TextParts.Count -eq 0) {
        throw "$Context produced no readable UE log/stdout/stderr files."
    }
    Assert-YarlungNoRuntimeAssetSubstitution -Text ($TextParts -join [Environment]::NewLine) -Context $Context
}

if ($Build) {
    & $BuildBat CoasterSimEditor Win64 Development "-Project=$Project" -WaitMutex -NoHotReloadFromIDE
    if ($LASTEXITCODE -ne 0) {
        throw "Unreal build failed with exit code $LASTEXITCODE"
    }
}

$RunStartedAt = Get-Date
Remove-Item -LiteralPath $LogPath, $StdoutPath, $StderrPath -Force -ErrorAction SilentlyContinue

$BatchSeconds = Convert-YarlungToSecondList -Values $BatchJumpSeconds -ParameterName "BatchJumpSeconds" -AllowEmpty
if ($BatchSeconds.Count -gt 0) {
    if ([string]::IsNullOrWhiteSpace($BatchNamePrefix)) {
        throw "BatchNamePrefix is required when BatchJumpSeconds is provided."
    }

    $ExpectedOutputs = @()
    foreach ($Second in $BatchSeconds) {
        $Expected = Join-Path $OutputDir "$BatchNamePrefix-t$Second.png"
        Remove-Item -LiteralPath $Expected -Force -ErrorAction SilentlyContinue
        $ExpectedOutputs += $Expected
    }

    $TimesCsv = ($BatchSeconds | ForEach-Object { [string]$_ }) -join "+"
    $BatchOutputDir = $OutputDir.Replace("\", "/")
    $BatchArgs = @(
        $Project,
        "/Game/Generated/YarlungLandscape/YarlungLandscape_Level",
        "-game",
        "-RenderOffScreen",
        "-unattended",
        "-nop4",
        "-NoSplash",
        "-NoLoadingScreen",
        "-NoLiveCoding",
        "-NoSound",
        "-noscreenmessages",
        "-ForceRes",
        "-ResX=$ResX",
        "-ResY=$ResY",
        "-stdout",
        "-FullStdOutLogOutput",
        "-log=$LogPath",
        "-CoasterStartRatio=$StartRatio",
        "-CoasterStartSpeed=$StartSpeedMps",
        "-YarlungBatchShotTimes=$TimesCsv",
        "-YarlungBatchShotPrefix=$BatchNamePrefix",
        "-YarlungBatchShotDir=$BatchOutputDir",
        "-YarlungBatchShotResX=$ResX",
        "-YarlungBatchShotResY=$ResY",
        "-YarlungBatchShotSettleFrames=$BatchSettleFrames",
        "-YarlungBatchShotPostFrames=$BatchPostFrames",
        "-ExecCmds=DisableAllScreenMessages"
    )
    $BatchArgs += $EditorCommonArgs
    $BatchArgs += $ExtraArgs
    $BatchArgs = Convert-YarlungToFlatArgumentList -Items $BatchArgs

    $Process = Start-Process -FilePath $EditorCmd -ArgumentList $BatchArgs -WorkingDirectory $RepoRoot -PassThru -RedirectStandardOutput $StdoutPath -RedirectStandardError $StderrPath
    if (-not $Process.WaitForExit($TimeoutSeconds * 1000)) {
        Stop-Process -Id $Process.Id -Force -ErrorAction SilentlyContinue
        throw "Batch offscreen screenshot timed out after $TimeoutSeconds seconds. See $LogPath"
    }
    $Process.Refresh()
    if ($null -ne $Process.ExitCode -and $Process.ExitCode -ne 0) {
        throw "Batch offscreen screenshot failed with exit code $($Process.ExitCode). See $LogPath"
    }
    Assert-OffscreenRunNoAssetSubstitution -Context "Batch offscreen screenshot"

    $Missing = @()
    foreach ($Expected in $ExpectedOutputs) {
        if (-not (Test-YarlungFreshNonEmptyFile -Path $Expected -StartedAt $RunStartedAt)) {
            $Missing += "$Expected (missing, stale, or empty)"
        }
    }
    if ($Missing.Count -gt 0) {
        throw "Batch UE exited but did not produce expected screenshots: $($Missing -join '; '). See $LogPath"
    }

    foreach ($Expected in $ExpectedOutputs) {
        Write-Host "Screenshot=$Expected"
    }
    Write-Host "CaptureMode=BatchJumpToSeconds"
    Write-Host "TargetSeconds=$TimesCsv"
    return
}

if ($null -eq $JumpSeconds) {
    throw "JumpSeconds is required for single-shot capture. Use BatchJumpSeconds for multi-shot capture."
}
$TargetSeconds = [int]$JumpSeconds

$Before = @{}
$Output = Join-Path $OutputDir "$Name.png"
Remove-Item -LiteralPath $Output -Force -ErrorAction SilentlyContinue
Get-ChildItem -Path $RepoRoot -Recurse -File -Include *.png -ErrorAction SilentlyContinue |
    Where-Object { $_.FullName -like "*\Saved\*" } |
    ForEach-Object { $Before[$_.FullName] = $true }

$CommonArgs = @(
    $Project,
    "/Game/Generated/YarlungLandscape/YarlungLandscape_Level",
    "-game",
    "-RenderOffScreen",
    "-unattended",
    "-nop4",
    "-NoSplash",
    "-NoLoadingScreen",
    "-NoLiveCoding",
    "-NoSound",
    "-noscreenmessages",
    "-ForceRes",
    "-ResX=$ResX",
    "-ResY=$ResY",
    "-stdout",
    "-FullStdOutLogOutput",
    "-log=$LogPath"
)

$CommonArgs += $EditorCommonArgs
$CommonArgs += $ExtraArgs

$CommonArgs += @(
    "-CoasterStartRatio=$StartRatio",
    "-CoasterStartSpeed=$StartSpeedMps",
    "-CoasterStartSeconds=$TargetSeconds"
)

if ($CaptureMode -eq "MovieFrames") {
    $Args = $CommonArgs + @(
        "-BENCHMARK",
        "-FPS=$CaptureFps",
        "-BENCHMARKSECONDS=$CaptureSeconds",
        "-DUMPMOVIE",
        "-ExecCmds=DisableAllScreenMessages"
    )
} else {
    $Target = (Join-Path $OutputDir "$Name.png").Replace("\", "/")
    $Args = $CommonArgs + @(
        "-ExecCmds=DisableAllScreenMessages,HighResShot $($ResX)x$($ResY) filename=$Target,Quit"
    )
}

$Args = Convert-YarlungToFlatArgumentList -Items $Args
$Process = Start-Process -FilePath $EditorCmd -ArgumentList $Args -WorkingDirectory $RepoRoot -PassThru -RedirectStandardOutput $StdoutPath -RedirectStandardError $StderrPath
if (-not $Process.WaitForExit($TimeoutSeconds * 1000)) {
    Stop-Process -Id $Process.Id -Force -ErrorAction SilentlyContinue
    throw "Offscreen screenshot timed out after $TimeoutSeconds seconds. See $LogPath"
}
$Process.Refresh()
if ($null -ne $Process.ExitCode -and $Process.ExitCode -ne 0) {
    throw "Offscreen screenshot failed with exit code $($Process.ExitCode). See $LogPath"
}
Assert-OffscreenRunNoAssetSubstitution -Context "Offscreen screenshot"

$NewImages = Get-ChildItem -Path $RepoRoot -Recurse -File -Include *.png -ErrorAction SilentlyContinue |
    Where-Object { $_.FullName -like "*\Saved\*" -and -not $Before.ContainsKey($_.FullName) } |
    Sort-Object LastWriteTimeUtc

if (-not $NewImages) {
    throw "UE exited successfully but produced no new Saved PNGs. See $LogPath"
}

$Chosen = $NewImages | Select-Object -Last 1
Copy-Item -LiteralPath $Chosen.FullName -Destination $Output -Force
if (-not (Test-YarlungFreshNonEmptyFile -Path $Output -StartedAt $RunStartedAt)) {
    throw "UE exited but final screenshot is missing, stale, or empty: $Output. See $LogPath"
}
foreach ($Image in $NewImages) {
    if ($Image.FullName -ne $Output) {
        Remove-Item -LiteralPath $Image.FullName -Force -ErrorAction SilentlyContinue
    }
}
Write-Host "Screenshot=$Output"
Write-Host "SourceFrame=$($Chosen.FullName)"
Write-Host "CaptureMode=JumpToSeconds"
Write-Host "TargetSeconds=$TargetSeconds"
