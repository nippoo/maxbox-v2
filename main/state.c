#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "nvs_flash.h"
#include "esp_mac.h"
#include "esp_log.h"
#include "string.h"

#include "maxbox_defines.h"
#include "led.h"
#include "wifi.h"
#include "telemetry.h"

static const char* TAG = "MaxBox-STATE";

static EventGroupHandle_t s_box_event_group;

#define BOOTING_BIT              BIT0
#define BOOTING_DONE_BIT         BIT1
#define TOUCHED_BIT              BIT2
#define TOUCHED_DONE_BIT         BIT3
#define TELEMETRY_BIT            BIT4
#define TELEMETRY_DONE_BIT       BIT5
#define FW_UPDATING_BIT          BIT6
#define FW_UPDATING_DONE_BIT     BIT7

void mb_begin_event(box_event_t box_event)
{
    EventBits_t bits = 0;

    // List of blocking other events - state change not allowed until these events have finished
    switch (box_event) {
    case EVT_TOUCHED:
        ESP_LOGI(TAG, "State change requested: TOUCH");
        bits = (BOOTING_BIT | TOUCHED_BIT | FW_UPDATING_BIT);
        break;
    case EVT_TELEMETRY:
        ESP_LOGI(TAG, "State change requested: TELEMETRY");
        bits = (BOOTING_BIT | TOUCHED_BIT | TELEMETRY_BIT | FW_UPDATING_BIT);
        break;
    case EVT_FIRMWARE:
        ESP_LOGI(TAG, "State change requested: FIRMWARE");
        bits = (BOOTING_BIT | TOUCHED_BIT | TELEMETRY_BIT | FW_UPDATING_BIT);
        break;
    case EVT_BOOT:
        ESP_LOGI(TAG, "State change requested: BOOT");
        break;
    default:
        break;
    }

    bits = xEventGroupGetBits(s_box_event_group) & bits;

    if (bits) {
        ESP_LOGI(TAG, "Waiting to process event");
        bits = xEventGroupWaitBits(s_box_event_group, (bits << 1), pdFALSE, pdTRUE, MAX_EVENT_TIMEOUT_MS / portTICK_PERIOD_MS);
    }

    switch (box_event) {
    case EVT_TOUCHED:
        xEventGroupClearBits(s_box_event_group, TOUCHED_DONE_BIT);
        xEventGroupSetBits(s_box_event_group, TOUCHED_BIT);
        led_update(LED_TOUCH);
        break;
    case EVT_TELEMETRY:
        xEventGroupClearBits(s_box_event_group, TELEMETRY_DONE_BIT);
        xEventGroupSetBits(s_box_event_group, TELEMETRY_BIT);
        wifi_connect();
        break;
    case EVT_FIRMWARE:
        xEventGroupClearBits(s_box_event_group, FW_UPDATING_DONE_BIT);
        xEventGroupSetBits(s_box_event_group, FW_UPDATING_BIT);
        led_update(LED_FIRMWARE);
        break;
    case EVT_BOOT:
        xEventGroupClearBits(s_box_event_group, BOOTING_DONE_BIT);
        xEventGroupSetBits(s_box_event_group, BOOTING_BIT);
        led_update(LED_BOOT);
        break;
    default:
        break;
    }
}

void mb_complete_event(box_event_t box_event, event_return_t return_status)
{
    switch (box_event) {
    case EVT_TOUCHED:
        wifi_disconnect();
        switch (return_status) {
        case BOX_LOCKED:
            led_update(LED_LOCKED);
            vTaskDelay(1500 / portTICK_PERIOD_MS);
            led_update(LED_IDLE);
            break;
        case BOX_UNLOCKED:
            led_update(LED_UNLOCKED);
            vTaskDelay(1500 / portTICK_PERIOD_MS);
            led_update(LED_IDLE);
            break;
        case BOX_DENY:
            led_update(LED_DENY);
            vTaskDelay(1500 / portTICK_PERIOD_MS);
            led_update(LED_IDLE);
            break;
        case BOX_ERROR:
            led_update(LED_ERROR);
            vTaskDelay(4000 / portTICK_PERIOD_MS);
            led_update(LED_IDLE);
            break;
        default:
            break;
        }
        xEventGroupClearBits(s_box_event_group, TOUCHED_BIT);
        xEventGroupSetBits(s_box_event_group, TOUCHED_DONE_BIT);
        break;
    case EVT_TELEMETRY:
        if (!(xEventGroupGetBits(s_box_event_group) & TOUCHED_BIT)) {
            wifi_disconnect();
        }
        switch (return_status) {
        case BOX_LOCKED:
            led_update(LED_LOCKED);
            vTaskDelay(1500 / portTICK_PERIOD_MS);
            led_update(LED_IDLE);
            break;
        case BOX_UNLOCKED:
            led_update(LED_UNLOCKED);
            vTaskDelay(1500 / portTICK_PERIOD_MS);
            led_update(LED_IDLE);
            break;
        default:
            break;
        }
        xEventGroupClearBits(s_box_event_group, TELEMETRY_BIT);
        xEventGroupSetBits(s_box_event_group, TELEMETRY_DONE_BIT);
        break;
    case EVT_BOOT:
        led_update(LED_IDLE);
        xEventGroupClearBits(s_box_event_group, BOOTING_BIT);
        xEventGroupSetBits(s_box_event_group, BOOTING_DONE_BIT);
        break;
    case EVT_FIRMWARE: // note: this will only happen if the firmware update is unsuccessful, otherwise the box will reboot
        xEventGroupClearBits(s_box_event_group, FW_UPDATING_BIT);
        xEventGroupSetBits(s_box_event_group, FW_UPDATING_DONE_BIT);
        break;
    default:
        break;
    }
}

void state_init()
{
    s_box_event_group = xEventGroupCreate();
}