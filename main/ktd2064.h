#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "driver/i2c.h"

#define KTD2064_DEFAULT_I2C_ADDR             0x6C
#define KTD2064_DEFAULT_I2C_HOST_ID          0
#define KTD2064_DEFAULT_I2C_TIMEOUT_MS       1000
#define KTD2064_DEFAULT_MAX_CURRENT_MA       5
#define KTD2064_DEFAULT_COOLEXTEND_TEMP      2       
#define KTD2064_DEFAULT_BRIGHTEXTEND_ENABLE  true

typedef struct {
    i2c_port_t i2c_host_id;         /*<! KTD2064 I2C host (Default: 0) */
    uint8_t i2c_addr;               /*<! KTD2064 I2C address (0x6C, Ox6D for -B) */
    uint16_t i2c_timeout_ms;        /*<! KTD2064 I2C timeout (Default: 1000) */
    uint8_t max_current_r_ma;       /*<! Maximum allowable LED red channel current, in milliamps */
    uint8_t max_current_g_ma;       /*<! Maximum allowable LED green channel current, in milliamps */
    uint8_t max_current_b_ma;       /*<! Maximum allowable LED blue channel current, in milliamps */
    uint8_t coolextend_temp;        /*<! CoolExtend: 0=135C, 1=120C, 2=105C, 3=90C */
    bool brightextend_enable;       /*<! BrightExtend: 0=disabled, 1=enabled */
} ktd2064_config_t;

typedef ktd2064_config_t ktd2064_start_args_t;

/**
 * @brief Initialize KTD2064 module.
 * @param config Configuration
 * @return ESP_OK on success
 */
esp_err_t ktd2064_init(const ktd2064_config_t* config);

/**
 * @brief Check if KTD2064 is inited
 * @return true if KTD2064 is inited
 */
bool ktd2064_is_inited();

/**
 * @brief Destroy KTD2064 instance and free all resources
 */
void ktd2064_destroy();

/**
 * @brief Turn all LEDs on with specified fade time
 * @param fade_constant [0 = 0.032s, 1 = 0.063s, 2 = 0.125s, 3 = 0.25s, 4 = 0.5s, 5 = 1.0s, 6 = 2.0s, 7 = 4.0s]
 */
esp_err_t ktd2064_global_on(uint8_t fade_constant);

/**
 * @brief Turn all LEDs off with specified fade time
 * @param fade_constant [0 = 0.032s, 1 = 0.063s, 2 = 0.125s, 3 = 0.25s, 4 = 0.5s, 5 = 1.0s, 6 = 2.0s, 7 = 4.0s]
 */
esp_err_t ktd2064_global_off(uint8_t fade_constant);

void ktd2064_set_night_mode(bool night_mode);

/**
 * @brief Set palette colour 0
 * @param red Red intensity (0-255)
 * @param green Green intensity (0-255)
 * @param blue Blue intensity (0-255)
 */
void ktd2064_set_color0(uint8_t red, uint8_t green, uint8_t blue);

/**
 * @brief Set palette colour 1
 * @param red Red intensity (0-255)
 * @param green Green intensity (0-255)
 * @param blue Blue intensity (0-255)
 */
void ktd2064_set_color1(uint8_t red, uint8_t green, uint8_t blue);

void ktd2064_select_all(uint8_t data);

#define ktd2064_select_off() ktd2064_select_all(0x00)
#define ktd2064_select_all_color0() ktd2064_select_all(0x88)
#define ktd2064_select_all_color1() ktd2064_select_all(0xFF)

void ktd2064_select_one(uint8_t reg, uint8_t data);

void ktd2064_select_colors(uint8_t isela12, uint8_t isela34, uint8_t iselb12, uint8_t iselb34);

#ifdef __cplusplus
}
#endif