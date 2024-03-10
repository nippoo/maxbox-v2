#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "driver/uart.h"
#include "string.h"
#include "driver/gpio.h"
#include "esp_sleep.h"
#include "driver/rtc_io.h"
#include "esp_event.h"
#include "sdkconfig.h"
#include "ctype.h"

#include "esp_netif.h"
#include "esp_netif_ppp.h"

#include "maxbox_defines.h"

#define BUF_SIZE 1024

ESP_EVENT_DEFINE_BASE(ESP_NMEA_EVENT);

static char *s_buffer;                                  /*!< Runtime buffer */
static QueueHandle_t s_event_queue;                     /*!< UART event queue handle */
static esp_event_loop_handle_t s_event_loop_hdl;        /*!< Event loop handle */

const char *TAG = "MaxBox-SIM7600";

static void config_gpio()
{
    gpio_set_direction(SIM_RESET_PIN, GPIO_MODE_OUTPUT);
    gpio_set_direction(SIM_PWRKEY_PIN, GPIO_MODE_OUTPUT);
    gpio_set_direction(SIM_STATUS_PIN, GPIO_MODE_INPUT);

    gpio_set_level(SIM_RESET_PIN, 0);
    gpio_set_level(SIM_PWRKEY_PIN, 0);
}

static void send_sim_raw(char* data)
{
    uart_write_bytes(SIM_UART_PORT, data, strlen(data));
    ESP_LOGI(TAG, "Written data, %i bytes: %s", strlen(data), data);

}

static esp_err_t send_sim_at(char* data)
{
    uart_disable_pattern_det_intr(SIM_UART_PORT);
    uart_flush(SIM_UART_PORT);

    char data_recv[128];
    int length = 0;
    send_sim_raw(data);
    vTaskDelay(pdMS_TO_TICKS(500));
    uart_get_buffered_data_len(SIM_UART_PORT, (size_t*)&length);
    if (length) {
        uart_read_bytes(SIM_UART_PORT, data_recv, length, 100);
        ESP_LOGI(TAG, "Data returned, %i bytes: %s", length, data_recv);
        uart_flush(SIM_UART_PORT);
        uart_enable_pattern_det_baud_intr(SIM_UART_PORT, '\r', 1, 9, 0, 0);
        return ESP_OK;
    }
    uart_flush(SIM_UART_PORT);
    uart_enable_pattern_det_baud_intr(SIM_UART_PORT, '\r', 1, 9, 0, 0);
    ESP_LOGW(TAG, "No data returned from SIM7600", data);
    return ESP_FAIL;
}


static esp_err_t wait_for_sync(uint8_t count)
{
    ESP_LOGI(TAG, "Waiting for SIM7600 to boot for up to %is...", count);

    uart_disable_pattern_det_intr(SIM_UART_PORT);
    uart_flush(SIM_UART_PORT);

    char data[128];
    int length = 0;

    for (int i = 0; i < count; i++) {
        send_sim_raw("AT\r");

        uart_get_buffered_data_len(SIM_UART_PORT, (size_t*)&length);
        if (length) {
            uart_read_bytes(SIM_UART_PORT, data, length, 100);
            if (strncmp(data, "AT", 2) == 0) {
                uart_enable_pattern_det_baud_intr(SIM_UART_PORT, '\r', 1, 9, 0, 0);
                ESP_LOGI(TAG, "SIM7600 successfully online");
                vTaskDelay(pdMS_TO_TICKS(3000));
                uart_flush(SIM_UART_PORT);
                return ESP_OK;
            }
            uart_flush(SIM_UART_PORT);
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    uart_enable_pattern_det_baud_intr(SIM_UART_PORT, '\r', 1, 9, 0, 0);
    ESP_LOGW(TAG, "SIM7600 failed to come online in timeout");
    return ESP_FAIL;
}

static void gnss_start()
{
    ESP_LOGI(TAG, "Turning GPS on");
    send_sim_at("AT\r");
    send_sim_at("AT+CGPS=1,1\r");
    send_sim_at("AT+CGPSINFOCFG=10,256\r");
    ESP_LOGI(TAG, "GPS enabled");
}

static void gnss_stop()
{
    ESP_LOGI(TAG, "Turning GPS off");
    send_sim_raw("AT+CGPS=0");
    ESP_LOGI(TAG, "GPS disabled");
}

static void reset_modem()
{
    // Power on the modem
    ESP_LOGI(TAG, "Sending SIM7600 reset pulse");
    gpio_set_level(SIM_RESET_PIN, 1);
    vTaskDelay(200 / portTICK_PERIOD_MS);
    gpio_set_level(SIM_RESET_PIN, 0);
    wait_for_sync(20);
    gnss_start();
}

static void power_off_modem()
{
    // Is the modem already off?
    if (!gpio_get_level(SIM_STATUS_PIN)) {
        ESP_LOGI(TAG, "SIM7600 already powered off");
        return;
    }

    // Power off the modem
    ESP_LOGI(TAG, "Sending SIM7600 power-off pulse");
    gpio_set_level(SIM_PWRKEY_PIN, 1);
    vTaskDelay(3000 / portTICK_PERIOD_MS);
    gpio_set_level(SIM_PWRKEY_PIN, 0);
}

static void power_on_modem()
{
    // Is the modem already on? If so, power off first.
    if (gpio_get_level(SIM_STATUS_PIN)) {
        ESP_LOGI(TAG, "SIM7600 is already powered on, resetting");
        reset_modem();
        return;
    }

    // Power on the modem
    ESP_LOGI(TAG, "Sending SIM7600 power-on pulse");
    gpio_set_level(SIM_PWRKEY_PIN, 1);
    vTaskDelay(500 / portTICK_PERIOD_MS);
    gpio_set_level(SIM_PWRKEY_PIN, 0);
    wait_for_sync(16);
    gnss_start();
}

static float nmea_to_decimal(const char *lat_long)
{
    char deg[4] = {0};
    char *dot, *min;
    int len;
    float dec = 0;

    if ((dot = strchr(lat_long, '.'))) {
        // decimal point was found
        min = dot - 2;                          // mark the start of minutes 2 chars back
        len = min - lat_long;                   // find the length of degrees
        strncpy(deg, lat_long, len);            // copy the degree string to allow conversion to float
        dec = atof(deg) + atof(min) / 60;       // convert to float
    }
    return dec;
}

static esp_err_t process_gngns_string(char *buf)
{
    // Example string: $GNGNS,223254.00,5156.126739,N,00629.13328,W,AAA,10,1.0,18.9,46.0,,,V*49
    ESP_LOGI(TAG, "SIM7600 string %s", buf);

    char *p = buf;
    if (!(p = strstr(p, "$GNGNS"))) {
        return ESP_FAIL;
    }
    if (!(p = strchr(p, ','))) {
        return ESP_FAIL;
    }
    if (!(p = strchr(++p, ','))) {
        return ESP_FAIL;    // jump over UTC time
    }
    mb->tel->gnss_latitude = nmea_to_decimal(++p);
    if (!(p = strchr(p, ','))) {
        return ESP_FAIL;
    }
    if (*(++p) == 'S') {
        mb->tel->gnss_latitude = -mb->tel->gnss_latitude;
    }
    if (!(p = strchr(p, ','))) {
        return ESP_FAIL;
    }
    mb->tel->gnss_longitude = nmea_to_decimal(++p);
    if (!(p = strchr(p, ','))) {
        return ESP_FAIL;
    }
    if (*(++p) == 'W') {
        mb->tel->gnss_longitude = -mb->tel->gnss_longitude;
    }
    if (!(p = strchr(p, ','))) {
        return ESP_FAIL;    // jump over mode indicator
    }
    if (!(p = strchr(++p, ','))) {
        return ESP_FAIL;
    }
    mb->tel->gnss_nosats = atoi(++p);
    if (!(p = strchr(p, ','))) {
        return ESP_FAIL;
    }

    // HACK: HDoP isn't the same as horizontal precision, but for this
    // application we multiply by 3 to get a rough horizontal range
    mb->tel->gnss_hdop = atof(++p) * 3.0;

    mb->tel->gnss_updated_ts = box_timestamp();

    return ESP_OK;
}

static void esp_handle_uart_pattern()
{
    int pos = uart_pattern_pop_pos(SIM_UART_PORT);
    if (pos != -1) {
        /* read one line(include '\n') */
        int read_len = uart_read_bytes(SIM_UART_PORT, s_buffer, pos + 1, 100 / portTICK_PERIOD_MS);
        /* make sure the line is a standard string */
        s_buffer[read_len] = '\0';
        /* Send new line to handle */

        if (process_gngns_string(s_buffer) != ESP_OK) {
            ESP_LOGW(TAG, "GNSS decode line failed");
        } else {
            ESP_LOGI(TAG, "GNSS string successfully processed");
        }
    } else {
        ESP_LOGW(TAG, "Pattern queue size too small");
        uart_flush_input(SIM_UART_PORT);
    }
}
static void nmea_parser_task_entry(void *arg)
{
    uart_event_t event;
    while (1) {
        if (xQueueReceive(s_event_queue, &event, pdMS_TO_TICKS(200))) {
            switch (event.type) {
            case UART_DATA:
                break;
            case UART_FIFO_OVF:
                ESP_LOGW(TAG, "HW FIFO Overflow");
                uart_flush(SIM_UART_PORT);
                xQueueReset(s_event_queue);
                break;
            case UART_BUFFER_FULL:
                ESP_LOGW(TAG, "Ring Buffer Full");
                uart_flush(SIM_UART_PORT);
                xQueueReset(s_event_queue);
                break;
            case UART_BREAK:
                ESP_LOGW(TAG, "Rx Break");
                break;
            case UART_PARITY_ERR:
                ESP_LOGE(TAG, "Parity Error");
                break;
            case UART_FRAME_ERR:
                ESP_LOGE(TAG, "Frame Error");
                break;
            case UART_PATTERN_DET:
                esp_handle_uart_pattern();
                break;
            default:
                ESP_LOGW(TAG, "unknown uart event type: %d", event.type);
                break;
            }
        }
        /* Drive the event loop */
        esp_event_loop_run(s_event_loop_hdl, pdMS_TO_TICKS(50));
    }
    vTaskDelete(NULL);
}

void sim7600_init()
{
    s_buffer = calloc(1, 4096);

    /* Install UART friver */
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    uart_driver_install(SIM_UART_PORT, 2048, 2048, 30, &s_event_queue, 0);
    uart_param_config(SIM_UART_PORT, &uart_config);
    uart_set_pin(SIM_UART_PORT, SIM_TXD_PIN, SIM_RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

    /* Set pattern interrupt, used to detect the end of a line */
    uart_enable_pattern_det_baud_intr(SIM_UART_PORT, '\r', 1, 9, 0, 0);

    /* Set pattern queue size */
    uart_pattern_queue_reset(SIM_UART_PORT, 30);
    uart_flush(SIM_UART_PORT);

    /* Create Event loop */
    esp_event_loop_args_t loop_args = {
        .queue_size = 30,
        .task_name = NULL
    };

    esp_event_loop_create(&loop_args, &s_event_loop_hdl);

    xTaskCreate(nmea_parser_task_entry, "nmea_parser", 4096, NULL, 5, NULL);

    config_gpio();
    power_on_modem();
}
