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

void config_gpio()
{
    gpio_set_direction(SIM_RESET_PIN, GPIO_MODE_OUTPUT);
    gpio_set_direction(SIM_PWRKEY_PIN, GPIO_MODE_OUTPUT);
    gpio_set_direction(SIM_STATUS_PIN, GPIO_MODE_INPUT);

    gpio_set_level(SIM_RESET_PIN, 0);
    gpio_set_level(SIM_PWRKEY_PIN, 0);
}

void power_on_modem(esp_modem_dce_t *dce)
{
     // Power on the modem 
    ESP_LOGI(TAG, "Sending SIM7600 power-on pulse");
    gpio_set_level(SIM_PWRKEY_PIN, 1);
    vTaskDelay(500 / portTICK_PERIOD_MS);
    gpio_set_level(SIM_PWRKEY_PIN, 0);
}

void wait_for_sync(esp_modem_dce_t *dce, uint8_t count)
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

void check_connectivity(esp_modem_dce_t *dce)
{
    char data[BUF_SIZE];
    int rssi, ber;
    CHECK_ERR(esp_modem_get_signal_quality(dce, &rssi, &ber), ESP_LOGI(TAG, "OK. rssi=%d, ber=%d", rssi, ber));
    vTaskDelay(pdMS_TO_TICKS(1000));


    CHECK_ERR(esp_modem_at(dce, "AT+CPSMS?", data, 500), ESP_LOGI(TAG, "OK. %s", data));

    CHECK_ERR(esp_modem_at(dce, "AT+CGPSCOLD", data, 500), ESP_LOGI(TAG, "OK. %s", data));

    vTaskDelay(pdMS_TO_TICKS(1000));

    CHECK_ERR(esp_modem_at(dce, "AT+CPSI?", data, 500), ESP_LOGI(TAG, "OK. %s", data));             // Inquiring UE system information
    // CHECK_ERR(esp_modem_at(dce, "AT+CNACT=0,1", data, 500), ESP_LOGI(TAG, "OK. %s", data));         // Activate the APP network
    // CHECK_ERR(esp_modem_at(dce, "AT+SNPDPID=0", data, 500), ESP_LOGI(TAG, "OK. %s", data));         // Select PDP index for PING
    // CHECK_ERR(esp_modem_at(dce, "AT+SNPING4=\"8.8.8.8\",3,16,1000", data, 500), ESP_LOGI(TAG, "OK. %s", data));     // Send IPv4 PING
    // CHECK_ERR(esp_modem_at(dce, "AT+CNACT=0,0", data, 500), ESP_LOGI(TAG, "OK. %s", data));         // Deactivate the APP network

    CHECK_ERR(esp_modem_get_signal_quality(dce, &rssi, &ber), ESP_LOGI(TAG, "OK. rssi=%d, ber=%d", rssi, ber));
    CHECK_ERR(esp_modem_at(dce, "AT+CGPSINFO", data, 500), ESP_LOGI(TAG, "OK. %s", data));

    if (esp_modem_sms_txt_mode(dce, true) != ESP_OK || esp_modem_sms_character_set(dce) != ESP_OK) {
        ESP_LOGE(TAG, "Setting text mode or GSM character set failed");
        return;
    }
    else
    {
        ESP_LOGI(TAG, "Successfully set character mode. Now sending SMS");
    }

    ESP_LOGI(TAG, "Successfully set character mode. Now sending SMS");

    esp_err_t err = esp_modem_send_sms(dce, "+447533709265", "Hello, MaxBox world!");
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_modem_send_sms() failed with %d", err);
        return;
    }
    else
    {
        ESP_LOGI(TAG, "Successfully sent SMS");
    }

}

esp_err_t sim7600_init()
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    esp_modem_dce_config_t dce_config = ESP_MODEM_DCE_DEFAULT_CONFIG("internet");
    esp_modem_dte_config_t dte_config = ESP_MODEM_DTE_DEFAULT_CONFIG();
    esp_netif_config_t netif_ppp_config = ESP_NETIF_DEFAULT_PPP();
    esp_netif = esp_netif_new(&netif_ppp_config);
    assert(esp_netif);

    dte_config.uart_config.tx_io_num = SIM_TXD_PIN;
    dte_config.uart_config.rx_io_num = SIM_RXD_PIN;
    dte_config.uart_config.rx_buffer_size = 512;
    dte_config.uart_config.tx_buffer_size = 1024;
    dte_config.uart_config.event_queue_size = 30;
    dte_config.task_stack_size = 4096;
    dte_config.task_priority = 5;
    dte_config.dte_buffer_size = 256;

    ESP_LOGI(TAG, "Initializing esp_modem for SIM7600...");
    dce = esp_modem_new_dev(ESP_MODEM_DCE_SIM7600, &dte_config, &dce_config, esp_netif);
    assert(dce);

    // Power on the modem 
    config_gpio();
    power_on_modem(dce);

    wait_for_sync(dce, 15);
    check_connectivity(dce);

    return ESP_OK;
}

void sim7600_destroy()
{
    esp_modem_destroy(dce);
    esp_netif_destroy(esp_netif);
}
