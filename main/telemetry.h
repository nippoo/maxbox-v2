/* Telemetry class: formats and manages vehicle and box telemetry data
*/
#pragma once

#include "driver/twai.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Format LoRaWAN telemetry packet contents
 */
void lora_format_telemetry(char *lm);

/**
 * @brief Initialize telemetry and box monitoring
 */
esp_err_t telemetry_init();

#ifdef __cplusplus
}
#endif