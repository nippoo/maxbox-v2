/* HTTP class
*/
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef void(*rest_callback_t)(char*);

typedef struct {
    char *url;                   /*<! URL to POST to */
    char data[1023];             /*<! JSON data to send */
    rest_callback_t callback;    /*<! callback function */
    bool alert_on_error;         /*<! signal error if request fails */       
} rest_request_t;

/**
 * @brief Send HTTP request.
 */
void http_send(char* card_id);

#ifdef __cplusplus
}
#endif