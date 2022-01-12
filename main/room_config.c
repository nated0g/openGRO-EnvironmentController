#include "room_config.h"
#include "string.h"
#include <time.h>


int32_t temperature;
int32_t humidity;
int32_t co2;

enum output_map
{
    AC1_G,
    AC1_Y,
    AC1_W,
    AC2_G,
    AC2_Y,
    AC2_W,
    DH,
    CO2
};

output_config_t outputs[NUM_OUTPUTS] = {
        {"ac1_g", .sched = {.enabled = false}, .hyst = {.enabled = false}},
        {"ac1_y", .sched = {.enabled = false}, .hyst = {.enabled = true, .direction = REVERSE, .pv = &temperature}},
        {"ac1_w", .sched = {.enabled = false}, .hyst = {.enabled = true, .direction = FORWARD, .pv = &temperature}},
        {"ac2_g", .sched = {.enabled = false}, .hyst = {.enabled = false}},
        {"ac2_y", .sched = {.enabled = false}, .hyst = {.enabled = true, .direction = REVERSE, .pv = &temperature}},
        {"ac2_w", .sched = {.enabled = false}, .hyst = {.enabled = true, .direction = FORWARD, .pv = &temperature}},
        {"dh", .sched = {.enabled = false}, .hyst = {.enabled = true, .direction = REVERSE, .pv = &humidity}},
        {"co2", .sched = {.enabled = true}, .hyst = {.enabled = true, .direction = FORWARD, .pv = &co2}}};


// Next step... updating the configs over mqtt/populating from NVS

// sequential evaluation masks mapped on to outputs

// final stage of evaluation ends with a uint16 to send to mcp23017

int32_t get_current_tod() {
    time_t now_utc;
    time(&now_utc);
    struct tm timeinfo;
    localtime_r(&now_utc, &timeinfo);
    return timeinfo.tm_hour * 3600 + timeinfo.tm_min * 60 + timeinfo.tm_sec;
}

void update_sched_state(sched_config_t *s)
{
    time_t ctod = get_current_tod();
    s->state = (((ctod > s->on_time) && (ctod < s->off_time))           // if current time of day is between on and off time
                || ((s->off_time < s->on_time)                          // or, if on period spans into next day
                    && ((ctod < s->off_time) || (ctod > s->on_time)))); //
}

void update_hyst_state(hyst_config_t *h)
{
    float sp, db, os, on_at, off_at, pv;
    // scale int32_t to floats
    sp = h->setpoint / 10;
    db = h->deadband / 10;
    os = h->offset / 10;
    pv = *h->pv / 10;
    on_at = (sp + os - (h->direction * db / 2));
    off_at = (sp + os + (h->direction * db / 2));

    if (h->direction < 0)
        h->state = (pv >= on_at || (h->state && pv >= off_at)); // SR AND/OR Latch with Set Priority
    else if (h->direction > 0)
        h->state = (pv <= on_at || (h->state && pv <= off_at));
}

int32_t DUMMY;

config_item_t config[NUM_CONFIG_ITEMS] = {
    {"ac_g_mode", &(outputs[AC1_G].mode)},
    {"ac_y_mode", &(outputs[AC1_Y].mode)},
    {"ac_w_mode", &(outputs[AC1_W].mode)},
    {"dh_mode", &(outputs[DH].mode)},
    {"ef_mode", &DUMMY},
    {"co2_mode", &(outputs[CO2].mode)},
    {"cf_mode", &DUMMY},
    {"d_temp_sp", &DUMMY}, // this needs to get mapped to both the cooling and heating 
    {"n_temp_sp", &DUMMY}, // This is a special case, need to schedule a change 
    {"rh_sp", &(outputs[DH].hyst.setpoint)},
    {"co2_sp", &(outputs[CO2].hyst.setpoint)},
    {"co2_db", &(outputs[CO2].hyst.deadband)},
    {"co2_os", &(outputs[CO2].hyst.offset)},
    {"light_out_pct", &DUMMY},
    {"hitemp_dim", &DUMMY},
    {"hitemp_cutout", &DUMMY},
    {"hitemp_reset", &DUMMY},
    {"cool_db", &(outputs[AC1_Y].hyst.deadband)},
    {"cool_os", &(outputs[AC1_Y].hyst.offset)},
    {"heat_db", &(outputs[AC1_W].hyst.deadband)},
    {"heat_os", &(outputs[AC1_W].hyst.offset)},
    {"dh_db", &(outputs[DH].hyst.deadband)},
    {"dh_os", &(outputs[DH].hyst.offset)},
    {"co2_setback_s", &DUMMY},
    {"l_on_time_ts", &DUMMY},
    {"l_off_time_ts", &DUMMY},
    {"sr_len_s", &DUMMY},
    {"ss_len_s", &DUMMY},
};

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
            err = nvs_get_i32(handle, config[i].key, &value);
            switch (err)
            {
            case ESP_OK:
                // We have an initialized NVS key
                // Write the value into the config map
                // strcpy(config[i].key, keys[i]); Why?  These are pre initialized
                *config[i].value = value;
                break;
            case ESP_ERR_NVS_NOT_FOUND:
                printf("%s was not initialized yet!\n", config[i].key);
                nvs_set_i32(handle, config[i].key, 0);
                printf("%s initialized to 0.\n",config[i].key);
                break;
            default:
                printf("Error (%s) reading!\n", esp_err_to_name(err));
            }
        }
        printf("Committing config values to NVS.\n");
        nvs_commit(handle);
        nvs_close(handle);
    }
}

void print_config(void)
{
    for (int i = 0; i < NUM_CONFIG_ITEMS; i++)
    {
        printf("%s is %d\n", config[i].key, *(config[i].value));
    }
}

esp_err_t set_config(char *key, int32_t value)
{

    for (int i = 0; i < NUM_CONFIG_ITEMS; i++)
    {
        if (!strcmp(key, config[i].key))
        {
            *(config[i].value) = value;

            nvs_handle_t handle;
            esp_err_t err;

            // Open
            err = nvs_open("config", NVS_READWRITE, &handle);
            printf("%s:%d\n", config[i].key, *(config[i].value));
            if (err != ESP_OK)
                return err;
            nvs_set_i32(handle, key, value);
            nvs_commit(handle);
            nvs_close(handle);
        }
    }
    return ESP_OK;
}