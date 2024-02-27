#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "driver/uart.h"
#include "string.h"
#include "driver/gpio.h"
#include "esp_sleep.h"
#include "driver/rtc_io.h"
#include "esp_modem_api.h"
#include "esp_event.h"
#include "sdkconfig.h"
#include "ctype.h"

#include "esp_netif.h"
#include "esp_netif_ppp.h"

#include "maxbox_defines.h"

#define BUF_SIZE 1024

esp_modem_dce_t *dce;
esp_netif_t *esp_netif;

#define CHECK_ERR(cmd, success_action)  do {    \
        esp_err_t ret = cmd;                    \
        if (ret == ESP_OK) {                    \
            success_action;                     \
        } else {                                \
            ESP_LOGE(TAG, "Failed with %s", ret == ESP_ERR_TIMEOUT ? "TIMEOUT":"ERROR");  \
        } } while (0)


const char *TAG = "MaxBox-SIM7600";

static void config_gpio()
{
    gpio_set_direction(SIM_RESET_PIN, GPIO_MODE_OUTPUT);
    gpio_set_direction(SIM_PWRKEY_PIN, GPIO_MODE_OUTPUT);
    gpio_set_direction(SIM_STATUS_PIN, GPIO_MODE_INPUT);

    gpio_set_level(SIM_RESET_PIN, 0);
    gpio_set_level(SIM_PWRKEY_PIN, 0);
}

static void power_on_modem(esp_modem_dce_t *dce)
{
     // Power on the modem 
    ESP_LOGI(TAG, "Sending SIM7600 power-on pulse");
    gpio_set_level(SIM_PWRKEY_PIN, 1);
    vTaskDelay(500 / portTICK_PERIOD_MS);
    gpio_set_level(SIM_PWRKEY_PIN, 0);
}

static void wait_for_sync(esp_modem_dce_t *dce, uint8_t count)
{
    ESP_LOGI(TAG, "Waiting for SIM7600 to boot for up to %is...", count);
    for (int i = 0; i < count; i++)
    {
        if(esp_modem_sync(dce) == ESP_OK)
        {
            ESP_LOGI(TAG, "SIM7600 UART OK");
            return;
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

static void gnss_start()
{
    char data[BUF_SIZE];
    ESP_LOGI(TAG, "Turning GPS on");

    // Turn GPS off first and wait 2s before restarting - there might be an error if not
    // CHECK_ERR(esp_modem_at(dce, "AT+CGPS=0", data, 500), ESP_LOGI(TAG, "OK. %s", data));
    vTaskDelay(pdMS_TO_TICKS(3000));
    CHECK_ERR(esp_modem_at(dce, "AT+CGPS=1,1", data, 500), ESP_LOGI(TAG, "OK. %s", data));
    ESP_LOGI(TAG, "GPS enabled");
}

static void gnss_stop()
{
    char data[BUF_SIZE];
    ESP_LOGI(TAG, "Turning GPS off");
    CHECK_ERR(esp_modem_at(dce, "AT+CGPS=0", data, 500), ESP_LOGI(TAG, "OK. %s", data));
    ESP_LOGI(TAG, "GPS disabled");
}

static float nmea_to_decimal(const char *lat_long)
{
  char deg[4] = {0};
  char *dot, *min;
  int len;
  float dec = 0;

  if ((dot = strchr(lat_long, '.')))
  {                                         // decimal point was found
    min = dot - 2;                          // mark the start of minutes 2 chars back
    len = min - lat_long;                   // find the length of degrees
    strncpy(deg, lat_long, len);            // copy the degree string to allow conversion to float
    dec = atof(deg) + atof(min) / 60;       // convert to float
  }
  return dec;
}

static void process_gngns_string(char *buf)
{
    // Example string: $GNGNS,223254.00,5156.126739,N,00629.13328,W,AAA,10,1.0,18.9,46.0,,,V*49
    char *p;
    if (!(p = strstr(buf, "$GNGNS"))) return;
    if (!(p = strchr(p, ','))) return;
    if (!(p = strchr(++p, ','))) return; // jump over UTC time
    mb->tel->gnss_latitude = nmea_to_decimal(++p);
    if (!(p = strchr(p, ','))) return;
    if (*(++p) == 'S') mb->tel->gnss_latitude = -mb->tel->gnss_latitude;
    if (!(p = strchr(p, ','))) return;
    mb->tel->gnss_longitude = nmea_to_decimal(++p);
    if (!(p = strchr(p, ','))) return;
    if (*(++p) == 'W') mb->tel->gnss_longitude = -mb->tel->gnss_longitude;
    if (!(p = strchr(p, ','))) return; // jump over mode indicator
    if (!(p = strchr(++p, ','))) return;
    mb->tel->gnss_nosats = atoi(++p);
    if (!(p = strchr(p, ','))) return;
    mb->tel->gnss_hdop = atof(++p) * 3.0; //    HACK: HDoP isn't the same as horizontal precision, but for this
                                          //    application we multiply by 3 to get a rough horizontal range

    mb->tel->gnss_updated_ts = box_timestamp();
}

void gnss_task(void *args)
{
    char data[BUF_SIZE];

    gnss_start();

    // $GNGNS string only, every 10 seconds
    CHECK_ERR(esp_modem_at(dce, "AT+CGPSINFOCFG=10,256", data, 500), ESP_LOGI(TAG, "OK. %s", data));

    while (true) { // TODO: change this condition to detect low-power flag
        /*
        HACK: CGPSINFOCFG seems to be the only way to get multi-constellation NMEA strings out of
        the SIM7600, and this automatically outputs every N seconds. There doesn't seem to be any way
        to register a callback to the UART terminal with the esp_modem C API, so we use the new "raw"
        API to send a blank string to the modem and read the response with a timeout
        */
        esp_modem_at_raw(dce, "", data, "", "", 12000);
        ESP_LOGD(TAG, "Raw GNSS string: %s", data);
        process_gngns_string(data);

        vTaskDelay(9000 / portTICK_PERIOD_MS);
    }

    gnss_stop();

    vTaskDelete(NULL);
}

esp_err_t sim7600_init()
{
    esp_modem_dce_config_t dce_config = ESP_MODEM_DCE_DEFAULT_CONFIG("internet");
    esp_modem_dte_config_t dte_config = ESP_MODEM_DTE_DEFAULT_CONFIG();
    esp_netif_config_t netif_ppp_config = ESP_NETIF_DEFAULT_PPP();
    esp_netif = esp_netif_new(&netif_ppp_config);
    assert(esp_netif);

    dte_config.uart_config.tx_io_num = SIM_TXD_PIN;
    dte_config.uart_config.rx_io_num = SIM_RXD_PIN;
    dte_config.uart_config.rx_buffer_size = 512;
    dte_config.uart_config.tx_buffer_size = 512;
    dte_config.uart_config.event_queue_size = 30;
    dte_config.task_stack_size = 4096;
    dte_config.task_priority = 5;
    dte_config.dte_buffer_size = 512;

    ESP_LOGI(TAG, "Initializing esp_modem for SIM7600...");
    dce = esp_modem_new_dev(ESP_MODEM_DCE_SIM7600, &dte_config, &dce_config, esp_netif);
    assert(dce);

    // Power on the modem 
    config_gpio();
    power_on_modem(dce);

    wait_for_sync(dce, 15);

    xTaskCreate(gnss_task, "gnss_task", 8192, NULL, 4, NULL);

    return ESP_OK;
}

void sim7600_destroy()
{
    esp_modem_destroy(dce);
    esp_netif_destroy(esp_netif);
}
