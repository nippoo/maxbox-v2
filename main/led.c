#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "pthread.h"

#include "led.h"
#include "esp_log.h"

#include "ktd2064.h"
#include "ltr303.h"

#include "maxbox_defines.h"

led_status_t led_status = IDLE;
bool led_status_changed = false;
pthread_mutex_t led_status_mux;

static const char* TAG = "MaxBox-LED";

void led_init(void)
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = SDA_PIN,
        .scl_io_num = SCL_PIN,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 400000,
    };

    i2c_param_config(I2C_HOST_ID, &conf);
    i2c_driver_install(I2C_HOST_ID, conf.mode, 0, 0, 0);

    const ktd2064_start_args_t ktd2064_start_args = {
        .i2c_addr = 0x6C,
        .i2c_host_id = I2C_HOST_ID,
        .i2c_timeout_ms = 1000,
        .max_current_r_ma = 15,
        .max_current_g_ma = 24,
        .max_current_b_ma = 24
    };

    ktd2064_init(&ktd2064_start_args);

    const ltr303_start_args_t ltr303_start_args = {
        .i2c_host_id = I2C_HOST_ID,
        .i2c_timeout_ms = 1000,
    };

    ltr303_init(&ltr303_start_args);

    if(pthread_mutex_init (&led_status_mux, NULL) != 0){
        ESP_LOGE(TAG, "Failed to initialize LED mutex");
    }

    xTaskCreate(led_task, "led_task", 4096, NULL, 6, NULL);

    led_update(BOOT);
}

void led_update(led_status_t st)
{
    if(pthread_mutex_lock(&led_status_mux) == 0)
    {
        led_status = st;
        led_status_changed = true;
        pthread_mutex_unlock(&led_status_mux);
    }

    uint16_t lux = ltr303_read_lux();

    if (lux < CONFIG_NIGHT_MODE_THRESHOLD_LUX)
    {
        ESP_LOGI(TAG, "Ambient light: %i lux, night mode", lux);
        ktd2064_set_night_mode(true);
    }
    else
    {
        ESP_LOGI(TAG, "Ambient light: %i lux, full brightness", lux);
        ktd2064_set_night_mode(false);        
    }
}

static void led_swirl(uint16_t interval_ms, uint64_t elapsed_ms)
{
    // note: interval_ms must be even, and divisible exactly by elapsed_ms

    uint8_t j = ((elapsed_ms + interval_ms) % (interval_ms * 4)) / interval_ms;

    if (elapsed_ms % interval_ms == 0)
    {
        ktd2064_select_one(3 - j, 0xFF);
        j = (j + 1) % 4;
        ktd2064_select_one(3 - j, 0x08);
    }
    else if (elapsed_ms % interval_ms == interval_ms / 2)
    {
        j = (j + 1) % 4;
        ktd2064_select_one(3 - j, 0x8F);
    }
}

void led_task(void *args)
{
    uint64_t elapsed_ms = 0;

    while (true)
    {
        if(led_status_changed) // apply any colour selection, initial setup for new pattern
        {
            if(pthread_mutex_lock(&led_status_mux) == 0)
            {
                switch(led_status)
                {
                case BOOT:
                    ktd2064_set_color0(0, 255, 255); // cyan
                    ktd2064_set_color1(0, 0, 20); // deep blue
                    ktd2064_select_all_color1();
                    ktd2064_global_on(3);
                    break;
                case IDLE:
                    ktd2064_global_off(0);
                    break;
                case PROCESSING:
                    ktd2064_set_color0(255, 255, 255); // cyan
                    ktd2064_set_color1(0, 0, 0); // deep blue
                    ktd2064_select_all_color1();
                    ktd2064_global_on(2);
                    break;
                default:
                    break;
                }

                elapsed_ms = 0;
                led_status_changed = false;
                pthread_mutex_unlock(&led_status_mux);
            }
        }

        switch(led_status)
        {
        case BOOT:
            led_swirl(320, elapsed_ms);
            break;
        case PROCESSING:
            led_swirl(160, elapsed_ms);
            break;
        default:
            break;
        }

        elapsed_ms+= 20;
        vTaskDelay(pdMS_TO_TICKS(20));
    }

    vTaskDelete(NULL);
}