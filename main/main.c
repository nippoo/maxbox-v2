#include "stdio.h"
#include "string.h"
#include "driver/gpio.h"

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "pthread.h"

#include "esp_log.h"

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
#include "flash.h"
#include "state.h"

#include <time.h>
#include <sys/time.h>
#include "freertos/task.h"
#include "esp_sleep.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_check.h"
#include "esp_event.h"
#include "esp_netif.h"

#include "esp_intr_types.h"
#include "esp_intr_alloc.h"

static const char* TAG = "MaxBox";

maxbox_t mb = NULL;

static void core1init(void* pvParameter)
{
    // HACK: we have to initialise some tasks pinned to the second core. Interrupts are generally assigned to
    // the core on which the task runs and we run out of them on core 0, hence manually initialising application task
    // interrupts on core 1. We choose the routines that are fast to initialise so the LED feedback is still vaguely
    // correct

    touch_init();
    vehicle_init();
    telemetry_init();

    mb_complete_event(EVT_BOOT, BOX_OK); // boot complete
    ESP_LOGI(TAG, "Boot complete");

    vTaskDelete(NULL);
}

void app_main(void)
{
    mb = calloc(1, sizeof(struct maxbox));
    mb->tel = calloc(1, sizeof(telemetry_t));

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    state_init();
    led_init();
    mb_begin_event(EVT_BOOT); // boot begin

    flash_init();
    sim7600_init();
    wifi_init();
    xTaskCreatePinnedToCore(core1init, "core1init", 1024 * 4, (void* )0, 3, NULL, 1);
}
