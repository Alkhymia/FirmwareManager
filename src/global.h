#define CS 16
#define NCE 4
#define NCONFIG 5
#define DBG_OUTPUT_PORT Serial

#define FPGA_I2C_ADDR 0x3c
#define FPGA_I2C_FREQ_KHZ 800
#define FPGA_I2C_SCL 0
#define FPGA_I2C_SDA 2
#define CLOCK_STRETCH_TIMEOUT 200

#define FIRMWARE_FILE "/firmware.dc"
#define FIRMWARE_EXTENSION "dc"

#define ESP_FIRMWARE_FILE "/firmware.bin"
#define ESP_FIRMWARE_EXTENSION "bin"
#define ESP_INDEX_FILE "/index.html.gz"
#define ESP_INDEX_STAGING_FILE "/esp.index.html.gz"

#define PAGES 8192 // 8192 pages x 256 bytes = 2MB = 16MBit
#define DEBUG true

#ifndef GLOBAL_H
#define GLOBAL_H

#define ERROR_WRONG_MAGIC 16
#define ERROR_WRONG_VERSION 17
#define ERROR_FILE 18
#define ERROR_FILE_SIZE 19

#define FW_VERSION "__FW_VERSION__"

#define CHECK_BIT(var,pos) ((var) & (1<<(pos)))

#endif
