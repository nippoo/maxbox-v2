/* Telemetry class: formats and manages vehicle and box telemetry data
*/
#pragma once

#include "driver/twai.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Debug print all telemetry struct
 */
void print_all_telemetry();

/**
 * @brief Format LoRaWAN telemetry packet contents
 */
void lora_format_telemetry(uint8_t *lm);

/**
 * @brief Format LoRaWAN telemetry packet contents
 */
void json_format_telemetry(char *json_string, char* card_id);

/**
 * @brief Initialize telemetry and box monitoring
 */
esp_err_t telemetry_init();

#ifdef __cplusplus
}
#endif