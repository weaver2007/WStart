cmake_minimum_required(VERSION 3.21)

project(HotKeyManager VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Svg Test)
get_target_property(QT_QMAKE_EXECUTABLE Qt6::qmake IMPORTED_LOCATION)
get_filename_component(QT_BIN_DIR "${QT_QMAKE_EXECUTABLE}" DIRECTORY)
find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${QT_BIN_DIR}" REQUIRED)

if(MSVC)
    set(CMAKE_INSTALL_UCRT_LIBRARIES TRUE)
    include(InstallRequiredSystemLibraries)
endif()

set(HKM_SOURCES
    src/ActionRunner.cpp
    src/HotkeyEdit.cpp
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

add_custom_command(TARGET HotKeyManager POST_BUILD
    COMMAND "${WINDEPLOYQT_EXECUTABLE}"
            --no-translations
            --no-system-d3d-compiler
            --no-opengl-sw
            "$<TARGET_FILE:HotKeyManager>"
    COMMENT "Deploying Qt runtime DLLs next to HotKeyManager.exe"
)

if(MSVC)
    target_compile_options(HotKeyManager PRIVATE /W4 /permissive-)
    target_link_options(HotKeyManager PRIVATE /MANIFEST:NO)
    foreach(_runtime_dll ${CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS})
        add_custom_command(TARGET HotKeyManager POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${_runtime_dll}"
                    "$<TARGET_FILE_DIR:HotKeyManager>"
            COMMENT "Copying MSVC runtime ${_runtime_dll}"
        )
    endforeach()
else()
    target_compile_options(HotKeyManager PRIVATE -Wall -Wextra -pedantic)
endif()

add_executable(HotKeyManagerTests
    tests/test_core.cpp
    src/HotkeyTypes.cpp
    src/RuleStore.cpp
    src/UiText.cpp
)

target_link_libraries(HotKeyManagerTests PRIVATE Qt6::Test Qt6::Widgets)

enable_testing()
add_test(NAME HotKeyManagerTests COMMAND HotKeyManagerTests)
set_tests_properties(HotKeyManagerTests PROPERTIES
    ENVIRONMENT "PATH=${QT_BIN_DIR};$ENV{PATH}"
)
