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
#include "driver/gptimer.h"

#define TAG "SHIELD_TX"
#define NODE_ID 2   // Change to 2 for the second shield

// FreeNove MAC address
static uint8_t c3_mac[] = {0xD0, 0xEF, 0x76, 0x1F, 0x83, 0x2C};  // <-- FreeNove

#define EMG_CHANNEL ADC_CHANNEL_0   // GPIO 36, RAW EMG
#define BATCH_SIZE 10

typedef struct {
    uint8_t node_id;
    uint16_t seq_num;
    uint16_t emg[BATCH_SIZE];   // 10 samples @ 1kHz
} __attribute__((packed)) emg_batch_t;

static adc_oneshot_unit_handle_t adc_handle;
static volatile uint16_t emg_buffer[2][BATCH_SIZE];  // double buffer
static volatile uint8_t write_buf = 0;
static volatile uint8_t sample_idx = 0;
static volatile uint8_t ready_buf = 0;   // 0=none, 1=buf0, 2=buf1
static uint16_t sequence = 0;

// ==================== GPTimer ISR (Core 0) ====================
static bool IRAM_ATTR emg_timer_cb(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata, void *user_ctx) {
    int raw = 0;
    adc_oneshot_read(adc_handle, EMG_CHANNEL, &raw);
    
    emg_buffer[write_buf][sample_idx] = (uint16_t)raw;
    sample_idx++;
    
    if (sample_idx >= BATCH_SIZE) {
        sample_idx = 0;
        ready_buf = write_buf + 1;  // 1 or 2
        write_buf = (write_buf + 1) % 2;
    }
    
    return true;  // reload alarm
}

// ==================== ESP-NOW ====================
static void esp_now_send_cb(const wifi_tx_info_t *info, esp_now_send_status_t status) {}

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

// ==================== ADC ====================
static void adc_init(void) {
    adc_oneshot_unit_init_cfg_t init_cfg = {.unit_id = ADC_UNIT_1};
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_cfg, &adc_handle));
    
    adc_oneshot_chan_cfg_t chan_cfg = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_12,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, EMG_CHANNEL, &chan_cfg));
}

// ==================== Timer ====================
static void timer_init(void) {
    gptimer_handle_t timer = NULL;
    gptimer_config_t timer_config = {
        .clk_src = GPTIMER_CLK_SRC_DEFAULT,
        .direction = GPTIMER_COUNT_UP,
        .resolution_hz = 1000000,  // 1MHz = 1µs tick
    };
    ESP_ERROR_CHECK(gptimer_new_timer(&timer_config, &timer));
    
    gptimer_alarm_config_t alarm_config = {
        .reload_count = 0,
        .alarm_count = 1000,  // 1000µs = 1ms
        .flags.auto_reload_on_alarm = true,
    };
    ESP_ERROR_CHECK(gptimer_set_alarm_action(timer, &alarm_config));
    ESP_ERROR_CHECK(gptimer_register_event_callbacks(timer, 
        &(gptimer_event_callbacks_t){.on_alarm = emg_timer_cb}, NULL));
    ESP_ERROR_CHECK(gptimer_enable(timer));
    ESP_ERROR_CHECK(gptimer_start(timer));
}

// ==================== Main ====================
void app_main(void) {
    ESP_ERROR_CHECK(nvs_flash_init());
    wifi_init();
    esp_now_init_sender();
    adc_init();
    timer_init();
    
    uint8_t my_mac[6];
    esp_read_mac(my_mac, ESP_MAC_WIFI_STA);
    ESP_LOGI(TAG, "Shield MAC: %02x:%02x:%02x:%02x:%02x:%02x",
             my_mac[0], my_mac[1], my_mac[2], my_mac[3], my_mac[4], my_mac[5]);
    ESP_LOGI(TAG, "Target C3 MAC: %02x:%02x:%02x:%02x:%02x:%02x",
             c3_mac[0], c3_mac[1], c3_mac[2], c3_mac[3], c3_mac[4], c3_mac[5]);
    ESP_LOGI(TAG, "EMG sender ready, 1kHz ISR, 100Hz batch TX");
    
    emg_batch_t batch;
    
    while (1) {
        // Wait for buffer ready
        while (ready_buf == 0) {
            vTaskDelay(pdMS_TO_TICKS(1));
        }
        
        uint8_t buf_to_send = ready_buf - 1;  // 0 or 1
        ready_buf = 0;  // Clear flag
        batch.node_id = NODE_ID;
        batch.seq_num = sequence++;
        memcpy(batch.emg, (void*)emg_buffer[buf_to_send], sizeof(batch.emg));
        
        esp_err_t result = esp_now_send(c3_mac, (uint8_t*)&batch, sizeof(batch));
        if (result != ESP_OK) {
            ESP_LOGE(TAG, "Send failed: %s", esp_err_to_name(result));
        }
        
        // ~10ms period maintained by buffer production rate
    }
}