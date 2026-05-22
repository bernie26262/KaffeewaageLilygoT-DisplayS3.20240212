param(
    [string]$OutDir = "C:\Users\bernh\Downloads"
)

$ErrorActionPreference = "Stop"

$repoRoot = (git rev-parse --show-toplevel).Trim()
$repoName = Split-Path $repoRoot -Leaf
$timestamp = Get-Date -Format "yyyyMMdd_HHmm"
$outZip = Join-Path $OutDir "$repoName.$timestamp.zip"

$tempRoot = Join-Path $env:TEMP "$repoName-export-$timestamp"

if (Test-Path $tempRoot) {
    Remove-Item -Recurse -Force $tempRoot
}

New-Item -ItemType Directory -Path $tempRoot | Out-Null

Push-Location $repoRoot

try {
    $files = git ls-files --cached --others --exclude-standard |
    Where-Object {
        $_ -notmatch '^(?:\.pio|\.vscode|lib|libdeps|lib_deps|build|dist|tmp|temp|WLAN-ICONs)/' -and
        $_ -notmatch '/(?:lib|libdeps|lib_deps)/' -and
        $_ -notmatch '\.(?:zip|bin|elf|map|o|a|pyc|xlsx|code-workspace)$' -and
        $_ -ne 'PinOut.txt'
    }

    foreach ($file in $files) {
        $src = Join-Path $repoRoot $file
        $dst = Join-Path $tempRoot $file
        $dstDir = Split-Path $dst -Parent

        if (!(Test-Path $dstDir)) {
            New-Item -ItemType Directory -Path $dstDir -Force | Out-Null
        }

        Copy-Item $src $dst -Force
    }

    if (Test-Path $outZip) {
        Remove-Item $outZip -Force
    }

    Compress-Archive -Path (Join-Path $tempRoot "*") -DestinationPath $outZip -Force

    Write-Host ""
    Write-Host "Export erstellt:"
    Write-Host $outZip
    Write-Host ""
    Write-Host "Enthalten sind:"
    Write-Host "- getrackte Dateien"
    Write-Host "- ungetrackte, nicht ignorierte Dateien"
    Write-Host ""
    Write-Host "Nicht enthalten sind:"
    Write-Host "- .git"
    Write-Host "- .pio"
    Write-Host "- Dateien aus .gitignore, z.B. wifi_secrets.h"
}
finally {
    Pop-Location

    if (Test-Path $tempRoot) {
        Remove-Item -Recurse -Force $tempRoot
    }
}