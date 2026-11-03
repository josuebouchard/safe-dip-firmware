#include "ethernet_if.h"
#include "main.h"
#include "stm32f4xx_hal.h"
#include "stm32f4xx_hal_gpio.h"
#include "wizchip_conf.h"
#include <stdint.h>

// The handle of the SPI must be defined in main.c
extern SPI_HandleTypeDef hspi1;

static void spi_read_burst(uint8_t* pBuf, uint16_t len){
    HAL_StatusTypeDef status = HAL_SPI_Receive(&hspi1, pBuf, len, HAL_MAX_DELAY);
    if (status != HAL_OK) {
        Error_Handler();
    }
}

static void spi_write_burst(uint8_t* pBuf, uint16_t len){
    HAL_StatusTypeDef status = HAL_SPI_Transmit(&hspi1, pBuf, len, HAL_MAX_DELAY);
    if (status != HAL_OK) {
        Error_Handler();
    }
}

static uint8_t spi_read_byte() {
    uint8_t data;
    spi_read_burst(&data, 1);
    return data;
}

static void spi_write_byte(uint8_t byte_to_write) {
    spi_write_burst(&byte_to_write, 1);
}

static void spi_cs_select(void) {
    HAL_GPIO_WritePin(SPI_CS_GPIO_Port, SPI_CS_Pin, GPIO_PIN_RESET);
}

static void spi_cs_deselect(void) {
    HAL_GPIO_WritePin(SPI_CS_GPIO_Port, SPI_CS_Pin, GPIO_PIN_SET);
}

// === PUBLIC ===

// Array index 0 to 7 corresponds to Sockets 0 to 7
// Total sum of all elements must equal 16 (KB)
static uint8_t tx_size_kb[8] = {2, 2, 2, 2, 2, 2, 2, 2}; // KB for each TX buffer
static uint8_t rx_size_kb[8] = {2, 2, 2, 2, 2, 2, 2, 2}; // KB for each RX buffer

static wiz_NetInfo net_info = {
    .mac = {0x00, 0x08, 0xDC, 0x11, 0x22, 0x33}, // WIZnet assigned MAC block prefix
    .ip  = {192, 168, 0, 2},                           // Static IP of your STM32 server
    .sn  = {255, 255, 255, 0},                           // Subnet Mask
    .gw  = {192, 168, 1, 254},                           // Gateway / Router IP
    .dns = {8, 8, 8, 8},                                 // Google DNS (Optional for local server)
    .dhcp = NETINFO_STATIC,                                              // We are defining static parameters
};

void ethernet_W5500_init() {
    // === Function pointer registration ===

    // Define chip select
    reg_wizchip_cs_cbfunc(spi_cs_select, spi_cs_deselect);

    // Define SPI functions
    reg_wizchip_spi_cbfunc(spi_read_byte, spi_write_byte);
    reg_wizchip_spiburst_cbfunc(spi_read_burst, spi_write_burst);

    // @note: critical section is not defined, since no interrupt touches SPI.
    // If that wasn't the case, the critical section might need to be reintroduced.
    // It's not necessary to disable all IRQs, just the ones touching SPI.

    // === Initialization ===

    HAL_GPIO_WritePin(W5500_RST_GPIO_Port, W5500_RST_Pin, GPIO_PIN_RESET);
    HAL_Delay(100);
    HAL_GPIO_WritePin(W5500_RST_GPIO_Port, W5500_RST_Pin, GPIO_PIN_SET);
    HAL_Delay(100);


    if (wizchip_init(tx_size_kb, rx_size_kb) != 0) {
        Error_Handler();
    }

    // Write the network details directly into the W5500 network engine
    wizchip_setnetinfo(&net_info);
}