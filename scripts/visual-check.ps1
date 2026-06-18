param(
    [switch]$Build,
    [switch]$Fast,
    [int[]]$WaitSeconds = @(2, 6, 10, 15),
    [int]$ResX = 1280,
    [int]$ResY = 720,
    [string]$Name = "VisualCheck-latest",
    [string]$ExecCmds = ""
)

$ErrorActionPreference = "Stop"

$RepoRoot = Split-Path -Parent $PSScriptRoot
$Project = Join-Path $RepoRoot "CoasterSim.uproject"
$Editor = "C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe"
$BuildBat = "C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat"
$OutputDir = Join-Path $RepoRoot "Saved"

if ($Fast -and -not $PSBoundParameters.ContainsKey("WaitSeconds")) {
    $WaitSeconds = @(10)
}

if ($Build) {
    & $BuildBat CoasterSimEditor Win64 Development "-Project=$Project" -WaitMutex -NoHotReloadFromIDE
    if ($LASTEXITCODE -ne 0) {
        throw "Unreal build failed with exit code $LASTEXITCODE"
    }
}

Get-Process UnrealEditor -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue

$CombinedExecCmds = "DisableAllScreenMessages"
if (-not [string]::IsNullOrWhiteSpace($ExecCmds)) {
    $CombinedExecCmds = "$CombinedExecCmds;$ExecCmds"
}

$Process = Start-Process -FilePath $Editor -ArgumentList @(
    $Project,
    "-game",
    "-windowed",
    "-ResX=$ResX",
    "-ResY=$ResY",
    "-NoSplash",
    "-NoLoadingScreen",
    "-NoLiveCoding",
    "-ExecCmds=$CombinedExecCmds"
) -WorkingDirectory $RepoRoot -PassThru

$GameWindow = $null
for ($Index = 0; $Index -lt 90; ++$Index) {
    Start-Sleep -Milliseconds 500
    $GameWindow = Get-Process UnrealEditor -ErrorAction SilentlyContinue |
        Where-Object { $_.MainWindowHandle -ne 0 -and $_.MainWindowTitle -like "*CoasterSim*" } |
        Select-Object -First 1
    if ($GameWindow) {
        break
    }
}

if (-not $GameWindow) {
    Get-Process UnrealEditor -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
    throw "CoasterSim window was not found"
}

$CaptureTimes = @($WaitSeconds | Sort-Object -Unique)
$SingleCapture = $CaptureTimes.Count -eq 1
$Outputs = @()
$ElapsedSeconds = 0

if (-not ("WinCapVisualCheck" -as [type])) {
    Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
using System.Drawing;

public class WinCapVisualCheck {
  [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr hwnd, IntPtr hdcBlt, int nFlags);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr hwnd, out RECT lpRect);
  public struct RECT { public int Left; public int Top; public int Right; public int Bottom; }

  public static void Capture(IntPtr hwnd, string path) {
    RECT rc; GetWindowRect(hwnd, out rc);
    int width = Math.Max(rc.Right - rc.Left, 1);
    int height = Math.Max(rc.Bottom - rc.Top, 1);
    using (Bitmap bitmap = new Bitmap(width, height)) {
      using (Graphics graphics = Graphics.FromImage(bitmap)) {
        IntPtr hdc = graphics.GetHdc();
        PrintWindow(hwnd, hdc, 2);
        graphics.ReleaseHdc(hdc);
      }
      bitmap.Save(path, System.Drawing.Imaging.ImageFormat.Png);
    }
  }
}
'@ -ReferencedAssemblies System.Drawing
}

foreach ($CaptureSecond in $CaptureTimes) {
    $DeltaSeconds = [Math]::Max($CaptureSecond - $ElapsedSeconds, 0)
    if ($DeltaSeconds -gt 0) {
        Start-Sleep -Seconds $DeltaSeconds
    }

    if ($SingleCapture) {
        $Output = Join-Path $OutputDir "$Name.png"
    } else {
        $Output = Join-Path $OutputDir ("{0}-t{1}s.png" -f $Name, $CaptureSecond)
    }

    [WinCapVisualCheck]::Capture($GameWindow.MainWindowHandle, $Output)
    $Outputs += $Output
    $ElapsedSeconds = $CaptureSecond
}

Get-Process UnrealEditor -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue

foreach ($Output in $Outputs) {
    Write-Host "Screenshot=$Output"
}
