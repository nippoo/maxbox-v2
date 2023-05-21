#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// User config

#define CONFIG_NIGHT_MODE_THRESHOLD_LUX 1000

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

#define LORA_MISO_PIN   47
#define LORA_MOSI_PIN   48
#define LORA_SCK_PIN    35
#define LORA_SDA_PIN    36
#define LORA_DIO1_PIN   14
#define LORA_RXE_PIN    21
#define LORA_BUSY_PIN   13

#define SDA_PIN         11
#define SCL_PIN         12

#define ONEWIRE_PIN     9

#define LED_STATUS_PIN  46

#define I2C_HOST_ID     0
#define RFID_SPI_HOST_ID     SPI2_HOST
#define LORA_SPI_HOST_ID     SPI3_HOST


// FreeRTOS task priority



#ifdef __cplusplus
}
#endif