param(
    [switch]$Build,
    [int]$WaitSeconds = 12,
    [int]$ResX = 2560,
    [int]$ResY = 1440,
    [string]$Name = "offscreen-latest",
    [int]$TimeoutSeconds = 240,
    [int]$CaptureFps = 1,
    [int]$CaptureSeconds = 1,
    [double]$StartRatio = 0.34,
    [double]$StartSpeedMps = 18.0,
    [switch]$KeepSourceFrames,
    [switch]$SimulateWait,
    [string[]]$ExtraArgs = @(),
    [ValidateSet("MovieFrames", "ImmediateHighResShot")]
    [string]$Mode = "MovieFrames"
)

$ErrorActionPreference = "Stop"

$RepoRoot = Split-Path -Parent $PSScriptRoot
$Project = Join-Path $RepoRoot "CoasterSim.uproject"
$EditorCmd = "C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
$BuildBat = "C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat"
$OutputDir = Join-Path $RepoRoot "Saved\OffscreenShots"
$LogPath = Join-Path $RepoRoot "Saved\Logs\offscreen-shot.log"

New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $LogPath) | Out-Null

if ($Build) {
    & $BuildBat CoasterSimEditor Win64 Development "-Project=$Project" -WaitMutex -NoHotReloadFromIDE
    if ($LASTEXITCODE -ne 0) {
        throw "Unreal build failed with exit code $LASTEXITCODE"
    }
}

$Before = @{}
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

$CommonArgs += $ExtraArgs

if (-not $SimulateWait) {
    $CommonArgs += @(
        "-CoasterStartRatio=$StartRatio",
        "-CoasterStartSpeed=$StartSpeedMps",
        "-CoasterStartSeconds=$WaitSeconds"
    )
}

if ($Mode -eq "MovieFrames") {
    $BenchmarkSeconds = if ($SimulateWait) { $WaitSeconds } else { $CaptureSeconds }
    $Args = $CommonArgs + @(
        "-BENCHMARK",
        "-FPS=$CaptureFps",
        "-BENCHMARKSECONDS=$BenchmarkSeconds",
        "-DUMPMOVIE",
        "-ExecCmds=DisableAllScreenMessages"
    )
} else {
    $Target = (Join-Path $OutputDir "$Name.png").Replace("\", "/")
    $Args = $CommonArgs + @(
        "-ExecCmds=DisableAllScreenMessages,HighResShot $($ResX)x$($ResY) filename=$Target,Quit"
    )
}

$Process = Start-Process -FilePath $EditorCmd -ArgumentList $Args -WorkingDirectory $RepoRoot -PassThru -NoNewWindow
if (-not $Process.WaitForExit($TimeoutSeconds * 1000)) {
    Stop-Process -Id $Process.Id -Force -ErrorAction SilentlyContinue
    throw "Offscreen screenshot timed out after $TimeoutSeconds seconds. See $LogPath"
}
$Process.Refresh()
if ($null -ne $Process.ExitCode -and $Process.ExitCode -ne 0) {
    throw "Offscreen screenshot failed with exit code $($Process.ExitCode). See $LogPath"
}

$NewImages = Get-ChildItem -Path $RepoRoot -Recurse -File -Include *.png -ErrorAction SilentlyContinue |
    Where-Object { $_.FullName -like "*\Saved\*" -and -not $Before.ContainsKey($_.FullName) } |
    Sort-Object LastWriteTimeUtc

if (-not $NewImages) {
    throw "UE exited successfully but produced no new Saved PNGs. See $LogPath"
}

$Chosen = $NewImages | Select-Object -Last 1
$Output = Join-Path $OutputDir "$Name.png"
Copy-Item -LiteralPath $Chosen.FullName -Destination $Output -Force
if (-not $KeepSourceFrames) {
    foreach ($Image in $NewImages) {
        if ($Image.FullName -ne $Output) {
            Remove-Item -LiteralPath $Image.FullName -Force -ErrorAction SilentlyContinue
        }
    }
}
Write-Host "Screenshot=$Output"
Write-Host "SourceFrame=$($Chosen.FullName)"
