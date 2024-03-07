#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "pthread.h"
#include "math.h"

#include "led.h"
#include "esp_log.h"

#include "lp50xx.h"
#include "ltr303.h"

#include "maxbox_defines.h"

 #define max(a,b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a > _b ? _a : _b; })

led_status_t led_status = LED_IDLE;
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
    i2c_driver_install(I2C_HOST_ID, conf.mode, 0, 0, ESP_INTR_FLAG_LOWMED);

    const lp50xx_start_args_t lp50xx_start_args = {
        .i2c_addr = 0x3C,
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

    gpio_set_direction(LED_STATUS_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(LED_STATUS_PIN, 1);

    xTaskCreate(led_task, "led_task", 4096, NULL, 5, NULL);
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
        // lp50xx_set_global_scale(1.0);
    }
    else
    {
        ESP_LOGI(TAG, "Ambient light: %i lux, full brightness", lux);
        // lp50xx_set_global_scale(1.0);
    }
}

static void led_swirl(uint16_t interval_ms, uint64_t elapsed_ms,
    uint8_t red, uint8_t green, uint8_t blue,
    uint8_t dark_red, uint8_t dark_green, uint8_t dark_blue)
{
    uint8_t i, calc_red, calc_green, calc_blue;
    for (i=0; i<8; i++)
    {
        int16_t curr_period = ((elapsed_ms+(i*interval_ms/8))-(interval_ms/4)) % interval_ms - interval_ms/2;
        float multiplier = ((float)2/interval_ms)*(abs(curr_period));
        multiplier = pow(multiplier, 4);
        calc_red = red*multiplier;
        calc_green = green*multiplier;
        calc_blue = blue*multiplier;

        lp50xx_set_color_led(i, max(calc_red, dark_red), max(calc_green, dark_green), max(calc_blue, dark_blue));
    }
 }

static void led_breathe(uint16_t period_ms, uint64_t elapsed_ms,
    uint8_t red, uint8_t green, uint8_t blue) {

    int16_t curr_period = (elapsed_ms-(period_ms/4)) % period_ms - period_ms/2;
    float multiplier = ((float)2/period_ms)*abs(curr_period);

    uint8_t cur_red = (red*multiplier);
    uint8_t cur_green = (green*multiplier);
    uint8_t cur_blue = (blue*multiplier);

    lp50xx_set_color_bank(cur_red, cur_green, cur_blue);
}

static void led_breathe_2colour(uint16_t period_ms, uint64_t elapsed_ms,
    uint8_t red0, uint8_t green0, uint8_t blue0,
    uint8_t red1, uint8_t green1, uint8_t blue1) {

    int16_t curr_period = (elapsed_ms-(period_ms/4)) % period_ms - period_ms/2;
    float multiplier = ((float)2/period_ms)*abs(curr_period);

    uint8_t curr_colour = (((elapsed_ms + period_ms/4) % (period_ms * 2)) < (period_ms)) ? 0 : 1;

    uint8_t cur_red, cur_blue, cur_green;
    if (curr_colour == 0)
    {
        cur_red = (red0*multiplier);
        cur_green = (green0*multiplier);
        cur_blue = (blue0*multiplier);
    }
    else
    {
        cur_red = (red1*multiplier);
        cur_green = (green1*multiplier);
        cur_blue = (blue1*multiplier);
    }

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
                case LED_BOOT:
                    lp50xx_set_global_off(0);
                    lp50xx_set_bank_control(0);
                    break;
                case LED_IDLE:
                    lp50xx_set_global_off(1);
                    break;
                case LED_TOUCH:
                    lp50xx_set_global_off(0);
                    lp50xx_set_bank_control(0);
                    break;
                case LED_LOCKED:
                    lp50xx_set_bank_control(1);
                    lp50xx_set_global_off(0);
                    break;
                case LED_UNLOCKED:
                    lp50xx_set_bank_control(1);
                    lp50xx_set_global_off(0);
                    break;
                case LED_DENY:
                    lp50xx_set_bank_control(1);
                    lp50xx_set_global_off(0);
                    break;
                case LED_ERROR:
                    lp50xx_set_bank_control(1);
                    lp50xx_set_global_off(0);
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
        case LED_BOOT:
            led_swirl(1000, elapsed_ms, 0, 255, 255, 0, 0, 10);
            break;
        case LED_TOUCH:
            led_swirl(500, elapsed_ms, 255, 255, 255, 10, 10, 10);
            break;
        case LED_LOCKED:
                led_breathe(200, elapsed_ms, 0, 255, 0);
            if (elapsed_ms > 1500)
                led_update(LED_IDLE);
            break;
        case LED_UNLOCKED:
            led_breathe(200, elapsed_ms, 0, 0, 255);
            if (elapsed_ms > 1500)
                led_update(LED_IDLE);
            break;
        case LED_DENY:
            led_breathe(200, elapsed_ms, 255, 0, 0);
            if (elapsed_ms > 1500)
                led_update(LED_IDLE);
            break;
        case LED_ERROR:
            led_breathe_2colour(500, elapsed_ms, 255, 0, 255, 255, 0, 0);
            if (elapsed_ms > 5000)
                led_update(LED_IDLE);
            break;
        case LED_FIRMWARE:
            led_breathe_2colour(1000, elapsed_ms, 0, 255, 255, 255, 255, 0);
            break;
        default:
            break;
        }

        elapsed_ms+= 20;
        vTaskDelay(pdMS_TO_TICKS(20));
    }

    vTaskDelete(NULL);
}