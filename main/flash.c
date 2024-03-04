#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "nvs_flash.h"
#include "esp_mac.h"
#include "esp_log.h"
#include "string.h"

#include "maxbox_defines.h"
#include "flash.h"


static const char* TAG = "MaxBox-NVS";

void flash_init(void)
{
    //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_read_mac(mb->base_mac, ESP_MAC_WIFI_STA));

    ESP_LOGI(TAG, "Loaded NVS. Base MAC address %02x%02x%02x%02x%02x%02x", 
                mb->base_mac[0], mb->base_mac[1], mb->base_mac[2], mb->base_mac[3], mb->base_mac[4], mb->base_mac[5]);

    nvs_handle_t my_handle;
    esp_err_t err = nvs_open("storage", NVS_READWRITE, &my_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error (%s) opening NVS handle!", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "Reading operator card list from NVS ...");
        nvs_get_i32(my_handle, "etag", &mb->etag);

        size_t required_size = sizeof(mb->operator_card_list);
        nvs_get_blob(my_handle, "op_card_list", mb->operator_card_list, &required_size);
        if (err == ESP_OK)
        {
            ESP_LOGI(TAG, "Loaded etag: %i", mb->etag);
            int i;
            for (i=0; i<MAX_OPERATOR_CARDS; i++)
            {
                if (strcmp(mb->operator_card_list[i], "voidvoid") != 0)
                {
                    ESP_LOGI(TAG, "Operator card %i: %s", i+1, mb->operator_card_list[i]);
                }
            }
        }
        else if (err == ESP_ERR_NVS_NOT_FOUND)
        {
            ESP_LOGI(TAG, "No etag value written in flash");
        }
        else
        {
            ESP_LOGE(TAG, "Error (%s) reading flash", esp_err_to_name(err));
        }
    }
}

void flash_write_all(void)
{
    nvs_handle_t my_handle;

    esp_err_t err = nvs_open("storage", NVS_READWRITE, &my_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error (%s) opening NVS handle!", esp_err_to_name(err));
    } else {

    ESP_LOGI(TAG, "Writing operator card to NVS...");

    nvs_set_i32(my_handle, "etag", mb->etag);

    size_t required_size = sizeof(mb->operator_card_list);
    nvs_set_blob(my_handle, "op_card_list", mb->operator_card_list, required_size);
    }
}