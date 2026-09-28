#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "BLINK_APP";
#define BLINK_GPIO 0

void app_main(void) {
    ESP_LOGI(TAG, "Iniciando ejemplo de parpadeo en el Pin 0");

    // Configurar el pin como salida
    gpio_reset_pin(BLINK_GPIO);
    gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);

    int led_state = 0;

    while (1) {
        led_state = !led_state;
        gpio_set_level(BLINK_GPIO, led_state);
        
        ESP_LOGI(TAG, "LED estado: %s", led_state ? "ENCENDIDO" : "APAGADO");
        
        // Esperar 500ms (convertidos a ticks de FreeRTOS)
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
