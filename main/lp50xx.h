#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "driver/i2c.h"

#define LP50XX_DEFAULT_I2C_ADDR             0x28
#define LP50XX_DEFAULT_I2C_HOST_ID          0
#define LP50XX_DEFAULT_I2C_TIMEOUT_MS       1000
#define LP50XX_DEFAULT_MAX_CURRENT          0
#define LP50XX_DEFAULT_POWERSAVE_ENABLED    0
#define LP50XX_DEFAULT_LOG_DIM_ENABLED      0
#define LP50XX_DEFAULT_PWM_DIM_ENABLED      0


typedef struct {
    i2c_port_t i2c_host_id;         /*<! lp50xx I2C host (Default: 0) */
    uint8_t i2c_addr;               /*<! lp50xx I2C address (0x6C, Ox6D for -B) */
    uint16_t i2c_timeout_ms;        /*<! lp50xx I2C timeout (Default: 1000) */
    uint8_t max_current;            /*<! Maximum allowable LED current: 0=25mA, 1=35mA */
    bool powersave_enabled;         /*<! Auto powersave enabled */
    bool log_dim_enabled;           /*<! 0 = linear dimming, 1 = log-scale dimming */
    bool pwm_dim_enabled;           /*<! PWM dimming enabled */
} lp50xx_config_t;

typedef lp50xx_config_t lp50xx_start_args_t;

/**
 * @brief Initialize lp50xx module.
 * @param config Configuration
 * @return ESP_OK on success
 */
esp_err_t lp50xx_init(const lp50xx_config_t* config);

/**
 * @brief Check if lp50xx is inited
 * @return true if lp50xx is inited
 */
bool lp50xx_is_inited();

/**
 * @brief Destroy lp50xx instance and free all resources
 */
void lp50xx_destroy();

/**
 * @brief Turn all LEDs on
 */
esp_err_t lp50xx_set_global_off(bool global_off);

/**
 * @brief Enable/disable bank control (0 = individual control)
 */
esp_err_t lp50xx_set_bank_control(bool bank_control);

/**
 * @brief Enable/disable night mode
 */
void lp50xx_set_global_scale(float scale);

/**
 * @brief Set bank colour
 * @param red Red intensity (0-255)
 * @param green Green intensity (0-255)
 * @param blue Blue intensity (0-255)
 */
void lp50xx_set_color_bank(uint8_t red, uint8_t green, uint8_t blue);

/**
 * @brief Set LED colour
 * @param led LED ID (0-23, 0-17 for LP5018)
 * @param red Red intensity (0-255)
 * @param green Green intensity (0-255)
 * @param blue Blue intensity (0-255)
 */
void lp50xx_set_color_led(uint8_t led, uint8_t red, uint8_t green, uint8_t blue);


#ifdef __cplusplus
}
#endif