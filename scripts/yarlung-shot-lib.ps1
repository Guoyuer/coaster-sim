$ErrorActionPreference = "Stop"

function Convert-YarlungToFlatArgumentList {
    param([object[]]$Items)

    $Flat = @()
    foreach ($Item in $Items) {
        if ($Item -is [System.Array]) {
            foreach ($Nested in $Item) {
                $Flat += [string]$Nested
            }
        } else {
            $Flat += [string]$Item
        }
    }
    return $Flat
}

function Convert-YarlungToSecondList {
    param(
        [string[]]$Values,
        [Parameter(Mandatory = $true)][string]$ParameterName,
        [switch]$AllowEmpty
    )

    $Seconds = @()
    foreach ($Value in $Values) {
        foreach ($Token in ([string]$Value -split "[,+;\s]+")) {
            if ([string]::IsNullOrWhiteSpace($Token)) {
                continue
            }

            $Parsed = 0
            if (-not [int]::TryParse($Token, [System.Globalization.NumberStyles]::Integer, [System.Globalization.CultureInfo]::InvariantCulture, [ref]$Parsed)) {
                throw "$ParameterName contains a non-integer time value: $Token"
            }
            $Seconds += $Parsed
        }
    }

    if (-not $AllowEmpty -and $Seconds.Count -eq 0) {
        throw "$ParameterName must contain at least one time value."
    }
    return [int[]]$Seconds
}

function Test-YarlungFreshNonEmptyFile {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][datetime]$StartedAt
    )

    if (-not (Test-Path -LiteralPath $Path)) {
        return $false
    }
    $Item = Get-Item -LiteralPath $Path
    return $Item.Length -gt 0 -and $Item.LastWriteTime -ge $StartedAt.AddSeconds(-1)
}

function Assert-YarlungFreshNonEmptyFile {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][datetime]$StartedAt
    )

    if (-not (Test-YarlungFreshNonEmptyFile -Path $Path -StartedAt $StartedAt)) {
        throw "Expected output is missing, stale, or empty: $Path"
    }
}
