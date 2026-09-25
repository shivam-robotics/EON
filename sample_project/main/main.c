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
#include "esp_ota_ops.h"

#define TOUCH_SENSOR_PIN   GPIO_NUM_5
#define WIFI_SSID          "Shivam"
#define WIFI_PASS          "12345678"

// 🎯 GitHub browser se copy kiya hua exact RAW update endpoint
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
    esp_wifi_set_max_tx_power(56); // MacBook low-power safety config
}

// 🎯 Raw Low-Level HTTP Flash Writer with Strict SSL Certificate Skip
void launch_espressif_prebuilt_ota(void) {
    ESP_LOGI(TAG, "🚀 Invoking Low-Level Raw HTTP Client. Launching firmware stream fetch...");
    
    esp_http_client_config_t config = {
        .url = DEPLOYED_FIRMWARE_URL,
        .timeout_ms = 15000,
        .keep_alive_enable = true,
        .skip_cert_common_name_check = true, // Force skips SSL domain validation checks completely!
    };
    
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        ESP_LOGE(TAG, "❌ Failed to initialize HTTP network instance configuration.");
        return;
    }
    
    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "❌ Connection setup dropped. Cannot reach GitHub servers.");
        esp_http_client_cleanup(client);
        return;
    }
    
    int content_length = esp_http_client_fetch_headers(client);
    if (content_length <= 0) {
        ESP_LOGW(TAG, "⚠️ Received zero payload size from cloud binary registry.");
        esp_http_client_cleanup(client);
        return;
    }
    
    const esp_partition_t *update_partition = esp_ota_get_next_update_partition(NULL);
    if (update_partition == NULL) {
        ESP_LOGE(TAG, "❌ OTA target destination partitions tables are missing from configuration.");
        esp_http_client_cleanup(client);
        return;
    }
    
    esp_ota_handle_t ota_handle = 0;
    err = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "❌ Failed to initiate memory partition block writing parameters.");
        esp_http_client_cleanup(client);
        return;
    }
    
    static char ota_write_buffer[1024]; // 1KB stable memory buffer allocation
    int read_bytes = 0;
    int total_bytes_flashed = 0;
    
    // Continuous sequential block-by-block storage flash routine
    while ((read_bytes = esp_http_client_read(client, ota_write_buffer, sizeof(ota_write_buffer))) > 0) {
        err = esp_ota_write(ota_handle, (const void *)ota_write_buffer, read_bytes);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "❌ Critical data sector write crash inside memory mapping tables.");
            esp_ota_abort(ota_handle);
            esp_http_client_cleanup(client);
            return;
        }
        total_bytes_flashed += read_bytes;
        printf("📥 Flashed Data Stream: %d bytes successfully written...\r", total_bytes_flashed);
    }
    
    err = esp_ota_end(ota_handle);
    if (err == ESP_OK) {
        err = esp_ota_set_boot_partition(update_partition);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "🟢 100%% SUCCESS! Total bytes flashed completely: %d", total_bytes_flashed);
            ESP_LOGI(TAG, "🔄 Rebooting EON Rover Core onto custom updated firmware slots...");
            vTaskDelay(pdMS_TO_TICKS(1500));
            esp_restart();
        }
    }
    
    ESP_LOGE(TAG, "❌ Final signature verification parameters validation mismatch.");
    esp_ota_abort(ota_handle);
    esp_http_client_cleanup(client);
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
    ESP_LOGI(TAG, "🤖 EON Cubic Rover - Raw Stream Low-Level Active");
    ESP_LOGI(TAG, "================================================");

    initialize_network_drivers();
    xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT, pdFALSE, pdTRUE, pdMS_TO_TICKS(10000));
    
    launch_espressif_prebuilt_ota();

    ESP_LOGI(TAG, "🚀 Maintenance Phase Complete. Entering active operational telemetry hooks...");

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
