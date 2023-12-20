#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "pthread.h"

#include "led.h"
#include "esp_log.h"

#include "lp50xx.h"
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

    const lp50xx_start_args_t lp50xx_start_args = {
        .i2c_addr = 0x28,
        .i2c_host_id = I2C_HOST_ID,
    };

    lp50xx_init(&lp50xx_start_args);

    const ltr303_start_args_t ltr303_start_args = {
        .i2c_host_id = I2C_HOST_ID,
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
        lp50xx_set_night_mode(true);
    }
    else
    {
        ESP_LOGI(TAG, "Ambient light: %i lux, full brightness", lux);
        lp50xx_set_night_mode(false);
    }
}

static void led_swirl(uint16_t interval_ms, uint64_t elapsed_ms)
{
    // note: interval_ms must be even, and divisible exactly by elapsed_ms

    // uint8_t j = ((elapsed_ms + interval_ms) % (interval_ms * 4)) / interval_ms;

    lp50xx_set_color_bank(0, 255, ((elapsed_ms / interval_ms) % 255));
}

static void led_breathe(uint16_t period_ms, uint64_t elapsed_ms,
    uint8_t red, uint8_t green, uint8_t blue)
{
    int16_t curr_period = (elapsed_ms-(period_ms/4)) % period_ms - period_ms/2;
    float multiplier = ((float)2/period_ms)*abs(curr_period);

    uint8_t cur_red = (red*multiplier);
    uint8_t cur_green = (green*multiplier);
    uint8_t cur_blue = (blue*multiplier);

    lp50xx_set_color_bank(cur_red, cur_green, cur_blue);
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
                    lp50xx_set_global_on(1);
                    break;
                case IDLE:
                    lp50xx_set_global_on(0);
                    break;
                case PROCESSING:
                    // ktd2064_set_color0(255, 255, 255); // cyan
                    // ktd2064_set_color1(0, 0, 0);
                    // ktd2064_select_all_color1();
                    // ktd2064_global_on(2);
                    break;
                case LOCKED:
                    // ktd2064_set_color0(0, 0, 0);
                    // ktd2064_set_color1(0, 0, 255); // blue
                    // ktd2064_select_all_color1();
                    // ktd2064_global_on(0);
                    break;
                case UNLOCKED:
                    // ktd2064_set_color0(0, 0, 0);
                    // ktd2064_set_color1(0, 255, 0); // green
                    // ktd2064_select_all_color1();
                    // ktd2064_global_on(0);
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
            led_breathe(1000, elapsed_ms, 0, 200, 255);
            break;
        case PROCESSING:
            led_swirl(16, elapsed_ms);
            break;
        case LOCKED:
            if (elapsed_ms > 3000)
                led_update(IDLE);
            break;
        case UNLOCKED:
            if (elapsed_ms > 3000)
                led_update(IDLE);
            break;
        default:
            break;
        }

        elapsed_ms+= 20;
        vTaskDelay(pdMS_TO_TICKS(20));
    }

    vTaskDelete(NULL);
}