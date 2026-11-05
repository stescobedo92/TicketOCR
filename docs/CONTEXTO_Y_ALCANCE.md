# Contexto Expandido de la Problemática y Objetivos de Alcance del Sistema

## 1. Contexto Detallado del Problema Operativo y Técnico

### 1.1 La Realidad Operativa en Mesas de Servicio (Service Desk N1 / N2 / N3)
En organizaciones tecnológicas modernas (plataformas SaaS empresariales, banca digital, comercio electrónico, ERP fiscales, salud e infraestructura híbrida), los usuarios finales rara vez reportan incidentes copiando trazas estructuradas o archivos de registro. En cambio, entre el **65% y el 80% de los tickets entrantes en Nivel 1** consisten en un asunto ambiguo (*"No puedo facturar"*, *"Me saca del sistema"*, *"Error urgente"*) acompañado únicamente por una **captura de pantalla pegada o una fotografía tomada con teléfono celular al monitor**.

Esta dinámica provoca cinco cuellos de botella críticos en la operación de soporte:

1. **Pérdida Severa de Tiempo Operativo (Inflación de MTTR y AHT):**
   - Cada agente de soporte invierte en promedio **4.5 a 6.5 minutos por ticket** leyendo visualmente la imagen y transcribiendo a mano el mensaje de error, el código alfanumérico o hexadecimal (`0x80070005`, `ORA-12170`, `ERR-AUTH-4019`, `SQLSTATE[40001]`), la URL activa con rutas largas, el módulo o librería involucrada (`BillingEngine.CfdiSigner`, `AuthGateway.TokenValidator`) y la versión exacta del cliente (`v4.12.3`).
   - A escala de una mesa de ayuda con 3,000 tickets mensuales con capturas adjuntas, esto representa **más de 260 horas-hombre al mes** dedicadas exclusivamente a mecanografía repetitiva sin valor analítico.

2. **Errores Humanos de Transcripción y Falla en Búsqueda de Conocimiento:**
   - La transcripción manual bajo presión introduce confusiones frecuentes de caracteres (`0` vs `O`, `1` vs `l` vs `I`, `5` vs `S`, guiones omitidos o versiones truncadas).
   - Un solo carácter mal transcrito impide que el buscador del ITSM correlacione el ticket con incidentes masivos activos o con el artículo exacto de la **Base de Conocimiento (KB)**, provocando escalamientos innecesarios a ingeniería de Nivel 2/Nivel 3.

3. **Ciclos de Re-pregunta al Usuario (Degradación del First Contact Resolution - FCR):**
   - Con frecuencia el agente transcribe el texto principal pero pasa por alto metadatos clave visibles en la barra de direcciones, cabecera de consola o pie de ventana (versión, módulo o endpoint), o bien el usuario envía un recorte parcial sin saber qué datos son indispensables.
   - Esto dispara intercambios de correos pidiendo más información (*"¿Qué versión tiene instalada?"*, *"¿En qué URL ocurrió?"*), añadiendo horas o días de latencia pasiva al ticket.

4. **Heterogeneidad Visual Extrema en el Mundo Real:**
   - **Sistemas Operativos e Interfaces Diversas:** Diálogos modales de Windows 11, hojas de alerta de macOS, consolas Linux/SSH, aplicaciones web empresariales y clientes de escritorio heredados.
   - **Modo Oscuro (Dark Mode) y Paletas de Bajo Contraste:** Interfaces con fondos oscuros (`#161822`, `#1E2230`) y tipografías coloreadas (rojo coral, gris azulado), donde los pipelines OCR convencionales diseñados para documentos negros sobre papel blanco sufren caídas drásticas de precisión.
   - **Resoluciones Dispares:** Desde recortes pequeños de baja resolución (`860×520`) con glifos de apenas 1.5 píxeles de grosor hasta pantallas Full HD / QHD.
   - **Fotografías de Celular a Monitores LCD/OLED (Efecto Moiré e Iluminación No Uniforme):** En entornos restringidos (cajeros, plantas industriales, escritorios remotos bloqueados o usuarios no técnicos), el usuario fotografía el monitor con su smartphone. La interferencia espacial entre la matriz de subpíxeles RGB del monitor y la rejilla CMOS de la cámara produce **patrones ondulatorios de alta frecuencia (efecto moiré)**, sumados a viñeteado en las esquinas, reflejos especulares de luz ambiental, ruido de sensor y leve inclinación óptica.
   - **Multilingüismo Operativo:** Coexistencia de interfaces en Español (`ES`), Inglés (`EN`) y mensajes mixtos (interfaz localizada en español con códigos/módulos técnicos en inglés).

5. **Riesgo Crítico de Fuga de Datos Sensibles (PII, Credenciales y Cumplimiento Normativo):**
   - Las capturas de error contienen con alta frecuencia información altamente sensible visible en pantalla:
     - **Correos electrónicos** de empleados, clientes o cuentas operativas.
     - **Tokens de sesión y autenticación** (`JWT eyJ...`, cabeceras `Bearer`, identificadores de sesión).
     - ** Secretos de API e infraestructura** (`sk-live-...`, `ghp_...`, `AKIA...`, `?api_key=...`, `?token=...`, `?secret=...`, credenciales embebidas en URLs `https://usuario:clave@host`).
     - **Datos financieros (PCI-DSS)** como números de tarjeta de pago (PAN validables por algoritmo de Luhn).
   - Si estos datos pasan sin filtrar al texto del ticket ITSM, quedan indexados en texto plano en bases de datos de soporte, notificaciones por correo, webhooks y métricas históricas, generando incumplimientos graves bajo **GDPR, LFPDPPP, PCI-DSS, SOC 2 e ISO/IEC 27001**.

---

## 2. Objetivos del Proyecto y Alcance Expandido

### 2.1 Objetivo General
Diseñar, implementar y validar en **C++20 nativo de alto rendimiento** una plataforma integral con **Interfaz Gráfica de Usuario (GUI)**, **herramienta CLI automatizable** y **arnés de evaluación cuantitativa** capaz de ingerir capturas de pantalla y fotografías de monitores con errores, restaurar la calidad visual, extraer con mínima tasa de error a nivel carácter (CER) las entidades técnicas clave, redactar automáticamente el 100% de los datos sensibles detectados (**meta: 0 fugas**), clasificar el incidente conforme a la taxonomía del equipo de soporte, sugerir artículos accionables de la Base de Conocimiento (KB) y pre-llenar el ticket listo para sistemas ITSM.

---

### 2.2 Objetivos Específicos y Alcance Funcional Detallado (In-Scope)

#### OE-1: Adquisición y Preprocesamiento Digital Adaptativo de Imágenes
- Soporte nativo para formatos estándar de soporte técnico (`PNG`, `JPEG/JPG`, `BMP`, `TIFF`) tanto por selector de archivos como por **arrastrar y soltar (Drag & Drop)**.
- **Detección automática de Modo Oscuro** mediante análisis estadístico de histograma de luminancia perceptual en la región activa e inversión tonal con estiramiento percentil (`0.5% - 98%`) preservando rampas de suavizado (*anti-aliasing*) de los glifos.
- **Detección y Mitigación de Efecto Moiré e Iluminación Desigual:**
  - Estimador cuantitativo de energía oscilatoria de alta frecuencia y desfase cromático subpíxel (`moire_score`).
  - Filtrado pasa-bajas binomial separable `[1, 2, 1] ⊗ [1, 2, 1] / 16` a resolución nativa del sensor para atenuar interferencia moiré sin destruir trazos delgados.
  - Aplanamiento local de iluminación en tiempo constante **$O(1)$ por píxel mediante Imagen Integral (Summed-Area Table)** para eliminar viñeteado de lente móvil y reflejos de pantalla.
- **Super-escalado Bicúbico Catmull-Rom** para capturas de baja resolución y realce controlado de bordes (*Unsharp Masking*).
- Inspección visual transparente en la GUI permitiendo al humano alternar entre **Captura Original** y **Vista Preprocesada OCR**.

#### OE-2: Reconocimiento Óptico (OCR) Multi-Pasada y Post-Corrección Técnica
- Integración nativa asíncrona/multihilo con `Windows.Media.Ocr` (C++/WinRT) aislada en un worker MTA persistente libre de bloqueos de interfaz.
- **Reconstrucción geométrica de líneas por cajas delimitadoras (`BoundingRect`):** análisis de brecha horizontal en píxeles entre palabras para evitar espacios espurios dentro de URLs, rutas y nombres de módulos con puntos (`AuthGateway.TokenValidator`).
- **Post-corrección léxica y estructural:** reparación determinista de confusiones OCR frecuentes en prefijos hexadecimales (`0x...`), códigos con prefijo (`ERR-...`, `ORA-...`), versiones semánticas (`vX.Y.Z`) y normalización diacrítica contextual en español.

#### OE-3: Prevención de Fuga de Datos Sensibles (DLP Zero-Leakage)
- Ejecución obligatoria del motor `DlpRedactor` **antes** de poblar cualquier campo extraído, sugerencia o exportación.
- Cobertura multicapa:
  1. Credenciales Basic-Auth embebidas en URLs (`[REDACTED:URL_CREDENTIALS]`).
  2. Secretos en parámetros de consulta URL (`token`, `api_key`, `secret`, `password`, `jwt`, `session_id` -> `[REDACTED:URL_SECRET]`), conservando el dominio y endpoint para diagnóstico.
  3. Tokens JWT (`eyJ...` -> `[REDACTED:JWT_TOKEN]`), tolerando saltos o espacios accidentales de OCR.
  4. API Keys de nube y pasarelas (`sk-live-...`, `ghp_...`, `AKIA...`, `AIza...`, `xoxb-...` -> `[REDACTED:API_KEY]`).
  5. Secretos etiquetados (`Bearer`, `Authorization`, `Secret:`, `Password:`, `Clave:` -> `[REDACTED:SECRET_TOKEN]`).
  6. Correos electrónicos estándar y con ruido de espaciado alrededor de `@` (`[REDACTED:EMAIL]`).
  7. Números de tarjeta bancaria validados con suma de verificación **Luhn** (`[REDACTED:CARD_PAN]`).
  8. Cadenas autónomas de alta entropía de Shannon ($\ge 3.65$ bits/carácter) con mezcla de clases léxicas (`[REDACTED:HIGH_ENTROPY_SECRET]`).
- Auditoría automatizada de salida (`audit_for_leaks`) sobre el JSON y Markdown generados con **meta innegociable de 0 fugas en el set de prueba**.

#### OE-4: Extracción Estructurada de Campos Clave y Evaluación de Primer Contacto
- Extracción precisa de los 5 campos nucleares:
  1. **Código de Error (`error_code`)**
  2. **Módulo / Servicio (`module_name`)**
  3. **Versión del Software (`software_version`)**
  4. **URL / Endpoint Sanitizado (`sanitized_url`)**
  5. **Mensaje Principal del Error (`error_message`)** limpio de botones (`Aceptar`, `OK`, `Cancelar`) y marcos decorativos.
- Evaluación automática de **Accionabilidad en Primer Contacto (`actionable_without_followup`)**: determina objetivamente si el ticket reúne evidencia suficiente para ser atendido sin solicitar más datos al usuario final, listando explícitamente cualquier campo recomendado ausente.

#### OE-5: Clasificación Multiclase de Incidentes y Sugerencia de Artículos KB
- Clasificación supervisada/híbrida calibrada contra las **6 categorías del equipo de soporte**:
  - `AUTENTICACION_ACCESO` -> Cola `N2-Seguridad-IAM`
  - `RED_CONECTIVIDAD` -> Cola `N2-Infraestructura-Redes`
  - `BASE_DE_DATOS` -> Cola `N2-DBA-PlataformaDatos`
  - `FACTURACION_PAGOS` -> Cola `N2-FinOps-Facturacion`
  - `SISTEMA_MEMORIA_CRASH` -> Cola `N3-Ingenieria-CoreRuntime`
  - `CONFIGURACION_COMPATIBILIDAD` -> Cola `N1-SoporteAplicaciones`
- Asignación automática de prioridad sugerida (`P1 Crítica` a `P4 Baja`) y porcentaje de confianza probabilístico.
- Motor de recomendación de **Base de Conocimiento (`KbRecommender`)** alimentado por catálogo JSON extensible (`data/knowledge_base.json`), sugiriendo el Top-3 de artículos con causa raíz y pasos de resolución inmediata.

#### OE-6: Pre-llenado Interactivo en GUI, Exportación ITSM y Suite Dual de Pruebas
- Interfaz gráfica nativa Win32/GDI+ de alto DPI donde el agente revisa, edita si lo desea, copia al portapapeles o exporta en **JSON estructurado** o **Markdown**.
- Generador reproducible de corpus anonimizado multi-SO/multi-idioma/modo-oscuro/moiré (`18 escenarios balanceados`) y evaluador integral de KPIs (**CER**, **Macro-F1**, **Minutos por Ticket**, **% Sin Pedir Más Información** y **0 Fugas Sensibles**) ejecutable tanto por sistemas CI/CD como por evaluadores humanos.

---

### 2.3 Límites del Alcance Actual (Out-of-Scope) y Evolución Prevista
Para mantener un sistema determinista, auditable, sin dependencias frágiles en la nube y 100% seguro frente a fugas de datos (*Zero-Trust Local Processing*), se delimitan explícitamente fuera del alcance de esta versión v1.0:
- **Procesamiento de video continuo (`.mp4` / `.webm`)**: el sistema opera sobre capturas estáticas individuales o lotes de imágenes.
- **Envío directo autenticado vía REST a instancias SaaS externas (Jira Cloud / ServiceNow Live)**: el sistema genera los payloads JSON y Markdown estandarizados listos para integración o ingesta por webhook, evitando transmitir capturas crudas fuera del perímetro local.
- **Manuscritos o notas físicas fotografiadas**: el pipeline óptico está especializado en tipografía digital renderizada en pantallas y monitores.
