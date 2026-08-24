#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_now.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_mac.h"
#include "esp_timer.h"
#include "esp_adc/adc_oneshot.h"
#include "driver/gpio.h"

#define TAG "FREENOVE_HUB"

// ==================== GPIO CONFIG ====================
#define JOY_X_CHANNEL   ADC_CHANNEL_6   // GPIO 34
#define JOY_Y_CHANNEL   ADC_CHANNEL_7   // GPIO 35
#define JOY_BTN_GPIO    32

// ==================== PACKET FROM SHIELD ====================
// Must match the sender struct exactly
typedef struct {
    uint8_t node_id;
    uint16_t seq_num;
    uint16_t emg[10];
} __attribute__((packed)) emg_batch_t;

// ==================== STATE (DUAL NODE) ====================
static volatile uint8_t emg_ready_1 = 0;
static volatile uint8_t emg_ready_2 = 0;

static volatile emg_batch_t latest_emg_1;
static volatile emg_batch_t latest_emg_2;

static adc_oneshot_unit_handle_t adc_handle = NULL;
static uint32_t hub_seq = 0;

// ==================== ESP-NOW ====================
static void esp_now_recv_cb(const esp_now_recv_info_t *info,
                            const uint8_t *data, int len) {
    if (len == sizeof(emg_batch_t)) {
        // Cast incoming data to read the node_id
        emg_batch_t *incoming = (emg_batch_t *)data;
        
        // Route to the correct buffer based on node_id
        if (incoming->node_id == 1) {
            memcpy((void*)&latest_emg_1, data, sizeof(emg_batch_t));
            emg_ready_1 = 1;
        } 
        else if (incoming->node_id == 2) {
            memcpy((void*)&latest_emg_2, data, sizeof(emg_batch_t));
            emg_ready_2 = 1;
        }
    }
}

static void wifi_init(void) {
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
}

static void esp_now_init_rx(void) {
    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_recv_cb(esp_now_recv_cb));
}

// ==================== ADC INIT ====================
static void adc_init(void) {
    adc_oneshot_unit_init_cfg_t init_cfg = {.unit_id = ADC_UNIT_1};
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_cfg, &adc_handle));

    adc_oneshot_chan_cfg_t chan_cfg = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_12,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, JOY_X_CHANNEL, &chan_cfg));
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, JOY_Y_CHANNEL, &chan_cfg));
}

// ==================== GPIO INIT ====================
static void gpio_init(void) {
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << JOY_BTN_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);
}

// ==================== MAIN TASK ====================
static void hub_task(void *pv) {
    TickType_t last_wake = xTaskGetTickCount();

    while (1) {
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(10));  // 100 Hz Loop

        uint32_t ts = (uint32_t)(esp_timer_get_time() / 1000);

        // Read joystick
        int joy_x = 0, joy_y = 0;
        adc_oneshot_read(adc_handle, JOY_X_CHANNEL, &joy_x);
        adc_oneshot_read(adc_handle, JOY_Y_CHANNEL, &joy_y);
        int btn = gpio_get_level(JOY_BTN_GPIO);

        // Buffers for printing
        uint16_t emg_data_1[10] = {0};
        uint16_t emg_data_2[10] = {0};

        // Pull EMG 1 if available
        if (emg_ready_1) {
            memcpy(emg_data_1, (void*)latest_emg_1.emg, sizeof(emg_data_1));
            emg_ready_1 = 0;
        }
        
        // Pull EMG 2 if available
        if (emg_ready_2) {
            memcpy(emg_data_2, (void*)latest_emg_2.emg, sizeof(emg_data_2));
            emg_ready_2 = 0;
        }

        // CSV Format: seq, timestamp, emg1[0..9], emg2[0..9], joy_x, joy_y, joy_btn
        printf("%lu,%lu", (unsigned long)hub_seq++, (unsigned long)ts);
        
        // Print Node 1
        for (int i = 0; i < 10; i++) {
            printf(",%u", emg_data_1[i]);
        }
        
        // Print Node 2
        for (int i = 0; i < 10; i++) {
            printf(",%u", emg_data_2[i]);
        }
        
        // Print Joystick
        printf(",%d,%d,%d\n", joy_x, joy_y, btn);
        fflush(stdout);
    }
}

// ==================== MAIN ====================
void app_main(void) {
    ESP_ERROR_CHECK(nvs_flash_init());
    wifi_init();
    esp_now_init_rx();
    adc_init();
    gpio_init();

    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    ESP_LOGI(TAG, "FreeNove Hub MAC: %02x:%02x:%02x:%02x:%02x:%02x",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    ESP_LOGI(TAG, "Joystick: VRX=GPIO34, VRY=GPIO35, SW=GPIO32");
    ESP_LOGI(TAG, "Waiting for dual shield EMG data + reading joystick...");

    xTaskCreate(hub_task, "hub", 4096, NULL, 5, NULL);

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}