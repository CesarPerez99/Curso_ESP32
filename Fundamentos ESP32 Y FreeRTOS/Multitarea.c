#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_system.h"

// Definición de constantes
#define LED_GPIO_PIN        GPIO_NUM_2      // Pin del LED integrado
#define STACK_SIZE          2048            // Tamaño del stack para tareas
#define TASK_PRIORITY_HIGH  3               // Prioridad alta
#define TASK_PRIORITY_MED   2               // Prioridad media  
#define TASK_PRIORITY_LOW   1               // Prioridad baja

// Variables globales para compartir datos entre tareas
static int global_counter = 0;
static SemaphoreHandle_t counter_mutex;

// Tag para logging
static const char *TAG = "MULTITASK_PRACTICE";

// Función de la tarea LED
void led_task(void *pvParameters)
{
    // Configuración inicial del GPIO para el LED
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << LED_GPIO_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);
    
    ESP_LOGI(TAG, "LED Task iniciada en el núcleo %d", xPortGetCoreID());
    
    bool led_state = false;
    
    // Bucle infinito de la tarea
    while (1) {
        // Cambiar el estado del LED
        led_state = !led_state;
        gpio_set_level(LED_GPIO_PIN, led_state);
        
        ESP_LOGI(TAG, "LED %s", led_state ? "ON" : "OFF");
        
        // Suspender la tarea por 1000ms (1 segundo)
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

// Función de la tarea contador
void counter_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Counter Task iniciada en el núcleo %d", xPortGetCoreID());
    
    int local_counter = 0;
    
    while (1) {
        // Tomar el mutex antes de acceder a la variable global
        if (xSemaphoreTake(counter_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            global_counter++;
            local_counter = global_counter;
            
            // Liberar el mutex
            xSemaphoreGive(counter_mutex);
            
            ESP_LOGI(TAG, "Contador global: %d", local_counter);
        } else {
            ESP_LOGW(TAG, "No se pudo obtener el mutex del contador");
        }
        
        // Delay de 2 segundos
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

// Función de la tarea monitor del sistema
void monitor_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Monitor Task iniciada en el núcleo %d", xPortGetCoreID());
    
    while (1) {
        // Obtener información del heap (memoria libre)
        size_t free_heap = esp_get_free_heap_size();
        size_t min_free_heap = esp_get_minimum_free_heap_size();
        
        // Obtener información de las tareas
        UBaseType_t task_count = uxTaskGetNumberOfTasks();
        
        // Obtener el valor actual del contador de forma segura
        int current_counter = 0;
        if (xSemaphoreTake(counter_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            current_counter = global_counter;
            xSemaphoreGive(counter_mutex);
        }
        
        // Mostrar información del sistema
        ESP_LOGI(TAG, "=== MONITOR DEL SISTEMA ===");
        ESP_LOGI(TAG, "Memoria libre: %d bytes", free_heap);
        ESP_LOGI(TAG, "Mínima memoria libre: %d bytes", min_free_heap);
        ESP_LOGI(TAG, "Número de tareas: %d", task_count);
        ESP_LOGI(TAG, "Contador actual: %d", current_counter);
        ESP_LOGI(TAG, "Tiempo de ejecución: %lld ms", esp_timer_get_time() / 1000);
        ESP_LOGI(TAG, "===========================");
        
        // Delay de 5 segundos
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

// Función principal de la aplicación
void app_main(void)
{
    ESP_LOGI(TAG, "Iniciando práctica de múltiples tareas");
    ESP_LOGI(TAG, "Ejecutándose en el núcleo %d", xPortGetCoreID());
    
    // Crear mutex para proteger la variable global
    counter_mutex = xSemaphoreCreateMutex();
    if (counter_mutex == NULL) {
        ESP_LOGE(TAG, "Error al crear el mutex");
        return;
    }
    
    // Crear la tarea del LED con prioridad media
    TaskHandle_t led_task_handle;
    BaseType_t result = xTaskCreate(
        led_task,                   // Función de la tarea
        "LED_Task",                 // Nombre descriptivo
        STACK_SIZE,                 // Tamaño del stack
        NULL,                       // Parámetros de entrada
        TASK_PRIORITY_MED,          // Prioridad
        &led_task_handle            // Handle de la tarea
    );
    
    if (result != pdPASS) {
        ESP_LOGE(TAG, "Error al crear LED Task");
        return;
    }
    
    // Crear la tarea contador con prioridad alta
    TaskHandle_t counter_task_handle;
    result = xTaskCreate(
        counter_task,
        "Counter_Task",
        STACK_SIZE,
        NULL,
        TASK_PRIORITY_HIGH,
        &counter_task_handle
    );
    
    if (result != pdPASS) {
        ESP_LOGE(TAG, "Error al crear Counter Task");
        return;
    }
    
    // Crear la tarea monitor con prioridad baja
    TaskHandle_t monitor_task_handle;
    result = xTaskCreate(
        monitor_task,
        "Monitor_Task",
        STACK_SIZE,
        NULL,
        TASK_PRIORITY_LOW,
        &monitor_task_handle
    );
    
    if (result != pdPASS) {
        ESP_LOGE(TAG, "Error al crear Monitor Task");
        return;
    }
    
    ESP_LOGI(TAG, "Todas las tareas han sido creadas exitosamente");
    ESP_LOGI(TAG, "El planificador de FreeRTOS está manejando las tareas");
    
    // La tarea principal puede terminar aquí ya que las otras tareas seguirán ejecutándose
    // En un sistema embebido, normalmente tendríamos un bucle infinito aquí también
    while (1) {
        ESP_LOGI(TAG, "Tarea principal ejecutándose...");
        vTaskDelay(pdMS_TO_TICKS(10000)); // 10 segundos
    }
}