#include "freertos/FreeRTOS.h"
#include "esp_event.h"
#include "driver/gpio.h"
#include "nvs_flash.h"
#include "esp_log.h"

#include "ttn.h"

#include "maxbox_defines.h"
#include "telemetry.h"
#include "lorawan.h"

static const char* TAG = "MaxBox-LoRaWAN";

// static uint8_t msgData[] = "Hello, world";

void lorawan_send(void* pvParameter)
{
    while (1) {

        print_all_telemetry();
        uint8_t lora_telemetry_message[19] = {0};
        lora_format_telemetry(lora_telemetry_message);

        ESP_LOGI(TAG, "Sending LoRaWAN message");
        ttn_response_code_t res = ttn_transmit_message(lora_telemetry_message, sizeof(lora_telemetry_message) - 1, 1, false);
        if(res == TTN_SUCCESSFUL_TRANSMISSION)
            {
                ESP_LOGI(TAG, "Message sent");
            } else
            {
                ESP_LOGE(TAG, "Message sending failed");
            }

        vTaskDelay(LORA_TX_INTERVAL_MS / portTICK_PERIOD_MS);
    }
}

void lorawan_rx_callback(const uint8_t* message, size_t length, ttn_port_t port)
{
    ESP_LOGI(TAG, "Message of %d bytes received", length, port);
}

esp_err_t lorawan_init(void)
{
    esp_err_t err;
    // Initialize the GPIO ISR handler service
    err = gpio_install_isr_service(ESP_INTR_FLAG_IRAM);
    ESP_ERROR_CHECK(err);
    
    // Initialize the NVS (non-volatile storage) for saving and restoring the keys
    err = nvs_flash_init();
    ESP_ERROR_CHECK(err);

    // Initialize SPI bus
    spi_bus_config_t spi_bus_config = {
        .miso_io_num = LORA_SPI_MISO_PIN,
        .mosi_io_num = LORA_SPI_MOSI_PIN,
        .sclk_io_num = LORA_SPI_SCLK_PIN,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1
    }; 
    err = spi_bus_initialize(LORA_SPI_HOST_ID, &spi_bus_config, LORA_SPI_DMA_CHAN);
    ESP_ERROR_CHECK(err);

    // Initialize TTN
    ttn_init();

    // Configure the SX127x pins
    ttn_configure_pins(LORA_SPI_HOST_ID, LORA_NSS_PIN, LORA_RXTX_PIN, LORA_RST_PIN, LORA_DIO0_PIN, LORA_DIO1_PIN);

    // Register callback for received messages
    ttn_on_message(lorawan_rx_callback);

    ttn_set_adr_enabled(false);
    ttn_set_data_rate(TTN_DR_EU868_SF12);
    ttn_set_max_tx_pow(14);

    ESP_LOGI(TAG, "Joining");
    if (ttn_join_with_keys(CONFIG_LORAWAN_DEVEUI, "0000000000000000", CONFIG_LORAWAN_APPKEY))
    {
        ESP_LOGI(TAG, "Joined");

        ttn_set_adr_enabled(false);
        ttn_set_data_rate(TTN_DR_EU868_SF12);
        ttn_set_max_tx_pow(14);

        xTaskCreate(lorawan_send, "lorawan_send", 1024 * 4, (void* )0, 3, NULL);
        return ESP_OK;
    }
    else
    {
        ESP_LOGE(TAG, "Join failed");
        return ESP_FAIL;
    }
}
