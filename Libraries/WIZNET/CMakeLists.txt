# Define the static library target
add_library(wiznet_w5500 STATIC
    Ethernet/socket.c
    Ethernet/wizchip_conf.c
    Ethernet/W5500/w5500.c
)

# Expose the include directories to anyone who links this library
target_include_directories(wiznet_w5500 PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}/Ethernet
    ${CMAKE_CURRENT_SOURCE_DIR}/Ethernet/W5500
)

# Set `#defines` to the correct values
target_compile_definitions(wiznet_w5500 PUBLIC
    _WIZCHIP_=5500
)

# CRITICAL FIX: Intercept 'close' only inside this library compilation
target_compile_definitions(wiznet_w5500 PRIVATE
    close=wiznet_close
)