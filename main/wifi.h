/* WiFi class
*/
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize network class
 */
void wifi_init();

/**
 * @brief Connect to configured WiFi network
 */
void wifi_connect();

/**
 * @brief Disconnect from WiFi
 */
void wifi_disconnect();

#ifdef __cplusplus
}
#endif