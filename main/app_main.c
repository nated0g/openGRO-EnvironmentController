/* MQTT (over TCP) Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#define SDA_GPIO 13
#define SCL_GPIO 16
#define PIN_PHY_POWER 12

#define TOPIC_PREFIX "devices/"
#define TEMP_DEVICE_ID "1234567890ab"

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "esp_system.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_eth.h"
#include "sdkconfig.h"
#include "nvs_flash.h"
#include "nvs.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"

#include "esp_log.h"
#include "mqtt_client.h"

#include "driver/gpio.h"
#include "room_config.h"
#include "app_main.h"

static const char *TAG = "og-room-controller";

static xQueueHandle scd30_data_queue;
esp_mqtt_client_handle_t mqtt_client;

esp_err_t MQTT_OK = ESP_FAIL;

esp_err_t mqtt_message_receive(void *event_data);

static void log_error_if_nonzero(const char *message, int error_code)
{
    if (error_code != 0)
    {
        ESP_LOGE(TAG, "Last error %s: 0x%x", message, error_code);
    }
}

/** Event handler for Ethernet events */
static void eth_event_handler(void *arg, esp_event_base_t event_base,
                              int32_t event_id, void *event_data)
{
    uint8_t mac_addr[6] = {0};
    /* we can get the ethernet driver handle from event data */
    esp_eth_handle_t eth_handle = *(esp_eth_handle_t *)event_data;

    switch (event_id)
    {
    case ETHERNET_EVENT_CONNECTED:
        esp_eth_ioctl(eth_handle, ETH_CMD_G_MAC_ADDR, mac_addr);
        ESP_LOGI(TAG, "Ethernet Link Up");
        ESP_LOGI(TAG, "Ethernet HW Addr %02x:%02x:%02x:%02x:%02x:%02x",
                 mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
        break;
    case ETHERNET_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "Ethernet Link Down");
        break;
    case ETHERNET_EVENT_START:
        ESP_LOGI(TAG, "Ethernet Started");
        break;
    case ETHERNET_EVENT_STOP:
        ESP_LOGI(TAG, "Ethernet Stopped");
        break;
    default:
        break;
    }
}

/** Event handler for IP_EVENT_ETH_GOT_IP */
static void got_ip_event_handler(void *arg, esp_event_base_t event_base,
                                 int32_t event_id, void *event_data)
{
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
    const esp_netif_ip_info_t *ip_info = &event->ip_info;

    ESP_LOGI(TAG, "Ethernet Got IP Address");
    ESP_LOGI(TAG, "~~~~~~~~~~~");
    ESP_LOGI(TAG, "ETHIP:" IPSTR, IP2STR(&ip_info->ip));
    ESP_LOGI(TAG, "ETHMASK:" IPSTR, IP2STR(&ip_info->netmask));
    ESP_LOGI(TAG, "ETHGW:" IPSTR, IP2STR(&ip_info->gw));
    ESP_LOGI(TAG, "~~~~~~~~~~~");
}

/*
 * @brief Event handler registered to receive MQTT events
 *
 *  This function is called by the MQTT client event loop.
 *
 * @param handler_args user data registered to the event.
 * @param base Event base for the handler(always MQTT Base in this example).
 * @param event_id The id for the received event.
 * @param event_data The data for the event, esp_mqtt_event_handle_t.
 */
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%d", base, event_id);
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;
    switch ((esp_mqtt_event_id_t)event_id)
    {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        mqtt_sub_config_topics();
        MQTT_OK = ESP_OK;
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        MQTT_OK = ESP_FAIL;
        break;

    case MQTT_EVENT_SUBSCRIBED:
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        break;
    case MQTT_EVENT_PUBLISHED:
        break;
    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");
        mqtt_message_receive((void *)event_data);
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT)
        {
            log_error_if_nonzero("reported from esp-tls", event->error_handle->esp_tls_last_esp_err);
            log_error_if_nonzero("reported from tls stack", event->error_handle->esp_tls_stack_err);
            log_error_if_nonzero("captured as transport's socket errno", event->error_handle->esp_transport_sock_errno);
            ESP_LOGI(TAG, "Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));
        }
        break;
    default:
        ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        break;
    }
}

esp_err_t mqtt_sub_config_topics(void)
{
    // Wow this is all very unneccessary, just use a wildcard
    /*
    char *prefix = "devices/";
    char topic_prefix[strlen(prefix) + strlen(TEMP_DEVICE_ID) + 2];
    strcpy(topic_prefix, prefix);
    strcat(topic_prefix, TEMP_DEVICE_ID);
    for (int i = 0; i < NUM_CONFIG_ITEMS; i++)
    {
        char topic[strlen(topic_prefix) + MAX_KEY_LENGTH];
        sprintf(topic, "%s/%s", topic_prefix, keys[i]);

        printf("Subscribing to %s\n", topic);

        esp_mqtt_client_subscribe(mqtt_client, topic, 0);
    }
    */
    esp_mqtt_client_subscribe(mqtt_client, "devices/1234567890ab/#", 0);
    return ESP_OK;
}

esp_err_t mqtt_message_receive(void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;
    char topic[event->topic_len + 1];
    sprintf(topic, "%.*s", event->topic_len, event->topic);
    //"devices/xxxxxxxxxxxxxx/settings/ac_g_mode/set"
    char *rest = NULL;
    char *token;
    char last[MAX_KEY_LENGTH];
    int val = 0;
    // use strtok_r instead of strtok for thread safety
    for (token = strtok_r(topic, "/", &rest);
         token != NULL;
         token = strtok_r(NULL, "/", &rest))
    {
        // once we get to the final level, check the previous
        // level for which config item to update
        if (!strcmp(token, "set"))
        {
            char data[event->data_len + 1];
            sprintf(data, "%.*s", event->data_len, event->data);
            val = strtol(data, NULL, 10);
            set_config(last, val);
        }
        strcpy(last, token);
    }
    return ESP_OK;
}

static void mqtt_app_start(void)
{
    esp_mqtt_client_config_t mqtt_cfg = {
        .uri = CONFIG_BROKER_URL,
    };

    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    /* The last argument may be used to pass data to the event handler, in this example mqtt_event_handler */
    esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(mqtt_client);
}
/*
void task_mqtt_report(void *pvParameters)
{
    char data[200];
    while (1)
    {
        if (MQTT_OK == ESP_OK)
        {
            scd30_data_t env_data;
            BaseType_t queue_receive_result;

            queue_receive_result = xQueueReceive(scd30_data_queue, &env_data, portMAX_DELAY);

            if (queue_receive_result != pdPASS)
            {
                ESP_LOGE(TAG, "Receiving from scd30_data_queue failed.");
            }
            sprintf(data, "{\"temperature\":%0.2f, \"humidity\":%0.1f, \"co2\":%0.0f}", env_data.temperature, env_data.humidity, env_data.co2);
            ESP_LOGI(TAG, "{\"temp\":%0.2f}", env_data.temperature);
            esp_mqtt_client_publish(mqtt_client, SCD30_DATA_TOPIC, data, 0, 0, 0);
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
*/
void set_esp_log_levels(void)
{
    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("MQTT_CLIENT", ESP_LOG_VERBOSE);
    esp_log_level_set("MQTT_EXAMPLE", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT_BASE", ESP_LOG_VERBOSE);
    esp_log_level_set("esp-tls", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT", ESP_LOG_VERBOSE);
    esp_log_level_set("OUTBOX", ESP_LOG_VERBOSE);
}

void print_config_task(void)
{
    while (1)
    {
        vTaskDelay(10000 / portTICK_PERIOD_MS);
        print_config();
    }
}

void app_main(void)
{

    xTaskCreatePinnedToCore(print_config_task, "print_config_task", 1024 * 16, NULL, 5, NULL, APP_CPU_NUM);

    set_esp_log_levels();
    fflush(stdout);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    esp_netif_config_t cfg = ESP_NETIF_DEFAULT_ETH();
    esp_netif_t *eth_netif = esp_netif_new(&cfg);
    // Set default handlers to process TCP/IP stuffs
    ESP_ERROR_CHECK(esp_eth_set_default_handlers(eth_netif));
    // Register user defined event handers
    ESP_ERROR_CHECK(esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, &eth_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, &got_ip_event_handler, NULL));

    eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
    eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();
    phy_config.phy_addr = CONFIG_EXAMPLE_ETH_PHY_ADDR;
    phy_config.reset_gpio_num = CONFIG_EXAMPLE_ETH_PHY_RST_GPIO;

    gpio_pad_select_gpio(PIN_PHY_POWER);
    gpio_set_direction(PIN_PHY_POWER, GPIO_MODE_OUTPUT);
    gpio_set_level(PIN_PHY_POWER, 1);

    mac_config.smi_mdc_gpio_num = CONFIG_EXAMPLE_ETH_MDC_GPIO;
    mac_config.smi_mdio_gpio_num = CONFIG_EXAMPLE_ETH_MDIO_GPIO;
    esp_eth_mac_t *mac = esp_eth_mac_new_esp32(&mac_config);
    esp_eth_phy_t *phy = esp_eth_phy_new_lan8720(&phy_config);
    esp_eth_config_t config = ETH_DEFAULT_CONFIG(mac, phy);
    esp_eth_handle_t eth_handle = NULL;
    ESP_ERROR_CHECK(esp_eth_driver_install(&config, &eth_handle));
    // attach Ethernet driver to TCP/IP stack
    ESP_ERROR_CHECK(esp_netif_attach(eth_netif, esp_eth_new_netif_glue(eth_handle)));
    // start Ethernet driver state machine
    ESP_ERROR_CHECK(esp_eth_start(eth_handle));

    //ESP_ERROR_CHECK(i2cdev_init());
    mqtt_app_start();
    init_config();
    //xTaskCreatePinnedToCore(task_mqtt_report, "mqtt_report", 1024 * 36, NULL, 5, NULL, APP_CPU_NUM);
}
