#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "math.h"

#include "driver/i2c_master.h"

#include "lp50xx.h"

static const char* TAG = "ESP-LP50XX";

struct lp50xx {
    lp50xx_config_t* config;
    float brightness_scale;
};

typedef struct lp50xx* lp50xx_handle_t;

static lp50xx_handle_t hndl = NULL;

bool lp50xx_is_inited()
{
    return hndl != NULL;
}

static esp_err_t lp50xx_write(uint8_t addr, uint8_t val)
{
    uint8_t write_buf[2] = {addr, val};

    esp_err_t ret = i2c_master_transmit(hndl->config->i2c_handle, write_buf, sizeof(write_buf), hndl->config->i2c_timeout_ms / portTICK_PERIOD_MS);

    return ret;
}

static uint8_t lp50xx_read(uint8_t addr)
{
    uint8_t buffer[2];

    esp_err_t ret = i2c_master_transmit_receive(hndl->config->i2c_handle, &addr, 1, buffer, 1, hndl->config->i2c_timeout_ms / portTICK_PERIOD_MS);
    assert(ret == ESP_OK);

    uint8_t res = buffer[0];
    return res;
}

esp_err_t lp50xx_init(const lp50xx_config_t* config)
{
    if (! config) {
        return ESP_ERR_INVALID_ARG;
    }

    if (hndl) {
        ESP_LOGW(TAG, "Already initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (!(hndl = calloc(1, sizeof(struct lp50xx)))) {
        return ESP_ERR_NO_MEM;
    }

    if (!(hndl->config = calloc(1, sizeof(lp50xx_config_t)))) {
        lp50xx_destroy();
        return ESP_ERR_NO_MEM;
    }

    // copy config considering defaults
    hndl->config->i2c_handle        = config->i2c_handle;
    hndl->config->i2c_timeout_ms    = config->i2c_timeout_ms == 0 ? LP50XX_DEFAULT_I2C_TIMEOUT_MS : config->i2c_timeout_ms;
    hndl->config->max_current       = config->max_current == 0 ? LP50XX_DEFAULT_MAX_CURRENT : config->max_current;
    hndl->config->powersave_enabled = config->powersave_enabled == 0 ? LP50XX_DEFAULT_POWERSAVE_ENABLED : config->powersave_enabled;
    hndl->config->log_dim_enabled   = config->log_dim_enabled == 0 ? LP50XX_DEFAULT_LOG_DIM_ENABLED : config->log_dim_enabled;
    hndl->config->pwm_dim_enabled   = config->pwm_dim_enabled == 0 ? LP50XX_DEFAULT_PWM_DIM_ENABLED : config->pwm_dim_enabled;

    // set Chip_EN on
    lp50xx_write(0x00, 0b01000000);

    lp50xx_set_global_scale(1.0);

    // write to control register
    lp50xx_set_global_off(1);

    ESP_LOGI(TAG, "Initialized");

    return ESP_OK;
}

void lp50xx_set_global_scale(float scale)
{
    hndl->brightness_scale = scale;
}

esp_err_t lp50xx_set_global_off(bool global_off)
{
    uint8_t control_reg = (global_off & 0b00000001)
                          + (hndl->config->max_current << 1 & 0b00000010)
                          + (hndl->config->pwm_dim_enabled << 2 & 0b00000100)
                          + (hndl->config->powersave_enabled << 4 & 0b00010000)
                          + (hndl->config->log_dim_enabled << 5 & 0b00100000);

    return lp50xx_write(0x01, control_reg);
}

esp_err_t lp50xx_set_bank_control(bool bank_control)
{
    if (bank_control) {
        return lp50xx_write(0x02, 0b11111111);
    } else {
        return lp50xx_write(0x02, 0b00000000);
    }
}

void lp50xx_set_color_bank(uint8_t red, uint8_t green, uint8_t blue)
{
    lp50xx_write(0x04, red * hndl->brightness_scale);
    lp50xx_write(0x05, green * hndl->brightness_scale);
    lp50xx_write(0x06, blue * hndl->brightness_scale);
}

void lp50xx_set_color_led(uint8_t led, uint8_t red, uint8_t green, uint8_t blue)
{
    lp50xx_write((led * 3) + 0x0F, red * hndl->brightness_scale);
    lp50xx_write((led * 3) + 0x10, green * hndl->brightness_scale);
    lp50xx_write((led * 3) + 0x11, blue * hndl->brightness_scale);
}

void lp50xx_destroy()
{
    if (! hndl) {
        return;
    }

    free(hndl->config);
    hndl->config = NULL;

    free(hndl);
    hndl = NULL;
}