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

#include <time.h>
#include <sys/time.h>
#include "freertos/task.h"
#include "esp_sleep.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_check.h"
#include "esp_mac.h"

static const char* TAG = "MaxBox";

maxbox_t mb = NULL;

static void io_init(void)
{
    gpio_set_direction(LED_STATUS_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(LED_STATUS_PIN, 1);
}

void app_main(void)
{
    mb = calloc(1, sizeof(struct maxbox));
    mb->tel = calloc(1, sizeof(telemetry_t));

    io_init();
    telemetry_init();
    led_init();
    touch_init();
    sim7600_init();
    vehicle_init();
    lorawan_init();

    led_update(LED_IDLE); // boot complete
    ESP_LOGI(TAG, "Boot complete");
}
