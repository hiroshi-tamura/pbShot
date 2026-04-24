cmake_minimum_required(VERSION 3.20)
project(pbShot VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

if(NOT DEFINED CMAKE_PREFIX_PATH)
    set(CMAKE_PREFIX_PATH "C:/Qt/6.9.1/mingw_64")
endif()

find_package(Qt6 REQUIRED COMPONENTS Widgets Gui Core)
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
)

set_target_properties(pbShot PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

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
