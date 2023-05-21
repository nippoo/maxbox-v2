#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "math.h"

#include "driver/i2c.h"

#include "ktd2064.h"

static const char* TAG = "ESP-KTD2064";

struct ktd2064 {
    ktd2064_config_t* config;
    bool night_mode;
};

typedef struct ktd2064* ktd2064_handle_t;

static ktd2064_handle_t hndl = NULL;

#define ktd2064_whoami() ktd2064_read(0x00)

bool ktd2064_is_inited() {
    return hndl != NULL;
}

static esp_err_t ktd2064_write(uint8_t addr, uint8_t val) {
    uint8_t write_buf[2] = {addr, val};

    esp_err_t ret = i2c_master_write_to_device(hndl->config->i2c_host_id,
        hndl->config->i2c_addr, write_buf, sizeof(write_buf), hndl->config->i2c_timeout_ms / portTICK_PERIOD_MS);

    return ret;
}

static uint8_t ktd2064_read(uint8_t addr) {
    uint8_t buffer[2];

    esp_err_t ret = i2c_master_write_read_device(hndl->config->i2c_host_id,
        hndl->config->i2c_addr, &addr, 1, buffer, 1, hndl->config->i2c_timeout_ms / portTICK_PERIOD_MS);
    assert(ret == ESP_OK);

    uint8_t res = buffer[0];
    return res;
}

// static esp_err_t ktd2064_set_bitmask(uint8_t addr, uint8_t mask) {
//     return ktd2064_write(addr, ktd2064_read(addr) | mask);
// }

// static esp_err_t ktd2064_clear_bitmask(uint8_t addr, uint8_t mask) {
//     return ktd2064_write(addr, ktd2064_read(addr) & ~mask);
// }

esp_err_t ktd2064_init(const ktd2064_config_t* config) {
    if(! config) {
        return ESP_ERR_INVALID_ARG;
    }

    if(hndl) {
        ESP_LOGW(TAG, "Already initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if(! (hndl = calloc(1, sizeof(struct ktd2064)))) {
        return ESP_ERR_NO_MEM;
    }

    if(! (hndl->config = calloc(1, sizeof(ktd2064_config_t)))) {
        ktd2064_destroy();
        return ESP_ERR_NO_MEM;
    }

    // copy config considering defaults
    hndl->config->i2c_addr         = config->i2c_addr == 0 ? KTD2064_DEFAULT_I2C_ADDR : config->i2c_addr;
    hndl->config->i2c_timeout_ms   = config->i2c_timeout_ms == 0 ? KTD2064_DEFAULT_I2C_TIMEOUT_MS : config->i2c_timeout_ms;
    hndl->config->i2c_host_id      = config->i2c_host_id == 0 ? KTD2064_DEFAULT_I2C_HOST_ID : config->i2c_host_id;
    hndl->config->max_current_r_ma = config->max_current_r_ma == 0 ? KTD2064_DEFAULT_MAX_CURRENT_MA : config->max_current_r_ma;
    hndl->config->max_current_g_ma = config->max_current_g_ma == 0 ? KTD2064_DEFAULT_MAX_CURRENT_MA : config->max_current_g_ma;
    hndl->config->max_current_b_ma = config->max_current_b_ma == 0 ? KTD2064_DEFAULT_MAX_CURRENT_MA : config->max_current_b_ma;
    hndl->config->coolextend_temp = config->coolextend_temp == 0 ? KTD2064_DEFAULT_COOLEXTEND_TEMP : config->coolextend_temp;
    hndl->config->brightextend_enable = config->brightextend_enable == 0 ? KTD2064_DEFAULT_BRIGHTEXTEND_ENABLE : config->brightextend_enable;

    hndl->night_mode = false;

    uint8_t whoami = ktd2064_whoami();

    if (whoami != 0xa4)
    {
        ESP_LOGE(TAG, "Detection error: expected 0xa4, got 0x%x", whoami);
        ktd2064_destroy();
        return ESP_ERR_INVALID_STATE;
    }

    // reset control register
    ktd2064_write(0x02, 0b11000000);
    ktd2064_global_off(0);

    ESP_LOGI(TAG, "Initialized");

    return ESP_OK;
}

esp_err_t ktd2064_global_on(uint8_t fade_constant)
{
    uint8_t control_reg = (fade_constant & 0b00000111)
        + ((hndl->config->coolextend_temp << 3) & 0b00011000)
        + ((hndl->config->brightextend_enable << 5) & 0b00100000)
        + (((!hndl->night_mode + 1) << 6) & 0b11000000);

    return ktd2064_write(0x02, control_reg);
}

esp_err_t ktd2064_global_off(uint8_t fade_constant)
{
    uint8_t control_reg = (fade_constant & 0b00000111)
        + ((hndl->config->coolextend_temp << 3) & 0b00011000)
        + ((hndl->config->brightextend_enable << 5) & 0b00100000);

    return ktd2064_write(0x02, control_reg);
}

void ktd2064_set_night_mode(bool night_mode)
{
    hndl->night_mode = night_mode;
}

static inline uint8_t scale_color_current(uint8_t color, uint8_t max_current)
{
    return round(pow((color/255.0 + 0.055)/(1 + 0.055), 2.4) * (max_current * 8));
}

void ktd2064_set_color0(uint8_t red, uint8_t green, uint8_t blue)
{
    ktd2064_write(0x03, scale_color_current(red, hndl->config->max_current_r_ma));
    ktd2064_write(0x04, scale_color_current(green, hndl->config->max_current_g_ma));
    ktd2064_write(0x05, scale_color_current(blue, hndl->config->max_current_b_ma));
}

void ktd2064_set_color1(uint8_t red, uint8_t green, uint8_t blue)
{
    ktd2064_write(0x06, scale_color_current(red, hndl->config->max_current_r_ma));
    ktd2064_write(0x07, scale_color_current(green, hndl->config->max_current_g_ma));
    ktd2064_write(0x08, scale_color_current(blue, hndl->config->max_current_b_ma));
}

void ktd2064_select_all(uint8_t data)
{
    ktd2064_write(0x09, data);
    ktd2064_write(0x0A, data);
    ktd2064_write(0x0B, data);
    ktd2064_write(0x0C, data);
}

void ktd2064_select_one(uint8_t reg, uint8_t data)
{
    ktd2064_write(0x09 + reg, data);
}

void ktd2064_select_colors(uint8_t isela12, uint8_t isela34, uint8_t iselb12, uint8_t iselb34)
{ 
    ktd2064_write(0x09, isela12);
    ktd2064_write(0x0A, isela34);
    ktd2064_write(0x0B, iselb12);
    ktd2064_write(0x0C, iselb34);
}

void ktd2064_destroy() {
    if(! hndl) { return; }

    free(hndl->config);
    hndl->config = NULL;

    free(hndl);
    hndl = NULL;
}