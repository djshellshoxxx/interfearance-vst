<#
.SYNOPSIS
    Builds the Interference release ZIP (VST3 + stand-alone + docs + licence texts) in dist\.

.DESCRIPTION
    The archive is reproducible for identical inputs AND the same PowerShell / .NET version (deflate output differs
    between Windows PowerShell 5.1 and PowerShell 7; CI uses 5.1): entries are added in ordinal order, every entry
    gets the timestamp of the last git commit (or 2026-01-01 without git), paths use forward slashes and no
    machine-specific metadata is stored. A SHA-256 checksum file is written next to it. MSVC binaries embed link
    timestamps, so re-linking produces different bytes; the packaging step is what this script makes deterministic.

    The ZIP carries LICENSE, THIRD_PARTY.md and the third-party licence texts (ThirdPartyLicenses/), copied from the
    JUCE 8.0.8 checkout; the script fails if any is missing. SIGNATURE.txt states whether the binaries were signed
    (set IFX_SIGNED_BY to the certificate subject in a signing pipeline; unset = UNSIGNED).

    -Build     configure + build Release and run both test suites first (aborts on any failure); the tests write
               their data into a temporary folder (IFX_DATA_ROOT), never into Documents
    -BuildDir  build folder (default: build)

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File tools\make_release.ps1 -Build
#>
param(
    [switch] $Build,
    [string] $BuildDir = "build"
)

$ErrorActionPreference = "Stop"
$root = (Resolve-Path (Split-Path -Parent $PSScriptRoot)).ProviderPath
Set-Location $root
$bd = if ([IO.Path]::IsPathRooted($BuildDir)) { $BuildDir } else { Join-Path $root $BuildDir }

# native tools write warnings to stderr; under Windows PowerShell 5.1 with "Stop" that would abort the script
function Invoke-Native([scriptblock] $block) {
    $eap = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    try { & $block } finally { $ErrorActionPreference = $eap }
}

# ---- version from CMakeLists.txt ------------------------------------------------------------
$cm = Get-Content -Raw (Join-Path $root "CMakeLists.txt")
if ($cm -notmatch 'project\(Interference VERSION (\d+\.\d+\.\d+)') { throw "Version not found in CMakeLists.txt" }
$version = $Matches[1]
$name = "Interference-$version-win64"
Write-Host "Interference $version"

# ---- optional build + tests -------------------------------------------------------------------
if ($Build) {
    Invoke-Native { cmake -S . -B $BuildDir -G "Visual Studio 17 2022" -A x64 }
    if ($LASTEXITCODE -ne 0) { throw "CMake configure failed" }
    Invoke-Native { cmake --build $BuildDir --config Release -- -m:2 }
    if ($LASTEXITCODE -ne 0) { throw "Build failed" }
    $env:IFX_DATA_ROOT = Join-Path ([IO.Path]::GetTempPath()) "interference-release-tests"
    New-Item -ItemType Directory -Force $env:IFX_DATA_ROOT | Out-Null
    foreach ($t in @("InterferenceTests", "InterferenceGuiTests")) {
        $testExe = Join-Path $bd "${t}_artefacts\Release\$t.exe"
        Invoke-Native { & $testExe }
        if ($LASTEXITCODE -ne 0) { throw "$t failed (exit $LASTEXITCODE)" }
    }
}

# ---- gather inputs -----------------------------------------------------------------------------
$art = Join-Path $bd "Interference_artefacts\Release"
$vst3 = Join-Path $art "VST3\Interference.vst3"
$exe = Join-Path $art "Standalone\Interference.exe"
foreach ($p in @($vst3, $exe)) { if (-not (Test-Path -LiteralPath $p)) { throw "Missing build artifact: $p (build Release first or pass -Build)" } }
$vst3 = (Resolve-Path -LiteralPath $vst3).ProviderPath.TrimEnd('\')
$exe = (Resolve-Path -LiteralPath $exe).ProviderPath

$items = New-Object System.Collections.Generic.List[object]
function Add-Item([string] $source, [string] $entry) { $items.Add([pscustomobject]@{ Source = $source; Entry = $entry }) }
function Add-Text([string] $entry, [string] $text) { $items.Add([pscustomobject]@{ Source = $null; Text = $text; Entry = $entry }) }

Get-ChildItem -Recurse -File -LiteralPath $vst3 | ForEach-Object {
    $rel = $_.FullName.Substring($vst3.Length).TrimStart('\')
    Add-Item $_.FullName ("$name/VST3/Interference.vst3/" + $rel.Replace('\', '/'))
}
Add-Item $exe "$name/Standalone/Interference.exe"
$clap = Join-Path $art "CLAP\Interference.clap"
if (Test-Path -LiteralPath $clap) { Add-Item (Resolve-Path -LiteralPath $clap).ProviderPath "$name/CLAP/Interference.clap" }
foreach ($f in @("README.md", "CHANGELOG.md", "LICENSE", "THIRD_PARTY.md")) { Add-Item (Join-Path $root $f) "$name/$f" }
Get-ChildItem -File (Join-Path $root "Docs") -Filter *.md | ForEach-Object { Add-Item $_.FullName "$name/Docs/$($_.Name)" }
# the one research note the README and Research.md link to (the other internal reports stay out)
Add-Item (Join-Path $root "Docs\research\ElectricalInterference.md") "$name/Docs/research/ElectricalInterference.md"

# ---- third-party licence texts (THIRD_PARTY.md), found the same way CMake finds JUCE ----------
$juce = @((Join-Path $root "JUCE"), (Join-Path (Split-Path $root -Parent) "JUCE_8_0_8")) |
        Where-Object { Test-Path (Join-Path $_ "LICENSE.md") } | Select-Object -First 1
if (-not $juce) { throw "JUCE checkout not found: third-party licence texts cannot be packed" }
$texts = [ordered]@{
    "JUCE-LICENSE.md"       = "LICENSE.md"
    "HarfBuzz-COPYING.txt"  = "modules/juce_graphics/fonts/harfbuzz/COPYING"
    "SheenBidi-LICENSE.txt" = "modules/juce_graphics/unicode/sheenbidi/LICENSE"
    "libpng-LICENSE.txt"    = "modules/juce_graphics/image_formats/pnglib/LICENSE"
    "IJG-README.txt"        = "modules/juce_graphics/image_formats/jpglib/README"
    "zlib-README.txt"       = "modules/juce_core/zip/zlib/README"
    "VST3-SDK-LICENSE.txt"  = "modules/juce_audio_processors/format_types/VST3_SDK/LICENSE.txt"
}
foreach ($k in $texts.Keys) {
    $src = Join-Path $juce $texts[$k]
    if (-not (Test-Path -LiteralPath $src)) { throw "Missing licence text: $src" }
    Add-Item $src "$name/ThirdPartyLicenses/$k"
}

# ---- signing statement -------------------------------------------------------------------------
$signedBy = $env:IFX_SIGNED_BY
$sig = if ([string]::IsNullOrWhiteSpace($signedBy)) {
    "UNSIGNED build: the VST3 and Interference.exe carry no Authenticode signature.`r`n"
} else {
    "Signed build: the VST3 binary and Interference.exe are Authenticode-signed by: $signedBy`r`n"
}
Add-Text "$name/SIGNATURE.txt" $sig

# ---- notice checks: the archive must carry the licence and every third-party text ---------------
foreach ($req in @("LICENSE", "THIRD_PARTY.md")) {
    if (-not ($items | Where-Object { $_.Entry -eq "$name/$req" })) { throw "Release is missing $req" }
}
foreach ($k in $texts.Keys) {
    if (-not ($items | Where-Object { $_.Entry -eq "$name/ThirdPartyLicenses/$k" })) { throw "Release is missing ThirdPartyLicenses/$k" }
}
$tp = Get-Content -Raw (Join-Path $root "THIRD_PARTY.md")
foreach ($word in @("JUCE", "VST3", "HarfBuzz", "SheenBidi", "Independent JPEG Group")) {
    if ($tp -notmatch [regex]::Escape($word)) { throw "THIRD_PARTY.md does not mention $word" }
}

# ---- deterministic ZIP -------------------------------------------------------------------------
$stamp = [DateTimeOffset]::new(2026, 1, 1, 0, 0, 0, [TimeSpan]::Zero)
try {
    $ct = Invoke-Native { git log -1 --format=%ct 2>$null }
    if ($LASTEXITCODE -eq 0 -and $ct) { $stamp = [DateTimeOffset]::FromUnixTimeSeconds([int64]"$ct".Trim()) }
} catch { }

Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem
$dist = Join-Path $root "dist"
New-Item -ItemType Directory -Force $dist | Out-Null
$zipPath = Join-Path $dist "$name.zip"
if (Test-Path -LiteralPath $zipPath) { Remove-Item -Force -LiteralPath $zipPath }

# ordinal (culture-independent) entry order
$sorted = New-Object System.Collections.Generic.List[object]
$items | ForEach-Object { $sorted.Add($_) }
$sorted.Sort([System.Comparison[object]] { param($a, $b) [string]::CompareOrdinal($a.Entry, $b.Entry) })

$fs = [System.IO.File]::Open($zipPath, [System.IO.FileMode]::CreateNew)
try {
    $zip = New-Object System.IO.Compression.ZipArchive($fs, [System.IO.Compression.ZipArchiveMode]::Create)
    try {
        foreach ($it in $sorted) {
            $e = $zip.CreateEntry($it.Entry, [System.IO.Compression.CompressionLevel]::Optimal)
            $e.LastWriteTime = $stamp
            $out = $e.Open()
            try {
                # plain assignments: an `if` expression would unroll the byte[] through the pipeline
                if ($null -ne $it.Source) { $bytes = [System.IO.File]::ReadAllBytes($it.Source) }
                else { $bytes = [System.Text.Encoding]::UTF8.GetBytes($it.Text) }
                if ($bytes.Length -gt 0) { $out.Write($bytes, 0, $bytes.Length) }
            } finally { $out.Dispose() }
        }
    } finally { $zip.Dispose() }
} finally { $fs.Dispose() }

$hash = (Get-FileHash -Algorithm SHA256 -LiteralPath $zipPath).Hash.ToLowerInvariant()
Set-Content -NoNewline -Encoding ascii -Path "$zipPath.sha256" -Value "$hash  $name.zip`n"
Write-Host ("Wrote {0} ({1} files, {2:N0} bytes)" -f $zipPath, $sorted.Count, (Get-Item -LiteralPath $zipPath).Length)
Write-Host "SHA-256 $hash"
