/* LoRaWAN telemetry class
*/
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief FreeRTOS CAN bus receive task
 */
void can_receive_task(void *arg);

/**
 * @brief Initialize LoRaWAN and create telemetry task
 */
esp_err_t lorawan_init();

#ifdef __cplusplus
}
#endif