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

#include "wifi.h"

static const char* TAG = "MaxBox-WiFi";

// WiFi
#define ESP_WIFI_SSID      CONFIG_ESP_WIFI_SSID
#define ESP_WIFI_PASS      CONFIG_ESP_WIFI_PASSWORD
#define API_SECRET         CONFIG_MAXBOX_API_SECRET
#define FW_VERSION         CONFIG_MAXBOX_FW_VERSION

#define ESP_MAXIMUM_RETRY           3
#define MAX_HTTP_RECV_BUFFER        512
#define MAX_HTTP_OUTPUT_BUFFER      2048
#define MAX_WAIT_MS                 5000 // maximum time to wait for wifi connection

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;

#define WIFI_CONNECTED_BIT          BIT0 // we are connected to the AP with an IP
#define WIFI_FAIL_BIT               BIT1 // we failed to connect after the maximum amount of retries
#define WIFI_OPERATION_FINISHED_BIT BIT2 // we are not in the process of (dis)connecting

static int s_retry_num = 0;
static int desired_connection_state = 0;

extern int etag;
extern EventGroupHandle_t s_status_group;

extern char firmware_update_url[255];

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG, "WIFI_EVENT_STA_START");
        s_retry_num = 0;
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < ESP_MAXIMUM_RETRY && desired_connection_state == 1) {
            ESP_LOGI(TAG, "WIFI_STA_DISCONNECTED retry");
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        } else {
            if (s_retry_num >= ESP_MAXIMUM_RETRY)
            {
                xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
                ESP_LOGI(TAG, "WIFI_STA_DISCONNECTED too many retries");
            }
            else if (desired_connection_state == 0)
            {
                ESP_LOGI(TAG, "Disconnecting from wifi");
            }
            xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
            desired_connection_state = 0;
            esp_wifi_stop();
        }
        ESP_LOGI(TAG,"Not connected to the AP");

    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) { 
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

void wifi_init(void)
{
    ESP_LOGI(TAG, "ESP_WIFI_MODE_STA");
    desired_connection_state = 1;
    
    s_wifi_event_group = xEventGroupCreate();

    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = ESP_WIFI_SSID,
            .password = ESP_WIFI_PASS,
            /* Setting a password implies station will connect to all security modes including WEP/WPA.
             * However these modes are deprecated and not advisable to be used. Incase your Access point
             * doesn't support WPA2, these mode can be enabled by commenting below line */
         .threshold.authmode = WIFI_AUTH_WPA2_PSK,

            .pmf_cfg = {
                .capable = true,
                .required = false
            },
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
    xEventGroupSetBits(s_wifi_event_group, WIFI_OPERATION_FINISHED_BIT);

    ESP_LOGI(TAG, "WiFi init complete");
}

void wifi_connect()
{
    xEventGroupWaitBits(s_wifi_event_group,
            WIFI_OPERATION_FINISHED_BIT,
            pdFALSE,
            pdFALSE,
            20000/portTICK_PERIOD_MS);

    if (!(xEventGroupGetBits(s_wifi_event_group) & WIFI_CONNECTED_BIT)) // we're already connected
    {
        xEventGroupClearBits(s_wifi_event_group, WIFI_OPERATION_FINISHED_BIT);
        desired_connection_state = 1;
        s_retry_num = 0;

        xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        xEventGroupClearBits(s_wifi_event_group, WIFI_FAIL_BIT);

        esp_wifi_start();

        /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
         * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
        EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                pdFALSE,
                pdFALSE,
                MAX_WAIT_MS);

        /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
         * happened. */
        if (bits & WIFI_CONNECTED_BIT) {
            ESP_LOGI(TAG, "Connected to SSID:%s",
                     ESP_WIFI_SSID);
        } else if (bits & WIFI_FAIL_BIT) {
            ESP_LOGI(TAG, "Failed to connect to SSID:%s",
                     ESP_WIFI_SSID, ESP_WIFI_PASS);
        } else {
            ESP_LOGE(TAG, "UNEXPECTED EVENT");
        }
    }
    xEventGroupSetBits(s_wifi_event_group, WIFI_OPERATION_FINISHED_BIT);
}

void wifi_disconnect()
{
    ESP_LOGI(TAG, "Waiting for WiFi operations to complete...");
    xEventGroupWaitBits(s_wifi_event_group,
            WIFI_OPERATION_FINISHED_BIT,
            pdFALSE,
            pdFALSE,
            20000/portTICK_PERIOD_MS);
    ESP_LOGI(TAG, "WiFi operations complete, disconnecting");

    if ((xEventGroupGetBits(s_wifi_event_group) & WIFI_CONNECTED_BIT)) // 
    {
        xEventGroupClearBits(s_wifi_event_group, WIFI_OPERATION_FINISHED_BIT);
        desired_connection_state = 0;
        esp_wifi_stop();
        xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT);
        xEventGroupSetBits(s_wifi_event_group, WIFI_OPERATION_FINISHED_BIT);
    }

}
