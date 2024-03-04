/* Flash / Non Volatile Storage class
*/
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize network class
 */
void flash_init();

/**
 * @brief Write operator cards and etag to flash
 */
void flash_write_all();

#ifdef __cplusplus
}
#endif