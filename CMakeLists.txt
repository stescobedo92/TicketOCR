cmake_minimum_required(VERSION 3.24)
project(TicketOcrGenerator
    VERSION 1.0.0
    DESCRIPTION "Intelligent error screenshot reader with native WinRT OCR, Anti-Moire DSP, Zero-Leakage DLP, i18n, and ITSM ticket pre-filling"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(WIN32)
    enable_language(RC)
endif()

if(MSVC)
    add_compile_options(/utf-8 /W4 /EHsc /MP /permissive-)
    add_compile_definitions(
        UNICODE
        _UNICODE
        NOMINMAX
        WIN32_LEAN_AND_MEAN
        _CRT_SECURE_NO_WARNINGS
    )
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /O2 /Ob2 /Oi /Ot")
endif()

# Core static library shared by GUI, CLI and Integration Tests
add_library(ticket_ocr_core STATIC
    src/core/json_utils.cpp
    src/core/i18n.cpp
    src/core/image_io.cpp
    src/core/image_preprocessor.cpp
    src/core/ocr_engine.cpp
    src/core/dlp_redactor.cpp
    src/core/entity_extractor.cpp
    src/core/incident_classifier.cpp
    src/core/kb_recommender.cpp
    src/core/ticket_pipeline.cpp
    src/core/metrics_evaluator.cpp
    src/core/corpus_generator.cpp
    src/app/gui_app.cpp
)

target_include_directories(ticket_ocr_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_definitions(ticket_ocr_core PUBLIC
    TICKET_OCR_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
    TICKET_OCR_VERSION="${PROJECT_VERSION}"
)

target_link_libraries(ticket_ocr_core PUBLIC
    windowsapp
    gdiplus
    comctl32
    uxtheme
    dwmapi
    comdlg32
    shell32
    ole32
    oleaut32
    uuid
    shlwapi
    gdi32
    user32
)

# Graphical User Interface executable (Win32 + GDI+ Modern High-DPI App + Custom Icon Resource)
add_executable(ticket_ocr_gui WIN32
    src/app/main_gui.cpp
    resources/app.rc
)
target_link_libraries(ticket_ocr_gui PRIVATE ticket_ocr_core)

# Command-Line & Automated Benchmark / Corpus Tool
add_executable(ticket_ocr_cli
    src/cli/main_cli.cpp
    resources/app.rc
)
target_link_libraries(ticket_ocr_cli PRIVATE ticket_ocr_core)

# Automated & Human-Inspectable Integration Tests
enable_testing()

function(add_ticket_ocr_test TEST_NAME TEST_SOURCE)
    add_executable(${TEST_NAME} ${TEST_SOURCE} resources/app.rc)
    target_include_directories(${TEST_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(${TEST_NAME} PRIVATE ticket_ocr_core)
    add_test(NAME ${TEST_NAME}
             COMMAND ${TEST_NAME}
             WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
endfunction()

add_ticket_ocr_test(test_image_and_ocr      tests/test_image_and_ocr.cpp)
add_ticket_ocr_test(test_dlp_zero_leak      tests/test_dlp_zero_leak.cpp)
add_ticket_ocr_test(test_extraction_and_kb  tests/test_extraction_and_kb.cpp)
add_ticket_ocr_test(test_e2e_corpus_metrics tests/test_e2e_corpus_metrics.cpp)
add_ticket_ocr_test(test_gui_integration    tests/test_gui_integration.cpp)
