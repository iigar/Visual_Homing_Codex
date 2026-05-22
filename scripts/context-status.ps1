param(
    [int] $MaxTokens = 128000,
    [int] $BarWidth = 30
)

$ErrorActionPreference = "Stop"

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")

$contextFiles = @(
    "README.md",
    "docs/PROJECT_MEMORY.md",
    "docs/SESSION_LOG.md",
    "docs/DECISIONS.md",
    "docs/ROADMAP.md",
    "docs/ARCHITECTURE.md"
)

function Get-TextStats {
    param([string] $Text)

    $chars = $Text.Length
    $words = ([regex]::Matches($Text, "\S+")).Count

    # Conservative approximation for mixed Ukrainian/English technical text.
    # Real tokenizer counts vary by model, punctuation, code, and Unicode text.
    $estimatedTokens = [math]::Ceiling($chars / 3.5)

    [pscustomobject]@{
        Chars = $chars
        Words = $words
        EstimatedTokens = [int] $estimatedTokens
    }
}

function Get-Bar {
    param(
        [double] $Ratio,
        [int] $Width
    )

    $filled = [math]::Min($Width, [math]::Max(0, [math]::Round($Ratio * $Width)))
    $empty = $Width - $filled
    return ("#" * $filled) + ("." * $empty)
}

$totalText = ""
$rows = @()

foreach ($relativePath in $contextFiles) {
    $path = Join-Path $repoRoot $relativePath
    if (-not (Test-Path -LiteralPath $path)) {
        continue
    }

    $text = Get-Content -Raw -LiteralPath $path
    $stats = Get-TextStats -Text $text
    $totalText += "`n`n--- $relativePath ---`n$text"

    $rows += [pscustomobject]@{
        Source = $relativePath
        Chars = $stats.Chars
        Words = $stats.Words
        EstTokens = $stats.EstimatedTokens
    }
}

$gitLog = ""
try {
    $gitLog = git -C $repoRoot log -3 --pretty=format:"%h %s%n%b"
} catch {
    $gitLog = ""
}

if (-not [string]::IsNullOrWhiteSpace($gitLog)) {
    $stats = Get-TextStats -Text $gitLog
    $totalText += "`n`n--- git log -3 ---`n$gitLog"
    $rows += [pscustomobject]@{
        Source = "git log -3"
        Chars = $stats.Chars
        Words = $stats.Words
        EstTokens = $stats.EstimatedTokens
    }
}

$totalStats = Get-TextStats -Text $totalText
$ratio = if ($MaxTokens -gt 0) { $totalStats.EstimatedTokens / $MaxTokens } else { 0 }
$percent = [math]::Round($ratio * 100, 1)
$bar = Get-Bar -Ratio $ratio -Width $BarWidth
$remaining = [math]::Max(0, $MaxTokens - $totalStats.EstimatedTokens)

Write-Host "Visual_Homing_Codex context pack estimate"
Write-Host ""
Write-Host "Sources:"
$rows | Format-Table -AutoSize
Write-Host "Total estimated tokens: $($totalStats.EstimatedTokens)"
Write-Host "Budget:                 $MaxTokens"
Write-Host "Remaining estimate:     $remaining"
Write-Host "Usage:                  [$bar] $percent%"
Write-Host ""
Write-Host "Important:"
Write-Host "- This is not the live Codex conversation context meter."
Write-Host "- It estimates the startup context pack: project memory docs plus git log -3."
Write-Host "- Real token counts depend on the model tokenizer and the active chat history."
