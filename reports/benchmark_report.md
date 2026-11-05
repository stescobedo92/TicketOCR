# Reporte de Evaluación y Métricas del Corpus (End-to-End)

- **Total de Capturas Evaluadas:** 18
- **Estado Global de Compuertas de Calidad:** APROBADO (PASS)

## 1. Resumen Ejecutivo de KPIs

| Métrica Clave | Resultado Obtenido | Meta / Referencia |
|---|---|---|
| **CER Promedio en Campos Clave** | `7.5170%` (0.0752) | `<= 15.0%` |
| **CER Subconjunto Efecto Moiré (Foto Celular)** | `5.8452%` | Mitigación activa |
| **CER Subconjunto Modo Oscuro** | `4.0904%` | Inversión adaptativa |
| **Macro-F1 de Categoría (vs Soporte)** | `1.0000` (`100.0%` acc) | `>= 0.85` |
| **Minutos por Ticket (Asistido vs Manual)** | `0.36 min` (ahorro `93.16%`) | Baseline manual: `5.20 min` |
| **Latencia de Cómputo C++ por Captura** | `388.85 ms` | `< 500 ms` |
| **% Tickets sin Pedir Más Información** | `94.44%` | `>= 80.0%` |
| **Fuga de Datos Sensibles (PII / Tokens)** | **`0` fugas** (de 17 secretos plantados) | **Meta estricta: `0`** |

## 2. Desglose de CER (Character Error Rate) por Campo Clave

| Campo Extraído | CER Promedio |
|---|---|
| `error_code` (Código) | 0.0719 (`7.19%`) |
| `module` (Módulo) | 0.0546 (`5.46%`) |
| `version` (Versión) | 0.0266 (`2.66%`) |
| `url` (URL Sanitizada) | 0.1876 (`18.76%`) |
| `error_message` (Mensaje) | 0.0351 (`3.51%`) |

## 3. Desempeño de Clasificación por Categoría (Etiquetado Soporte)

| Categoría | TP | FP | FN | Precisión | Recall | F1-Score |
|---|---:|---:|---:|---:|---:|---:|
| `AUTENTICACION_ACCESO` | 3 | 0 | 0 | 1.000 | 1.000 | 1.000 |
| `RED_CONECTIVIDAD` | 3 | 0 | 0 | 1.000 | 1.000 | 1.000 |
| `BASE_DE_DATOS` | 3 | 0 | 0 | 1.000 | 1.000 | 1.000 |
| `FACTURACION_PAGOS` | 3 | 0 | 0 | 1.000 | 1.000 | 1.000 |
| `SISTEMA_MEMORIA_CRASH` | 3 | 0 | 0 | 1.000 | 1.000 | 1.000 |
| `CONFIGURACION_COMPATIBILIDAD` | 3 | 0 | 0 | 1.000 | 1.000 | 1.000 |

## 4. Detalle por Muestra del Corpus

| ID | SO / Estilo | Idioma | Dark | Moiré | CER Medio | Categoría Predicha | Sin +Info | Redacciones DLP | Fugas | ms |
|---|---|---|---|---|---:|---|---|---:|---:|---:|
| `S01_WIN_ES_AUTH_LIGHT` | Windows11 | ES | No | No | 0.264 | `AUTENTICACION_ACCESO` ✓ | Sí | 2 | 0 | 157.1 |
| `S02_WEB_EN_AUTH_DARK` | WebSaaS | EN | Sí | No | 0.000 | `AUTENTICACION_ACCESO` ✓ | Sí | 2 | 0 | 470.5 |
| `S03_MAC_ES_AUTH_MOIRE` | macOS | ES | No | Sí | 0.012 | `AUTENTICACION_ACCESO` ✓ | Sí | 1 | 0 | 355.4 |
| `S04_WEB_ES_NET_LIGHT` | WebSaaS | ES | No | No | 0.235 | `RED_CONECTIVIDAD` ✓ | Sí | 0 | 0 | 43.4 |
| `S05_LIN_EN_NET_DARK_MOIRE` | Linux | EN | Sí | Sí | 0.019 | `RED_CONECTIVIDAD` ✓ | Sí | 1 | 0 | 367.3 |
| `S06_WIN_EN_NET_LOWRES` | Windows11 | EN | No | No | 0.025 | `RED_CONECTIVIDAD` ✓ | Sí | 0 | 0 | 125.1 |
| `S07_LIN_ES_DB_DARK` | Linux | ES | Sí | No | 0.201 | `BASE_DE_DATOS` ✓ | Sí | 1 | 0 | 465.5 |
| `S08_WEB_EN_DB_MOIRE` | WebSaaS | EN | No | Sí | 0.149 | `BASE_DE_DATOS` ✓ | Sí | 0 | 0 | 339.5 |
| `S09_WIN_ES_DB_DARK` | Windows11 | ES | Sí | No | 0.075 | `BASE_DE_DATOS` ✓ | Sí | 0 | 0 | 303.4 |
| `S10_WIN_ES_BILL_LIGHT` | Windows11 | ES | No | No | 0.020 | `FACTURACION_PAGOS` ✓ | Sí | 1 | 0 | 218.8 |
| `S11_WEB_ES_PAY_DARK_MOIRE` | WebSaaS | ES | Sí | Sí | 0.000 | `FACTURACION_PAGOS` ✓ | Sí | 2 | 0 | 645.9 |
| `S12_MAC_EN_BILL_LIGHT` | macOS | EN | No | No | 0.004 | `FACTURACION_PAGOS` ✓ | Sí | 1 | 0 | 289.8 |
| `S13_WIN_EN_CRASH_DARK` | Windows11 | EN | Sí | No | 0.000 | `SISTEMA_MEMORIA_CRASH` ✓ | Sí | 0 | 0 | 561.3 |
| `S14_LIN_ES_MEM_MOIRE` | Linux | ES | No | Sí | 0.112 | `SISTEMA_MEMORIA_CRASH` ✓ | Sí | 1 | 0 | 616.8 |
| `S15_MAC_EN_SYS_DARK` | macOS | EN | Sí | No | 0.023 | `SISTEMA_MEMORIA_CRASH` ✓ | Sí | 0 | 0 | 914.8 |
| `S16_WEB_ES_CFG_LIGHT` | WebSaaS | ES | No | No | 0.165 | `CONFIGURACION_COMPATIBILIDAD` ✓ | Sí | 0 | 0 | 287.9 |
| `S17_WIN_EN_CFG_LOWRES` | Windows11 | EN | No | No | 0.000 | `CONFIGURACION_COMPATIBILIDAD` ✓ | Sí | 0 | 0 | 255.9 |
| `S18_LIN_ES_CFG_PARTIAL_DARK` | Linux | ES | Sí | No | 0.009 | `CONFIGURACION_COMPATIBILIDAD` ✓ | No | 1 | 0 | 580.8 |
