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

static const char* TAG = "MaxBox-STATE";

// static pthread_mutex_t box_status_mux;


void mb_begin_event(box_event_t box_event)
{
    switch(box_event)
    {
    case EVT_TOUCHED:
        led_update(LED_TOUCH);
        wifi_connect();
        break;
    case EVT_TELEMETRY:
        wifi_connect();
        break;
    case EVT_FIRMWARE:
        led_update(LED_FIRMWARE);
        break;
    case EVT_BOOT:
        led_update(LED_BOOT);
        break;
    default:
        break;
    }
}

void mb_complete_event(box_event_t box_event, event_return_t return_status)
{
    switch(box_event)
    {
    case EVT_TOUCHED:
        wifi_disconnect();
        switch(return_status)
        {
        case BOX_LOCKED:
            led_update(LED_LOCKED);
            break;
        case BOX_UNLOCKED:
            led_update(LED_UNLOCKED);
            break;
        case BOX_DENY:
            led_update(LED_DENY);
            break;
        case BOX_ERROR:
            led_update(LED_ERROR);
            break;
        default:
            break;
        }
        break;
    case EVT_TELEMETRY:
        wifi_disconnect();
        switch(return_status)
        {
        case BOX_LOCKED:
            led_update(LED_LOCKED);
            break;
        case BOX_UNLOCKED:
            led_update(LED_UNLOCKED);
            break;
        default:
            break;
        }
        break;
    case EVT_BOOT:
        led_update(LED_IDLE);
    default:
        break;
    }
}

// static void state_watchdog_task(void *args)
// {
//     // Looks 

//     while (true)
//     {

//     }
//     vTaskDelete(NULL);
// }

// void set_state(box_state_t req_state)
// {
//     switch(mb->box_state)
//     {
//     case STATE_FW_UPDATE:
//         // wait 15 mins or reboot
//         break;

//     case STATE_IDLE_LOW_POWER:
//     case STATE_IDLE_HI_POWER:
//         break;

//     case STATE_TOUCHED:

//     }
//     if(pthread_mutex_lock(&box_status_mux) == 0)
//     {

//     }
// }

void state_init()
{
    return;
    // pthread_mutex_init(&box_status_mux, NULL);
    // xTaskCreate(state_watchdog_task, "state_watchdog", 4096, NULL, 6, NULL);
}