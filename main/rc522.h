#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "driver/spi_master.h"

#define RC522_DEFAULT_MISO                 (25)
#define RC522_DEFAULT_MOSI                 (23)
#define RC522_DEFAULT_SCK                  (19)
#define RC522_DEFAULT_SDA                  (22)
#define RC522_DEFAULT_SPI_HOST             (SPI2_HOST)

typedef struct {
    int miso_io;                    /*<! MFRC522 MISO gpio (Default: 25) */
    int mosi_io;                    /*<! MFRC522 MOSI gpio (Default: 23) */
    int sck_io;                     /*<! MFRC522 SCK gpio  (Default: 19) */
    int sda_io;                     /*<! MFRC522 SDA gpio  (Default: 22) */
    spi_host_device_t spi_host_id;  /*<! Default VSPI_HOST (SPI3) */
} rc522_config_t;

typedef rc522_config_t rc522_start_args_t;

/**
 * @brief Initialize RC522 module.
 * @param config Configuration
 * @return ESP_OK on success
 */
esp_err_t rc522_init(const rc522_config_t* config);

/**
 * @brief Convert serial number (array of 5 bytes) to uint64_t number
 * @param sn Serial number
 * @return Serial number in number representation. If fail, 0 will be retured
 */
uint64_t rc522_sn_to_u64(uint8_t* sn);

/**
 * @brief Get tag
 * @param
 * @return Pointer to tag, if found, else NULL
 */
uint8_t* rc522_get_tag();

/**
 * @brief Check if RC522 is inited
 * @return true if RC522 is inited
 */
bool rc522_is_inited();

/**
 * @brief Destroy RC522 and free all resources
 */
void rc522_destroy();

#ifdef __cplusplus
}
#endif