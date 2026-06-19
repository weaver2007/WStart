set(CMAKE_SYSTEM_NAME Windows)

if(NOT DEFINED HKM_MINGW_ROOT OR HKM_MINGW_ROOT STREQUAL "")
    message(FATAL_ERROR "HKM_MINGW_ROOT must point to the MinGW toolchain root, for example D:/Qt/Tools/mingw1310_64.")
endif()

file(TO_CMAKE_PATH "${HKM_MINGW_ROOT}" HKM_MINGW_ROOT)
set(HKM_MINGW_ROOT "${HKM_MINGW_ROOT}" CACHE PATH "MinGW toolchain root")
set(CMAKE_TRY_COMPILE_PLATFORM_VARIABLES HKM_MINGW_ROOT)
set(CMAKE_C_COMPILER "${HKM_MINGW_ROOT}/bin/gcc.exe" CACHE FILEPATH "")
set(CMAKE_CXX_COMPILER "${HKM_MINGW_ROOT}/bin/g++.exe" CACHE FILEPATH "")
set(CMAKE_RC_COMPILER "${HKM_MINGW_ROOT}/bin/windres.exe" CACHE FILEPATH "")

if(CMAKE_GENERATOR MATCHES "Ninja" AND EXISTS "D:/Qt/Tools/Ninja/ninja.exe")
    set(CMAKE_MAKE_PROGRAM "D:/Qt/Tools/Ninja/ninja.exe" CACHE FILEPATH "Ninja executable")
elseif(CMAKE_GENERATOR STREQUAL "MinGW Makefiles")
    set(CMAKE_MAKE_PROGRAM "${HKM_MINGW_ROOT}/bin/mingw32-make.exe" CACHE FILEPATH "MinGW make executable")
endif()
