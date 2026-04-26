cmake_minimum_required(VERSION 3.20)
project(pbShot VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

if(NOT DEFINED CMAKE_PREFIX_PATH)
    set(CMAKE_PREFIX_PATH "C:/Qt/6.9.1/mingw_64")
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
    src/Toast.cpp
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
    src/Toast.h
)

qt_add_executable(pbShot WIN32
    ${SOURCES}
    ${HEADERS}
    resources/resources.qrc
)

if(WIN32)
    target_sources(pbShot PRIVATE resources/app.rc)
    target_link_libraries(pbShot PRIVATE user32)
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

if(EXISTS "${ORT_INCLUDE_DIR}/onnxruntime_cxx_api.h" AND EXISTS "${ORT_LIB_DIR}/onnxruntime.dll")
    message(STATUS "ONNX Runtime: enabled")
    target_compile_definitions(pbShot PRIVATE ORT_AVAILABLE=1)
    target_include_directories(pbShot PRIVATE "${ORT_INCLUDE_DIR}")
    target_link_libraries(pbShot PRIVATE "${ORT_LIB_DIR}/onnxruntime.dll")
    # MinGW gcc は ORT C ヘッダの SAL アノテーションを正しく解釈できないため
    # 関数ポインタ型の整合性が崩れる。実害なし（C 関数ポインタは型に関係なく
    # 呼び出せる）なので Ocr.cpp 単体で -fpermissive を許容。
    if(MINGW OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        set_source_files_properties(src/Ocr.cpp PROPERTIES
            COMPILE_FLAGS "-fpermissive -w")
    endif()
    add_custom_command(TARGET pbShot POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${ORT_LIB_DIR}/onnxruntime.dll"
                "$<TARGET_FILE_DIR:pbShot>/onnxruntime.dll"
        COMMENT "Copy onnxruntime.dll")
    if(EXISTS "${ORT_LIB_DIR}/onnxruntime_providers_shared.dll")
        add_custom_command(TARGET pbShot POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${ORT_LIB_DIR}/onnxruntime_providers_shared.dll"
                    "$<TARGET_FILE_DIR:pbShot>/onnxruntime_providers_shared.dll"
            COMMENT "Copy onnxruntime_providers_shared.dll")
    endif()
else()
    message(STATUS "ONNX Runtime: not found (OCR will return placeholder text). "
                   "Place headers in ${ORT_INCLUDE_DIR} and onnxruntime.dll in ${ORT_LIB_DIR}.")
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
