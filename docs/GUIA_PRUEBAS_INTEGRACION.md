# Guía de Pruebas de Integración: Ejecución por Sistemas Automatizados y por Humanos

Esta guía documenta cómo validar de extremo a extremo (**End-to-End**) cada módulo funcional del proyecto, tanto desde **pipelines automatizados (CI/CD, CTest, scripts sin intervención humana)** como mediante **evaluación interactiva por humanos (QA / Equipo de Soporte / UAT)**.

---

## 1. Matriz de Cobertura Funcional por Suite de Integración

| Funcionalidad Requerida | Suite Automatizada (`CTest`) | Prueba Interactiva Humana |
|---|---|---|
| Carga de capturas PNG/JPG/BMP y baja resolución | `test_image_and_ocr` (`LowResolutionUpscaling...`) | Botón **Cargar Captura...** o **Drag & Drop** en `ticket_ocr_gui.exe` |
| Detección e inversión de **Modo Oscuro** | `test_image_and_ocr` (`DarkModeDetection...`) | Cargar `S02_WEB_EN_AUTH_DARK.png` y pulsar **Vista: Preprocesada OCR** |
| Mitigación de **Efecto Moiré** (foto de celular a monitor) | `test_image_and_ocr` (`MobileCameraMoire...`) | Cargar `S11_WEB_ES_PAY_DARK_MOIRE.jpg` y alternar vista Original/Preprocesada |
| Redacción automática DLP (Emails, JWT, API Keys, Tarjetas, URL Secrets) con **0 fugas** | `test_dlp_zero_leak` + auditoría completa en `test_e2e_corpus_metrics` | Inspeccionar etiqueta verde **DLP Seguro: 0 fugas** y placeholders `[REDACTED:...]` |
| Extracción de Código, Módulo, Versión, URL y Mensaje + % sin pedir más info | `test_extraction_and_kb` | Verificar campos pre-llenados en panel derecho y caso parcial `S18` |
| Clasificación contra etiquetado de soporte (6 clases) + Artículos KB | `test_extraction_and_kb` + `test_e2e_corpus_metrics` | Verificar ComboBox de Categoría, Cola N2 y tabla **Artículos Sugeridos KB** |
| Integración real de controles Win32 GUI y exportación JSON/Markdown | `test_gui_integration` (headless o `--visual`) | Editar cualquier campo en GUI y pulsar **Exportar JSON / Markdown / Copiar** |
| Métricas globales (CER, Macro-F1, Minutos/Ticket, % sin +info, 0 fugas) | `ticket_ocr_cli --benchmark` + `test_e2e_corpus_metrics` | Botón **Evaluar Corpus (Métricas)** en GUI o lectura de `reports/benchmark_report.md` |

---

## 2. Ejecución por Sistemas Automatizados (CI / Sin Intervención Humana)

### 2.1 Ejecución Completa en un Solo Comando
```powershell
pwsh -NoProfile -ExecutionPolicy Bypass -File .\scripts\run_integration_tests.ps1
```
Este comando realiza automáticamente:
1. Compilación optimizada C++20 (`/O2`) con MSVC y Ninja.
2. Generación determinista de `data/knowledge_base.json` y las 18 capturas del corpus en `assets/corpus/` junto con `manifest.json`.
3. Ejecución de las **5 suites binarias en CTest** (`ctest --test-dir build --output-on-failure`).
4. Evaluación del benchmark completo validando umbrales estrictos y devolviendo código de salida `0` si pasa todas las compuertas o `!= 0` ante cualquier regresión o fuga de datos.

### 2.2 Ejecución Individual de Binarios Automatizados
```powershell
# Suites individuales
.\build\test_image_and_ocr.exe
.\build\test_dlp_zero_leak.exe
.\build\test_extraction_and_kb.exe
.\build\test_e2e_corpus_metrics.exe
.\build\test_gui_integration.exe

# Análisis automatizado de una captura arbitraria con salida JSON para pipelines ITSM
.\build\ticket_ocr_cli.exe --analyze assets\corpus\S01_WIN_ES_AUTH_LIGHT.png --json salida_ticket.json
```

---

## 3. Protocolo de Pruebas de Integración para Humanos (QA / Soporte)

### 3.1 Asistente Guiado Interactivo en Interfaz Gráfica
Ejecute el modo interactivo para humanos:
```powershell
pwsh -NoProfile -ExecutionPolicy Bypass -File .\scripts\run_integration_tests.ps1 -InteractiveHuman
```
O abra directamente la aplicación gráfica:
```powershell
.\build\ticket_ocr_gui.exe
```

#### Casos de Prueba Paso a Paso para el Evaluador Humano:

1. **Prueba H-01 — Fotografía de Celular con Efecto Moiré + Modo Oscuro + Tarjeta Bancaria (`S11_WEB_ES_PAY_DARK_MOIRE.jpg`):**
   - En el desplegable superior **Corpus de Prueba**, seleccione `S11_WEB_ES_PAY_DARK_MOIRE [WebSaaS/ES/Dark/MoiréCelular]`.
   - Observe en el lienzo izquierdo las ondas de interferencia moiré y la inclinación de cámara sobre el monitor oscuro.
   - Pulse el botón **`Vista: Captura Original`** para cambiar a **`Vista: Preprocesada OCR`**: compruebe visualmente cómo el filtro pasa-bajas y la imagen integral aplanan la iluminación y eliminan el moiré convirtiéndolo en tinta oscura nítida sobre fondo claro.
   - Verifique en el formulario derecho:
     - **Código de Error:** `ERR-PAY-4021`
     - **Módulo:** `PaymentGateway.ChargeProcessor`
     - **Versión:** `v4.9.5`
     - **Categoría:** `FACTURACION_PAGOS`
     - **Descripción:** compruebe que ni la tarjeta `4532-0151-1283-0366` ni la llave `sk-live-...` aparecen en claro (sustituidas por `[REDACTED:CARD_PAN]` y `[REDACTED:API_KEY]`).
     - **Artículos KB:** haga clic en `KB-4002` en la tabla inferior y revise los pasos de solución sugeridos.

2. **Prueba H-02 — Captura Windows/Web con JWT y Secretos en URL (`S01_WIN_ES_AUTH_LIGHT.png`):**
   - Seleccione `S01_WIN_ES_AUTH_LIGHT` en el selector del corpus.
   - Verifique que el campo **URL Sanitizada** conserve el endpoint diagnóstico (`https://sso.corporativo-latam.mx/oauth2/authorize?token=[REDACTED:URL_SECRET]`) habiendo neutralizado el token de sesión, además del correo electrónico y el JWT en el cuerpo.

3. **Prueba H-03 — Captura Incompleta Recortada por el Usuario (`S18_LIN_ES_CFG_PARTIAL_DARK.png`):**
   - Seleccione `S18_LIN_ES_CFG_PARTIAL_DARK`.
   - Observe que al no existir URL ni Versión en la captura original, el indicador cambia a **`Primer Contacto: PARCIAL (Revisar campos)`**, validando la métrica de detección de tickets que sí requieren información adicional.

4. **Prueba H-04 — Observación en Vivo de Automatización GUI (`--visual`):**
   - Ejecute:
     ```powershell
     .\build\test_gui_integration.exe --visual
     ```
   - Observará la ventana real abrirse en pantalla, cargar una captura compleja, poblar todos los controles, conmutar automáticamente a la vista preprocesada, validar la exportación cero-fugas y cerrarse limpiamente.

5. **Prueba H-05 — Consola Interactiva QA en Terminal:**
   - Ejecute:
     ```powershell
     .\build\ticket_ocr_cli.exe --interactive-qa
     ```
   - Ingrese cualquier número del `1` al `18` (o la ruta de una captura propia) para inspeccionar el Markdown completo generado en consola.
