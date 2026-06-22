# Define the static library target
add_library(modbus STATIC
    Src/modbus.c
    Src/coils.c
)

# Expose the include directories to anyone who links this library
target_include_directories(modbus PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}/Inc
)

# Set `#defines` to the correct values
target_compile_definitions(modbus PUBLIC
)