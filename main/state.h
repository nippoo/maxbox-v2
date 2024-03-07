/* MaxBox state handling functions
*/
#pragma once

#include "maxbox_defines.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Request state change. Blocks until state change allowed.
 */
void set_state(box_state_t req_state);


/**
 * @brief Request state change. Blocks until state change allowed.
 */
void mb_begin_event(box_event_t box_event);

void mb_complete_event(box_event_t box_event, event_return_t return_status);

/**
 * @brief Setup state handling task
 */
void state_init();

#ifdef __cplusplus
}
#endif