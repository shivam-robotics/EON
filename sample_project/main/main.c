#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"

#define TOUCH_SENSOR_PIN   GPIO_NUM_5
#define WIFI_SSID          "Shivam"
#define WIFI_PASS          "12345678"

// 🎯 GitHub Releases ya custom cloud storage ka direct configuration update URL link endpoint
#define DEPLOYED_FIRMWARE_URL "https://github.com/shivam-robotics/EON/blob/main/sample_project/build/sample_project.bin"

static const char *TAG = "EON_NATIVE_OTA";
static EventGroupHandle_t s_wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0

static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        esp_wifi_connect();
        xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        ESP_LOGW(TAG, "🔄 Re-connecting to system hotspot matrix...");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "📡 Linked! Local Allocation Node IP: " IPSTR, IP2STR(&event->ip_info.ip));
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

// 🚀 Absolute 0-Custom Code Native Espressif Engine Call (Standard Industry Practice)
// Highly Stable 0-Custom Code Native Espressif Prebuilt OTA Framework Configuration
void launch_espressif_prebuilt_ota(void) {
    ESP_LOGI(TAG, "🚀 Invoking Espressif Native Engine Core. Initializing update check...");
    
    // 1. Core Network Client parameter bindings
    esp_http_client_config_t http_config = {
        .url = DEPLOYED_FIRMWARE_URL,
        .timeout_ms = 10000,
        .keep_alive_enable = true,
    };

    // 2. Clear clean type configuration mapping optimized for stable toolchains
    // Variable constraints handling will automatically register via global sdkconfig flags!
    esp_https_ota_config_t ota_config = {
        .http_config = &http_config,
    };

    // Espressif native background handles execute validation parameters here
    esp_err_t ret = esp_https_ota(&ota_config);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "🟢 Espressif Native Verification Verified! Rebooting EON Core...");
        vTaskDelay(pdMS_TO_TICKS(2000));
        esp_restart();
    } else {
        ESP_LOGE(TAG, "❌ Espressif Core Engine check dropped. Code status: %s", esp_err_to_name(ret));
    }
}


void app_main(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    gpio_reset_pin(TOUCH_SENSOR_PIN);
    gpio_set_direction(TOUCH_SENSOR_PIN, GPIO_MODE_INPUT);

    s_wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL));

    wifi_config_t wifi_config = {
        .sta = { .ssid = WIFI_SSID, .password = WIFI_PASS },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    // MacBook USB low transient protection calibrations
    esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
    esp_wifi_set_max_tx_power(56); // 14dBm Limit

    // Wait until network connection flag locks down successfully
    xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT, pdFALSE, pdTRUE, pdMS_TO_TICKS(10000));
    
    // Jump straight into Espressif native background tool check routine
    launch_espressif_prebuilt_ota();

    ESP_LOGI(TAG, "🚀 Main Core Logic Telemetry Processing Triggered.");
    while (1) {
        int touch_signal = gpio_get_level(TOUCH_SENSOR_PIN);
        if (touch_signal == 1) {
            ESP_LOGI(TAG, "[Status: HIGH] Wi-Fi Open for High Bandwidth Control.");
        } else {
            ESP_LOGI(TAG, "[Status: LOW] Power Switch Logic Matrix Active.");
        }
        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}
