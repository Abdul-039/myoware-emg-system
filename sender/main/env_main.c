#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_now.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_mac.h"
#include "esp_adc/adc_oneshot.h"

#define TAG "ENV_TX"
#define NODE_ID 2
#define ENV_CHANNEL ADC_CHANNEL_0
#define SAMPLE_PERIOD_MS 10

// FreeNove MAC address
static uint8_t c3_mac[] = {0xD0, 0xEF, 0x76, 0x1F, 0x83, 0x2C};

typedef struct {
    uint8_t node_id;
    uint16_t seq_num;
    uint16_t env_value;
} __attribute__((packed)) env_packet_t;

static adc_oneshot_unit_handle_t adc_handle;
static uint16_t sequence = 0;

static void esp_now_send_cb(const wifi_tx_info_t *info, esp_now_send_status_t status) {
    // Optional: keep empty if you don't need status
}

static void wifi_init(void) {
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
}

static void esp_now_init_sender(void) {
    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_send_cb(esp_now_send_cb));

    esp_now_peer_info_t peer = {
        .channel = 0,
        .encrypt = false,
    };

    memcpy(peer.peer_addr, c3_mac, 6);
    ESP_ERROR_CHECK(esp_now_add_peer(&peer));
}

static void adc_init(void) {
    adc_oneshot_unit_init_cfg_t init_cfg = {
        .unit_id = ADC_UNIT_1,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_cfg, &adc_handle));

    adc_oneshot_chan_cfg_t chan_cfg = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_12,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, ENV_CHANNEL, &chan_cfg));
}

void app_main(void) {
    ESP_ERROR_CHECK(nvs_flash_init());
    wifi_init();
    esp_now_init_sender();
    adc_init();

    uint8_t my_mac[6];
    esp_read_mac(my_mac, ESP_MAC_WIFI_STA);
    ESP_LOGI(TAG, "Shield MAC: %02x:%02x:%02x:%02x:%02x:%02x",
             my_mac[0], my_mac[1], my_mac[2], my_mac[3], my_mac[4], my_mac[5]);
    ESP_LOGI(TAG, "Target C3 MAC: %02x:%02x:%02x:%02x:%02x:%02x",
             c3_mac[0], c3_mac[1], c3_mac[2], c3_mac[3], c3_mac[4], c3_mac[5]);
    ESP_LOGI(TAG, "ENV sender ready: 100 Hz, one sample per packet");

    while (1) {
        int raw = 0;
        esp_err_t ret = adc_oneshot_read(adc_handle, ENV_CHANNEL, &raw);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "ADC read failed: %s", esp_err_to_name(ret));
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        env_packet_t packet = {
            .node_id = NODE_ID,
            .seq_num = sequence++,
            .env_value = (uint16_t)raw,
        };

        esp_err_t send_result = esp_now_send(c3_mac, (uint8_t *)&packet, sizeof(packet));
        if (send_result != ESP_OK) {
            ESP_LOGE(TAG, "Send failed: %s", esp_err_to_name(send_result));
        }

        vTaskDelay(pdMS_TO_TICKS(SAMPLE_PERIOD_MS));
    }
}
