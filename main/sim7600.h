#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize SIM7600 module UART and GPIO.
 * @return ESP_OK on success
 */
esp_err_t sim7600_init();

/**
 * @brief Destroy SIM7600 instance and free all resources
 */
void sim7600_destroy();

#ifdef __cplusplus
}
#endif