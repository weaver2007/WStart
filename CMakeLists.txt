cmake_minimum_required(VERSION 3.21)

if(POLICY CMP0091)
    cmake_policy(SET CMP0091 NEW)
endif()

project(HotKeyManager VERSION 0.1.0 LANGUAGES CXX)

option(HKM_DEPLOY_QT_RUNTIME "Run windeployqt and copy MSVC runtime DLLs after building HotKeyManager." ON)
option(HKM_STATIC_RUNTIME "Link HotKeyManager targets with the static MSVC runtime (/MT or /MTd)." OFF)
option(HKM_REQUIRE_STATIC_QT "Fail configuration unless the selected Qt installation is static." OFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

if(MSVC AND HKM_STATIC_RUNTIME)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

find_package(Qt6 REQUIRED COMPONENTS Widgets Svg Test)
get_target_property(QT_QMAKE_EXECUTABLE Qt6::qmake IMPORTED_LOCATION)
get_filename_component(QT_BIN_DIR "${QT_QMAKE_EXECUTABLE}" DIRECTORY)

if(HKM_REQUIRE_STATIC_QT AND QT6_IS_SHARED_LIBS_BUILD)
    message(FATAL_ERROR "HKM_REQUIRE_STATIC_QT=ON requires a static Qt build. CMAKE_PREFIX_PATH currently points to a shared Qt installation.")
endif()

if(HKM_DEPLOY_QT_RUNTIME)
    find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${QT_BIN_DIR}" REQUIRED)
endif()

if(MSVC AND HKM_DEPLOY_QT_RUNTIME)
    set(CMAKE_INSTALL_UCRT_LIBRARIES TRUE)
    include(InstallRequiredSystemLibraries)
endif()

set(HKM_SOURCES
    src/ActionRunner.cpp
    src/HotkeyEdit.cpp
    src/HotkeyConflictDetector.cpp
    src/HotkeyHookService.cpp
    src/HotkeyTypes.cpp
    src/MainWindow.cpp
    src/RuleDialog.cpp
    src/RuleStore.cpp
    src/TrayController.cpp
    src/UiText.cpp
)

set(HKM_HEADERS
    src/ActionRunner.h
    src/HotkeyEdit.h
    src/HotkeyConflictDetector.h
    src/HotkeyHookService.h
    src/HotkeyTypes.h
    src/MainWindow.h
    src/RuleDialog.h
    src/RuleStore.h
    src/TrayController.h
    src/UiText.h
)

add_executable(HotKeyManager WIN32
    src/main.cpp
    ${HKM_SOURCES}
    ${HKM_HEADERS}
    resources/app.rc
    resources/resources.qrc
)

target_link_libraries(HotKeyManager PRIVATE Qt6::Widgets Qt6::Svg user32 shell32)

if(MSVC AND HKM_STATIC_RUNTIME)
    set_property(TARGET HotKeyManager PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

if(NOT QT6_IS_SHARED_LIBS_BUILD)
    qt_import_plugins(HotKeyManager
        INCLUDE_BY_TYPE platforms Qt6::QWindowsIntegrationPlugin
        INCLUDE_BY_TYPE iconengines Qt6::QSvgIconPlugin
        INCLUDE_BY_TYPE imageformats Qt6::QSvgPlugin Qt6::QICOPlugin
        EXCLUDE_BY_TYPE generic networkinformation platforminputcontexts tls
    )
endif()

if(HKM_DEPLOY_QT_RUNTIME)
    add_custom_command(TARGET HotKeyManager POST_BUILD
        COMMAND "${WINDEPLOYQT_EXECUTABLE}"
                --no-translations
                --no-system-d3d-compiler
                --no-opengl-sw
                "$<TARGET_FILE:HotKeyManager>"
        COMMENT "Deploying Qt runtime DLLs next to HotKeyManager.exe"
    )
endif()

if(MSVC)
    target_compile_options(HotKeyManager PRIVATE /W4 /permissive-)
    target_link_options(HotKeyManager PRIVATE /MANIFEST:NO)
    if(HKM_DEPLOY_QT_RUNTIME)
        foreach(_runtime_dll ${CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS})
            add_custom_command(TARGET HotKeyManager POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "${_runtime_dll}"
                        "$<TARGET_FILE_DIR:HotKeyManager>"
                COMMENT "Copying MSVC runtime ${_runtime_dll}"
            )
        endforeach()
    endif()
else()
    target_compile_options(HotKeyManager PRIVATE -Wall -Wextra -pedantic)
endif()

add_executable(HotKeyManagerTests
    tests/test_core.cpp
    src/HotkeyConflictDetector.cpp
    src/HotkeyTypes.cpp
    src/RuleStore.cpp
    src/UiText.cpp
)

target_link_libraries(HotKeyManagerTests PRIVATE Qt6::Test Qt6::Widgets)

if(MSVC AND HKM_STATIC_RUNTIME)
    set_property(TARGET HotKeyManagerTests PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

enable_testing()
add_test(NAME HotKeyManagerTests COMMAND HotKeyManagerTests)
set_tests_properties(HotKeyManagerTests PROPERTIES
    ENVIRONMENT "PATH=${QT_BIN_DIR};$ENV{PATH}"
)
