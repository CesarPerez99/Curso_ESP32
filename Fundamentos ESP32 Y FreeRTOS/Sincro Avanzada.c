#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_random.h"

// ============================================================================
// DEFINICIONES Y ESTRUCTURAS
// ============================================================================

#define QUEUE_SIZE 10           // Tamaño de la cola para datos de sensores
#define MAX_SENSOR_VALUE 100    // Valor máximo del sensor
#define STACK_SIZE 2048         // Tamaño del stack para las tareas

// Tag para logging
static const char* TAG = "FREERTOS_PRACTICE";

// Estructura para datos del sensor
typedef struct {
    uint8_t sensor_id;          // ID del sensor (1-3)
    float value;                // Valor del sensor
    uint32_t timestamp;         // Timestamp del dato
} sensor_data_t;

// Estructura para estadísticas compartidas (protegida por mutex)
typedef struct {
    float temperature_avg;      // Promedio de temperatura
    float humidity_avg;         // Promedio de humedad  
    float pressure_avg;         // Promedio de presión
    uint32_t total_samples;     // Total de muestras procesadas
} shared_stats_t;

// ============================================================================
// VARIABLES GLOBALES DE SINCRONIZACIÓN
// ============================================================================

// Cola para comunicación productor-consumidor
static QueueHandle_t sensor_queue = NULL;

// Semáforos
static SemaphoreHandle_t binary_semaphore = NULL;      // Para inicialización
static SemaphoreHandle_t counting_semaphore = NULL;    // Para control de recursos

// Event Group para coordinación de eventos
static EventGroupHandle_t system_events = NULL;

// Bits para el Event Group
#define SENSOR_1_READY_BIT    (1 << 0)    // Bit 0: Sensor 1 listo
#define SENSOR_2_READY_BIT    (1 << 1)    // Bit 1: Sensor 2 listo  
#define SENSOR_3_READY_BIT    (1 << 2)    // Bit 2: Sensor 3 listo
#define PROCESSING_DONE_BIT   (1 << 3)    // Bit 3: Procesamiento completo
#define ALL_SENSORS_READY     (SENSOR_1_READY_BIT | SENSOR_2_READY_BIT | SENSOR_3_READY_BIT)

// Mutex para proteger recurso compartido
static SemaphoreHandle_t stats_mutex = NULL;

// Recurso compartido protegido por mutex
static shared_stats_t global_stats = {0};

// ============================================================================
// TAREAS PRODUCTORAS (SENSORES)
// ============================================================================

/**
 * Tarea del Sensor de Temperatura (Productor 1)
 * Genera datos de temperatura cada 2 segundos y los envía a la cola
 */
void temperature_sensor_task(void *pvParameters) {
    sensor_data_t sensor_data;
    sensor_data.sensor_id = 1;  // ID del sensor de temperatura
    
    ESP_LOGI(TAG, "Sensor de Temperatura iniciado");
    
    // Señalar que este sensor está listo
    xEventGroupSetBits(system_events, SENSOR_1_READY_BIT);
    
    while (1) {
        // Simular lectura de sensor (valor entre 20-40°C)
        sensor_data.value = 20.0 + (esp_random() % 2000) / 100.0;
        sensor_data.timestamp = xTaskGetTickCount();
        
        // Intentar enviar dato a la cola
        if (xQueueSend(sensor_queue, &sensor_data, pdMS_TO_TICKS(100)) == pdTRUE) {
            ESP_LOGI(TAG, "Temp: %.2f°C enviada", sensor_data.value);
        } else {
            ESP_LOGW(TAG, "Cola llena, dato de temperatura perdido");
        }
        
        // Esperar 2 segundos antes de la siguiente lectura
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

/**
 * Tarea del Sensor de Humedad (Productor 2)  
 * Genera datos de humedad cada 3 segundos y los envía a la cola
 */
void humidity_sensor_task(void *pvParameters) {
    sensor_data_t sensor_data;
    sensor_data.sensor_id = 2;  // ID del sensor de humedad
    
    ESP_LOGI(TAG, "Sensor de Humedad iniciado");
    
    // Señalar que este sensor está listo
    xEventGroupSetBits(system_events, SENSOR_2_READY_BIT);
    
    while (1) {
        // Simular lectura de sensor (valor entre 30-90% RH)
        sensor_data.value = 30.0 + (esp_random() % 6000) / 100.0;
        sensor_data.timestamp = xTaskGetTickCount();
        
        // Intentar enviar dato a la cola
        if (xQueueSend(sensor_queue, &sensor_data, pdMS_TO_TICKS(100)) == pdTRUE) {
            ESP_LOGI(TAG, "Humedad: %.2f%% enviada", sensor_data.value);
        } else {
            ESP_LOGW(TAG, "Cola llena, dato de humedad perdido");
        }
        
        // Esperar 3 segundos antes de la siguiente lectura
        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}

/**
 * Tarea del Sensor de Presión (Productor 3)
 * Genera datos de presión cada 4 segundos y los envía a la cola
 */
void pressure_sensor_task(void *pvParameters) {
    sensor_data_t sensor_data;
    sensor_data.sensor_id = 3;  // ID del sensor de presión
    
    ESP_LOGI(TAG, "Sensor de Presión iniciado");
    
    // Señalar que este sensor está listo
    xEventGroupSetBits(system_events, SENSOR_3_READY_BIT);
    
    while (1) {
        // Simular lectura de sensor (valor entre 950-1050 hPa)
        sensor_data.value = 950.0 + (esp_random() % 10000) / 100.0;
        sensor_data.timestamp = xTaskGetTickCount();
        
        // Intentar enviar dato a la cola
        if (xQueueSend(sensor_queue, &sensor_data, pdMS_TO_TICKS(100)) == pdTRUE) {
            ESP_LOGI(TAG, "Presión: %.2f hPa enviada", sensor_data.value);
        } else {
            ESP_LOGW(TAG, "Cola llena, dato de presión perdido");
        }
        
        // Esperar 4 segundos antes de la siguiente lectura
        vTaskDelay(pdMS_TO_TICKS(4000));
    }
}

// ============================================================================
// TAREAS CONSUMIDORAS (PROCESADORES)  
// ============================================================================

/**
 * Tarea Procesadora de Datos (Consumidor)
 * Recibe datos de la cola, los procesa y actualiza estadísticas compartidas
 */
void data_processor_task(void *pvParameters) {
    sensor_data_t received_data;
    static float temp_sum = 0, humidity_sum = 0, pressure_sum = 0;
    static uint16_t temp_count = 0, humidity_count = 0, pressure_count = 0;
    
    ESP_LOGI(TAG, "Procesador de datos iniciado");
    
    // Esperar a que todos los sensores estén listos usando Event Group
    ESP_LOGI(TAG, "Esperando que todos los sensores estén listos...");
    xEventGroupWaitBits(system_events, 
                        ALL_SENSORS_READY,  // Bits a esperar
                        pdFALSE,            // No limpiar bits después de esperar
                        pdTRUE,             // Esperar TODOS los bits
                        portMAX_DELAY);     // Esperar indefinidamente
    
    ESP_LOGI(TAG, "Todos los sensores listos, iniciando procesamiento");
    
    while (1) {
        // Intentar recibir dato de la cola
        if (xQueueReceive(sensor_queue, &received_data, pdMS_TO_TICKS(1000)) == pdTRUE) {
            
            // Tomar semáforo contador para limitar procesamiento concurrente
            if (xSemaphoreTake(counting_semaphore, pdMS_TO_TICKS(500)) == pdTRUE) {
                
                ESP_LOGI(TAG, "Procesando dato del sensor %d: %.2f", 
                         received_data.sensor_id, received_data.value);
                
                // Simular procesamiento (tiempo de cálculo)
                vTaskDelay(pdMS_TO_TICKS(100));
                
                // Acumular datos por tipo de sensor
                switch (received_data.sensor_id) {
                    case 1: // Temperatura
                        temp_sum += received_data.value;
                        temp_count++;
                        break;
                    case 2: // Humedad
                        humidity_sum += received_data.value;
                        humidity_count++;
                        break;
                    case 3: // Presión
                        pressure_sum += received_data.value;
                        pressure_count++;
                        break;
                }
                
                // Actualizar estadísticas globales (recurso compartido)
                // SECCIÓN CRÍTICA protegida por mutex
                if (xSemaphoreTake(stats_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                    
                    // Calcular promedios
                    if (temp_count > 0) {
                        global_stats.temperature_avg = temp_sum / temp_count;
                    }
                    if (humidity_count > 0) {
                        global_stats.humidity_avg = humidity_sum / humidity_count;
                    }
                    if (pressure_count > 0) {
                        global_stats.pressure_avg = pressure_sum / pressure_count;
                    }
                    
                    global_stats.total_samples = temp_count + humidity_count + pressure_count;
                    
                    // Liberar mutex
                    xSemaphoreGive(stats_mutex);
                } else {
                    ESP_LOGW(TAG, "No se pudo acceder a estadísticas globales");
                }
                
                // Liberar semáforo contador
                xSemaphoreGive(counting_semaphore);
                
                // Si hemos procesado suficientes muestras, señalar procesamiento completo
                if (global_stats.total_samples > 0 && (global_stats.total_samples % 10) == 0) {
                    xEventGroupSetBits(system_events, PROCESSING_DONE_BIT);
                }
                
            } else {
                ESP_LOGW(TAG, "Semáforo contador no disponible, saltando procesamiento");
            }
        }
    }
}

// ============================================================================
// TAREA DE DISPLAY (CONSUMIDOR DE ESTADÍSTICAS)
// ============================================================================

/**
 * Tarea de Display
 * Muestra estadísticas actualizadas cada cierto tiempo
 * Espera eventos de procesamiento completo usando Event Groups
 */
void display_task(void *pvParameters) {
    EventBits_t event_bits;
    shared_stats_t local_stats;
    
    ESP_LOGI(TAG, "Display iniciado");
    
    while (1) {
        // Esperar evento de procesamiento completo
        event_bits = xEventGroupWaitBits(system_events,
                                         PROCESSING_DONE_BIT,   // Bit a esperar
                                         pdTRUE,                // Limpiar bit después de detectarlo
                                         pdFALSE,               // No necesitar todos los bits
                                         pdMS_TO_TICKS(5000));  // Timeout de 5 segundos
        
        if (event_bits & PROCESSING_DONE_BIT) {
            ESP_LOGI(TAG, "Evento de procesamiento detectado, actualizando display");
        }
        
        // Acceder a estadísticas globales (recurso compartido)
        // SECCIÓN CRÍTICA protegida por mutex
        if (xSemaphoreTake(stats_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            
            // Copiar estadísticas a variable local para minimizar tiempo en sección crítica
            memcpy(&local_stats, &global_stats, sizeof(shared_stats_t));
            
            // Liberar mutex inmediatamente
            xSemaphoreGive(stats_mutex);
            
            // Mostrar estadísticas (fuera de la sección crítica)
            ESP_LOGI(TAG, "=== ESTADÍSTICAS DEL SISTEMA ===");
            ESP_LOGI(TAG, "Temperatura promedio: %.2f°C", local_stats.temperature_avg);
            ESP_LOGI(TAG, "Humedad promedio: %.2f%%", local_stats.humidity_avg);
            ESP_LOGI(TAG, "Presión promedio: %.2f hPa", local_stats.pressure_avg);
            ESP_LOGI(TAG, "Total muestras procesadas: %lu", local_stats.total_samples);
            ESP_LOGI(TAG, "================================");
            
        } else {
            ESP_LOGW(TAG, "No se pudieron obtener estadísticas para display");
        }
        
        // Actualizar display cada 8 segundos
        vTaskDelay(pdMS_TO_TICKS(8000));
    }
}

// ============================================================================
// TAREA DE INICIALIZACIÓN
// ============================================================================

/**
 * Tarea de Inicialización del Sistema
 * Coordina el arranque usando semáforos binarios
 */
void system_init_task(void *pvParameters) {
    ESP_LOGI(TAG, "Iniciando sistema de monitoreo...");
    
    // Simular tiempo de inicialización del hardware
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    ESP_LOGI(TAG, "Hardware inicializado correctamente");
    
    // Dar semáforo binario para permitir que otras tareas continúen
    xSemaphoreGive(binary_semaphore);
    
    ESP_LOGI(TAG, "Sistema listo para operar");
    
    // Esta tarea ha completado su función, se puede eliminar
    vTaskDelete(NULL);
}

// ============================================================================
// FUNCIÓN PRINCIPAL DE LA APLICACIÓN
// ============================================================================

void app_main(void) {
    ESP_LOGI(TAG, "=== PRÁCTICA FREERTOS: SINCRONIZACIÓN AVANZADA ===");
    
    // ========================================================================
    // CREACIÓN DE OBJETOS DE SINCRONIZACIÓN
    // ========================================================================
    
    // Crear cola para comunicación productor-consumidor
    sensor_queue = xQueueCreate(QUEUE_SIZE, sizeof(sensor_data_t));
    if (sensor_queue == NULL) {
        ESP_LOGE(TAG, "Error creando cola de sensores");
        return;
    }
    ESP_LOGI(TAG, "Cola de sensores creada exitosamente");
    
    // Crear semáforo binario para sincronización de inicialización
    binary_semaphore = xSemaphoreCreateBinary();
    if (binary_semaphore == NULL) {
        ESP_LOGE(TAG, "Error creando semáforo binario");
        return;
    }
    ESP_LOGI(TAG, "Semáforo binario creado exitosamente");
    
    // Crear semáforo contador para limitar procesamiento concurrente (máximo 2)
    counting_semaphore = xSemaphoreCreateCounting(2, 2);
    if (counting_semaphore == NULL) {
        ESP_LOGE(TAG, "Error creando semáforo contador");
        return;
    }
    ESP_LOGI(TAG, "Semáforo contador creado exitosamente");
    
    // Crear Event Group para coordinación de eventos
    system_events = xEventGroupCreate();
    if (system_events == NULL) {
        ESP_LOGE(TAG, "Error creando Event Group");
        return;
    }
    ESP_LOGI(TAG, "Event Group creado exitosamente");
    
    // Crear mutex para proteger estadísticas globales
    stats_mutex = xSemaphoreCreateMutex();
    if (stats_mutex == NULL) {
        ESP_LOGE(TAG, "Error creando mutex");
        return;
    }
    ESP_LOGI(TAG, "Mutex creado exitosamente");
    
    // ========================================================================
    // CREACIÓN DE TAREAS
    // ========================================================================
    
    // Crear tarea de inicialización del sistema
    if (xTaskCreate(system_init_task, "SystemInit", STACK_SIZE, NULL, 5, NULL) != pdPASS) {
        ESP_LOGE(TAG, "Error creando tarea de inicialización");
        return;
    }
    
    // Esperar a que el sistema esté inicializado
    ESP_LOGI(TAG, "Esperando inicialización del sistema...");
    xSemaphoreTake(binary_semaphore, portMAX_DELAY);
    
    // Crear tareas productoras (sensores)
    if (xTaskCreate(temperature_sensor_task, "TempSensor", STACK_SIZE, NULL, 3, NULL) != pdPASS) {
        ESP_LOGE(TAG, "Error creando tarea sensor temperatura");
        return;
    }
    
    if (xTaskCreate(humidity_sensor_task, "HumiditySensor", STACK_SIZE, NULL, 3, NULL) != pdPASS) {
        ESP_LOGE(TAG, "Error creando tarea sensor humedad");
        return;
    }
    
    if (xTaskCreate(pressure_sensor_task, "PressureSensor", STACK_SIZE, NULL, 3, NULL) != pdPASS) {
        ESP_LOGE(TAG, "Error creando tarea sensor presión");
        return;
    }
    
    // Crear tarea consumidora (procesador)
    if (xTaskCreate(data_processor_task, "DataProcessor", STACK_SIZE, NULL, 4, NULL) != pdPASS) {
        ESP_LOGE(TAG, "Error creando tarea procesador");
        return;
    }
    
    // Crear tarea de display
    if (xTaskCreate(display_task, "Display", STACK_SIZE, NULL, 2, NULL) != pdPASS) {
        ESP_LOGE(TAG, "Error creando tarea display");
        return;
    }
    
    ESP_LOGI(TAG, "Todas las tareas creadas exitosamente");
    ESP_LOGI(TAG, "Sistema en funcionamiento...");
}