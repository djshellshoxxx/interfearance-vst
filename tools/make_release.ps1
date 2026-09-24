<#
.SYNOPSIS
    Builds the Interference release ZIP (VST3 + stand-alone + docs) in dist\.

.DESCRIPTION
    The archive is reproducible for identical inputs: entries are added in sorted order, every entry
    gets the timestamp of the last git commit (or 2026-01-01 without git), paths use forward slashes
    and no machine-specific metadata is stored. A SHA-256 checksum file is written next to it.
    MSVC binaries themselves embed link timestamps, so re-linking produces different bytes; the
    packaging step is what this script makes deterministic.

    -Build     configure + build Release and run both test suites first (aborts on any failure)
    -BuildDir  build folder (default: build)

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File tools\make_release.ps1 -Build
#>
param(
    [switch] $Build,
    [string] $BuildDir = "build"
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

# ---- version from CMakeLists.txt ------------------------------------------------------------
$cm = Get-Content -Raw (Join-Path $root "CMakeLists.txt")
if ($cm -notmatch 'project\(Interference VERSION (\d+\.\d+\.\d+)') { throw "Version not found in CMakeLists.txt" }
$version = $Matches[1]
$name = "Interference-$version-win64"
Write-Host "Interference $version"

# ---- optional build + tests -------------------------------------------------------------------
if ($Build) {
    cmake -S . -B $BuildDir -G "Visual Studio 17 2022" -A x64
    if ($LASTEXITCODE -ne 0) { throw "CMake configure failed" }
    cmake --build $BuildDir --config Release -- -m:2
    if ($LASTEXITCODE -ne 0) { throw "Build failed" }
    foreach ($t in @("InterferenceTests", "InterferenceGuiTests")) {
        $exe = Join-Path $root "$BuildDir\${t}_artefacts\Release\$t.exe"
        & $exe
        if ($LASTEXITCODE -ne 0) { throw "$t failed (exit $LASTEXITCODE)" }
    }
}

# ---- gather inputs -----------------------------------------------------------------------------
$art = Join-Path $root "$BuildDir\Interference_artefacts\Release"
$vst3 = Join-Path $art "VST3\Interference.vst3"
$exe = Join-Path $art "Standalone\Interference.exe"
foreach ($p in @($vst3, $exe)) { if (-not (Test-Path $p)) { throw "Missing build artifact: $p (build Release first or pass -Build)" } }

$items = New-Object System.Collections.Generic.List[object]
function Add-Item([string] $source, [string] $entry) { $items.Add([pscustomobject]@{ Source = $source; Entry = $entry }) }

Get-ChildItem -Recurse -File $vst3 | ForEach-Object {
    $rel = $_.FullName.Substring($vst3.Length).TrimStart('\')
    Add-Item $_.FullName ("$name/VST3/Interference.vst3/" + $rel.Replace('\', '/'))
}
Add-Item $exe "$name/Standalone/Interference.exe"
foreach ($f in @("README.md", "CHANGELOG.md", "LICENSE", "THIRD_PARTY.md")) { Add-Item (Join-Path $root $f) "$name/$f" }
Get-ChildItem -File (Join-Path $root "Docs") -Filter *.md | ForEach-Object { Add-Item $_.FullName "$name/Docs/$($_.Name)" }

# licence notice check: the archive must carry the licence and third-party notices
foreach ($req in @("LICENSE", "THIRD_PARTY.md")) {
    if (-not ($items | Where-Object { $_.Entry -eq "$name/$req" })) { throw "Release is missing $req" }
}
if (-not (Select-String -Quiet -Path (Join-Path $root "THIRD_PARTY.md") -Pattern "JUCE")) { throw "THIRD_PARTY.md does not list JUCE" }

# ---- deterministic ZIP -------------------------------------------------------------------------
$stamp = [DateTimeOffset]::new(2026, 1, 1, 0, 0, 0, [TimeSpan]::Zero)
try {
    $ct = (git log -1 --format=%ct 2>$null)
    if ($LASTEXITCODE -eq 0 -and $ct) { $stamp = [DateTimeOffset]::FromUnixTimeSeconds([int64]$ct) }
} catch { }

Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem
$dist = Join-Path $root "dist"
New-Item -ItemType Directory -Force $dist | Out-Null
$zipPath = Join-Path $dist "$name.zip"
if (Test-Path $zipPath) { Remove-Item -Force $zipPath }

$sorted = $items | Sort-Object -Property Entry -CaseSensitive
$fs = [System.IO.File]::Open($zipPath, [System.IO.FileMode]::CreateNew)
try {
    $zip = New-Object System.IO.Compression.ZipArchive($fs, [System.IO.Compression.ZipArchiveMode]::Create)
    try {
        foreach ($it in $sorted) {
            $e = $zip.CreateEntry($it.Entry, [System.IO.Compression.CompressionLevel]::Optimal)
            $e.LastWriteTime = $stamp
            $out = $e.Open()
            try {
                $bytes = [System.IO.File]::ReadAllBytes($it.Source)
                $out.Write($bytes, 0, $bytes.Length)
            } finally { $out.Dispose() }
        }
    } finally { $zip.Dispose() }
} finally { $fs.Dispose() }

$hash = (Get-FileHash -Algorithm SHA256 $zipPath).Hash.ToLowerInvariant()
Set-Content -NoNewline -Encoding ascii -Path "$zipPath.sha256" -Value "$hash  $name.zip`n"
Write-Host ("Wrote {0} ({1} files, {2:N0} bytes)" -f $zipPath, $sorted.Count, (Get-Item $zipPath).Length)
Write-Host "SHA-256 $hash"
