cmake_minimum_required(VERSION 3.22)

#
# This file is generated only once,
# and is not re-generated if converter is called multiple times.
#
# User is free to modify the file as much as necessary
#

# Setup compiler settings
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Define the build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

# Set the project name
set(CMAKE_PROJECT_NAME nucleo-project)

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Core project settings
project(${CMAKE_PROJECT_NAME})
message("Build type: " ${CMAKE_BUILD_TYPE})

# Enable CMake support for ASM, C, and C++ languages
enable_language(C CXX ASM)

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME})

# Add STM32CubeMX generated sources
add_subdirectory(cmake/stm32cubemx)

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined library search paths
)

# Add sources to executable
set(APP_SOURCES
    App/Src/effector_driver.cpp
    App/Src/effector_operations.cpp
    App/Src/entrypoint.cpp
    App/Src/ethernet_if.c
    App/Src/hardware.cpp
    App/Src/modbus_map.cpp
    App/Src/modbus_tcp_task.c
    App/Src/movement_operations.cpp
    App/Src/sequence.cpp
    App/Src/sequencer.cpp
    App/Src/stepper_controller.cpp
    App/Src/stepper_driver.cpp
    App/Src/stm32_output_compare_channel.cpp
    App/Src/timer_operations.cpp
)

target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user sources here
    ${APP_SOURCES}
)

# Extra warnings for our own code only. The CubeMX/HAL sources share this target
# and would drown the output, so the options are set per source file.
set(APP_WARNINGS
    -Wextra
    -Wconversion            # implicit narrowing: int -> uint8_t, float -> int, ...
    -Wsign-conversion       # signed <-> unsigned, e.g. -1 passed as std::uint8_t
    -Wdouble-promotion      # float silently promoted to double (no double FPU on the M4)
    -Wshadow                # a local hiding another name
    -Wundef                 # #if on an undefined macro
    -Wcast-align
    -Wformat=2
    -Wimplicit-fallthrough
    $<$<COMPILE_LANGUAGE:CXX>:-Wold-style-cast>
    $<$<COMPILE_LANGUAGE:CXX>:-Wnon-virtual-dtor>
    $<$<COMPILE_LANGUAGE:CXX>:-Woverloaded-virtual>
    $<$<COMPILE_LANGUAGE:CXX>:-Wextra-semi>
)
set_source_files_properties(${APP_SOURCES} PROPERTIES COMPILE_OPTIONS "${APP_WARNINGS}")

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined include paths
    App/Inc
)

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined symbols
)

# Remove wrong libob.a library dependency when using cpp files
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

# Add libraries subdirectory
add_subdirectory(Libraries/WIZNET)
set(MODBUS_WARNINGS_AS_ERRORS OFF)
add_subdirectory(Libraries/modbus)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx

    # Add user defined libraries
    wiznet_w5500
    modbus
)

# Treat vendor headers as system headers for our code, so the warnings above skip
# macros we cannot change (GPIOB, GPIO_PIN_5 and TIM2 expand to C casts, getSn_SR()
# to int arithmetic). Only consumers are affected; the libraries build as before.
foreach(vendor_target stm32cubemx wiznet_w5500)
    get_target_property(vendor_includes ${vendor_target} INTERFACE_INCLUDE_DIRECTORIES)
    set_property(TARGET ${vendor_target} APPEND PROPERTY
        INTERFACE_SYSTEM_INCLUDE_DIRECTORIES ${vendor_includes})
endforeach()
