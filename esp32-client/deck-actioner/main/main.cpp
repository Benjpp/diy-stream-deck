#include <cstdint>
#include <cstring>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include "esp_err.h"
#include "esp_event.h"
#include "esp_event_base.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_wifi_types_generic.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "driver/gpio.h"
#include "hal/gpio_types.h"
#include "nvs_flash.h"
#include "portmacro.h"
#include "rom/gpio.h"
#include "../include/cmd.h"
#include "sdkconfig.h"
#include "esp_wifi.h"
#include "soc/gpio_num.h"
#include "mqtt_client.h"

#define BUTTON_1 GPIO_NUM_0
#define BUTTON_2 GPIO_NUM_1
#define BUTTON_3 GPIO_NUM_4
#define BUTTON_4 GPIO_NUM_3

// Status LEDs
#define WIFI_CONNECT_LED            GPIO_NUM_6        // Green
#define NETWORK_ERROR_LED           GPIO_NUM_5        // Red
#define MQTT_PUBLISH_SUCCESS_LED    GPIO_NUM_7        // Blue

#define ESP_WIFI_MAXIMUM_RETRY 15

// Mqtt utils 
#define MQTT_QOS 1
#define MQTT_RETAIN 0
static const char *MQTT_TOPIC = "homelab/stream-deck";

// Tags for ESP_LOG macros
static const char *TAG_WIFI = "WIFI";
static const char *TAG_MQTT = "MQTT";
static const char *TAG_MAIN = "MAIN";

// Event Group bits for Wi-Fi connection state
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

struct button{
    gpio_num_t gpio_pin;
    CMD button_action;
    bool pressed = false;
};

// Global vars definitions. Some shouldnt be global? :V
struct button buttons[CMD_COUNT] = {
    {BUTTON_1, RESTART_CONTAINERS},
    {BUTTON_2, STOP_CONTAINERS},
    {BUTTON_3, START_CONTAINERS},
    {BUTTON_4, RETURN_TEMP}
};
bool button_pressed[CMD_COUNT];
static int s_retry_num = 0;

esp_mqtt_client_handle_t mqtt_client_handle;

// FreeRTOS event group signal handle
static EventGroupHandle_t s_wifi_event_group;

// Auxiliar functions definition
void setWifiStatusLED(bool isConnected);
esp_err_t sendCmd(CMD cmd);
void showSendCmdResponse(esp_err_t error);

void setWifiStatusLED(bool isConnected){
    if(isConnected == true){
        gpio_set_level(WIFI_CONNECT_LED, 1);
        gpio_set_level(NETWORK_ERROR_LED, 0);
    }else{
        gpio_set_level(WIFI_CONNECT_LED, 0);
        gpio_set_level(NETWORK_ERROR_LED, 1);
    }
}

void showSendCmdResponse(esp_err_t error){
    // If there was a error response, we blink the network error led. On OK response, blink mqtt success led.
    if(error == ESP_OK){
        gpio_set_level(MQTT_PUBLISH_SUCCESS_LED, 1);
        vTaskDelay(250 / portTICK_PERIOD_MS);
        gpio_set_level(MQTT_PUBLISH_SUCCESS_LED, 0);
    }else{
        gpio_set_level(NETWORK_ERROR_LED, 1);
        vTaskDelay(250 / portTICK_PERIOD_MS);
        gpio_set_level(NETWORK_ERROR_LED, 0);
    }
}

void wifiDisconnected(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data){
    setWifiStatusLED(false);
}

static void wifiConnectEventHandler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data){
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < ESP_WIFI_MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG_WIFI, "Retrying connection to AP...");
        } else {
            // Signal failure after reaching max retry count
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
            ESP_LOGI(TAG_WIFI, "Failed to connect to AP");
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG_WIFI, "Got IP:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        // Signal successful connection and IP assignment
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

esp_err_t wifiConnect(){
    // Create the event group
    s_wifi_event_group = xEventGroupCreate();

    // System initialization
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta(); // Initialize the station interface

    const char* SSID = CONFIG_WiFi_SSID;
    const char* PASSWORD = CONFIG_WiFi_PASSWORD; 
    esp_err_t error;

    wifi_init_config_t wifi_init_config = WIFI_INIT_CONFIG_DEFAULT(); 
    if((error = esp_wifi_init(&wifi_init_config)) != ESP_OK){
        ESP_LOGE(TAG_WIFI, "Error initializing Wi-Fi. ERROR: %s", esp_err_to_name(error));
        return error;
    }

    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifiConnectEventHandler, NULL, NULL);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifiConnectEventHandler, NULL, NULL);
    esp_event_handler_instance_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &wifiDisconnected, NULL, NULL);

    wifi_config_t wifi_config = {
        .sta = {
            .threshold = {
                .authmode = WIFI_AUTH_WPA2_PSK,
            },
        },
    };

    // Safely copy SSID and Password buffers
    strncpy((char *)wifi_config.sta.ssid, SSID, sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char *)wifi_config.sta.password, PASSWORD, sizeof(wifi_config.sta.password) - 1);

    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    esp_wifi_start();

    ESP_LOGI(TAG_WIFI, "wifi_init_sta finished.");

    /* Block until either connection succeeds (WIFI_CONNECTED_BIT)
     * or fails after max retries (WIFI_FAIL_BIT). */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    /* Evaluate the bit set by the event handler */
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG_WIFI, "Connected to AP SSID:%s", SSID);
        setWifiStatusLED(true);
        return ESP_OK;
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGE(TAG_WIFI, "Could not connect to Wi-Fi network SSID:%s", SSID);
        return ESP_FAIL;
    } else {
        ESP_LOGE(TAG_WIFI, "UNEXPECTED EVENT");
        return ESP_FAIL;
    }
}

esp_err_t mqttStart(){
    esp_mqtt_client_config_t mqtt_client_config = {
        {
            {"mqtt://192.168.1.132:1883"},
        }
    };
    
    mqtt_client_handle = esp_mqtt_client_init(&mqtt_client_config);
    ESP_LOGI(TAG_MQTT, "Mqtt client initialized. Starting client.");

    esp_err_t error;
    if((error = esp_mqtt_client_start(mqtt_client_handle)) != ESP_OK){
        return error;
    }

    ESP_LOGI(TAG_MQTT, "Mqtt client started. Returning to main flow of execution.");
    return ESP_OK;
}

extern "C" void app_main(void)
{
    ESP_LOGI(TAG_MAIN, "Total number of commands detected: %d", CMD_COUNT);
    ESP_LOGI(TAG_MAIN, "Prepping GPIO pins...");

    gpio_config_t gpio_conf = {
        .pin_bit_mask = (1ULL << BUTTON_1) | (1ULL << BUTTON_2) | (1ULL << BUTTON_3) | (1ULL << BUTTON_4),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_DISABLE
    };

    esp_err_t error;
    if((error = gpio_config(&gpio_conf)) != ESP_OK){
        ESP_LOGE(TAG_MAIN, "Error assigning button GPIO configs. ERROR: %s", esp_err_to_name(error));
        exit(error);
    }

    gpio_conf = {
        .pin_bit_mask = (1ULL << NETWORK_ERROR_LED) | (1ULL << WIFI_CONNECT_LED) | (1ULL << MQTT_PUBLISH_SUCCESS_LED),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    if((error = gpio_config(&gpio_conf)) != ESP_OK){
        ESP_LOGE(TAG_MAIN, "Error assigning button GPIO configs. ERROR: %s", esp_err_to_name(error));
        exit(error);
    }
    setWifiStatusLED(false);

    ESP_LOGI(TAG_MAIN, "Connecting to Wi-Fi network...");

    if(wifiConnect() != ESP_OK){
        ESP_LOGE(TAG_MAIN, "Failed to connect to Wi-Fi network. Restarting");
        esp_restart();
    } else {
        ESP_LOGI(TAG_MAIN, "Connected to Wi-Fi network!");
    }

    ESP_LOGI(TAG_MQTT, "Starting MQTT service");
    if(mqttStart() != ESP_OK){
        ESP_LOGI(TAG_MQTT, "Mqtt service could not be started. Restarting board.\n");
        esp_restart();
    }

    ESP_LOGI(TAG_MAIN, "Starting loop...");

    while(true){

        uint32_t gpio_input = gpio_input_get();

        for(int i = 0; i < CMD_COUNT; i++) {
            // Check if the current pin is pressed
            bool is_pressed = gpio_input & (1ULL << buttons[i].gpio_pin);

            if(is_pressed && !buttons[i].pressed) {
                ESP_LOGI(TAG_MAIN, "Button %d pressed", i + 1);
                buttons[i].pressed = true;
                if((error = sendCmd(buttons[i].button_action)) != ESP_OK){
                    ESP_LOGI(TAG_MQTT, "Error sending cmd");
                }
                vTaskDelay(1000 / portTICK_PERIOD_MS);
                showSendCmdResponse(error);
            }
            else if(!is_pressed && buttons[i].pressed) {
                ESP_LOGI(TAG_MAIN, "Release button %d...", i + 1);
                buttons[i].pressed = false;
                vTaskDelay(50 / portTICK_PERIOD_MS); // Debounce on release
            }
        }

        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}

esp_err_t sendCmd(CMD cmd){
    uint32_t cmdVal = cmd;
    const std::string cmdStr = std::to_string(cmdVal);
    const char *cmdChar = cmdStr.c_str();
    int status = esp_mqtt_client_publish(mqtt_client_handle, MQTT_TOPIC, cmdChar, sizeof(CMD), MQTT_QOS, MQTT_RETAIN);
    if(MQTT_QOS != 0 && status < 0){
        return ESP_FAIL;
    }

    ESP_LOGI(TAG_MQTT, "CMD Sent succesfully.");
    return ESP_OK;
}