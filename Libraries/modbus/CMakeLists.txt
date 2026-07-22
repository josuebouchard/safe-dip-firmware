cmake_minimum_required(VERSION 3.20)

# Define the static library target
add_library(modbus STATIC
    Src/modbus.c
    Src/modbus_helpers.c
    Src/modbus_coils.c
    Src/modbus_discrete_inputs.c
    Src/modbus_input_registers.c
    Src/modbus_holding_registers.c
    Src/modbus_tcp.c
)

# Require standard C11 without compiler-specific language extensions.
set_target_properties(modbus PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED YES
    C_EXTENSIONS NO
)

# Expose the include directories to anyone who links this library
target_include_directories(modbus PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}/Inc/Public
)

# Internal headers only visible when compiling the library itself
target_include_directories(modbus PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Inc/Private
)

# Set `#defines` to the correct values
target_compile_definitions(modbus PUBLIC
)

# Allow warnings-as-errors to be disabled temporarily when bringing the
# existing codebase up to the stricter warning level.
option(
    MODBUS_WARNINGS_AS_ERRORS
    "Treat Modbus library compiler warnings as errors"
    ON
)

if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
    target_compile_options(modbus
        PRIVATE
            -Wall
            -Wextra
            -Wpedantic

            # Conversions and arithmetic.
            -Wconversion
            -Wsign-conversion
            -Wdouble-promotion

            # Pointer and memory usage.
            -Wcast-align
            -Wcast-qual
            -Wpointer-arith
            -Wwrite-strings
            -Wnull-dereference
            -Wvla

            # Control flow and declarations.
            -Wshadow
            -Wswitch-enum
            -Wswitch-default
            -Wmissing-prototypes
            -Wmissing-declarations
            -Wstrict-prototypes
            -Wold-style-definition
            -Wbad-function-cast
            -Wredundant-decls
            -Wnested-externs

            # Preprocessor, formatting, and suspicious constructs.
            -Wundef
            -Wformat=2
            -Wformat-overflow=2
            -Wformat-truncation=2
            -Wlogical-op
            -Wduplicated-cond
            -Wduplicated-branches
    )

    if(MODBUS_WARNINGS_AS_ERRORS)
        target_compile_options(modbus PRIVATE -Werror)
    endif()

elseif(CMAKE_C_COMPILER_ID MATCHES "Clang|AppleClang")
    target_compile_options(modbus
        PRIVATE
            -Wall
            -Wextra
            -Wpedantic

            # Conversions and arithmetic.
            -Wconversion
            -Wsign-conversion
            -Wdouble-promotion

            # Pointer and memory usage.
            -Wcast-align
            -Wcast-qual
            -Wpointer-arith
            -Wwrite-strings
            -Wnull-dereference
            -Wvla

            # Control flow and declarations.
            -Wshadow
            -Wswitch-enum
            -Wswitch-default
            -Wmissing-prototypes
            -Wmissing-declarations
            -Wstrict-prototypes
            -Wold-style-definition
            -Wbad-function-cast
            -Wredundant-decls
            -Wnested-externs

            # Preprocessor and formatting.
            -Wundef
            -Wformat=2
    )

    if(MODBUS_WARNINGS_AS_ERRORS)
        target_compile_options(modbus PRIVATE -Werror)
    endif()

elseif(MSVC)
    target_compile_options(modbus
        PRIVATE
            /W4
            /permissive-
    )

    if(MODBUS_WARNINGS_AS_ERRORS)
        target_compile_options(modbus PRIVATE /WX)
    endif()

else()
    message(
        WARNING
        "No strict warning configuration is defined for C compiler "
        "'${CMAKE_C_COMPILER_ID}'."
    )
endif()