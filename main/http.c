#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "maxbox_defines.h"

#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_http_client.h"
#include "esp_mac.h"
#include "esp_event.h"
#include "esp_ota_ops.h"
#include "esp_https_ota.h"
#include "esp_system.h"
#include "esp_tls.h"
#include "esp_crt_bundle.h"

#include "cJSON.h"

#include "wifi.h"
#include "http.h"
#include "telemetry.h"
#include "vehicle.h"
#include "flash.h"
#include "state.h"

static const char* TAG = "MaxBox-HTTP";

char firmware_update_url[255];

esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{
    static char *output_buffer;  // Buffer to store response of http request from event handler
    static int output_len;       // Stores number of bytes read
    switch (evt->event_id) {
    case HTTP_EVENT_ERROR:
        ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
        break;
    case HTTP_EVENT_ON_CONNECTED:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
        break;
    case HTTP_EVENT_HEADER_SENT:
        ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
        break;
    case HTTP_EVENT_ON_HEADER:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
        break;
    case HTTP_EVENT_ON_DATA:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
        /*
         *  Check for chunked encoding is added as the URL for chunked encoding used in this example returns binary data.
         *  However, event handler can also be used in case chunked encoding is used.
         */
        if (!esp_http_client_is_chunked_response(evt->client)) {
            // If user_data buffer is configured, copy the response into the buffer
            if (evt->user_data) {
                memcpy(evt->user_data + output_len, evt->data, evt->data_len);
            } else {
                if (output_buffer == NULL) {
                    output_buffer = (char *) malloc(esp_http_client_get_content_length(evt->client));
                    output_len = 0;
                    if (output_buffer == NULL) {
                        ESP_LOGE(TAG, "Failed to allocate memory for output buffer");
                        return ESP_FAIL;
                    }
                }
                memcpy(output_buffer + output_len, evt->data, evt->data_len);
            }
            output_len += evt->data_len;
        }

        break;
    case HTTP_EVENT_ON_FINISH:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
        if (output_buffer != NULL) {
            // Response is accumulated in output_buffer. Uncomment the below line to print the accumulated response
            // ESP_LOG_BUFFER_HEX(TAG, output_buffer, output_len);
            free(output_buffer);
            output_buffer = NULL;
        }
        output_len = 0;
        break;
    case HTTP_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
        int mbedtls_err = 0;
        esp_err_t err = esp_tls_get_and_clear_last_error(evt->data, &mbedtls_err, NULL);
        if (err != 0) {
            ESP_LOGI(TAG, "Last esp error code: 0x%x", err);
            ESP_LOGI(TAG, "Last mbedtls failure: 0x%x", mbedtls_err);
        }
        if (output_buffer != NULL) {
            free(output_buffer);
            output_buffer = NULL;
        }
        output_len = 0;
        break;
    case HTTP_EVENT_REDIRECT:
        ESP_LOGD(TAG, "HTTP_EVENT_REDIRECT");
        esp_http_client_set_header(evt->client, "From", "user@example.com");
        esp_http_client_set_header(evt->client, "Accept", "text/html");
        esp_http_client_set_redirection(evt->client);
        break;
    }
    return ESP_OK;
}

static esp_err_t _http_set_headers(esp_http_client_handle_t http_client)
{
    char mac_addr_string[13];
    char rendered_etag[10];

    sprintf(mac_addr_string, "%02x%02x%02x%02x%02x%02x",    mb->base_mac[0],
            mb->base_mac[1],
            mb->base_mac[2],
            mb->base_mac[3],
            mb->base_mac[4],
            mb->base_mac[5]);
    sprintf(rendered_etag, "%d", mb->etag);

    esp_http_client_set_header(http_client, "Accept", "application/json");
    esp_http_client_set_header(http_client, "Content-Type", "application/json");
    esp_http_client_set_header(http_client, "X-Carshare-Box-ID", mac_addr_string);
    esp_http_client_set_header(http_client, "X-Carshare-Box-Secret", API_SECRET);
    esp_http_client_set_header(http_client, "X-Carshare-Operator-Card-List-ETag", rendered_etag);
    esp_http_client_set_header(http_client, "X-Carshare-Firmware-Version", FW_VERSION);

    return ESP_OK;
}

event_return_t json_return_handler(char* result)
{
    event_return_t status = BOX_ERROR;

    cJSON *result_json = cJSON_Parse(result);

    if (cJSON_GetObjectItem(result_json, "operator_card_list")) {
        cJSON *card_list = cJSON_GetObjectItem(result_json, "operator_card_list");
        cJSON *recv_etag = cJSON_GetObjectItem(card_list, "etag");

        if (cJSON_IsNumber(recv_etag)) {
            if (recv_etag->valuedouble != mb->etag) {
                ESP_LOGI(TAG, "New etag is %d", recv_etag->valueint);

                cJSON *card;
                cJSON *op_cards = cJSON_GetObjectItem(card_list, "cards");

                int i = 0;
                cJSON_ArrayForEach(card, op_cards) {
                    char *cardid = card->valuestring;
                    ESP_LOGI(TAG, "Added card to operator list with id: %s", cardid);
                    strncpy(mb->operator_card_list[i], cardid, 9);
                    i++;
                }

                for (; i < MAX_OPERATOR_CARDS; i++) {
                    strcpy(mb->operator_card_list[i], "voidvoid");
                }

                mb->etag = recv_etag->valuedouble;

                flash_write_all();
            }
        }
    }

    // Optionally, there may be an action to manually lock or unlock the car remotely
    if (cJSON_GetObjectItem(result_json, "action")) {
        char *action = cJSON_GetObjectItem(result_json, "action")->valuestring;
        if (strcmp(action, "lock") == 0) {
            mb->lock_desired = 1;
            status = vehicle_un_lock();
        } else if (strcmp(action, "unlock") == 0) {
            mb->lock_desired = 0;
            status = vehicle_un_lock();
        } else if (strcmp(action, "reject") == 0) {
            status = BOX_DENY;
        }
    }

    // if(cJSON_GetObjectItem(result_json, "firmware_update_url"))
    // {
    //     char *fw_url = cJSON_GetObjectItem(result_json, "firmware_update_url")->valuestring;
    //     strcpy(firmware_update_url, fw_url);
    //     ESP_LOGI(TAG, "Firmware update detected, updating from URL %s", firmware_update_url);
    //     xEventGroupSetBits(s_status_group, FIRMWARE_UPDATING_BIT);
    //     xTaskCreate(firmware_update, "firmware_update", 8192, NULL, 5, NULL);
    // }

    cJSON_Delete(result_json);

    return status;
}

static void http_auth_rfid(void *rest_request)
{
    event_return_t status = BOX_ERROR;

    const rest_request_t request = rest_request;
    char local_response_buffer[MAX_HTTP_OUTPUT_BUFFER + 1] = {0};

    esp_http_client_config_t config = {
        .user_agent = "Carshare Box v2",
        .event_handler = _http_event_handler,
        .user_data = local_response_buffer,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };

    if (request->box_event == EVT_TOUCHED) {
        config.url = API_ENDPOINT_TOUCH;
    } else {
        config.url = API_ENDPOINT_TELEMETRY;
    }

    esp_http_client_handle_t client = esp_http_client_init(&config);

    ESP_LOGI(TAG, "URL is %s", config.url);
    ESP_LOGI(TAG, "POST DATA is %s", request->data);
    esp_http_client_set_method(client, HTTP_METHOD_POST);

    _http_set_headers(client);

    esp_http_client_set_post_field(client, request->data, strlen(request->data));
    esp_err_t err = esp_http_client_perform(client);

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "HTTP POST Status = %d, content_length = %d",
                 esp_http_client_get_status_code(client),
                 esp_http_client_get_content_length(client));

        ESP_LOGI(TAG, "Got data: %s", local_response_buffer);

        status = json_return_handler(local_response_buffer);

    } else {
        ESP_LOGE(TAG, "HTTP POST request failed: %s", esp_err_to_name(err));
        status = BOX_ERROR;
    }

    esp_http_client_cleanup(client);
    mb_complete_event(request->box_event, status);
    free(request);
    vTaskDelete(NULL);
}

void http_send(char* card_id)
{
    rest_request_t req = NULL;
    req = calloc(1, sizeof(struct rest_request));

    json_format_telemetry(req->data, card_id);

    if (card_id) {
        req->box_event = EVT_TOUCHED;
    } else {
        req->box_event = EVT_TELEMETRY;
    }

    xTaskCreate(&http_auth_rfid, "http_auth_rfid", 8192, req, 6, NULL);
}

void firmware_update(void* pxParameters)
{
    esp_err_t ota_finish_err = ESP_OK;

    ESP_LOGI(TAG, "Updating firmware from %s", firmware_update_url);

    esp_http_client_config_t config = {
        .url = firmware_update_url,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .keep_alive_enable = true,
    };

    esp_https_ota_config_t ota_config = {
        .http_config = &config,
        .http_client_init_cb = _http_set_headers,
    };

    esp_https_ota_handle_t https_ota_handle = NULL;
    esp_err_t err = esp_https_ota_begin(&ota_config, &https_ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ESP HTTPS OTA Begin failed");
        goto ota_end;
    }

    esp_app_desc_t app_desc;
    err = esp_https_ota_get_img_desc(https_ota_handle, &app_desc);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_https_ota_read_img_desc failed");
        goto ota_end;
    }

    while (1) {
        err = esp_https_ota_perform(https_ota_handle);
        if (err != ESP_ERR_HTTPS_OTA_IN_PROGRESS) {
            break;
        }
        // esp_https_ota_perform returns after every read operation which gives user the ability to
        // monitor the status of OTA upgrade by calling esp_https_ota_get_image_len_read, which gives length of image
        // data read so far.
        ESP_LOGD(TAG, "Image bytes read: %d", esp_https_ota_get_image_len_read(https_ota_handle));
    }

    if (esp_https_ota_is_complete_data_received(https_ota_handle) != true) {
        // the OTA image was not completely received and user can customise the response to this situation.
        ESP_LOGE(TAG, "Complete data was not received.");
    } else {
        ota_finish_err = esp_https_ota_finish(https_ota_handle);
        if ((err == ESP_OK) && (ota_finish_err == ESP_OK)) {
            ESP_LOGI(TAG, "ESP_HTTPS_OTA upgrade successful. Rebooting ...");
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            esp_restart();
        } else {
            if (ota_finish_err == ESP_ERR_OTA_VALIDATE_FAILED) {
                ESP_LOGE(TAG, "Image validation failed, image is corrupted");
            }
            ESP_LOGE(TAG, "ESP_HTTPS_OTA upgrade failed 0x%x", ota_finish_err);
            goto ota_end;

        }
    }

ota_end:
    esp_https_ota_abort(https_ota_handle);
    ESP_LOGE(TAG, "ESP_HTTPS_OTA upgrade failed");

    vTaskDelete(NULL);
}
