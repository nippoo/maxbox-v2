/* LED functions
*/
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {LED_BOOT, LED_IDLE, LED_TOUCH, LED_LOCKED, LED_UNLOCKED, LED_DENY, LED_ERROR, LED_FIRMWARE} led_status_t;

void led_init(void);
void led_update(led_status_t);
void led_task(void *args);

#ifdef __cplusplus
}
#endif