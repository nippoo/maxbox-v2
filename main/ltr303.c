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

#include "ltr303.h"

static const char* TAG = "ESP-LTR303";

struct ltr303 {
    ltr303_config_t* config;
    bool is_reset;
    bool is_running;
};

typedef struct ltr303* ltr303_handle_t;

static ltr303_handle_t hndl = NULL;

#define ltr303_whoami() ltr303_read(0x87)

bool ltr303_is_inited() {
    return hndl != NULL;
}

static esp_err_t ltr303_write(uint8_t addr, uint8_t val) {
    uint8_t write_buf[2] = {addr, val};

    esp_err_t ret = i2c_master_write_to_device(hndl->config->i2c_host_id,
        hndl->config->i2c_addr, write_buf, sizeof(write_buf), hndl->config->i2c_timeout_ms / portTICK_PERIOD_MS);

    return ret;
}

static uint8_t ltr303_read(uint8_t addr) {
    uint8_t buffer[2];

    esp_err_t ret = i2c_master_write_read_device(hndl->config->i2c_host_id,
        hndl->config->i2c_addr, &addr, 1, buffer, 1, hndl->config->i2c_timeout_ms / portTICK_PERIOD_MS);
    assert(ret == ESP_OK);

    uint8_t res = buffer[0];
    return res;
}

static void ltr303_write_control_reg()
{
    uint8_t control_reg = hndl->is_running
                          + (hndl->is_reset << 1)
                          + (hndl->config->gain << 2);

    ltr303_write(0x80, control_reg);
}

esp_err_t ltr303_init(const ltr303_config_t* config) {
    if(! config) {
        return ESP_ERR_INVALID_ARG;
    }

    if(hndl) {
        ESP_LOGW(TAG, "Already initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if(! (hndl = calloc(1, sizeof(struct ltr303)))) {
        return ESP_ERR_NO_MEM;
    }

    if(! (hndl->config = calloc(1, sizeof(ltr303_config_t)))) {
        ltr303_destroy();
        return ESP_ERR_NO_MEM;
    }

    // copy config considering defaults
    hndl->config->i2c_addr         = config->i2c_addr == 0 ? LTR303_DEFAULT_I2C_ADDR : config->i2c_addr;
    hndl->config->i2c_timeout_ms   = config->i2c_timeout_ms == 0 ? LTR303_DEFAULT_I2C_TIMEOUT_MS : config->i2c_timeout_ms;
    hndl->config->i2c_host_id      = config->i2c_host_id == 0 ? LTR303_DEFAULT_I2C_HOST_ID : config->i2c_host_id;

    uint8_t whoami = ltr303_whoami();

    if (whoami != 0x05)
    {
        ESP_LOGE(TAG, "Detection error: expected 0x05, got 0x%x", whoami);
        ltr303_destroy();
        return ESP_ERR_INVALID_STATE;
    }

    hndl->is_running = true;

    ltr303_set_gain(config->gain);
    ltr303_set_measurement_rate(config->integration_time, config->measurement_rate);

    vTaskDelay(pdMS_TO_TICKS(10)); // wait 10ms before reading, as per datasheet

    ESP_LOGI(TAG, "Initialized");

    return ESP_OK;
}

void ltr303_set_gain(uint8_t gain)
{
    if ((gain > 3 && gain < 6) || gain >= 7) {gain = LTR303_DEFAULT_GAIN;}
    
    hndl->config->gain = gain;
    ltr303_write_control_reg();

}

void ltr303_set_measurement_rate(uint8_t integration_time, uint8_t measurement_rate)
{
    if(integration_time >= 0x07) {integration_time = 0x00;}
    if(measurement_rate >= 0x07) {measurement_rate = 0x00;}

    hndl->config->integration_time = integration_time;
    hndl->config->measurement_rate = measurement_rate;

    uint8_t rate_reg = measurement_rate + (integration_time << 3);

    ltr303_write(0x85, rate_reg);
}

uint16_t ltr303_read_lux()
{
    uint8_t ch1_low = ltr303_read(0x88);
    uint16_t CH1 = (ltr303_read(0x89) << 8) + ch1_low;

    uint8_t ch0_low = ltr303_read(0x8A);
    uint16_t CH0 = (ltr303_read(0x8B) << 8) + ch0_low;


    double ratio, ALS_INT;
    uint16_t lux;
    uint8_t ALS_GAIN;
    
    // Determine if either sensor saturated (0xFFFF)
    // If so, abandon ship (calculation will not be accurate)
    if ((CH0 == 0xFFFF) || (CH1 == 0xFFFF)) {
        lux = 0.0;
        return(false);
    }

    // We will need the ratio for subsequent calculations
    ratio = CH1 / (CH0 + CH1);


  // Gain can take any value from 0-7, except 4 & 5
  // If gain = 4, invalid
  // If gain = 5, invalid
    switch(hndl->config->gain){
        case 0:            // If gain = 0, device is set to 1X gain (default)
            ALS_GAIN = 1;
            break;
        case 1:            // If gain = 1, device is set to 2X gain
            ALS_GAIN = 2;
            break;
        case 2:           // If gain = 2, device is set to 4X gain   
            ALS_GAIN = 4;
            break;
        case 3:           // If gain = 3, device is set to 8X gain    
            ALS_GAIN = 8;
            break;
        case 6:          // If gain = 6, device is set to 48X gain
            ALS_GAIN = 48;
            break;  
        case 7:           // If gain = 7, device is set to 96X gain  
            ALS_GAIN = 96;
            break;
        default:          // If gain = 0, device is set to 1X gain (default)         
            ALS_GAIN = 1;
            break;
    }


    switch(hndl->config->integration_time){
        case 0:              // If integrationTime = 0, integrationTime will be 100ms (default)
            ALS_INT = 1;
            break;
        case 1:              // If integrationTime = 1, integrationTime will be 50ms
            ALS_INT = 0.5;
            break;
        case 2:              // If integrationTime = 2, integrationTime will be 200ms
            ALS_INT = 2;
            break;
        case 3:               // If integrationTime = 3, integrationTime will be 400ms
            ALS_INT = 4;
            break;
        case 4:               // If integrationTime = 4, integrationTime will be 150ms
            ALS_INT = 1.5;
            break;
        case 5:               // If integrationTime = 5, integrationTime will be 250ms
            ALS_INT = 2.5;
            break;
        case 6:               // If integrationTime = 6, integrationTime will be 300ms
            ALS_INT = 3;
            break;  
        case 7:               // If integrationTime = 7, integrationTime will be 350ms
            ALS_INT = 3.5;
            break;
        default:             // If integrationTime = 0, integrationTime will be 100ms (default)
            ALS_INT = 1;
            break;
        }

    // Determine lux per datasheet equations:
    if (ratio < 0.45) {
        lux = ((1.7743 * CH0) + (1.1059 * CH1))/ALS_GAIN/ALS_INT;
    }
    else if ((ratio < 0.64) && (ratio >= 0.45)){
        lux = ((4.2785 * CH0) + (1.9548 * CH1))/ALS_GAIN/ALS_INT;
    }
    else if ((ratio < 0.85) && (ratio >= 0.64)){
        lux = ((0.5926 * CH0) + (0.1185 * CH1))/ALS_GAIN/ALS_INT;
    }
    // if (ratio >= 0.85)
    else {  
        lux = 0.0;
    }   

    return(lux);
}

void ltr303_destroy() {
    if(! hndl) { return; }

    free(hndl->config);
    hndl->config = NULL;

    free(hndl);
    hndl = NULL;
}