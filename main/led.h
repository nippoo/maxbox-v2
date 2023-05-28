/* LED functions
*/
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {BOOT, IDLE, HEARTBEAT, PROCESSING, LOCKED, UNLOCKED, DENY, ERROR, FIRMWARE} led_status_t;

void led_init(void);
void led_update(led_status_t);
void led_task(void *args);

#ifdef __cplusplus
}
#endif