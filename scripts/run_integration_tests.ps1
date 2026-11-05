param(
    [switch]$InteractiveHuman,
    [switch]$VisualGuiTest,
    [switch]$SkipBuild
)

$ErrorActionPreference = "Stop"
$RepoRoot = Split-Path -Parent $PSScriptRoot
Set-Location $RepoRoot

if (-not $SkipBuild) {
    & "$PSScriptRoot\build.ps1" -Configuration Release
}

Write-Host "`n[1/4] Exportando Base de Conocimiento y generando Corpus Anonimizado..." -ForegroundColor Cyan
& "$RepoRoot\build\ticket_ocr_cli.exe" --export-kb "data\knowledge_base.json"
& "$RepoRoot\build\ticket_ocr_cli.exe" --generate-corpus "assets\corpus"

Write-Host "`n[2/4] Ejecutando Suites Automatizadas de Integración (CTest)..." -ForegroundColor Cyan
$VsWhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$VsPath = & $VsWhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$VcVars = Join-Path $VsPath "VC\Auxiliary\Build\vcvars64.bat"
cmd.exe /c "`"$VcVars`" >nul && ctest --test-dir build --output-on-failure"
if ($LASTEXITCODE -ne 0) {
    throw "Falló al menos una suite de integración en CTest."
}

if ($VisualGuiTest) {
    Write-Host "`n[2b/4] Ejecutando Prueba Visual Automatizada de Interfaz Gráfica (--visual)..." -ForegroundColor Cyan
    & "$RepoRoot\build\test_gui_integration.exe" --visual
}

Write-Host "`n[3/4] Ejecutando Evaluación Cuantitativa End-to-End del Corpus (Compuertas KPI)..." -ForegroundColor Cyan
& "$RepoRoot\build\ticket_ocr_cli.exe" --benchmark "assets\corpus" --report "reports\benchmark_report.md" --json "reports\benchmark_metrics.json"
if ($LASTEXITCODE -ne 0) {
    throw "No se superaron las compuertas de métricas de calidad."
}

if ($InteractiveHuman) {
    Write-Host "`n[4/4] Iniciando Protocolo Guiado de Pruebas de Integración para Humanos (UAT/QA)..." -ForegroundColor Yellow
    Write-Host "  Se abrirá la Interfaz Gráfica con una captura Modo Oscuro + Efecto Moiré + Datos Sensibles."
    Write-Host "  Checklist sugerido para el evaluador humano:"
    Write-Host "    1. Verificar pre-llenado automático de Código, Módulo, Versión, URL y Categoría."
    Write-Host "    2. Pulsar 'Vista: Captura Original' -> 'Vista: Preprocesada OCR' para inspeccionar el filtro Anti-Moiré."
    Write-Host "    3. Comprobar que el PAN de tarjeta y la API Key aparezcan redactados ([REDACTED:...])."
    Write-Host "    4. Probar el selector superior 'Corpus de Prueba' con otros sistemas operativos e idiomas."
    Write-Host "    5. Pulsar 'Evaluar Corpus (Métricas)' o exportar el ticket a JSON/Markdown."
    & "$RepoRoot\build\ticket_ocr_gui.exe" "$RepoRoot\assets\corpus\S11_WEB_ES_PAY_DARK_MOIRE.jpg"
} else {
    Write-Host "`n[4/4] Todas las pruebas de integración automatizadas finalizaron con ÉXITO (0 fugas DLP)." -ForegroundColor Green
    Write-Host "  Para ejecutar la verificación guiada por humanos ejecute:"
    Write-Host "    pwsh .\scripts\run_integration_tests.ps1 -InteractiveHuman"
}
