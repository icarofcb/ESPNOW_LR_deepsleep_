#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_now.h"
#include "esp_sleep.h"
#include <inttypes.h>
#include <stdbool.h>
#include "esp_system.h"
#include "esp_log.h"
#include "driver/rtc_io.h"

#include "sdkconfig.h"

// Define the structure of your data
typedef struct __attribute__((packed)) {
    uint32_t random_value;
    bool button_pushed;
} my_data_t;

#define MY_RECEIVER_MAC {0x24, 0x62, 0xAB, 0xFB, 0x25, 0xEC}

#define MY_ESPNOW_PMK "pmk1234567890123"
#define MY_ESPNOW_CHANNEL 1

#define MY_ESPNOW_ENABLE_LONG_RANGE 1

#define MY_SLAVE_DEEP_SLEEP_TIME_MS 5000

static const char *TAG = "MORPHEUS";

void my_data_prepare(my_data_t *data);

static EventGroupHandle_t s_evt_group;

#define MY_ESPNOW_WIFI_MODE WIFI_MODE_STA
#define MY_ESPNOW_WIFI_IF   ESP_IF_WIFI_STA
// #define MY_ESPNOW_WIFI_MODE WIFI_MODE_AP
// #define MY_ESPNOW_WIFI_IF   ESP_IF_WIFI_AP

static void onDataSend(const uint8_t *mac_addr, esp_now_send_status_t status)
{
    if(mac_addr == NULL)
    {
        ESP_LOGE(TAG,"I can't see anything");
        return;
    }

    assert(status == ESP_NOW_SEND_SUCCESS || status == ESP_NOW_SEND_FAIL);
    xEventGroupSetBits(s_evt_group, BIT(status));
}

void my_data_prepare(my_data_t *data)
{
    ESP_LOGI(TAG, "creating random values, and try pressing boot button");
    data->random_value = esp_random();
    data->button_pushed = (rtc_gpio_get_level(GPIO_NUM_0) == 0);
}

static esp_err_t send_espnow_data(void)
{
    const uint8_t destination_mac[] = MY_RECEIVER_MAC;
    static my_data_t data;

    my_data_prepare(&data);

    ESP_LOGI(TAG, "Wake Up NEO ... sending %u bytes to "MACSTR, sizeof(data), MAC2STR(destination_mac));
    esp_err_t err = esp_now_send(destination_mac,(uint8_t *)&data, sizeof(data) );
    if(err != ESP_OK)
    {
        ESP_LOGE(TAG, "NEO, why can't I call you");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Follow the white rabbit");
    return ESP_OK;
}

static void init_espnow_slave(void)
{
    const wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();


    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK( nvs_flash_erase() );
        ret = nvs_flash_init();
    }
    
    ESP_ERROR_CHECK( ret );
    ESP_ERROR_CHECK( esp_netif_init() );
    ESP_ERROR_CHECK( esp_event_loop_create_default() );
    ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
    ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_RAM) );
    ESP_ERROR_CHECK( esp_wifi_set_mode(MY_ESPNOW_WIFI_MODE) );
    ESP_ERROR_CHECK( esp_wifi_start() );
#if MY_ESPNOW_ENABLE_LONG_RANGE
    ESP_ERROR_CHECK( esp_wifi_set_protocol(MY_ESPNOW_WIFI_IF, WIFI_PROTOCOL_11B|WIFI_PROTOCOL_11G|WIFI_PROTOCOL_11N|WIFI_PROTOCOL_LR) );
#endif
    ESP_ERROR_CHECK( esp_now_init() );
    ESP_ERROR_CHECK( esp_now_register_send_cb(onDataSend) );
    ESP_ERROR_CHECK( esp_now_set_pmk((const uint8_t *)MY_ESPNOW_PMK) );

    // Alter this if you want to specify the gateway mac, enable encyption, etc
    const esp_now_peer_info_t broadcast_destination = {
        .peer_addr = MY_RECEIVER_MAC,
        .channel = MY_ESPNOW_CHANNEL,
        .ifidx = MY_ESPNOW_WIFI_IF
    };
    ESP_ERROR_CHECK( esp_now_add_peer(&broadcast_destination) );

}



void app_main(void)
{
    s_evt_group = xEventGroupCreate();
    assert(s_evt_group);

    init_espnow_slave();
    send_espnow_data();

    esp_deep_sleep(1000ULL * MY_SLAVE_DEEP_SLEEP_TIME_MS);
}