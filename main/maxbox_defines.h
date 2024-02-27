#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "driver/spi_master.h"
#include "esp_timer.h"

// User config

#define TAG_CHECK_INTERVAL_MS       	500

#define LORA_TX_INTERVAL_MS             240000
#define CONFIG_LORAWAN_DATARATE         TTN_DR_EU868_SF8

#define CONFIG_NIGHT_MODE_THRESHOLD_LUX 1000

#define MAX_OPERATOR_CARDS          	32

// GPIO

#define RFID_MISO_PIN   42
#define RFID_MOSI_PIN   41
#define RFID_SCK_PIN    40
#define RFID_SDA_PIN    38
#define RFID_NRSTPD_PIN 1
#define RFID_IRQ_PIN    39

#define CAN_TX_PIN      8
#define CAN_RX_PIN      18
#define CAN_SLEEP_PIN   2

#define SIM_STATUS_PIN  15
#define SIM_RESET_PIN   17
#define SIM_PWRKEY_PIN  16
#define SIM_RXD_PIN     6
#define SIM_TXD_PIN     4
#define SIM_RI_PIN      5
#define SIM_DTR_PIN     7

#define LORA_SPI_DMA_CHAN  SPI_DMA_DISABLED
#define LORA_SPI_SCLK_PIN  35
#define LORA_SPI_MOSI_PIN  48
#define LORA_SPI_MISO_PIN  47
#define LORA_NSS_PIN       36
#define LORA_RXTX_PIN      TTN_NOT_CONNECTED
#define LORA_RST_PIN       37
#define LORA_DIO0_PIN      13
#define LORA_DIO1_PIN      14

#define SDA_PIN         11
#define SCL_PIN         12

#define ONEWIRE_PIN     9

#define LED_STATUS_PIN  46

#define VBAT_ADC_CHANNEL ADC_CHANNEL_9

#define I2C_HOST_ID     0
#define RFID_SPI_HOST_ID     SPI2_HOST
#define LORA_SPI_HOST_ID     SPI3_HOST

// WiFi/HTTP
#define ESP_WIFI_SSID      CONFIG_ESP_WIFI_SSID
#define ESP_WIFI_PASS      CONFIG_ESP_WIFI_PASSWORD
#define API_SECRET         CONFIG_MAXBOX_API_SECRET
#define FW_VERSION         CONFIG_MAXBOX_FW_VERSION

#define MAX_WIFI_RETRY           3
#define MAX_HTTP_RECV_BUFFER        512
#define MAX_HTTP_OUTPUT_BUFFER      2048
#define MAX_WIFI_WAIT_MS                 5000 // maximum time to wait for wifi connection

#define box_timestamp() esp_timer_get_time()/1000000

// Shared data structs

typedef struct {
	int8_t doors_locked;                   /*<! 1 = doors locked, 0 = doors unlocked */
    int32_t doors_updated_ts;              /*<! Box timestamp doors last updated, in seconds */
    int32_t odometer_miles;                /*<! current odometer reading, in miles */
    int32_t odometer_updated_ts;           /*<! Box timestamp odometer last updated, in seconds */
    float aux_battery_voltage;             /*<! standby battery voltage, from ADC */
    float soc_percent;                     /*<! HV state of charge, in percent */
    int32_t soc_updated_ts;                /*<! Box timestamp SoC was last updated, in seconds */
    float gnss_latitude;                   /*<! GNSS decimal latitude */
    float gnss_longitude;                  /*<! GNSS decimal longitude */
    float gnss_hdop;                       /*<! GNSS horizontal position uncertainty */
    int8_t gnss_nosats;                    /*<! GNSS number of satellites in use */
    int32_t gnss_updated_ts;               /*<! Box timestamp GNSS position last updated, in seconds */
    int8_t tyre_pressure_fl;               /*<! Front left tyre pressure */
    int8_t tyre_pressure_fr;               /*<! Front right tyre pressure */
    int8_t tyre_pressure_rl;               /*<! Rear left tyre pressure */
    int8_t tyre_pressure_rr;               /*<! Rear right tyre pressure */
    int32_t tp_updated_ts;                 /*<! Tyre pressure last updated, in seconds */
    uint8_t soh_percent;                   /*<! HV battery SoH, percent */
    int32_t soh_updated_ts;                /*<! SoH last updated, in seconds */
    char ibutton_id[17];                   /*<! ID of iButton currently attached */
} telemetry_t;

struct maxbox {
	telemetry_t* tel;
    uint8_t base_mac[6];                                /*<! HW MAC address */
    int32_t etag;                                       /*<! etag for sequential operator card list update */
    char operator_card_list[MAX_OPERATOR_CARDS][9];     /*<! List of operator card IDs */
    bool lock_desired;                                  /*<! Desired lock status (0: unlocked, 1: locked) */
};

typedef struct maxbox* maxbox_t;

extern maxbox_t mb;

#ifdef __cplusplus
}
#endif