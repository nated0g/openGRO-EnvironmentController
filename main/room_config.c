#include "room_config.h"
#include "string.h"

char keys[NUM_CONFIG_ITEMS][MAX_KEY_LENGTH] = {
    "ac_g_mode",
    "ac_y_mode",
    "ac_w_mode",
    "dh_mode",
    "ef_mode",
    "co2_mode",
    "cf_mode",
    "d_temp_sp",
    "n_temp_sp",
    "rh_sp",
    "co2_sp",
    "co2_db",
    "co2_os",
    "light_out_pct",
    "hitemp_dim",
    "hitemp_cutout",
    "hitemp_reset",
    "cool_db",
    "cool_os",
    "heat_db",
    "heat_os",
    "dh_db",
    "dh_os",
    "co2_setback_s",
    "l_on_time_ts",
    "l_off_time_ts",
    "sr_len_s",
    "ss_len_s"
};


typedef struct
{
    char key[MAX_KEY_LENGTH];
    int32_t value;
} config_item_t;

config_item_t config[NUM_CONFIG_ITEMS];

void init_config(void)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        // NVS partition was truncated and needs to be erased
        // Retry nvs_flash_init
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    err = nvs_open("config", NVS_READWRITE, &handle);

    if (err != ESP_OK)
    {
        printf("Error (%s) opening NVS handle!\n", esp_err_to_name(err));
    }
    else
    {
        int32_t value = 0;
        for (int i = 0; i < NUM_CONFIG_ITEMS; i++)
        {
            err = nvs_get_i32(handle, keys[i], &value);
            switch (err)
            {
            case ESP_OK:
                // We have an initialized NVS key
                // Write the value into the config map
                strcpy(config[i].key, keys[i]);
                config[i].value = value;
                nvs_set_i32(handle, keys[i], config[i].value + 1);
                break;
            case ESP_ERR_NVS_NOT_FOUND:
                printf("%s was not initialized yet!\n", keys[i]);
                nvs_set_i32(handle, keys[i], 0);
                printf("%s initialized to 0.\n", keys[i]);
                break;
            default:
                printf("Error (%s) reading!\n", esp_err_to_name(err));
            }
        }
        printf("Committing config values to NVS.\n");
        nvs_commit(handle);
        nvs_close(handle);
        for (int i = 0; i < NUM_CONFIG_ITEMS; i++)
        {
            printf("%s is %d\n", config[i].key, config[i].value);
        }
    }
}

esp_err_t set_config(char *key, int32_t value)
{

    for (int i = 0; i < NUM_CONFIG_ITEMS; i++)
    {
        if (!strcmp(key, config[i].key))
        {
            config[i].value = value;
            
            nvs_handle_t handle;
            esp_err_t err;

            // Open
            err = nvs_open("config", NVS_READWRITE, &handle);
            if (err != ESP_OK)
                return err;
            nvs_set_i32(handle, key, value);
            nvs_commit(handle);
            nvs_close(handle);
        }
    }
    return ESP_OK;
}