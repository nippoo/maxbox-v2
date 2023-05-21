#include "stdio.h"
#include "string.h"
#include "driver/gpio.h"
#include "inttypes.h"

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "pthread.h"

#include "esp_log.h"

#include "nvs_flash.h"
#include "cJSON.h"

#include "maxbox_defines.h"
#include "rc522.h"
#include "sim7600.h"
#include "led.h"

#include <time.h>
#include <sys/time.h>
#include "freertos/task.h"
#include "driver/uart.h"
#include "esp_sleep.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_check.h"
#include "esp_mac.h"

#include "driver/i2c.h"


static const char* TAG = "MaxBox";

static void io_init(void)
{
    // Power up the MFRC522
    gpio_set_direction(RFID_NRSTPD_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(RFID_NRSTPD_PIN, 1);

    // TODO: Sleep CAN transmitter until required (to save power) 
    // For the moment we just leave it awake all the time
    gpio_set_direction(CAN_SLEEP_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(CAN_SLEEP_PIN, 0);

    gpio_set_direction(LED_STATUS_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(LED_STATUS_PIN, 1);
}

static void rfid_init(void)
{
    const rc522_start_args_t start_args = {
        .miso_io  = RFID_MISO_PIN,
        .mosi_io  = RFID_MOSI_PIN,
        .sck_io   = RFID_SCK_PIN,
        .sda_io   = RFID_SDA_PIN,
        .spi_host_id = RFID_SPI_HOST_ID
    };

    rc522_init(&start_args);
}

static void i2c_init(void)
{
    int i2c_master_port = I2C_HOST_ID;

    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = SDA_PIN,
        .scl_io_num = SCL_PIN,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 400000,
    };

    i2c_param_config(i2c_master_port, &conf);
    i2c_driver_install(i2c_master_port, conf.mode, 0, 0, 0);
}

static void main_task(void *args)
{
    while (true) {
        // is there a tag?
        uint8_t* sn = rc522_get_tag();

        if(sn)
        {
            led_update(PROCESSING);
            char card_id[9];
            sprintf(card_id, "%02x%02x%02x%02x", sn[0], sn[1], sn[2], sn[3]);
            ESP_LOGI(TAG, "Detected card %s", card_id);
            vTaskDelay(5000 / portTICK_PERIOD_MS);
            led_update(IDLE);

        }

        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }
    vTaskDelete(NULL);
}

void app_main(void)
{
    io_init();
    i2c_init();
    led_init();
    led_update(BOOT);
    rfid_init();
    sim7600_init();
    led_update(IDLE);

    xTaskCreate(main_task, "main_task", 4096, NULL, 6, NULL);

}
