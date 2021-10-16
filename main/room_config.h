#include "nvs_flash.h"
#include "nvs.h"

#define NUM_BUCKETS 4
#define NUM_CONFIG_ITEMS 28
#define MAX_KEY_LENGTH 16

extern char keys[NUM_CONFIG_ITEMS][MAX_KEY_LENGTH];

esp_err_t nvs_init(nvs_handle_t handle);

void init_config(void);
esp_err_t set_config(char *key, int32_t value);

