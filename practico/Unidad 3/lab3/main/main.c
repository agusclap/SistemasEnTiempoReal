#include <stdio.h>
#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "driver/gpio.h"
#include "esp_timer.h"

// =========================
// Configuración de pines
// =========================

#define LED_PIN GPIO_NUM_2
#define BUTTON_PIN GPIO_NUM_4

// =========================
// Variables globales
// =========================

static SemaphoreHandle_t eventSemaphore = NULL;

static bool ledState = false;
static int64_t lastEventTime = 0;

// =========================
// Rutina de Interrupción
// =========================

static void IRAM_ATTR button_isr_handler(void *arg)
{
    BaseType_t higherPriorityTaskWoken = pdFALSE;

    xSemaphoreGiveFromISR(eventSemaphore, &higherPriorityTaskWoken);

    portYIELD_FROM_ISR(higherPriorityTaskWoken);
}

// =========================
// Tarea de procesamiento
// Priority: 3 - Core 1
// =========================

static void processing_task(void *pvParameters)
{
    while (1)
    {
        if (xSemaphoreTake(eventSemaphore, portMAX_DELAY) == pdTRUE)
        {
            int64_t currentTime = esp_timer_get_time();
            int64_t elapsedTime = currentTime - lastEventTime;

            lastEventTime = currentTime;

            ledState = !ledState;
            gpio_set_level(LED_PIN, ledState);

            printf("Evento detectado. Tiempo desde el ultimo evento: %lld ms\n",
                   elapsedTime / 1000);
        }
    }
}

// =========================
// Tarea de telemetría
// Priority: 1 - Core 1
// =========================

static void telemetry_task(void *pvParameters)
{
    while (1)
    {
        printf("Sistema Operativo Saludable\n");
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

// =========================
// app_main
// =========================

void app_main(void)
{
    // Configuración del LED como salida
    gpio_config_t led_config = {
        .pin_bit_mask = (1ULL << LED_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };

    gpio_config(&led_config);
    gpio_set_level(LED_PIN, 0);

    // Configuración del pulsador como entrada con pull-up interno
    gpio_config_t button_config = {
        .pin_bit_mask = (1ULL << BUTTON_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_NEGEDGE
    };

    gpio_config(&button_config);

    // Creación del semáforo binario
    eventSemaphore = xSemaphoreCreateBinary();

    if (eventSemaphore == NULL)
    {
        printf("Error al crear el semaforo binario\n");
        return;
    }

    // Inicializamos el tiempo de referencia
    lastEventTime = esp_timer_get_time();

    // Instalación del servicio de interrupciones GPIO
    gpio_install_isr_service(0);

    // Asociación de la ISR al pin del botón
    gpio_isr_handler_add(BUTTON_PIN, button_isr_handler, NULL);

    // Creación de la tarea de procesamiento en Core 1
    xTaskCreatePinnedToCore(
        processing_task,
        "processing_task",
        2048,
        NULL,
        3,
        NULL,
        1
    );

    // Creación de la tarea de telemetría en Core 1
    xTaskCreatePinnedToCore(
        telemetry_task,
        "telemetry_task",
        2048,
        NULL,
        1,
        NULL,
        1
    );
}