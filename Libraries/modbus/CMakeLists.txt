# Define the static library target
add_library(modbus STATIC
    Src/modbus.c
    Src/modbus_helpers.c
    Src/modbus_discrete_inputs.c
    Src/modbus_holding_registers.c
    Src/modbus_coils.c
    Src/modbus_tcp.c
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