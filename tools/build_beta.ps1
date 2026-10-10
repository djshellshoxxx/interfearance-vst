<#
.SYNOPSIS
    One-paste Windows build of Interference 0.0.1 beta: checks tools, fetches the source and JUCE, builds
    VST3 + CLAP + stand-alone with Visual Studio 2022, runs both test suites, and packs the release ZIP.

.DESCRIPTION
    Needs (the script tells you what is missing): Git for Windows, CMake 3.22+, and Visual Studio 2022 or its
    Build Tools with the "Desktop development with C++" workload. Takes roughly 15-30 minutes the first time.
    Output: <Dir>\interfearance-vst\dist\Interference-0.0.1-beta-win64.zip and its .sha256.
    The repository is private: Git will open a sign-in window the first time.

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File build_beta.ps1
    powershell -ExecutionPolicy Bypass -File build_beta.ps1 -NoClap     # if the CLAP step fails
#>
param(
    [string] $Branch = "claude/nice-heisenberg-smnq6j",
    [string] $Dir = (Join-Path $env:USERPROFILE "interference-build"),
    [switch] $NoClap
)

$ErrorActionPreference = "Stop"

# native tools print progress on stderr; Windows PowerShell 5.1 would treat that as an error under "Stop"
function Run([string] $what, [scriptblock] $block) {
    $eap = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    try { & $block } finally { $ErrorActionPreference = $eap }
    if ($LASTEXITCODE -ne 0) { throw "$what failed (exit code $LASTEXITCODE)" }
}

function Need([string] $cmd, [string] $hint) {
    if (-not (Get-Command $cmd -ErrorAction SilentlyContinue)) { throw "$cmd was not found. $hint" }
}

Write-Host "== 1/5 Checking tools" -ForegroundColor Cyan
Need git   "Install Git for Windows from https://git-scm.com/download/win and reopen PowerShell."
Need cmake "Install CMake 3.22 or newer from https://cmake.org/download/ (tick 'Add CMake to the system PATH') and reopen PowerShell."
$vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path -LiteralPath $vswhere)) {
    throw "Visual Studio 2022 was not found. Install 'Build Tools for Visual Studio 2022' from https://visualstudio.microsoft.com/downloads/ with the 'Desktop development with C++' workload."
}
$vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vs) {
    throw "The 'Desktop development with C++' workload (MSVC x64 tools) is not installed. Open the Visual Studio Installer and add it."
}
Write-Host "   Visual Studio: $vs"

Write-Host "== 2/5 Fetching the source ($Branch)" -ForegroundColor Cyan
New-Item -ItemType Directory -Force -Path $Dir | Out-Null
Set-Location -LiteralPath $Dir
if (-not (Test-Path -LiteralPath "interfearance-vst\.git")) {
    Run "git clone" { git clone --branch $Branch https://github.com/djshellshoxxx/interfearance-vst.git }
}
Set-Location -LiteralPath (Join-Path $Dir "interfearance-vst")
Run "git fetch"    { git fetch origin $Branch }
Run "git checkout" { git checkout $Branch }
Run "git pull"     { git pull --ff-only origin $Branch }

Write-Host "== 3/5 Fetching JUCE 8.0.8" -ForegroundColor Cyan
if (-not (Test-Path -LiteralPath "JUCE\CMakeLists.txt")) {
    Run "git clone JUCE" { git clone --depth 1 --branch 8.0.8 https://github.com/juce-framework/JUCE.git JUCE }
}

Write-Host "== 4/5 Building, testing and packing (this is the long step)" -ForegroundColor Cyan
$psArgs = @("-NoProfile", "-ExecutionPolicy", "Bypass", "-File", "tools\make_release.ps1", "-Build")
if ($NoClap) { $psArgs += "-NoClap" }
Run "make_release.ps1" { powershell @psArgs }

Write-Host "== 5/5 Done" -ForegroundColor Cyan
$zip = Get-ChildItem -LiteralPath "dist" -Filter "Interference-*-win64.zip" | Sort-Object LastWriteTime -Descending | Select-Object -First 1
if (-not $zip) { throw "The ZIP was not created." }
$hash = (Get-FileHash -Algorithm SHA256 -LiteralPath $zip.FullName).Hash
Write-Host ""
Write-Host "Release ZIP : $($zip.FullName)" -ForegroundColor Green
Write-Host "SHA-256     : $hash" -ForegroundColor Green
Write-Host "Install     : see Docs\Install.md inside the ZIP (copy Interference.vst3 to C:\Program Files\Common Files\VST3\)."
