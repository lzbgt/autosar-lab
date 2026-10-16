cmake_minimum_required(VERSION 3.20)
project(autosar_lab VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_library(lab_core STATIC
    src/core/lab_ecu.c
    src/core/lab_com.c
    src/core/lab_cantp.c
    src/core/lab_dcm.c
)
target_include_directories(lab_core PUBLIC src/core)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(lab_core PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_executable(lab_ecu src/host/main.c src/platform/lab_transport.c)
    target_link_libraries(lab_ecu PRIVATE lab_core)
    target_include_directories(lab_ecu PRIVATE src/platform)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(lab_ecu PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(test_core src/tests/test_core.c)
    target_link_libraries(test_core PRIVATE lab_core)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(test_core PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
    add_test(NAME core_protocol COMMAND test_core)
endif()
