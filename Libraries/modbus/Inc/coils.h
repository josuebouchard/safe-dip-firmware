#ifndef COILS_H
#define COILS_H

#include <stdint.h>
#include <stdbool.h>

// 0x01
struct modbus_read_coils_request {
    uint16_t starting_address;
    uint16_t quantity_of_coils;
};

// 0x05
struct modbus_write_single_coil_request {
    uint16_t output_address;
    uint16_t output_value;
};

// 0x01
struct modbus_data_write_multiple_coils_request {
    uint16_t starting_address;
    uint16_t quantity_of_outputs;

    // Outputs
    uint8_t byte_count;             // Outputs value count
    const uint8_t *outputs_value;   // view into caller/scratch storage
};

bool modbus_coil_is_set(struct modbus_data_write_multiple_coils_request *data, uint8_t coil);


#endif // COILS_H
