#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "soc/soc_caps.h"
#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "string.h"
#include "cJSON.h"

#include "ttn.h"

#include "maxbox_defines.h"
#include "lorawan.h"
#include "state.h"
#include "telemetry.h"
#include "http.h"


static const char* TAG = "MaxBox-telemetry";

#define ts_stale(ts) ((box_timestamp() - ts) > (LORA_TELEMETRY_INTERVAL_MS/1000))

adc_oneshot_unit_handle_t adc1_handle;
adc_cali_handle_t adc1_cali_handle = NULL;

static void update_battery_voltage(void)
{
    int voltage_raw, voltage_cal;
    adc_oneshot_read(adc1_handle, VBAT_ADC_CHANNEL, &voltage_raw);
    adc_cali_raw_to_voltage(adc1_cali_handle, voltage_raw, &voltage_cal);
    mb->tel->aux_battery_voltage = (float)voltage_cal * 0.0057;
}

static uint8_t calculate_age_t(int32_t timestamp)
{
    // Calculates age_t of telemetry data for LoRaWAN packet

    if (timestamp == 0) return 255;
    int32_t delta_timestamp = box_timestamp() - timestamp;
    uint8_t age_byte;

    if (delta_timestamp < 60) {age_byte = 0;}
    else if (delta_timestamp >= 14860800) {age_byte = 254;}
    else if (delta_timestamp < 3600) {age_byte = (uint8_t)(delta_timestamp / 60);}
    else if (delta_timestamp < 86400) {age_byte = 59 + (uint8_t)(delta_timestamp / 3600);}
    else {age_byte = (uint8_t)(82 + delta_timestamp / 86400);}
    return age_byte;
}

void print_all_telemetry()
{
    int32_t box_ts = box_timestamp();
    ESP_LOGI(TAG, "Doors locked: %i", mb->tel->doors_locked);
    ESP_LOGI(TAG, "Doors locked last updated: %ld", mb->tel->doors_updated_ts);
    ESP_LOGI(TAG, "Odometer (miles): %ld", mb->tel->odometer_miles);
    ESP_LOGI(TAG, "Odometer last updated: %ld", mb->tel->odometer_updated_ts);
    ESP_LOGI(TAG, "Aux battery voltage, %fV", mb->tel->aux_battery_voltage);
    ESP_LOGI(TAG, "SoC: %f%", mb->tel->soc_percent);
    ESP_LOGI(TAG, "SoC last updated: %ld", mb->tel->soc_updated_ts);
    ESP_LOGI(TAG, "SoH: %i%", mb->tel->soh_percent);
    ESP_LOGI(TAG, "SoH last updated: %ld", mb->tel->soh_updated_ts);
    ESP_LOGI(TAG, "Odometer last updated: %ld", mb->tel->odometer_updated_ts);
    ESP_LOGI(TAG, "GNSS position: %f, %f", mb->tel->gnss_latitude, mb->tel->gnss_longitude);
    ESP_LOGI(TAG, "GNSS HDoP: %f", mb->tel->gnss_hdop);
    ESP_LOGI(TAG, "GNSS number of satellites: %i", mb->tel->gnss_nosats);
    ESP_LOGI(TAG, "GNSS last updated: %ld", mb->tel->gnss_updated_ts);
    ESP_LOGI(TAG, "Tyre pressure front left: %i", mb->tel->tyre_pressure_fl);
    ESP_LOGI(TAG, "Tyre pressure front right: %i", mb->tel->tyre_pressure_fr);
    ESP_LOGI(TAG, "Tyre pressure rear left: %i", mb->tel->tyre_pressure_rl);
    ESP_LOGI(TAG, "Tyre pressure rear right: %i",   mb->tel->tyre_pressure_rr);
    ESP_LOGI(TAG, "Tyre pressure last updated: %ld", mb->tel->tp_updated_ts);
    ESP_LOGI(TAG, "iButton ID: %s", mb->tel->ibutton_id);
    ESP_LOGI(TAG, "Box uptime: %ld", box_ts);
}

void json_format_telemetry(char *json_string, char *card_id)
{
    update_battery_voltage();

    cJSON *root, *tel, *tp, *gnss, *soc, *soh, *odo, *doors, *ab, *maxbox;
    root=cJSON_CreateObject();
    cJSON_AddItemToObject(root, "telemetry", tel=cJSON_CreateObject());

    if (card_id) cJSON_AddItemToObject(root, "card_id", cJSON_CreateString(card_id));

    cJSON_AddItemToObject(tel, "gnss", gnss=cJSON_CreateObject());
    cJSON_AddNumberToObject(gnss, "lat",  mb->tel->gnss_latitude);
    cJSON_AddNumberToObject(gnss, "lng",  mb->tel->gnss_longitude);
    cJSON_AddNumberToObject(gnss, "hdop",  mb->tel->gnss_hdop);
    cJSON_AddNumberToObject(gnss, "nosats",  mb->tel->gnss_nosats);
    cJSON_AddNumberToObject(gnss, "ts",  mb->tel->gnss_updated_ts);

    cJSON_AddItemToObject(tel, "soc", soc=cJSON_CreateObject());
    cJSON_AddNumberToObject(soc, "percent", mb->tel->soc_percent);
    cJSON_AddNumberToObject(soc, "ts", mb->tel->soc_updated_ts);

    cJSON_AddItemToObject(tel, "soh", soh=cJSON_CreateObject());
    cJSON_AddNumberToObject(soh, "percent", mb->tel->soh_percent);
    cJSON_AddNumberToObject(soh, "ts", mb->tel->soh_updated_ts);

    cJSON_AddItemToObject(tel, "odometer", odo=cJSON_CreateObject());
    cJSON_AddNumberToObject(odo, "miles", mb->tel->odometer_miles);
    cJSON_AddNumberToObject(odo, "ts", mb->tel->odometer_updated_ts);

    cJSON_AddItemToObject(tel, "tyre_pressures", tp=cJSON_CreateObject());
    cJSON_AddNumberToObject(tp, "fl_psi", mb->tel->tyre_pressure_fl);
    cJSON_AddNumberToObject(tp, "fr_psi", mb->tel->tyre_pressure_fr);
    cJSON_AddNumberToObject(tp, "rl_psi", mb->tel->tyre_pressure_rl);
    cJSON_AddNumberToObject(tp, "rr_psi", mb->tel->tyre_pressure_rr);
    cJSON_AddNumberToObject(tp, "ts", mb->tel->tp_updated_ts);

    cJSON_AddItemToObject(tel, "doors", doors=cJSON_CreateObject());
    cJSON_AddNumberToObject(doors, "locked", mb->tel->doors_locked);
    cJSON_AddNumberToObject(doors, "ts", mb->tel->doors_updated_ts);

    cJSON_AddItemToObject(tel, "aux_battery", ab=cJSON_CreateObject());
    cJSON_AddNumberToObject(ab, "voltage",  mb->tel->aux_battery_voltage);

    cJSON_AddItemToObject(tel, "maxbox", maxbox=cJSON_CreateObject());
    cJSON_AddStringToObject(maxbox, "ibutton_id",  mb->tel->ibutton_id);
    cJSON_AddNumberToObject(maxbox, "uptime_s", box_timestamp());
    cJSON_AddNumberToObject(maxbox, "free_heap_bytes", esp_get_free_heap_size());

    char *rendered=cJSON_Print(root);

    strcpy(json_string, rendered);

    cJSON_Delete(root);
    free(rendered);
}

void lora_format_telemetry(uint8_t *lm)
{
    /*
    Formats telemetry struct ready for transmission as LoRaWAN packet
    
    FORMAT:
    lm[0] bit 0: SOC_STALE (has SoC been updated since last packet)
    lm[0] bit 1: TP_STALE (have tyre pressures been updated since last packet)
    lm[0] bit 2: GNSS_STALE (has location been updated since last packet)
    lm[0] bit 7: DOORS_LOCKED (0: locked, 1: locked)
    lm[1-4]: GNSS_LAT (float, GNSS latitude in decimal)
    lm[5-8]: GNSS_LONG (float, GNSS longitude in decimal)
    lm[9]: GNSS_HDOP (uint8, 1-9 = metres, 10-254 = 9 + (metres/10) [254 = <2.45km], 255=NaN)
    lm[10]: GNSS_AGE (age_t, see below)
    lm[11]: AUX_BAT_V (uint8, V/10 (255 = 25.5V))
    lm[12]: SOC_PERCENTAGE (uint8, 0-100, >100 NaN)
    lm[13]: SOC_AGE (age_t, see below)
    lm[14-16]: TYRE_PRESSURE_PSI (uint6 * 4, packed into 3 bytes)
    lm[17]: TYRE_PRESSURE_AGE (age_t, see below)
    
    age_t format (variable precision time in one byte)
    0           less than 1 minute
    1 to 60     minutes
    61          1-2 hours
    62          2-3 hours
    …           
    83          23-24 hours
    84          1-2 days
    …           
    253         170-171 days
    254         older than 171 days
    255         invalid/NaN
    */

    // Byte 0 is a bitfield
    uint8_t bitfield = 0;
    if (mb->tel->doors_locked) bitfield = bitfield | 0b00000001;
    if (ts_stale(mb->tel->soc_updated_ts)) bitfield = bitfield | 0b10000000;
    if (ts_stale(mb->tel->tp_updated_ts)) bitfield = bitfield | 0b01000000;
    if (ts_stale(mb->tel->gnss_updated_ts)) bitfield = bitfield | 0b00100000;
    memcpy(lm, &bitfield, 1);

    // GNSS data-packing routines: lat_long direct memcpy
    memcpy(lm+1, &mb->tel->gnss_latitude, 4);
    memcpy(lm+5, &mb->tel->gnss_longitude, 4);

    // GNSS HDoP (horizontal uncertainty) needs to be rescaled
    uint8_t gnss_hdop_byte = 255;

    if (mb->tel->gnss_hdop < 10.0) {gnss_hdop_byte = (uint8_t)mb->tel->gnss_hdop ;}
    else if (mb->tel->gnss_hdop >= 2450.0) {gnss_hdop_byte = 254;}
    else {gnss_hdop_byte = (uint8_t)(9 + (mb->tel->gnss_hdop / 10));}

    memcpy(lm+9, &gnss_hdop_byte, 1);

    // GNSS data age, rescaled into an age_t
    uint8_t gnss_data_age_byte = calculate_age_t(mb->tel->gnss_updated_ts);
    memcpy(lm+10, &gnss_data_age_byte, 1);

    // Aux battery voltage needs to be rescaled
    uint8_t aux_v_byte;

    if (mb->tel->aux_battery_voltage < 0) {aux_v_byte = 0;}
    else if (mb->tel->aux_battery_voltage > 25.4) {aux_v_byte = 255;}
    else {aux_v_byte = (uint8_t)(mb->tel->aux_battery_voltage * 10);}

    memcpy(lm+11, &aux_v_byte, 1);

    uint8_t soc_byte = (uint8_t)mb->tel->soc_percent;

    memcpy(lm+12, &soc_byte, 1);

    uint32_t tp_combined = (mb->tel->tyre_pressure_fl & 0b00111111)
    + ((mb->tel->tyre_pressure_fr & 0b00111111) << 6)
    + ((mb->tel->tyre_pressure_rl & 0b00111111) << 12)
    + ((mb->tel->tyre_pressure_rr & 0b00111111) << 18);

    // HACK: copying from unaligned memory throws up compiler errors so we copy the full 4-byte uint32
    // and then copy the one-byte SoC data age over the top of it - the high byte of this uint32 will be 0. 
    tp_combined = __builtin_bswap32(tp_combined);
    memcpy(lm+13, &tp_combined, 4);

    // SoC data age, rescaled into an age_t
    uint8_t soc_data_age_byte = calculate_age_t(mb->tel->soc_updated_ts);
    memcpy(lm+13, &soc_data_age_byte, 1); 

    // Tyre pressure data age, rescaled into an age_t
    uint8_t tp_age_byte = calculate_age_t(mb->tel->tp_updated_ts);
    memcpy(lm+17, &tp_age_byte, 1);
}

void wifi_telemetry_task(void* pvParameter)
{
    while (1) {
        mb_begin_event(EVT_TELEMETRY);
        http_send(NULL);

        vTaskDelay(WIFI_TELEMETRY_INTERVAL_MS / portTICK_PERIOD_MS);
    }
}

void lorawan_telemetry_task(void* pvParameter)
{
    while (1) {
        uint8_t lora_telemetry_message[19] = {0};
        lora_format_telemetry(lora_telemetry_message);

        ESP_LOGI(TAG, "Sending LoRaWAN message");
        ttn_response_code_t res = ttn_transmit_message(lora_telemetry_message, sizeof(lora_telemetry_message) - 1, 1, false);
        if(res == TTN_SUCCESSFUL_TRANSMISSION)
            {
                ESP_LOGI(TAG, "Message sent");
            } else
            {
                ESP_LOGE(TAG, "Message sending failed");
            }

        vTaskDelay(LORA_TELEMETRY_INTERVAL_MS / portTICK_PERIOD_MS);
    }
}

esp_err_t telemetry_init(void)
{
    // Set up oneshot measurement for VBAT ADC

    adc_oneshot_unit_init_cfg_t init_config1 = {
        .unit_id = ADC_UNIT_1,
    };
    adc_oneshot_new_unit(&init_config1, &adc1_handle);

    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = ADC_ATTEN_DB_12,
    };

    adc_oneshot_config_channel(adc1_handle, VBAT_ADC_CHANNEL, &config);

    adc_cali_curve_fitting_config_t cali_config = {
        .unit_id = ADC_UNIT_1,
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    adc_cali_create_scheme_curve_fitting(&cali_config, &adc1_cali_handle);

    update_battery_voltage();

    lorawan_init();

    xTaskCreate(wifi_telemetry_task, "wifi_telemetry", 1024 * 4, (void* )0, 3, NULL);
    xTaskCreate(lorawan_telemetry_task, "lorawan_telemetry", 1024 * 4, (void* )0, 3, NULL);

    return ESP_OK;
}
