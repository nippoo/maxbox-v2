/* Vehicle class: currently interacts with Nissan Leaf / e-NV200
*/
#pragma once

#include "driver/twai.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief FreeRTOS CAN bus receive task
 */
void can_receive_task(void *arg);

/**
 * @brief Initialize vehicle CAN bus communications.
 * @param vehicle Vehicle struct to be updated when CAN bus wakes
 * @return ESP_OK on success
 */
esp_err_t vehicle_init();

/**
 * @brief Lock / unlock vehicle doors, depending on mb->lock_desired
 */
void vehicle_un_lock();

#ifdef __cplusplus
}
#endif