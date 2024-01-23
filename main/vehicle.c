#include "driver/twai.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "pthread.h"

#include "esp_log.h"

#include "maxbox_defines.h"
#include "vehicle.h"
#include "led.h"

static const char* TAG = "MaxBox-vehicle";

void can_receive_task(void *arg)
{
    // Receives and processes CAN bus data from Nissan E-NV200/Leaf and updates telemetry

    while (1) {
        twai_message_t msg;
        twai_receive(&msg, portMAX_DELAY);
        if (msg.identifier == 0x5c5)
        {
            mb->tel->odometer_miles = (msg.data[1] << 16) | (msg.data[2] << 8) | (msg.data[3]);
            mb->tel->odometer_updated_ts = box_timestamp();
        }
        else if (msg.identifier == 0x55b)
        {
            mb->tel->soc_percent = ((msg.data[0] << 2) | (msg.data[1] >> 6)) / 10;
            mb->tel->soc_updated_ts = box_timestamp();
        }
        else if (msg.identifier == 0x60d)
        {
            if (msg.data[2] == 0x18)
            {
                mb->tel->doors_locked = 1;
            }
            else
            {
                mb->tel->doors_locked = 0;
            }
            mb->tel->doors_updated_ts = box_timestamp();
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

    if(mb->lock_desired)
    {
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

    if(mb->lock_desired)
    {
        ESP_LOGI(TAG, "Car locked");
        led_update(LED_LOCKED);
    } else {
        ESP_LOGI(TAG, "Car unlocked");
        led_update(LED_UNLOCKED);
    }

    vTaskDelete(NULL);
}

esp_err_t vehicle_init()
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

    //Install CAN driver
    if (twai_driver_install(&g_config, &t_config, &f_config) == ESP_OK) {
        ESP_LOGI(TAG, "Driver installed");
    } else {
        ESP_LOGE(TAG, "Failed to install driver");
        return ESP_FAIL;
    }

    //Start CAN driver
    if (twai_start() == ESP_OK) {
        ESP_LOGI(TAG, "Driver started");
    } else {
        ESP_LOGE(TAG, "Failed to start driver");
        return ESP_FAIL;
    }

    xTaskCreatePinnedToCore(can_receive_task, "can_receive_task", 4096, NULL, 3, NULL, tskNO_AFFINITY);

    return ESP_OK;
}

void vehicle_un_lock()
{
    xTaskCreate(&un_lock, "un_lock", 8192, NULL, 2, NULL);
}
