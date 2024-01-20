#include "freertos/FreeRTOS.h"
#include "esp_event.h"
#include "driver/gpio.h"
#include "nvs_flash.h"

#include "ttn.h"

#include "maxbox_defines.h"

static const char* TAG = "MaxBox-LoRaWAN";

const char *appEui = "0000000000000000";


// Pins and other resources
#define TTN_SPI_HOST      SPI3_HOST
#define TTN_SPI_DMA_CHAN  SPI_DMA_DISABLED
#define TTN_PIN_SPI_SCLK  35
#define TTN_PIN_SPI_MOSI  48
#define TTN_PIN_SPI_MISO  47
#define TTN_PIN_NSS       36
#define TTN_PIN_RXTX      TTN_NOT_CONNECTED
#define TTN_PIN_RST       37
#define TTN_PIN_DIO0      13
#define TTN_PIN_DIO1      14

#define TX_INTERVAL 30
static uint8_t msgData[] = "Hello, world";


void lorawan_send(void* pvParameter)
{
    while (1) {
        printf("Sending message...\n");
        ttn_response_code_t res = ttn_transmit_message(msgData, sizeof(msgData) - 1, 1, false);
        printf(res == TTN_SUCCESSFUL_TRANSMISSION ? "Message sent.\n" : "Transmission failed.\n");

        vTaskDelay(TX_INTERVAL * pdMS_TO_TICKS(1000));
    }
}

void lorawan_rx_callback(const uint8_t* message, size_t length, ttn_port_t port)
{
    printf("Message of %d bytes received on port %d:", length, port);
    for (int i = 0; i < length; i++)
        printf(" %02x", message[i]);
    printf("\n");
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
        .miso_io_num = TTN_PIN_SPI_MISO,
        .mosi_io_num = TTN_PIN_SPI_MOSI,
        .sclk_io_num = TTN_PIN_SPI_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1
    }; 
    err = spi_bus_initialize(TTN_SPI_HOST, &spi_bus_config, TTN_SPI_DMA_CHAN);
    ESP_ERROR_CHECK(err);

    // Initialize TTN
    ttn_init();

    // Configure the SX127x pins
    ttn_configure_pins(TTN_SPI_HOST, TTN_PIN_NSS, TTN_PIN_RXTX, TTN_PIN_RST, TTN_PIN_DIO0, TTN_PIN_DIO1);

    // The below line can be commented after the first run as the data is saved in NVS
    ttn_provision(devEui, appEui, appKey);

    // Register callback for received messages
    ttn_on_message(lorawan_rx_callback);

    // ttn_set_adr_enabled(false);
    // ttn_set_data_rate(TTN_DR_US915_SF7);
    // ttn_set_max_tx_pow(14);

    printf("Joining...\n");
    if (ttn_join())
    {
        printf("Joined.\n");
        xTaskCreate(lorawan_send, "lorawan_send", 1024 * 4, (void* )0, 3, NULL);
        return ESP_OK;
    }
    else
    {
        printf("Join failed. Goodbye\n");
        return ESP_FAIL;
    }
}
