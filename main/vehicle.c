#include "driver/twai.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "pthread.h"

#include "esp_log.h"

#include "maxbox_defines.h"
#include "vehicle.h"
#include "led.h"

static const char* TAG = "MaxBox-vehicle";

static EventGroupHandle_t s_can_event_group;

#define CAN_SENDING_BIT          BIT0
#define CAN_SENDING_DONE_BIT     BIT1

void can_receive_task(void *arg)
{
    // Receives and processes CAN bus data from Nissan E-NV200/Leaf and updates telemetry

    while (1) {
        twai_message_t msg;
        twai_receive(&msg, portMAX_DELAY);
        if (msg.identifier == 0x5c5) {
            mb->tel->odometer_miles = (msg.data[1] << 16) | (msg.data[2] << 8) | (msg.data[3]);
            mb->tel->odometer_updated_ts = box_timestamp();
        } else if (msg.identifier == 0x55b) {
            mb->tel->soc_percent = ((msg.data[0] << 2) | (msg.data[1] >> 6)) / 10;
            mb->tel->soc_updated_ts = box_timestamp();
        } else if (msg.identifier == 0x60d) {
            if (msg.data[2] == 0x18) {
                mb->tel->doors_locked = 1;
            } else {
                mb->tel->doors_locked = 0;
            }
            mb->tel->doors_updated_ts = box_timestamp();
        } else if (msg.identifier == 0x385) {
            if (msg.data[2]) {
                mb->tel->tyre_pressure_fr = (msg.data[2] / 4);
            } else {
                mb->tel->tyre_pressure_fr = 63;
            }
            if (msg.data[3]) {
                mb->tel->tyre_pressure_fl = (msg.data[3] / 4);
            } else {
                mb->tel->tyre_pressure_fl = 63;
            }
            if (msg.data[4]) {
                mb->tel->tyre_pressure_rr = (msg.data[4] / 4);
            } else {
                mb->tel->tyre_pressure_rr = 63;
            }
            if (msg.data[5]) {
                mb->tel->tyre_pressure_rl = (msg.data[5] / 4);
            } else {
                mb->tel->tyre_pressure_rl = 63;
            }
            mb->tel->tp_updated_ts = box_timestamp();
        } else if (msg.identifier == 0x5b3) {
            mb->tel->soh_percent = (msg.data[1] >> 1);
            mb->tel->soh_updated_ts = box_timestamp();
        }
    }
    vTaskDelete(NULL);
}

static void send_can(twai_message_t message)
{
    twai_transmit(&message, pdMS_TO_TICKS(100));
}

static void un_lock()
{
    if (xEventGroupGetBits(s_can_event_group) & CAN_SENDING_BIT) {
        xEventGroupWaitBits(s_can_event_group,
                            CAN_SENDING_DONE_BIT,
                            pdTRUE,
                            pdFALSE,
                            20000 / portTICK_PERIOD_MS);
    }

    xEventGroupSetBits(s_can_event_group, CAN_SENDING_BIT);

    twai_message_t packet1;
    packet1.identifier = 0x745;
    packet1.data_length_code = 8;

    packet1.data[0] = 0x02;
    packet1.data[1] = 0x10;
    packet1.data[2] = 0x81;
    packet1.data[3] = 0xff;
    packet1.data[4] = 0xff;
    packet1.data[5] = 0xff;
    packet1.data[6] = 0xff;
    packet1.data[7] = 0xff;

    twai_message_t packet2;
    packet2.identifier = 0x745;
    packet2.data_length_code = 8;

    packet2.data[0] = 0x02;
    packet2.data[1] = 0x10;
    packet2.data[2] = 0xc0;
    packet2.data[3] = 0xff;
    packet2.data[4] = 0xff;
    packet2.data[5] = 0xff;
    packet2.data[6] = 0xff;
    packet2.data[7] = 0xff;

    twai_message_t packetlock;
    packetlock.identifier = 0x745;
    packetlock.data_length_code = 8;

    packetlock.data[0] = 0x04;
    packetlock.data[1] = 0x30;
    packetlock.data[2] = 0x07;
    packetlock.data[3] = 0x00;
    packetlock.data[4] = 0x01;
    packetlock.data[5] = 0xff;
    packetlock.data[6] = 0xff;
    packetlock.data[7] = 0xff;

    if (mb->lock_desired) {
        ESP_LOGI(TAG, "Transmitting lock CAN packet");
        packetlock.data[4] = 0x01;
    } else {
        ESP_LOGI(TAG, "Transmitting unlock CAN packet");
        packetlock.data[4] = 0x02;
    }

    send_can(packet1);
    vTaskDelay(200 / portTICK_PERIOD_MS);
    send_can(packet2);
    vTaskDelay(50 / portTICK_PERIOD_MS);
    send_can(packet2);
    vTaskDelay(50 / portTICK_PERIOD_MS);
    send_can(packet2);
    vTaskDelay(50 / portTICK_PERIOD_MS);
    send_can(packet2);
    vTaskDelay(50 / portTICK_PERIOD_MS);
    send_can(packet2);
    vTaskDelay(50 / portTICK_PERIOD_MS);
    send_can(packet2);
    vTaskDelay(50 / portTICK_PERIOD_MS);
    send_can(packet2);
    vTaskDelay(50 / portTICK_PERIOD_MS);
    send_can(packet2);
    vTaskDelay(50 / portTICK_PERIOD_MS);
    send_can(packet2);
    vTaskDelay(50 / portTICK_PERIOD_MS);
    send_can(packet2);
    vTaskDelay(50 / portTICK_PERIOD_MS);
    send_can(packet2);
    vTaskDelay(100 / portTICK_PERIOD_MS);
    send_can(packet1);
    vTaskDelay(100 / portTICK_PERIOD_MS);
    send_can(packet2);
    vTaskDelay(100 / portTICK_PERIOD_MS);
    send_can(packetlock);
    vTaskDelay(500 / portTICK_PERIOD_MS);
    send_can(packet1);

    xEventGroupClearBits(s_can_event_group, CAN_SENDING_BIT);
    xEventGroupSetBits(s_can_event_group, CAN_SENDING_DONE_BIT);

    vTaskDelete(NULL);
}

void vehicle_init()
{
    // TODO: Sleep CAN transmitter until required (to save power)
    // For the moment we just leave it awake all the time
    gpio_set_direction(CAN_SLEEP_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(CAN_SLEEP_PIN, 0);

    //Initialize configuration structures using macro initializers
    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(CAN_TX_PIN, CAN_RX_PIN, TWAI_MODE_NORMAL);
    twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS();
    twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    g_config.intr_flags = ESP_INTR_FLAG_LOWMED;

    twai_driver_install(&g_config, &t_config, &f_config);
    twai_start();

    s_can_event_group = xEventGroupCreate();

    xTaskCreatePinnedToCore(can_receive_task, "can_receive_task", 4096, NULL, 3, NULL, 1);
}

event_return_t vehicle_un_lock()
{
    xTaskCreate(&un_lock, "un_lock", 8192, NULL, 6, NULL);

    xEventGroupWaitBits(s_can_event_group,
                        CAN_SENDING_DONE_BIT,
                        pdTRUE,
                        pdFALSE,
                        20000 / portTICK_PERIOD_MS);

    if (mb->lock_desired) {
        return BOX_LOCKED;
    } else {
        return BOX_UNLOCKED;
    }
}
