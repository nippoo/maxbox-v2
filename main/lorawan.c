#include "freertos/FreeRTOS.h"
#include "esp_event.h"
#include "driver/gpio.h"
#include "nvs_flash.h"
#include "esp_log.h"

#include "ttn.h"

#include "maxbox_defines.h"
#include "telemetry.h"
#include "lorawan.h"

#include "esp_intr_types.h"

static const char* TAG = "MaxBox-LoRaWAN";

void lorawan_rx_callback(const uint8_t* message, size_t length, ttn_port_t port)
{
    ESP_LOGI(TAG, "Message of %d bytes received", length, port);
}

void lorawan_init_task(void* arg)
{
    // Generate devEUI from HW MAC by padding middle two bytes with FF
    char deveui_string[17];

    sprintf(deveui_string, "%02x%02x%02x%02x%02x%02x%02x%02x",
            mb->base_mac[0], mb->base_mac[1], mb->base_mac[2], 0xFF, 0xFF, mb->base_mac[3], mb->base_mac[4], mb->base_mac[5]);

    ESP_LOGI(TAG, "DevEUI (generated): %s", deveui_string);

    // Initialize the GPIO ISR handler service
    gpio_install_isr_service(ESP_INTR_FLAG_IRAM);

    // Initialize SPI bus
    spi_bus_config_t spi_bus_config = {
        .miso_io_num = LORA_SPI_MISO_PIN,
        .mosi_io_num = LORA_SPI_MOSI_PIN,
        .sclk_io_num = LORA_SPI_SCLK_PIN,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .intr_flags = ESP_INTR_FLAG_LOWMED,
    };
    spi_bus_initialize(LORA_SPI_HOST_ID, &spi_bus_config, LORA_SPI_DMA_CHAN);

    // Initialize TTN
    ttn_init();

    // Configure the SX127x pins
    ttn_configure_pins(LORA_SPI_HOST_ID, LORA_NSS_PIN, LORA_RXTX_PIN, LORA_RST_PIN, LORA_DIO0_PIN, LORA_DIO1_PIN);

    // Register callback for received messages
    ttn_on_message(lorawan_rx_callback);

    ttn_set_adr_enabled(false);
    ttn_set_data_rate(CONFIG_LORAWAN_DATARATE);
    ttn_set_max_tx_pow(14);

    ESP_LOGI(TAG, "Joining");
    while (!mb->lorawan_joined) {
        if (ttn_join_with_keys(deveui_string, "0000000000000000", CONFIG_LORAWAN_APPKEY)) {
            ESP_LOGI(TAG, "Joined");

            ttn_set_adr_enabled(false);
            ttn_set_data_rate(CONFIG_LORAWAN_DATARATE);
            ttn_set_max_tx_pow(14);
            mb->lorawan_joined = true;
        } else {
            ESP_LOGE(TAG, "Join failed. Waiting to retry");
        }
        vTaskDelay(LORA_JOIN_RETRY_INTERVAL_MS / portTICK_PERIOD_MS);
    }
    vTaskDelete(NULL);
}
