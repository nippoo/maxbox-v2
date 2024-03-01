#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "pthread.h"
#include "driver/gpio.h"
#include "string.h"

#include "touch.h"
#include "esp_log.h"

#include "rc522.h"
#include "led.h"
#include "vehicle.h"
#include "telemetry.h"
#include "http.h"

#include "maxbox_defines.h"

static const char* TAG = "MaxBox-touch";

void touch_handler(void *serial_no) // serial number is always 4 bytes long
{
    const uint8_t* sn = (uint8_t *) serial_no;
    led_update(LED_TOUCH);

    char card_id[9];
    sprintf(card_id, "%02x%02x%02x%02x", sn[0], sn[1], sn[2], sn[3]);

    ESP_LOGI(TAG, "Detected card %s", card_id);

    mb->lock_desired = !mb->lock_desired;

    // first let's check if this is a tag in our operator card list
    int i;
    for (i=0; i<MAX_OPERATOR_CARDS; i++)
    {
        if (strcmp(mb->operator_card_list[i], card_id) == 0)
        {
            ESP_LOGI(TAG, "Operator card detected");
            mb->lock_desired = !mb->lock_desired;
            vehicle_un_lock();
            break;
        }
    }

    http_send(card_id);
}

void touch_task(void *args)
{
    while (true) {
        // is there a tag?
        uint8_t* sn = rc522_get_tag();

        if(sn)
        {
            touch_handler(sn);
        }

        vTaskDelay(TAG_CHECK_INTERVAL_MS / portTICK_PERIOD_MS);
    }
    vTaskDelete(NULL);
}

void touch_init()
{
    // Power up the MFRC522
    gpio_set_direction(RFID_NRSTPD_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(RFID_NRSTPD_PIN, 1);

    const rc522_start_args_t start_args = {
        .miso_io  = RFID_MISO_PIN,
        .mosi_io  = RFID_MOSI_PIN,
        .sck_io   = RFID_SCK_PIN,
        .sda_io   = RFID_SDA_PIN,
        .spi_host_id = RFID_SPI_HOST_ID
    };

    rc522_init(&start_args);

    xTaskCreate(touch_task, "touch_task", 4096, NULL, 6, NULL);

}