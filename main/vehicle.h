/* Vehicle class: currently interacts with Nissan Leaf / e-NV200
*/
#pragma once

#include "driver/twai.h"
#include "maxbox_defines.h"

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
void vehicle_init();

/**
 * @brief Lock / unlock vehicle doors, depending on mb->lock_desired
 */
event_return_t vehicle_un_lock();

#ifdef __cplusplus
}
#endif