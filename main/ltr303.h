#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "driver/i2c.h"

#define LTR303_DEFAULT_I2C_ADDR             0x29
#define LTR303_DEFAULT_I2C_HOST_ID          0
#define LTR303_DEFAULT_I2C_TIMEOUT_MS       1000
#define LTR303_DEFAULT_GAIN                 0
#define LTR303_DEFAULT_INTEGRATION_TIME     0
#define LTR303_DEFAULT_MEASUREMENT_RATE     3

typedef struct {
    i2c_port_t i2c_host_id;         /*<! LTR303 I2C host (Default: 0) */
    uint8_t i2c_addr;               /*<! LTR303 I2C address (0x6C, Ox6D for -B) */
    uint16_t i2c_timeout_ms;        /*<! LTR303 I2C timeout (Default: 1000) */
    uint8_t gain;                   /*<! LTR303 gain valid values:
                                        0: 1x, 1: 2x, 2: 4x, 3: 8x, 6: 48x, 7: 96x. */
    uint8_t integration_time;       /*<! LTR303 ALS integration time values:
                                        0 (default): 100ms, 1: 50ms, 2: 200ms, 3: 400ms, 4: 150ms, 5: 250ms, 6: 300ms, 7: 350ms */
    uint8_t measurement_rate;       /*<! LTR303 ALS integration time values:
                                        0: 50ms, 1: 100ms, 2: 200ms, 3 (default): 500ms, 4: 1000ms, 5-7: 2000ms */
} ltr303_config_t;

typedef ltr303_config_t ltr303_start_args_t;

/**
 * @brief Initialize LTR303 module.
 * @param config Configuration
 * @return ESP_OK on success
 */
esp_err_t ltr303_init(const ltr303_config_t* config);

/**
 * @brief Check if LTR303 is inited
 * @return true if LTR303 is inited
 */
bool ltr303_is_inited();

/**
 * @brief Destroy LTR303 instance and free all resources
 */
void ltr303_destroy();


void ltr303_set_gain(uint8_t gain);

void ltr303_set_measurement_rate(uint8_t integration_time, uint8_t measurement_rate);

/**
 * @brief Read LED, outputs lumens
 */
uint16_t ltr303_read_lux();

#ifdef __cplusplus
}
#endif