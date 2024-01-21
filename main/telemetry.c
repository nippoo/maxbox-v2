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

#include "maxbox_defines.h"

#include "telemetry.h"

static const char* TAG = "MaxBox-telemetry";

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
	/*
	Calculates age_t of telemetry data for LoRaWAN packet
	*/

	return 255;
}

void print_all_telemetry()
{
	ESP_LOGI(TAG, "Scaled voltage, %f V", mb->tel->aux_battery_voltage);
}

void lora_format_telemetry(char *lm)
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
	lm[9]: GNSS_HDOP (uint8, metres*10 [255 = <2.55km])
	lm[10]: GNSS_AGE (age_t, see below)
	lm[11]: AUX_BAT_V (uint8, V/10 (255 = 25.5V))
	lm[12]: SOC_PERCENTAGE (uint8, 0-100, >100 NaN)
	lm[13]: SOC_AGE (age_t, see below)
	lm[14-16]: TYRE_PRESSURE_PSI (uint6 * 4, packed into 3 bytes)
	lm[17]: TYRE_PRESSURE_AGE (age_t, see below)
	
	age_t format (variable precision time in one byte)
	0			less than 1 minute
	1 to 60		minutes
	61			1-2 hours
	62			2-3 hours
	…			
	83			23-24 hours
	84			1-2 days
	…			
	253			170-171 days
	254			older than 171 days
	255			invalid/NaN
	*/

	// GNSS data-packing routines: lat_long direct memcpy
	memcpy(lm+1, &mb->tel->gnss_latitude, 4);
	memcpy(lm+5, &mb->tel->gnss_longitude, 4);

	// GNSS HDoP (horizontal uncertainty) needs to be rescaled
	uint8_t gnss_hdop_byte = 0;

	if (mb->tel->gnss_hdop < 10.0) {gnss_hdop_byte = 0;}
    else if (mb->tel->gnss_hdop > 2540.0) {gnss_hdop_byte = 255;}
    else {gnss_hdop_byte = (uint8_t)(mb->tel->gnss_hdop / 10);}

    memcpy(lm+9, &gnss_hdop_byte, 1);

	// Aux battery voltage needs to be rescaled
	uint8_t aux_v_byte = 0;

	if (mb->tel->aux_battery_voltage < 0) {aux_v_byte = 0;}
    else if (mb->tel->aux_battery_voltage > 25.4) {aux_v_byte = 255;}
    else {aux_v_byte = (uint8_t)(mb->tel->aux_battery_voltage * 10);}

    memcpy(lm+11, &aux_v_byte, 1);
}

esp_err_t telemetry_init(void)
{
	// Set up oneshot measurement for VBAT ADC
	
    adc_oneshot_unit_init_cfg_t init_config1 = {
        .unit_id = ADC_UNIT_1,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &adc1_handle));

    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = ADC_ATTEN_DB_11,
    };

    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, VBAT_ADC_CHANNEL, &config));

        adc_cali_curve_fitting_config_t cali_config = {
        .unit_id = ADC_UNIT_1,
        .atten = ADC_ATTEN_DB_11,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    adc_cali_create_scheme_curve_fitting(&cali_config, &adc1_cali_handle);

    update_battery_voltage();

    return ESP_OK;
}
