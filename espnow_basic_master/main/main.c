
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_now.h"
#include "sdkconfig.h"

#include <inttypes.h>
#include <stdbool.h>

// Define the structure of your data
typedef struct __attribute__((packed)) {

    uint32_t random_value;
    bool button_pushed;

} my_data_t;

#define MY_ESPNOW_PMK "pmk1234567890123"
#define MY_ESPNOW_CHANNEL 1

#define MY_ESPNOW_ENABLE_LONG_RANGE 1


static const char *TAG = "NEO";

void onDataReceive(const uint8_t *sender_mac_addr, const my_data_t *data);

static xQueueHandle s_recv_queue;

typedef struct{

    uint8_t sender_mac_addr[ESP_NOW_ETH_ALEN];
    my_data_t data;

}recv_packet_t;

static void queue_process_task(void *p)
{
    static recv_packet_t recv_packet;

    ESP_LOGI(TAG, "I can listen...");

    for(;;)
    {
        if(xQueueReceive(s_recv_queue,&recv_packet,portMAX_DELAY) != pdTRUE)
        {
            continue;
        }
        onDataReceive(recv_packet.sender_mac_addr, &recv_packet.data);
    }
}

void onDataReceive(const uint8_t *sender_mac_addr, const my_data_t *data)
{
    ESP_LOGI(TAG, "Data from "MACSTR": Random Value - %u, Button - %s",
                MAC2STR(sender_mac_addr), 
                data->random_value, 
                data->button_pushed ? "Pushed" : "Released");
                
}

#define MY_ESPNOW_WIFI_MODE WIFI_MODE_STA
#define MY_ESPNOW_WIFI_IF   ESP_IF_WIFI_STA
// #define MY_ESPNOW_WIFI_MODE WIFI_MODE_AP
// #define MY_ESPNOW_WIFI_IF   ESP_IF_WIFI_AP

static void recv_cb(const uint8_t *mac_addr, const uint8_t *data, int len)
{
    static recv_packet_t recv_packet;

    ESP_LOGI(TAG,"%d bytes incoming from" MACSTR , len, MAC2STR(mac_addr));

    if(len != sizeof(my_data_t))
    {
        ESP_LOGE(TAG, "Damn length is wrong bro...: %d != %u", len, sizeof(my_data_t));
        return;
    }

    memcpy(&recv_packet.sender_mac_addr, mac_addr, sizeof(recv_packet.sender_mac_addr));
    memcpy(&recv_packet.data, data, len);

    if(xQueueSend(s_recv_queue,&recv_packet,0) != pdTRUE)
    {
        ESP_LOGW(TAG, "QUEUE TA CHEIA BRO, recv discarded");
        return;
    }
}

static void init_espnow_master(void)
{
    const wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();

    esp_err_t ret = nvs_flash_init();
    if(ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK( nvs_flash_erase() );
        ret = nvs_flash_init();
    }

    ESP_ERROR_CHECK( ret );
    ESP_ERROR_CHECK( esp_netif_init() );
    ESP_ERROR_CHECK( esp_event_loop_create_default() );
    ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
    ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_RAM) );
    ESP_ERROR_CHECK( esp_wifi_set_mode( MY_ESPNOW_WIFI_MODE) );
    ESP_ERROR_CHECK( esp_wifi_start() );
#if MY_ESPNOW_ENABLE_LONG_RANGE
    ESP_ERROR_CHECK( esp_wifi_set_protocol(MY_ESPNOW_WIFI_IF, WIFI_PROTOCOL_11B|WIFI_PROTOCOL_11G|WIFI_PROTOCOL_11N|WIFI_PROTOCOL_LR) );
#endif
    ESP_ERROR_CHECK( esp_now_init() );
    ESP_ERROR_CHECK( esp_now_register_recv_cb(recv_cb)) ;
    ESP_ERROR_CHECK( esp_now_set_pmk( (const uint8_t *)MY_ESPNOW_PMK ));

    s_recv_queue = xQueueCreate(10, sizeof(recv_packet_t));
    assert(s_recv_queue);
    BaseType_t err = xTaskCreatePinnedToCore(queue_process_task, "recv_task", 8192, NULL, 4, NULL,tskNO_AFFINITY);
    assert(err == pdPASS);

}
void app_main(void)
{
    init_espnow_master();
    //clean build

}
