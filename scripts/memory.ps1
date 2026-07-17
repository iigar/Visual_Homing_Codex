param(
    [Parameter(Mandatory = $true, Position = 0)]
    [ValidateSet("startup", "decision", "session", "memory", "note")]
    [string] $Type,

    [Parameter(Position = 1)]
    [string] $Title,

    [Parameter(Position = 2)]
    [string] $Text
)

$ErrorActionPreference = "Stop"

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$date = Get-Date -Format "yyyy-MM-dd"

function Ensure-ParentDirectory {
    param([string] $Path)
    $parent = Split-Path -Parent $Path
    if (-not (Test-Path -LiteralPath $parent)) {
        New-Item -ItemType Directory -Force -Path $parent | Out-Null
    }
}

function Add-MarkdownEntry {
    param(
        [string] $Path,
        [string] $Heading,
        [string] $Body
    )

    Ensure-ParentDirectory -Path $Path

    if (-not (Test-Path -LiteralPath $Path)) {
        "# $Heading`n" | Set-Content -LiteralPath $Path -Encoding UTF8
    }

    $entry = @"

## $date - $Heading

$Body
"@

    Add-Content -LiteralPath $Path -Value $entry -Encoding UTF8
    Write-Host "Appended entry to $Path"
}

function Get-SafeFileName {
    param([string] $Name)
    $safe = $Name.ToLowerInvariant()
    $safe = $safe -replace "[^a-z0-9]+", "-"
    $safe = $safe.Trim("-")
    if ([string]::IsNullOrWhiteSpace($safe)) {
        return "note-$date"
    }
    return $safe
}

if ($Type -eq "startup") {
    Write-Output "Продовжуємо Visual_Homing_Codex. Перед роботою прочитай docs/PROJECT_MEMORY.md, docs/HARDWARE_ACCESS_BASELINE_UA.md, docs/COORDINATE_FRAME_CONTRACT_UA.md, docs/SESSION_LOG.md, docs/DECISIONS.md, docs/ROADMAP.md і git log -3."
    exit 0
}

if ([string]::IsNullOrWhiteSpace($Title)) {
    throw "Title is required for '$Type'."
}

if ([string]::IsNullOrWhiteSpace($Text)) {
    throw "Text is required for '$Type'."
}

switch ($Type) {
    "decision" {
        Add-MarkdownEntry `
            -Path (Join-Path $repoRoot "docs/DECISIONS.md") `
            -Heading $Title `
            -Body $Text
    }
    "session" {
        Add-MarkdownEntry `
            -Path (Join-Path $repoRoot "docs/SESSION_LOG.md") `
            -Heading $Title `
            -Body $Text
    }
    "memory" {
        Add-MarkdownEntry `
            -Path (Join-Path $repoRoot "docs/PROJECT_MEMORY.md") `
            -Heading $Title `
            -Body $Text
    }
    "note" {
        $fileName = "$(Get-SafeFileName -Name $Title).md"
        $path = Join-Path $repoRoot "notes/$fileName"
        Add-MarkdownEntry `
            -Path $path `
            -Heading $Title `
            -Body $Text
    }
}
