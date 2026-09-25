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
#include "esp_crt_bundle.h" // 🔒 Mandatory header for GitHub HTTPS validation

#define TOUCH_SENSOR_PIN   GPIO_NUM_5
#define WIFI_SSID          "Shivam"
#define WIFI_PASS          "12345678"

// 🎯 GitHub browser se copy kiya hua dynamic authentic link
#define DEPLOYED_FIRMWARE_URL "https://github.com/shivam-robotics/EON/blob/main/sample_project/build/sample_project.bin"

static const char *TAG = "EON_ROVER_CORE";
static EventGroupHandle_t s_wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0

static bool wifi_is_running = false;

void suspend_wifi_radio_stack(void) {
    if (wifi_is_running) {
        ESP_LOGW(TAG, "🔋 Touch LOW triggered: Suspending RF Radios... Disabling Wi-Fi driver.");
        esp_wifi_disconnect();
        esp_wifi_stop();
        wifi_is_running = false;
        ESP_LOGI(TAG, "🌙 Radio stack shut down. System entered Low Power Standby Protocol Mode.");
    }
}

void resume_wifi_radio_stack(void) {
    if (!wifi_is_running) {
        ESP_LOGI(TAG, "🌐 Touch HIGH triggered: Reactivating Wi-Fi Station Infrastructure...");
        esp_wifi_start();
        wifi_is_running = true;
    }
}

static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
        wifi_is_running = true;
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        if (wifi_is_running) {
            esp_wifi_connect();
            ESP_LOGW(TAG, "🔄 Re-connecting to network mesh...");
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "📡 Linked! Local Allocation Node IP: " IPSTR, IP2STR(&event->ip_info.ip));
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

void initialize_network_drivers(void) {
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

    esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
    esp_wifi_set_max_tx_power(56); // MacBook USB Tweak (14dBm)
}

void launch_espressif_prebuilt_ota(void) {
    ESP_LOGI(TAG, "🚀 Invoking Espressif Native Engine Cloud Client. Initializing secure handshake...");
    
    esp_http_client_config_t http_config = {
        .url = DEPLOYED_FIRMWARE_URL,
        .timeout_ms = 15000,
        .keep_alive_enable = true,
    };

    esp_https_ota_config_t ota_config = { .http_config = &http_config };

    esp_err_t ret = esp_https_ota(&ota_config);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "🟢 Espressif Cloud Verification Verified! Rebooting EON Rover Core...");
        vTaskDelay(pdMS_TO_TICKS(2000));
        esp_restart();
    } else {
        ESP_LOGE(TAG, "❌ Cloud Engine check dropped. Code status: %s", esp_err_to_name(ret));
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

    ESP_LOGI(TAG, "================================================");
    ESP_LOGI(TAG, "🤖 EON Cubic Rover - Cloud Native Secure Core Ready");
    ESP_LOGI(TAG, "================================================");

    initialize_network_drivers();
    xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT, pdFALSE, pdTRUE, pdMS_TO_TICKS(10000));
    
    launch_espressif_prebuilt_ota();

    ESP_LOGI(TAG, "🚀 Maintenance Phase Complete. Entering active operational loops...");

    while (1) {
        int touch_signal = gpio_get_level(TOUCH_SENSOR_PIN);
        
        if (touch_signal == 1) {
            resume_wifi_radio_stack();
            ESP_LOGI(TAG, "[Telemetry Matrix: HIGH] Wi-Fi Active. Streaming ROS2 control logs.");
        } else {
            suspend_wifi_radio_stack();
        }
        
        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}
