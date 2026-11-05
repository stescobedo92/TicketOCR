param(
    [ValidateSet("Release", "Debug")]
    [string]$Configuration = "Release",
    [switch]$Clean
)

$ErrorActionPreference = "Stop"
$RepoRoot = Split-Path -Parent $PSScriptRoot
Set-Location $RepoRoot

if ($Clean -and (Test-Path "build")) {
    Write-Host "[Clean] Eliminando carpeta build..." -ForegroundColor Yellow
    Remove-Item -Recurse -Force "build"
}

$VsWhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$VcVars = $null
if (Test-Path $VsWhere) {
    $VsPath = & $VsWhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if ($VsPath) {
        $Candidate = Join-Path $VsPath "VC\Auxiliary\Build\vcvars64.bat"
        if (Test-Path $Candidate) { $VcVars = $Candidate }
    }
}

if (-not $VcVars) {
    throw "No se encontró vcvars64.bat de Microsoft Visual Studio C++ x64."
}

Write-Host "[Build] Configurando y compilando en modo $Configuration con MSVC + Ninja..." -ForegroundColor Cyan
cmd.exe /c "`"$VcVars`" >nul && cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=$Configuration && cmake --build build --parallel"
if ($LASTEXITCODE -ne 0) {
    throw "La compilación falló con código de salida $LASTEXITCODE."
}

Write-Host "[OK] Binarios generados en $RepoRoot\build:" -ForegroundColor Green
Write-Host "  - GUI : build\ticket_ocr_gui.exe"
Write-Host "  - CLI : build\ticket_ocr_cli.exe"
