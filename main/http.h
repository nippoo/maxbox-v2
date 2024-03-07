/* HTTP class
*/
#pragma once

#include "maxbox_defines.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void(*rest_callback_t)(char*);

struct rest_request{
    char *url;                   /*<! URL to POST to */
    char data[1023];             /*<! JSON data to send */
    rest_callback_t callback;    /*<! callback function */
    box_event_t box_event;       /*<! EVT_TOUCHED or EVT_TELEMETRY */
};

typedef struct rest_request* rest_request_t;

/**
 * @brief Send HTTP request. If card_id is NULL then this is a telemetry request.
 */
void http_send(char* card_id);

#ifdef __cplusplus
}
#endif