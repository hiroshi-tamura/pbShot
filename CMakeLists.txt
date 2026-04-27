cmake_minimum_required(VERSION 3.20)
project(pbShot VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

if(NOT DEFINED CMAKE_PREFIX_PATH)
    if(WIN32)
        set(CMAKE_PREFIX_PATH "C:/Qt/6.9.1/mingw_64")
    elseif(APPLE)
        # 公式インストーラ既定: ~/Qt/<ver>/macos/
        file(GLOB _qt_candidates "$ENV{HOME}/Qt/*/macos")
        if(_qt_candidates)
            list(SORT _qt_candidates)
            list(REVERSE _qt_candidates)
            list(GET _qt_candidates 0 _qt_default)
            set(CMAKE_PREFIX_PATH "${_qt_default}")
        endif()
    endif()
endif()

if(APPLE AND NOT CMAKE_OSX_DEPLOYMENT_TARGET)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "13.0" CACHE STRING "macOS deployment target")
endif()

find_package(Qt6 REQUIRED COMPONENTS Widgets Gui Core Network Concurrent)
qt_standard_project_setup()

set(SOURCES
    src/main.cpp
    src/TrayApp.cpp
    src/HotkeyManager.cpp
    src/ScreenCapture.cpp
    src/OverlayWindow.cpp
    src/EditToolbar.cpp
    src/ActionToolbar.cpp
    src/Annotation.cpp
    src/Settings.cpp
    src/SettingsDialog.cpp
    src/Ocr.cpp
    src/OcrModelManager.cpp
    src/TesseractOcr.cpp
    src/Toast.cpp
    src/Logger.cpp
    src/IOcrEngine.cpp
)

set(HEADERS
    src/TrayApp.h
    src/HotkeyManager.h
    src/ScreenCapture.h
    src/OverlayWindow.h
    src/EditToolbar.h
    src/ActionToolbar.h
    src/Annotation.h
    src/Settings.h
    src/SettingsDialog.h
    src/Ocr.h
    src/OcrModelManager.h
    src/TesseractOcr.h
    src/Toast.h
    src/Logger.h
    src/IOcrEngine.h
)

qt_add_executable(pbShot WIN32 MACOSX_BUNDLE
    ${SOURCES}
    ${HEADERS}
    resources/resources.qrc
)

if(WIN32)
    target_sources(pbShot PRIVATE resources/app.rc)
    target_link_libraries(pbShot PRIVATE user32)
endif()

if(APPLE)
    set(MACOSX_BUNDLE_BUNDLE_NAME            "pbShot")
    set(MACOSX_BUNDLE_GUI_IDENTIFIER         "com.hiroshitamura.pbShot")
    set(MACOSX_BUNDLE_BUNDLE_VERSION         "${PROJECT_VERSION}")
    set(MACOSX_BUNDLE_SHORT_VERSION_STRING   "${PROJECT_VERSION}")
    set(MACOSX_BUNDLE_COPYRIGHT              "Copyright (c) 2026 Hiroshi Tamura")
    set(MACOSX_BUNDLE_ICON_FILE              "pbShot.icns")

    set_target_properties(pbShot PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/resources/Info.plist.in"
        MACOSX_BUNDLE_BUNDLE_NAME "pbShot"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.hiroshitamura.pbShot"
        MACOSX_BUNDLE_ICON_FILE "pbShot.icns"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}")

    # Carbon (HotkeyManager) と Cocoa を link
    find_library(CARBON_LIB Carbon REQUIRED)
    find_library(COCOA_LIB  Cocoa  REQUIRED)
    target_link_libraries(pbShot PRIVATE ${CARBON_LIB} ${COCOA_LIB})

    # .icns をバンドルに同梱（resources/pbShot.icns があれば）
    set(_icns_src "${CMAKE_CURRENT_SOURCE_DIR}/resources/pbShot.icns")
    if(EXISTS "${_icns_src}")
        target_sources(pbShot PRIVATE "${_icns_src}")
        set_source_files_properties("${_icns_src}" PROPERTIES
            MACOSX_PACKAGE_LOCATION "Resources")
    endif()
endif()

target_include_directories(pbShot PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(pbShot PRIVATE
    Qt6::Widgets
    Qt6::Gui
    Qt6::Core
    Qt6::Network
    Qt6::Concurrent
)

set_target_properties(pbShot PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

# ---- ONNX Runtime (OCR 推論用) ----
# 配置先は third_party/onnxruntime/{include,lib}。なければ GitHub から自動 DL。
# MinGW gcc は .dll を直接 -l に渡せばリンクできる（gcc が export テーブルから
# 自動で import lib を生成）。
set(ORT_VERSION "1.20.1")
set(ORT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/onnxruntime")
set(ORT_INCLUDE_DIR "${ORT_DIR}/include")
set(ORT_LIB_DIR "${ORT_DIR}/lib")

if(WIN32 AND NOT EXISTS "${ORT_INCLUDE_DIR}/onnxruntime_cxx_api.h")
    set(_ort_zip "${CMAKE_BINARY_DIR}/onnxruntime-win-x64-${ORT_VERSION}.zip")
    set(_ort_url "https://github.com/microsoft/onnxruntime/releases/download/v${ORT_VERSION}/onnxruntime-win-x64-${ORT_VERSION}.zip")
    if(NOT EXISTS "${_ort_zip}")
        message(STATUS "Downloading ONNX Runtime ${ORT_VERSION}...")
        file(DOWNLOAD "${_ort_url}" "${_ort_zip}" SHOW_PROGRESS STATUS _ort_st)
        list(GET _ort_st 0 _ort_code)
        if(NOT _ort_code EQUAL 0)
            message(WARNING "ONNX Runtime download failed: ${_ort_st}")
            file(REMOVE "${_ort_zip}")
        endif()
    endif()
    if(EXISTS "${_ort_zip}")
        set(_ort_extract "${CMAKE_BINARY_DIR}/_ort_extract")
        file(MAKE_DIRECTORY "${_ort_extract}")
        file(ARCHIVE_EXTRACT INPUT "${_ort_zip}" DESTINATION "${_ort_extract}")
        file(GLOB _ort_root "${_ort_extract}/onnxruntime-win-x64-*")
        list(LENGTH _ort_root _ort_root_n)
        if(_ort_root_n GREATER 0)
            list(GET _ort_root 0 _ort_root_dir)
            file(MAKE_DIRECTORY "${ORT_DIR}")
            file(COPY "${_ort_root_dir}/include" DESTINATION "${ORT_DIR}")
            file(COPY "${_ort_root_dir}/lib"     DESTINATION "${ORT_DIR}")
            message(STATUS "ONNX Runtime extracted to ${ORT_DIR}")
        endif()
    endif()
endif()

if(APPLE AND NOT EXISTS "${ORT_INCLUDE_DIR}/onnxruntime_cxx_api.h")
    if(CMAKE_OSX_ARCHITECTURES MATCHES "x86_64" AND NOT CMAKE_OSX_ARCHITECTURES MATCHES "arm64")
        set(_ort_arch "x86_64")
    else()
        set(_ort_arch "arm64")
    endif()
    set(_ort_tgz "${CMAKE_BINARY_DIR}/onnxruntime-osx-${_ort_arch}-${ORT_VERSION}.tgz")
    set(_ort_url "https://github.com/microsoft/onnxruntime/releases/download/v${ORT_VERSION}/onnxruntime-osx-${_ort_arch}-${ORT_VERSION}.tgz")
    if(NOT EXISTS "${_ort_tgz}")
        message(STATUS "Downloading ONNX Runtime ${ORT_VERSION} (osx-${_ort_arch})...")
        file(DOWNLOAD "${_ort_url}" "${_ort_tgz}" SHOW_PROGRESS STATUS _ort_st)
        list(GET _ort_st 0 _ort_code)
        if(NOT _ort_code EQUAL 0)
            message(WARNING "ONNX Runtime download failed: ${_ort_st}")
            file(REMOVE "${_ort_tgz}")
        endif()
    endif()
    if(EXISTS "${_ort_tgz}")
        set(_ort_extract "${CMAKE_BINARY_DIR}/_ort_extract")
        file(MAKE_DIRECTORY "${_ort_extract}")
        file(ARCHIVE_EXTRACT INPUT "${_ort_tgz}" DESTINATION "${_ort_extract}")
        file(GLOB _ort_root "${_ort_extract}/onnxruntime-osx-*")
        list(LENGTH _ort_root _ort_root_n)
        if(_ort_root_n GREATER 0)
            list(GET _ort_root 0 _ort_root_dir)
            file(MAKE_DIRECTORY "${ORT_DIR}")
            file(COPY "${_ort_root_dir}/include" DESTINATION "${ORT_DIR}")
            file(COPY "${_ort_root_dir}/lib"     DESTINATION "${ORT_DIR}")
            message(STATUS "ONNX Runtime extracted to ${ORT_DIR}")
        endif()
    endif()
endif()

# OS ごとの ORT 共有ライブラリ名
if(WIN32)
    set(_ort_shared "${ORT_LIB_DIR}/onnxruntime.dll")
elseif(APPLE)
    file(GLOB _ort_dylib "${ORT_LIB_DIR}/libonnxruntime*.dylib")
    list(LENGTH _ort_dylib _ort_dylib_n)
    if(_ort_dylib_n GREATER 0)
        list(GET _ort_dylib 0 _ort_shared)
    else()
        set(_ort_shared "")
    endif()
endif()

if(EXISTS "${ORT_INCLUDE_DIR}/onnxruntime_cxx_api.h" AND _ort_shared AND EXISTS "${_ort_shared}")
    message(STATUS "ONNX Runtime: enabled (${_ort_shared})")
    target_compile_definitions(pbShot PRIVATE ORT_AVAILABLE=1)
    target_include_directories(pbShot PRIVATE "${ORT_INCLUDE_DIR}")
    target_link_libraries(pbShot PRIVATE "${_ort_shared}")
    if(MINGW OR (WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU"))
        # MinGW gcc は ORT C ヘッダの SAL アノテーションを正しく解釈できない。
        set_source_files_properties(src/Ocr.cpp PROPERTIES
            COMPILE_FLAGS "-fpermissive -w")
    endif()
    if(WIN32)
        add_custom_command(TARGET pbShot POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${_ort_shared}"
                    "$<TARGET_FILE_DIR:pbShot>/onnxruntime.dll"
            COMMENT "Copy onnxruntime.dll")
        if(EXISTS "${ORT_LIB_DIR}/onnxruntime_providers_shared.dll")
            add_custom_command(TARGET pbShot POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "${ORT_LIB_DIR}/onnxruntime_providers_shared.dll"
                        "$<TARGET_FILE_DIR:pbShot>/onnxruntime_providers_shared.dll"
                COMMENT "Copy onnxruntime_providers_shared.dll")
        endif()
    elseif(APPLE)
        # .app/Contents/Frameworks/ に dylib を配置（macdeployqt と同居）
        add_custom_command(TARGET pbShot POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory
                    "$<TARGET_BUNDLE_CONTENT_DIR:pbShot>/Frameworks"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${_ort_shared}"
                    "$<TARGET_BUNDLE_CONTENT_DIR:pbShot>/Frameworks/"
            COMMAND install_name_tool -change
                    "@rpath/$<PATH:GET_FILENAME,${_ort_shared}>"
                    "@rpath/$<PATH:GET_FILENAME,${_ort_shared}>"
                    "$<TARGET_FILE:pbShot>" || true
            COMMENT "Copy onnxruntime dylib into .app/Contents/Frameworks")
        # 実行時 dylib 検索パスを Frameworks/ に向ける
        set_target_properties(pbShot PROPERTIES
            INSTALL_RPATH "@executable_path/../Frameworks"
            BUILD_WITH_INSTALL_RPATH TRUE)
    endif()
else()
    message(STATUS "ONNX Runtime: not found (OCR will return placeholder text).")
endif()

# ---- Tesseract OCR (conda-forge ポータブル同梱) ----
# CMake ビルド時に conda-forge の .conda パッケージを取得し、
# tesseract.exe + 必要 DLL + tessdata を exe 隣にコピーする。
# .conda は外殻 zip + 内部 pkg-*.tar.zst の 2 段構造。
set(TESS_CONDA_VERSION "5.5.2")
set(TESS_CONDA_BUILD   "hfa586c3_0")
set(TESS_DIR           "${CMAKE_CURRENT_SOURCE_DIR}/third_party/tesseract")
set(TESS_BIN_DIR       "${TESS_DIR}/bin")
set(TESS_DATA_DIR      "${TESS_DIR}/tessdata")

if(WIN32 AND NOT EXISTS "${TESS_BIN_DIR}/tesseract.exe")
    set(_tess_pkg "${CMAKE_BINARY_DIR}/tesseract-${TESS_CONDA_VERSION}.conda")
    set(_tess_url "https://conda.anaconda.org/conda-forge/win-64/tesseract-${TESS_CONDA_VERSION}-${TESS_CONDA_BUILD}.conda")
    if(NOT EXISTS "${_tess_pkg}")
        message(STATUS "Downloading Tesseract ${TESS_CONDA_VERSION} (conda-forge)...")
        file(DOWNLOAD "${_tess_url}" "${_tess_pkg}" SHOW_PROGRESS STATUS _tess_st)
        list(GET _tess_st 0 _tess_code)
        if(NOT _tess_code EQUAL 0)
            message(WARNING "Tesseract download failed: ${_tess_st}")
            file(REMOVE "${_tess_pkg}")
        endif()
    endif()
    if(EXISTS "${_tess_pkg}")
        set(_tess_outer "${CMAKE_BINARY_DIR}/_tess_outer")
        file(REMOVE_RECURSE "${_tess_outer}")
        file(MAKE_DIRECTORY "${_tess_outer}")
        file(ARCHIVE_EXTRACT INPUT "${_tess_pkg}" DESTINATION "${_tess_outer}")
        file(GLOB _pkg_zst "${_tess_outer}/pkg-*.tar.zst")
        list(LENGTH _pkg_zst _pkg_zst_n)
        if(_pkg_zst_n GREATER 0)
            list(GET _pkg_zst 0 _pkg_zst_one)
            set(_tess_inner "${CMAKE_BINARY_DIR}/_tess_inner")
            file(REMOVE_RECURSE "${_tess_inner}")
            file(MAKE_DIRECTORY "${_tess_inner}")
            file(ARCHIVE_EXTRACT INPUT "${_pkg_zst_one}" DESTINATION "${_tess_inner}")
            if(EXISTS "${_tess_inner}/Library/bin/tesseract.exe")
                file(MAKE_DIRECTORY "${TESS_BIN_DIR}")
                file(GLOB _bin_files "${_tess_inner}/Library/bin/*")
                file(COPY ${_bin_files} DESTINATION "${TESS_BIN_DIR}")
                message(STATUS "Tesseract bundle extracted to ${TESS_BIN_DIR}")
            else()
                message(WARNING "tesseract.exe not found in extracted package")
            endif()
        else()
            message(WARNING "pkg-*.tar.zst not found inside ${_tess_pkg}")
        endif()
    endif()
endif()

# tessdata (jpn / eng / osd) を tesseract-ocr/tessdata_fast から取得
if(WIN32)
    file(MAKE_DIRECTORY "${TESS_DATA_DIR}")
    foreach(_lang jpn eng osd)
        set(_tdata "${TESS_DATA_DIR}/${_lang}.traineddata")
        if(NOT EXISTS "${_tdata}")
            message(STATUS "Downloading tessdata: ${_lang}")
            file(DOWNLOAD
                "https://github.com/tesseract-ocr/tessdata_fast/raw/main/${_lang}.traineddata"
                "${_tdata}"
                SHOW_PROGRESS STATUS _td_st)
            list(GET _td_st 0 _td_code)
            if(NOT _td_code EQUAL 0)
                message(WARNING "tessdata ${_lang} download failed: ${_td_st}")
                file(REMOVE "${_tdata}")
            endif()
        endif()
    endforeach()
endif()

if(WIN32 AND EXISTS "${TESS_BIN_DIR}/tesseract.exe")
    message(STATUS "Tesseract: enabled (${TESS_BIN_DIR})")
    # tesseract バンドル → exe 隣の tesseract/
    add_custom_command(TARGET pbShot POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:pbShot>/tesseract"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${TESS_BIN_DIR}" "$<TARGET_FILE_DIR:pbShot>/tesseract"
        COMMENT "Copy Tesseract bundle next to pbShot.exe")
    # tessdata → exe 隣の tessdata/
    if(EXISTS "${TESS_DATA_DIR}/eng.traineddata" OR EXISTS "${TESS_DATA_DIR}/jpn.traineddata")
        add_custom_command(TARGET pbShot POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:pbShot>/tessdata"
            COMMAND ${CMAKE_COMMAND} -E copy_directory "${TESS_DATA_DIR}" "$<TARGET_FILE_DIR:pbShot>/tessdata"
            COMMENT "Copy tessdata next to pbShot.exe")
    endif()
elseif(APPLE)
    # Mac は third_party/tesseract_mac/{bin,tessdata} があれば .app/Contents/Resources/ に同梱。
    # 無ければランタイムは Homebrew の /opt/homebrew/bin/tesseract や PATH を検索する
    # （TesseractOcr.cpp 側で対応）。
    set(TESS_MAC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/tesseract_mac")
    if(EXISTS "${TESS_MAC_DIR}/bin/tesseract")
        message(STATUS "Tesseract (mac): bundling from ${TESS_MAC_DIR}")
        add_custom_command(TARGET pbShot POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory
                    "$<TARGET_BUNDLE_CONTENT_DIR:pbShot>/Resources/tesseract"
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                    "${TESS_MAC_DIR}/bin"
                    "$<TARGET_BUNDLE_CONTENT_DIR:pbShot>/Resources/tesseract"
            COMMENT "Copy Tesseract bundle into .app/Contents/Resources/tesseract")
        if(EXISTS "${TESS_MAC_DIR}/tessdata")
            add_custom_command(TARGET pbShot POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E make_directory
                        "$<TARGET_BUNDLE_CONTENT_DIR:pbShot>/Resources/tessdata"
                COMMAND ${CMAKE_COMMAND} -E copy_directory
                        "${TESS_MAC_DIR}/tessdata"
                        "$<TARGET_BUNDLE_CONTENT_DIR:pbShot>/Resources/tessdata"
                COMMENT "Copy tessdata into .app/Contents/Resources/tessdata")
        endif()
    else()
        message(STATUS "Tesseract (mac): bundle not found, runtime falls back to /opt/homebrew or PATH")
    endif()
else()
    message(STATUS "Tesseract: bundle not found, runtime fallback to PATH")
endif()

# ---- ビルド後に DLL を exe の隣にまとめる（ポータブル起動できるように） ----
if(WIN32)
    # windeployqt
    get_target_property(_qt_qmake_executable Qt6::qmake IMPORTED_LOCATION)
    get_filename_component(_qt_bin_dir "${_qt_qmake_executable}" DIRECTORY)
    find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${_qt_bin_dir}")
    if(WINDEPLOYQT_EXECUTABLE)
        add_custom_command(TARGET pbShot POST_BUILD
            COMMAND "${WINDEPLOYQT_EXECUTABLE}"
                    --release
                    --no-translations
                    --no-system-d3d-compiler
                    --no-opengl-sw
                    --no-compiler-runtime
                    "$<TARGET_FILE:pbShot>"
            COMMENT "Running windeployqt on pbShot.exe")
    endif()

    # MinGW ランタイム DLL
    set(_mingw_bin "C:/gcc/mingw64/bin")
    foreach(_dll libgcc_s_seh-1.dll libstdc++-6.dll libwinpthread-1.dll libssp-0.dll)
        if(EXISTS "${_mingw_bin}/${_dll}")
            add_custom_command(TARGET pbShot POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "${_mingw_bin}/${_dll}"
                        "$<TARGET_FILE_DIR:pbShot>/${_dll}"
                COMMENT "Copy MinGW runtime ${_dll}")
        endif()
    endforeach()
endif()
