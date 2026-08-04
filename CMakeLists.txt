cmake_minimum_required(VERSION 3.21)

set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION")
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" HKM_BASE_VERSION LIMIT_COUNT 1)
if(NOT HKM_BASE_VERSION MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+$")
    message(FATAL_ERROR "VERSION must contain one semantic version such as 0.3.46.")
endif()

if(POLICY CMP0091)
    cmake_policy(SET CMP0091 NEW)
endif()

project(WStart VERSION ${HKM_BASE_VERSION} LANGUAGES CXX)

# Newer localized MSVC toolsets can report /showIncludes text in an encoding
# that CMake misdetects. Ninja then misses header dependencies and may link
# stale object files. Repair only the known VS 2026 Chinese mojibake value;
# English CI runners and correctly detected locales retain CMake's value.
if(MSVC AND CMAKE_GENERATOR MATCHES "Ninja")
    string(HEX "${CMAKE_CXX_CL_SHOWINCLUDES_PREFIX}" HKM_MSVC_SHOWINCLUDES_PREFIX_HEX)
    if(HKM_MSVC_SHOWINCLUDES_PREFIX_HEX STREQUAL
       "e5a889e384a6e589b03a20e98d96e591ade68388e98f82e59ba6e6aca23a2020")
        string(ASCII 230 179 168 230 132 143 58 32 229 140 133 229 144 171 230 150 135 228 187 182 58 32 32
               CMAKE_CXX_CL_SHOWINCLUDES_PREFIX)
        set(CMAKE_CL_SHOWINCLUDES_PREFIX "${CMAKE_CXX_CL_SHOWINCLUDES_PREFIX}")
    endif()
endif()

option(HKM_DEPLOY_QT_RUNTIME "Run windeployqt and copy MSVC runtime DLLs after building WStart on Windows." ON)
option(HKM_DEPLOY_QT_RUNTIME_MANUAL "Copy a minimal Qt runtime set manually instead of running windeployqt." OFF)
option(HKM_STATIC_RUNTIME "Link WStart targets with the static MSVC runtime (/MT or /MTd)." OFF)
option(HKM_REQUIRE_STATIC_QT "Fail configuration unless the selected Qt installation is static." OFF)
set(HKM_WINVER "0x0601" CACHE STRING "Windows target version macro, for example 0x0601 for Windows 7 or 0x0501 for Windows XP.")
set_property(CACHE HKM_WINVER PROPERTY STRINGS 0x0501 0x0601 0x0A00)
set(HKM_NTDDI_VERSION "" CACHE STRING "Windows NTDDI_VERSION macro. Leave empty to derive it from HKM_WINVER.")
set(HKM_QT_MAJOR "AUTO" CACHE STRING "Qt major version to use: AUTO, 6, 5, or 4.")
set_property(CACHE HKM_QT_MAJOR PROPERTY STRINGS AUTO 6 5 4)
set(HKM_CXX_STANDARD "17" CACHE STRING "C++ standard used by WStart targets.")
set_property(CACHE HKM_CXX_STANDARD PROPERTY STRINGS 14 17)
set(HKM_APP_VERSION_OVERRIDE "" CACHE STRING "Override the application version embedded in binaries.")
set(HKM_UPDATE_MANIFEST_URL "" CACHE STRING "HTTPS URL of the update manifest JSON used by the in-app update checker.")
set(HKM_EFFECTIVE_APP_VERSION "${PROJECT_VERSION}")
if(NOT HKM_APP_VERSION_OVERRIDE STREQUAL "")
    set(HKM_EFFECTIVE_APP_VERSION "${HKM_APP_VERSION_OVERRIDE}")
endif()

set(CMAKE_CXX_STANDARD ${HKM_CXX_STANDARD})
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

if(WIN32)
    if(HKM_NTDDI_VERSION STREQUAL "")
        if(HKM_WINVER STREQUAL "0x0501")
            set(HKM_EFFECTIVE_NTDDI_VERSION "0x05010300")
        elseif(HKM_WINVER STREQUAL "0x0601")
            set(HKM_EFFECTIVE_NTDDI_VERSION "0x06010000")
        elseif(HKM_WINVER STREQUAL "0x0A00")
            set(HKM_EFFECTIVE_NTDDI_VERSION "0x0A000000")
        else()
            set(HKM_EFFECTIVE_NTDDI_VERSION "${HKM_WINVER}")
        endif()
    else()
        set(HKM_EFFECTIVE_NTDDI_VERSION "${HKM_NTDDI_VERSION}")
    endif()
    add_compile_definitions(WINVER=${HKM_WINVER} _WIN32_WINNT=${HKM_WINVER} NTDDI_VERSION=${HKM_EFFECTIVE_NTDDI_VERSION})
endif()

if(MSVC AND HKM_STATIC_RUNTIME)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

set(HKM_QT4_COMPAT_SOURCES)

if(HKM_QT_MAJOR STREQUAL "4")
    find_package(Qt4 4.8.7 REQUIRED QtCore QtGui QtNetwork QtSvg QtTest)
    include(${QT_USE_FILE})
    set(QT_VERSION_MAJOR 4)
    set(HKM_QT_PACKAGE "Qt4")
    set(HKM_QT_LIBS ${QT_QTGUI_LIBRARY} ${QT_QTSVG_LIBRARY} ${QT_QTNETWORK_LIBRARY} ${QT_QTCORE_LIBRARY})
    set(HKM_QT_TEST_LIBS ${QT_QTTEST_LIBRARY} ${QT_QTGUI_LIBRARY} ${QT_QTNETWORK_LIBRARY} ${QT_QTCORE_LIBRARY})
    set(QT_QMAKE_EXECUTABLE "${QT_QMAKE_EXECUTABLE}")
    set(HKM_QT4_COMPAT_SOURCES
        src/qt4compat/QJsonArray.cpp
        src/qt4compat/QJsonDocument.cpp
    )
elseif(HKM_QT_MAJOR STREQUAL "AUTO")
    find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets Svg Network Test)
    find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Svg Network Test)
    set(HKM_QT_PACKAGE "Qt${QT_VERSION_MAJOR}")
    set(HKM_QT_LIBS
        ${HKM_QT_PACKAGE}::Widgets
        ${HKM_QT_PACKAGE}::Svg
        ${HKM_QT_PACKAGE}::Network
    )
    set(HKM_QT_TEST_LIBS
        ${HKM_QT_PACKAGE}::Test
        ${HKM_QT_PACKAGE}::Widgets
        ${HKM_QT_PACKAGE}::Network
    )
else()
    find_package(Qt${HKM_QT_MAJOR} REQUIRED COMPONENTS Widgets Svg Network Test)
    set(QT_VERSION_MAJOR "${HKM_QT_MAJOR}")
    set(HKM_QT_PACKAGE "Qt${QT_VERSION_MAJOR}")
    set(HKM_QT_LIBS
        ${HKM_QT_PACKAGE}::Widgets
        ${HKM_QT_PACKAGE}::Svg
        ${HKM_QT_PACKAGE}::Network
    )
    set(HKM_QT_TEST_LIBS
        ${HKM_QT_PACKAGE}::Test
        ${HKM_QT_PACKAGE}::Widgets
        ${HKM_QT_PACKAGE}::Network
    )
endif()
set(HKM_QT5_STATIC_APP_COMPAT_LIBS)
set(HKM_QT5_STATIC_PLATFORM_COMPAT_LIBS)
set(HKM_QT5_STATIC_THIRDPARTY_COMPAT_LIBS)
set(HKM_QT5_STATIC_SYSTEM_LIBS)
set(HKM_STATIC_SYSTEM_LIBS)
set(HKM_QT4_QTMAIN_LIB)

if(TARGET ${HKM_QT_PACKAGE}::qmake)
    get_target_property(QT_QMAKE_EXECUTABLE ${HKM_QT_PACKAGE}::qmake IMPORTED_LOCATION)
endif()

if(NOT QT_QMAKE_EXECUTABLE)
    find_program(QT_QMAKE_EXECUTABLE qmake REQUIRED)
endif()
get_filename_component(QT_BIN_DIR "${QT_QMAKE_EXECUTABLE}" DIRECTORY)
get_filename_component(QT_PREFIX_DIR "${QT_BIN_DIR}" DIRECTORY)
execute_process(
    COMMAND "${QT_QMAKE_EXECUTABLE}" -query QT_CONFIG
    OUTPUT_VARIABLE HKM_QT_CONFIG
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
# Static Qt is reported differently by Qt 4, Qt 5, and locally built Qt 6 packages.
# Keep these probes together so static presets can fail early and import plugins correctly.
set(HKM_QT_IS_STATIC OFF)
if(HKM_QT_CONFIG MATCHES "(^|[ ;])static($|[ ;])")
    set(HKM_QT_IS_STATIC ON)
endif()
if(NOT HKM_QT_IS_STATIC)
    set(HKM_QCONFIG_PRI "${QT_PREFIX_DIR}/mkspecs/qconfig.pri")
    if(EXISTS "${HKM_QCONFIG_PRI}")
        file(STRINGS "${HKM_QCONFIG_PRI}" HKM_QCONFIG_PRI_LINES)
        foreach(HKM_QCONFIG_PRI_LINE IN LISTS HKM_QCONFIG_PRI_LINES)
            if(HKM_QCONFIG_PRI_LINE MATCHES "^[ \t]*(QT_CONFIG|CONFIG|QT\\.global\\.enabled_features)[^=]*="
                AND HKM_QCONFIG_PRI_LINE MATCHES "(^|[ \t;])static([ \t;]|$)")
                set(HKM_QT_IS_STATIC ON)
            endif()
        endforeach()
    endif()
endif()
if(NOT HKM_QT_IS_STATIC)
    set(HKM_QT_CORE_TARGETS "${QT_PREFIX_DIR}/lib/cmake/Qt${QT_VERSION_MAJOR}Core/Qt${QT_VERSION_MAJOR}CoreTargets.cmake")
    if(EXISTS "${HKM_QT_CORE_TARGETS}")
        file(STRINGS "${HKM_QT_CORE_TARGETS}" HKM_QT_CORE_TARGET_STATIC_LINES REGEX "QT_QMAKE_PUBLIC_CONFIG.*static")
        if(HKM_QT_CORE_TARGET_STATIC_LINES)
            set(HKM_QT_IS_STATIC ON)
        endif()
    endif()
endif()
if(DEFINED QT_FEATURE_static AND QT_FEATURE_static)
    set(HKM_QT_IS_STATIC ON)
endif()

if(QT_VERSION_MAJOR EQUAL 4 AND WIN32)
    find_library(HKM_QT4_QTMAIN_LIB
        NAMES qtmain libqtmain
        HINTS "${QT_PREFIX_DIR}/lib"
        NO_DEFAULT_PATH
    )
endif()

if(WIN32 AND HKM_QT_IS_STATIC AND QT_VERSION_MAJOR EQUAL 4)
    if(MINGW)
        set(HKM_STATIC_SYSTEM_LIBS
            gdi32 comdlg32 oleaut32 imm32 winmm winspool msimg32 ole32 uuid ws2_32 advapi32 shell32 user32 kernel32 z
        )
    elseif(MSVC)
        set(HKM_STATIC_SYSTEM_LIBS
            gdi32 comdlg32 oleaut32 imm32 winmm winspool msimg32 ole32 uuid ws2_32 advapi32 shell32 user32 kernel32 version
        )
    endif()
endif()

if(WIN32 AND HKM_QT_IS_STATIC AND QT_VERSION_MAJOR EQUAL 5)
    foreach(_hkm_qt5_static_app_lib IN ITEMS
        "${QT_PREFIX_DIR}/plugins/platforms/libqwindows.a"
        "${QT_PREFIX_DIR}/plugins/platforms/qwindows.lib"
        "${QT_PREFIX_DIR}/plugins/iconengines/libqsvgicon.a"
        "${QT_PREFIX_DIR}/plugins/iconengines/qsvgicon.lib"
        "${QT_PREFIX_DIR}/plugins/imageformats/libqsvg.a"
        "${QT_PREFIX_DIR}/plugins/imageformats/qsvg.lib"
        "${QT_PREFIX_DIR}/plugins/imageformats/libqico.a"
        "${QT_PREFIX_DIR}/plugins/imageformats/qico.lib"
    )
        if(EXISTS "${_hkm_qt5_static_app_lib}")
            list(APPEND HKM_QT5_STATIC_APP_COMPAT_LIBS "${_hkm_qt5_static_app_lib}")
        endif()
    endforeach()

    foreach(_hkm_qt5_static_platform_lib IN ITEMS
        "${QT_PREFIX_DIR}/lib/libQt5PlatformSupport.a"
        "${QT_PREFIX_DIR}/lib/Qt5PlatformSupport.lib"
        "${QT_PREFIX_DIR}/lib/libQt5DBus.a"
        "${QT_PREFIX_DIR}/lib/Qt5DBus.lib"
    )
        if(EXISTS "${_hkm_qt5_static_platform_lib}")
            list(APPEND HKM_QT5_STATIC_PLATFORM_COMPAT_LIBS "${_hkm_qt5_static_platform_lib}")
        endif()
    endforeach()

    foreach(_hkm_qt5_static_thirdparty_lib IN ITEMS
        "${QT_PREFIX_DIR}/lib/libqtfreetype.a"
        "${QT_PREFIX_DIR}/lib/qtfreetype.lib"
        "${QT_PREFIX_DIR}/lib/libqtharfbuzzng.a"
        "${QT_PREFIX_DIR}/lib/qtharfbuzzng.lib"
        "${QT_PREFIX_DIR}/lib/qtharfbuzz.lib"
        "${QT_PREFIX_DIR}/lib/libqtpcre.a"
        "${QT_PREFIX_DIR}/lib/qtpcre.lib"
        "${QT_PREFIX_DIR}/lib/qtpcre2.lib"
    )
        if(EXISTS "${_hkm_qt5_static_thirdparty_lib}")
            list(APPEND HKM_QT5_STATIC_THIRDPARTY_COMPAT_LIBS "${_hkm_qt5_static_thirdparty_lib}")
        endif()
    endforeach()

    if(MINGW)
        set(HKM_QT5_STATIC_SYSTEM_LIBS
            winspool shlwapi netapi32 gdi32 comdlg32 oleaut32 imm32 winmm ws2_32 advapi32 mpr z
        )
    elseif(MSVC)
        set(HKM_QT5_STATIC_SYSTEM_LIBS
            winspool shlwapi netapi32 gdi32 comdlg32 oleaut32 imm32 winmm ws2_32 advapi32 mpr version
        )
    endif()
endif()

if(WIN32 AND HKM_QT5_STATIC_SYSTEM_LIBS)
    set(HKM_STATIC_SYSTEM_LIBS ${HKM_QT5_STATIC_SYSTEM_LIBS})
endif()

if(HKM_REQUIRE_STATIC_QT AND NOT HKM_QT_IS_STATIC)
    message(FATAL_ERROR "HKM_REQUIRE_STATIC_QT=ON requires a static Qt build. CMAKE_PREFIX_PATH currently points to a shared Qt installation.")
endif()

if(HKM_DEPLOY_QT_RUNTIME AND HKM_QT_IS_STATIC)
    message(STATUS "Static Qt detected; windeployqt is not needed.")
elseif(WIN32 AND HKM_DEPLOY_QT_RUNTIME AND NOT HKM_DEPLOY_QT_RUNTIME_MANUAL)
    find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${QT_BIN_DIR}" REQUIRED)
    set(HKM_DEPLOY_ENV_PATH "${QT_BIN_DIR};$ENV{PATH}")
    string(REPLACE ";" "\\;" HKM_DEPLOY_ENV_PATH "${HKM_DEPLOY_ENV_PATH}")
endif()

if(MSVC AND HKM_DEPLOY_QT_RUNTIME)
    set(CMAKE_INSTALL_UCRT_LIBRARIES TRUE)
    include(InstallRequiredSystemLibraries)
    if(CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS)
        set(HKM_EXISTING_SYSTEM_RUNTIME_LIBS)
        foreach(_runtime_dll IN LISTS CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS)
            if(EXISTS "${_runtime_dll}")
                list(APPEND HKM_EXISTING_SYSTEM_RUNTIME_LIBS "${_runtime_dll}")
            else()
                message(STATUS "Skipping missing MSVC runtime file: ${_runtime_dll}")
            endif()
        endforeach()
        set(CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS ${HKM_EXISTING_SYSTEM_RUNTIME_LIBS})
    endif()
endif()

set(HKM_SOURCES
    src/AppIcon.cpp
    src/AppLogger.cpp
    src/ActionRunner.cpp
    src/CredentialStore.cpp
    src/HotkeyConflictDetector.cpp
    src/HotkeyHookService.cpp
    src/HotkeyState.cpp
    src/HotkeyTypes.cpp
    src/PathUtils.cpp
    src/RuleStore.cpp
    src/SelfUpdater.cpp
    src/SingleInstanceServer.cpp
    src/SystemIconProvider.cpp
    src/TrayController.cpp
    src/UiText.cpp
    src/UpdateChecker.cpp
)

set(HKM_HEADERS
    src/AppIcon.h
    src/AppLogger.h
    src/ActionRunner.h
    src/BuiltInActions.h
    src/CredentialStore.h
    src/HotkeyConflictDetector.h
    src/HotkeyHookService.h
    src/HotkeyState.h
    src/LauncherWindowInterface.h
    src/HotkeyTypes.h
    src/RuleStore.h
    src/SelfUpdater.h
    src/SingleInstanceServer.h
    src/SystemIconProvider.h
    src/TrayController.h
    src/UiText.h
    src/UpdateChecker.h
)

if(QT_VERSION_MAJOR EQUAL 4)
    set(HKM_MAIN_SOURCE src/main_qt4.cpp)
else()
    set(HKM_MAIN_SOURCE src/main.cpp)
endif()

list(APPEND HKM_SOURCES
    src/RuleDialog.cpp
    src/MainWindow.cpp
)
list(APPEND HKM_HEADERS
    src/RuleDialog.h
    src/MainWindow.h
)

set(HKM_EXECUTABLE_OPTIONS)
set(HKM_PLATFORM_SOURCES)
if(WIN32)
    list(APPEND HKM_EXECUTABLE_OPTIONS WIN32)
    list(APPEND HKM_PLATFORM_SOURCES resources/app.rc)
elseif(APPLE)
    list(APPEND HKM_EXECUTABLE_OPTIONS MACOSX_BUNDLE)
endif()

add_executable(HotKeyManager ${HKM_EXECUTABLE_OPTIONS}
    ${HKM_MAIN_SOURCE}
    ${HKM_SOURCES}
    ${HKM_HEADERS}
    ${HKM_QT4_COMPAT_SOURCES}
    ${HKM_PLATFORM_SOURCES}
    resources/resources.qrc
)

if(QT_VERSION_MAJOR EQUAL 4)
    target_include_directories(HotKeyManager BEFORE PRIVATE src/qt4compat)
    target_compile_definitions(HotKeyManager PRIVATE HKM_QT4_COMPAT)
endif()

target_link_libraries(HotKeyManager PRIVATE
    ${HKM_QT_LIBS}
    ${HKM_QT4_QTMAIN_LIB}
    ${HKM_QT5_STATIC_APP_COMPAT_LIBS}
    ${HKM_QT5_STATIC_PLATFORM_COMPAT_LIBS}
    ${HKM_QT5_STATIC_THIRDPARTY_COMPAT_LIBS}
    ${HKM_STATIC_SYSTEM_LIBS}
)

if(WIN32)
    target_link_libraries(HotKeyManager PRIVATE user32 shell32 ole32 uuid psapi advapi32)
endif()

target_compile_definitions(HotKeyManager PRIVATE
    HKM_APP_VERSION="${HKM_EFFECTIVE_APP_VERSION}"
    HKM_UPDATE_MANIFEST_URL="${HKM_UPDATE_MANIFEST_URL}"
)

if(NOT QT_VERSION_MAJOR EQUAL 4)
    target_link_libraries(HotKeyManager PRIVATE
        ${HKM_QT_PACKAGE}::Gui
        ${HKM_QT_PACKAGE}::Core
    )
endif()

if(HKM_QT_IS_STATIC AND QT_VERSION_MAJOR EQUAL 5)
    target_link_libraries(HotKeyManager PRIVATE
        ${HKM_QT5_STATIC_THIRDPARTY_COMPAT_LIBS}
        ${HKM_STATIC_SYSTEM_LIBS}
    )
endif()

if(MSVC AND HKM_STATIC_RUNTIME)
    set_property(TARGET HotKeyManager PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

if(MINGW AND HKM_STATIC_RUNTIME)
    target_link_options(HotKeyManager PRIVATE -static -static-libgcc -static-libstdc++)
endif()

set_target_properties(HotKeyManager PROPERTIES OUTPUT_NAME WStart)

if(WIN32)
    add_executable(WStartUpdater WIN32 src/updater/WindowsUpdater.cpp)
    target_link_libraries(WStartUpdater PRIVATE shell32)
    if(MSVC)
        target_compile_options(WStartUpdater PRIVATE /W4 /utf-8)
        target_link_options(WStartUpdater PRIVATE /MANIFEST:NO)
        if(HKM_WINVER STREQUAL "0x0501")
            target_link_options(WStartUpdater PRIVATE /SUBSYSTEM:WINDOWS,5.01)
        endif()
    else()
        target_compile_options(WStartUpdater PRIVATE -Wall -Wextra -pedantic)
    endif()
    if(MSVC AND HKM_STATIC_RUNTIME)
        set_property(TARGET WStartUpdater PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    endif()
    if(MINGW AND HKM_STATIC_RUNTIME)
        target_link_options(WStartUpdater PRIVATE -static -static-libgcc -static-libstdc++)
    endif()
    add_custom_command(TARGET HotKeyManager POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_FILE:WStartUpdater>"
                "$<TARGET_FILE_DIR:HotKeyManager>"
        COMMENT "Copying WStartUpdater next to WStart"
    )
endif()
if(APPLE)
    set_target_properties(HotKeyManager PROPERTIES
        MACOSX_BUNDLE_BUNDLE_NAME "WStart"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.wstart.hotkeymanager"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${HKM_EFFECTIVE_APP_VERSION}"
        MACOSX_BUNDLE_BUNDLE_VERSION "${HKM_EFFECTIVE_APP_VERSION}"
    )
endif()

if(WIN32 AND HKM_QT_IS_STATIC AND QT_VERSION_MAJOR EQUAL 6 AND COMMAND qt_import_plugins)
    qt_import_plugins(HotKeyManager
        INCLUDE_BY_TYPE platforms Qt6::QWindowsIntegrationPlugin
        INCLUDE_BY_TYPE iconengines Qt6::QSvgIconPlugin
        INCLUDE_BY_TYPE imageformats Qt6::QSvgPlugin Qt6::QICOPlugin
        EXCLUDE_BY_TYPE generic networkinformation platforminputcontexts tls
    )
elseif(WIN32 AND HKM_QT_IS_STATIC AND QT_VERSION_MAJOR EQUAL 5 AND COMMAND qt5_import_plugins)
    qt5_import_plugins(HotKeyManager
        INCLUDE_BY_TYPE platforms Qt5::QWindowsIntegrationPlugin
        INCLUDE_BY_TYPE iconengines Qt5::QSvgIconPlugin
        INCLUDE_BY_TYPE imageformats Qt5::QSvgPlugin Qt5::QICOPlugin
        EXCLUDE_BY_TYPE generic platforminputcontexts
    )
elseif(WIN32 AND HKM_QT_IS_STATIC AND QT_VERSION_MAJOR EQUAL 5)
    target_compile_definitions(HotKeyManager PRIVATE HKM_IMPORT_QT5_STATIC_PLUGINS)
endif()

if(WIN32 AND HKM_DEPLOY_QT_RUNTIME AND HKM_DEPLOY_QT_RUNTIME_MANUAL AND NOT HKM_QT_IS_STATIC)
    if(QT_VERSION_MAJOR EQUAL 4)
        set(HKM_MANUAL_QT_RUNTIME_DLLS
            QtCore4
            QtGui4
            QtNetwork4
            QtSvg4
        )
    else()
        set(HKM_MANUAL_QT_RUNTIME_DLLS
            Qt5Core
            Qt5Gui
            Qt5Network
            Qt5Svg
            Qt5Widgets
        )
    endif()
    foreach(_qt_runtime_dll ${HKM_MANUAL_QT_RUNTIME_DLLS})
        add_custom_command(TARGET HotKeyManager POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${QT_BIN_DIR}/${_qt_runtime_dll}.dll"
                    "$<TARGET_FILE_DIR:HotKeyManager>"
            COMMENT "Copying Qt runtime ${_qt_runtime_dll}.dll"
        )
    endforeach()
    if(QT_VERSION_MAJOR EQUAL 4)
        set(HKM_MANUAL_QT_PLUGIN_FILES
            "${QT_PREFIX_DIR}/plugins/iconengines/qsvgicon4.dll"
            "${QT_PREFIX_DIR}/plugins/imageformats/qico4.dll"
            "${QT_PREFIX_DIR}/plugins/imageformats/qsvg4.dll"
        )
    else()
        set(HKM_MANUAL_QT_PLUGIN_FILES
            "${QT_PREFIX_DIR}/plugins/platforms/qwindows.dll"
            "${QT_PREFIX_DIR}/plugins/iconengines/qsvgicon.dll"
            "${QT_PREFIX_DIR}/plugins/imageformats/qico.dll"
            "${QT_PREFIX_DIR}/plugins/imageformats/qsvg.dll"
        )
    endif()
    foreach(_qt_plugin_file IN ITEMS ${HKM_MANUAL_QT_PLUGIN_FILES})
        if(EXISTS "${_qt_plugin_file}")
            get_filename_component(_qt_plugin_dir "${_qt_plugin_file}" DIRECTORY)
            get_filename_component(_qt_plugin_type "${_qt_plugin_dir}" NAME)
            add_custom_command(TARGET HotKeyManager POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E make_directory
                        "$<TARGET_FILE_DIR:HotKeyManager>/${_qt_plugin_type}"
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "${_qt_plugin_file}"
                        "$<TARGET_FILE_DIR:HotKeyManager>/${_qt_plugin_type}"
                COMMENT "Copying Qt plugin ${_qt_plugin_type}"
            )
        endif()
    endforeach()
elseif(WIN32 AND HKM_DEPLOY_QT_RUNTIME AND NOT HKM_QT_IS_STATIC)
    add_custom_command(TARGET HotKeyManager POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E env
                "PATH=${HKM_DEPLOY_ENV_PATH}"
                "${WINDEPLOYQT_EXECUTABLE}"
                --no-translations
                --no-system-d3d-compiler
                --no-opengl-sw
                "$<TARGET_FILE:HotKeyManager>"
        COMMENT "Deploying Qt runtime DLLs next to WStart.exe"
    )
endif()

if(MSVC)
    if(QT_VERSION_MAJOR EQUAL 5 AND MSVC_VERSION GREATER_EQUAL 1950)
        target_compile_options(HotKeyManager PRIVATE
            "/FI${CMAKE_CURRENT_SOURCE_DIR}/src/compat/MsvcQt5Compat.h"
        )
    endif()
    target_compile_options(HotKeyManager PRIVATE /W4 /utf-8)
    if(MSVC_VERSION GREATER_EQUAL 1910)
        target_compile_options(HotKeyManager PRIVATE /permissive-)
    endif()
    target_link_options(HotKeyManager PRIVATE /MANIFEST:NO)
    if(HKM_WINVER STREQUAL "0x0501")
        target_link_options(HotKeyManager PRIVATE /SUBSYSTEM:WINDOWS,5.01)
    endif()
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
    ${HKM_QT4_COMPAT_SOURCES}
    src/HotkeyConflictDetector.cpp
    src/HotkeyState.cpp
    src/HotkeyTypes.cpp
    src/PathUtils.cpp
    src/RuleStore.cpp
    src/UpdateChecker.cpp
    src/UiText.cpp
)

if(QT_VERSION_MAJOR EQUAL 4)
    target_include_directories(HotKeyManagerTests BEFORE PRIVATE src/qt4compat)
    target_compile_definitions(HotKeyManagerTests PRIVATE HKM_QT4_COMPAT)
endif()

target_link_libraries(HotKeyManagerTests PRIVATE
    ${HKM_QT_TEST_LIBS}
)

if(WIN32)
    target_link_libraries(HotKeyManagerTests PRIVATE
        ${HKM_QT5_STATIC_THIRDPARTY_COMPAT_LIBS}
        ${HKM_STATIC_SYSTEM_LIBS}
        advapi32
    )
endif()

target_compile_definitions(HotKeyManagerTests PRIVATE
    HKM_APP_VERSION="${HKM_EFFECTIVE_APP_VERSION}"
    HKM_UPDATE_MANIFEST_URL=""
)

if(NOT QT_VERSION_MAJOR EQUAL 4)
    target_link_libraries(HotKeyManagerTests PRIVATE
        ${HKM_QT_PACKAGE}::Gui
        ${HKM_QT_PACKAGE}::Core
    )
endif()

if(WIN32 AND HKM_QT_IS_STATIC AND QT_VERSION_MAJOR EQUAL 5)
    target_link_libraries(HotKeyManagerTests PRIVATE
        ${HKM_QT5_STATIC_THIRDPARTY_COMPAT_LIBS}
        ${HKM_STATIC_SYSTEM_LIBS}
    )
endif()

if(MSVC)
    if(QT_VERSION_MAJOR EQUAL 5 AND MSVC_VERSION GREATER_EQUAL 1950)
        target_compile_options(HotKeyManagerTests PRIVATE
            "/FI${CMAKE_CURRENT_SOURCE_DIR}/src/compat/MsvcQt5Compat.h"
        )
    endif()
    target_compile_options(HotKeyManagerTests PRIVATE /utf-8)
endif()

if(MSVC AND HKM_STATIC_RUNTIME)
    set_property(TARGET HotKeyManagerTests PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

if(MINGW AND HKM_STATIC_RUNTIME)
    target_link_options(HotKeyManagerTests PRIVATE -static -static-libgcc -static-libstdc++)
endif()

install(TARGETS HotKeyManager
    BUNDLE DESTINATION .
    RUNTIME DESTINATION bin
)

if(UNIX AND NOT APPLE)
    configure_file(packaging/linux/wstart.desktop.in "${CMAKE_CURRENT_BINARY_DIR}/wstart.desktop" @ONLY)
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/wstart.desktop" DESTINATION share/applications)
    install(FILES resources/app.svg DESTINATION share/icons/hicolor/scalable/apps RENAME wstart.svg)
endif()

enable_testing()
add_test(NAME HotKeyManagerTests COMMAND HotKeyManagerTests)
if(WIN32)
    set(HKM_TEST_ENV_PATH "${QT_BIN_DIR};$ENV{PATH}")
    if(MINGW AND DEFINED HKM_MINGW_ROOT AND NOT HKM_MINGW_ROOT STREQUAL "")
        file(TO_CMAKE_PATH "${HKM_MINGW_ROOT}" HKM_TEST_MINGW_ROOT)
        set(HKM_TEST_ENV_PATH "${HKM_TEST_MINGW_ROOT}/bin;${HKM_TEST_ENV_PATH}")
    endif()
    string(REPLACE ";" "\\;" HKM_TEST_ENV_PATH "${HKM_TEST_ENV_PATH}")
else()
    set(HKM_TEST_ENV_PATH "${QT_BIN_DIR}:$ENV{PATH}")
endif()
set(HKM_TEST_ENVIRONMENT "PATH=${HKM_TEST_ENV_PATH}")
if(UNIX AND NOT APPLE)
    list(APPEND HKM_TEST_ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()
set_tests_properties(HotKeyManagerTests PROPERTIES ENVIRONMENT "${HKM_TEST_ENVIRONMENT}")
