# One target per layer. The target_link_libraries() calls below are the dependency
# graph: a layer only sees the headers of what it links, so including from a layer
# it doesn't depend on fails to compile. See README.md for what each layer is for.

# Extra warnings for our own code only, so the CubeMX/HAL sources don't drown the output.
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

# OBJECT, not STATIC: hardware.cpp overrides a weak HAL callback, which must not
# depend on the object being pulled out of an archive at link time.
function(add_app_layer name)
  add_library(${name} OBJECT ${ARGN})
  target_include_directories(${name} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/${name})
  target_compile_options(${name} PRIVATE ${APP_WARNINGS})
endfunction()

# A layer with only headers. It has nothing to compile on its own, so it can't be an
# OBJECT library: its headers build inside the App layers that include them, under
# their warnings.
function(add_app_header_layer name)
  add_library(${name} INTERFACE)
  target_include_directories(${name} INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/${name})
endfunction()

add_app_layer(drivers
    drivers/effector_driver.cpp
    drivers/hardware.cpp
    drivers/stepper_driver.cpp
    drivers/stm32_output_compare_channel.cpp
)
target_link_libraries(drivers PUBLIC stm32cubemx)

add_app_layer(control
    control/stepper_controller.cpp
)
target_link_libraries(control PUBLIC drivers)

add_app_header_layer(machine)
target_link_libraries(machine INTERFACE control)

# Generic over its context, so it's compiled where that context is known: see
# app/sequencer_instance.cpp.
add_app_header_layer(sequencer)

add_app_layer(operations
    operations/movement_operations.cpp
    operations/timer_operations.cpp
)
target_link_libraries(operations PUBLIC sequencer machine control drivers)

add_app_layer(comms
    comms/ethernet_if.c
    comms/modbus_tcp_task.c
)
target_link_libraries(comms PUBLIC stm32cubemx wiznet_w5500 modbus)

add_app_layer(app
    app/entrypoint.cpp
    app/modbus_map.cpp
    app/sequence.cpp
    app/sequencer_instance.cpp
)
target_link_libraries(app PUBLIC operations sequencer machine control drivers comms stm32cubemx)
