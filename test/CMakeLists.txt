# =============================================================================
# Migration Switcher - console test project (separate from the GUI app)
#
#   mkdir build && cd build
#   cmake .. -G "Visual Studio 17 2022" -A x64
#   cmake --build . --config Release
#   Release\ms_test.exe            (all tests)
#   Release\ms_test.exe --live <file-with-api-key>
#
# NOTE: source files are UTF-8 with a BOM and CRLF line endings.
# =============================================================================
cmake_minimum_required(VERSION 3.15)
project(MigrationSwitcherTests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../src")

# every source of the GUI app except main.cpp, plus the test harness
file(GLOB MS_SOURCES "${SRC_DIR}/*.cpp")
list(REMOVE_ITEM MS_SOURCES "${SRC_DIR}/main.cpp")

add_executable(ms_test ${MS_SOURCES} "${CMAKE_CURRENT_SOURCE_DIR}/console_test.cpp")

target_include_directories(ms_test PRIVATE "${SRC_DIR}")
target_compile_definitions(ms_test PRIVATE UNICODE _UNICODE MS_BUILD_CONSOLE_TESTS)

if(MSVC)
    target_compile_options(ms_test PRIVATE
        /source-charset:utf-8 /execution-charset:.932 /W3 /EHsc /MP)
else()
    target_compile_options(ms_test PRIVATE
        -Wall -Wextra -finput-charset=UTF-8 -fexec-charset=CP932)
    target_link_options(ms_test PRIVATE -static -static-libgcc -static-libstdc++)
endif()

target_link_libraries(ms_test PRIVATE
    comctl32 comdlg32 shlwapi ole32 shell32 gdi32 user32 advapi32 winhttp)

message(STATUS "test/sample.c and test/existing_switch.c are sample INPUT files "
               "for the tool, not build targets.")
