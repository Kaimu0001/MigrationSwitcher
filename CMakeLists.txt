# =============================================================================
# Migration Switcher - CMakeLists.txt
# Windows native (MSVC / MinGW)
#   mkdir build && cd build
#   cmake .. -G "Visual Studio 17 2022" -A x64
#   cmake --build . --config Release
#
# NOTE: The source files are saved as Shift_JIS (CP932) with CRLF line endings,
#       so MSVC is told to read them as code page 932.
# =============================================================================
cmake_minimum_required(VERSION 3.15)
project(MigrationSwitcher VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT WIN32)
    message(WARNING "This project targets Windows (Win32 API) only. "
                    "From Linux, build with the mingw-w64 cross compiler.")
endif()

# NOTE: only src/*.cpp is compiled here. The files under test/ are a separate
#       console test project (see test/CMakeLists.txt) and two sample INPUT
#       files for the tool - never add test/ to this project.
file(GLOB MS_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
list(APPEND MS_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/app.rc")

add_executable(MigrationSwitcher WIN32 ${MS_SOURCES})

target_include_directories(MigrationSwitcher PRIVATE src)
target_compile_definitions(MigrationSwitcher PRIVATE UNICODE _UNICODE)

if(MSVC)
    # Source and execution code page 932 (Shift_JIS) - matches the files on disk.
    target_compile_options(MigrationSwitcher PRIVATE
        /source-charset:.932 /execution-charset:.932 /W3 /EHsc /MP)
    set_target_properties(MigrationSwitcher PROPERTIES
        LINK_FLAGS "/MANIFEST:NO")
else()
    target_compile_options(MigrationSwitcher PRIVATE
        -Wall -Wextra -finput-charset=CP932 -fexec-charset=CP932)
    target_link_options(MigrationSwitcher PRIVATE
        -mwindows -static -static-libgcc -static-libstdc++)
endif()

# comdlg32 : GetOpenFileNameW / GetSaveFileNameW  (file dialogs)
# winhttp  : WinHttpOpen / WinHttpSendRequest   (AI integration)
# advapi32 : GetUserNameW
# shlwapi  : path helpers
target_link_libraries(MigrationSwitcher PRIVATE
    comctl32 comdlg32 shlwapi ole32 shell32 gdi32 user32 advapi32 winhttp)
