#include "stdio.h"
#include "string.h"
#include "driver/gpio.h"

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
#include "touch.h"
#include "vehicle.h"
#include "lorawan.h"
#include "telemetry.h"
#include "wifi.h"

#include <time.h>
#include <sys/time.h>
#include "freertos/task.h"
#include "esp_sleep.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_check.h"
#include "esp_mac.h"
#include "esp_event.h"
#include "esp_netif.h"

#include "esp_intr_types.h"
#include "esp_intr_alloc.h"

static const char* TAG = "MaxBox";

maxbox_t mb = NULL;

static void flash_init(void)
{
    //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_read_mac(mb->base_mac, ESP_MAC_WIFI_STA));

    ESP_LOGI(TAG, "Loaded NVS. Base MAC address %02x%02x%02x%02x%02x%02x", 
                mb->base_mac[0], mb->base_mac[1], mb->base_mac[2], mb->base_mac[3], mb->base_mac[4], mb->base_mac[5]);

    nvs_handle_t my_handle;
    esp_err_t err = nvs_open("storage", NVS_READWRITE, &my_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error (%s) opening NVS handle!", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "Reading operator card list from NVS ...");
        nvs_get_i32(my_handle, "etag", &mb->etag);

        size_t required_size = sizeof(mb->operator_card_list);
        nvs_get_blob(my_handle, "op_card_list", mb->operator_card_list, &required_size);
        if (err == ESP_OK)
        {
            ESP_LOGI(TAG, "Loaded etag: %i", mb->etag);
            int i;
            for (i=0; i<MAX_OPERATOR_CARDS; i++)
            {
                if (strcmp(mb->operator_card_list[i], "voidvoid") != 0)
                {
                    ESP_LOGI(TAG, "Operator card %i: %s", i+1, mb->operator_card_list[i]);
                }
            }
        }
        else if (err == ESP_ERR_NVS_NOT_FOUND)
        {
            ESP_LOGI(TAG, "No etag value written in flash");
        }
        else
        {
            ESP_LOGE(TAG, "Error (%s) reading flash", esp_err_to_name(err));
        }
    }
}


static void io_init(void)
{
    gpio_set_direction(LED_STATUS_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(LED_STATUS_PIN, 1);
}

static void core1init(void* pvParameter)
{
    // HACK: we have to initialise some tasks pinned to the second core. Interrupts are generally assigned to
    // the core on which the task runs and we run out of them on core 0, hence manually initialising application task
    // interrupts on core 1. We choose the routines that are fast to initialise so the LED feedback is still vaguely
    // correct

    touch_init();
    vehicle_init();
    lorawan_init();

    vTaskDelete(NULL);
}

void app_main(void)
{
    mb = calloc(1, sizeof(struct maxbox));
    mb->tel = calloc(1, sizeof(telemetry_t));

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    io_init();
    flash_init();
    telemetry_init();
    led_init();
    sim7600_init();
    wifi_init();
    wifi_connect();
    xTaskCreatePinnedToCore(core1init, "core1init", 1024 * 4, (void* )0, 3, NULL, 1);

    led_update(LED_IDLE); // boot complete
    ESP_LOGI(TAG, "Boot complete");
}
