#include "nvs_flash.h"
#include "nvs.h"

#define NUM_BUCKETS 4
#define NUM_CONFIG_ITEMS 28
#define MAX_KEY_LENGTH 16

#define  NUM_OUTPUTS 8

#define FORWARD 1
#define REVERSE -1

#define OFF 0
#define ON 1


#define OFF_MODE 0
#define MANUAL_MODE 1
#define AUTO_MODE 2

esp_err_t nvs_init(nvs_handle_t handle);

extern int32_t temperature;
extern int32_t humidity;
extern int32_t co2;

void init_config(void);
void print_config(void);
esp_err_t set_config(char *key, int32_t value);

typedef struct
{
    char key[MAX_KEY_LENGTH];
    int32_t *value;
    // could we just hold a pointer here and keep the variable in the config structs?
} config_item_t;

typedef struct
{
    bool enabled, state;
    int8_t direction;
    int32_t setpoint, deadband, offset; // scaled up by a factor of 10
    int32_t *pv;
} hyst_config_t;

typedef struct
{
    bool enabled, state;
    int32_t on_time, off_time;
} sched_config_t;

typedef struct
{
    char key[MAX_KEY_LENGTH];
    int32_t mode;
    uint8_t state;
    sched_config_t sched;
    hyst_config_t hyst;
} output_config_t;

void update_sched_state(sched_config_t *s);

void update_hyst_state(hyst_config_t *h);
extern output_config_t outputs[NUM_OUTPUTS];