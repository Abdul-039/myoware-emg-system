#include <stdio.h>
#include "esp_log.h"
#include "esp_mac.h"
#include "nvs_flash.h"

#define TAG "MAC"

void app_main(void) {
    ESP_ERROR_CHECK(nvs_flash_init());
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    ESP_LOGI(TAG, "FreeNove MAC: %02x:%02x:%02x:%02x:%02x:%02x",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}
