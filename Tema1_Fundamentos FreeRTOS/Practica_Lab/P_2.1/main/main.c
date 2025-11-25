#include <stdio.h>
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h" //Librería básica
#include "freertos/task.h" //Para las tareas 
#include "freertos/queue.h" //El de las colas 
#include "driver/gptimer.h" //Manejo de timers de forma profesional 
#include "esp_log.h" //Para los logs por consola.

#include "freertos/semphr.h" //Aqui estan los semaforos y lo smutex 
#include "esp_random.h" //Para generar números aleatorios. 

//---------------------------------
// DEFINICIONES Y ESTRUCTURAS
//---------------------------------
#define STACK_SIZE          256
#define QUEUE_SIZE          10
#define MAX_SENSOR_VALUE    100

#define SENSOR_TEMPERATURE_READY_BIT    (1 << 0)    // Bit 0
#define SENSOR_HUMIDITY_READY_BIT    (1 << 1)    // Bit 1 
#define SENSOR_PRESSURE_READY_BIT    (1 << 2)    // Bit 2
#define PROCESSING_DONE_BIT   (1 << 3)    // Bit 3: Procesamiento completo
#define ALL_SENSORS_READY     (SENSOR_1_READY_BIT | SENSOR_2_READY_BIT | SENSOR_3_READY_BIT)

static char *TAG= "FRERTOS_PRACTICE"; //Es la etiqueta inicial de cada mensaje. 

/*Estructura para datos del sensor*/
typedef struct {
   uint8_t ID; 
   float value;
   uint32_t timestamp; 
}sensor_data_t;

/*Estructura de estadisticas de muestreo compartidas (Se protegeran por mutex)*/
typedef struct {
    float temperature_avg; 
    float humidity_avg;
    float pressure_avg;
    uint32_t total_samples; //Total de muestras procesadas
}shared_stats_t;

//---------------------------------
// VARIABLES PARA SINCRONIZAR TAREAS 
//---------------------------------
static QueueHandle_t Sensor_queue = NULL; 
static SemaphoreHandle_t binary_semaphore = NULL;
static SemaphoreHandle_t counting_semaphore = NULL; 
static EventGroupHandle_t system_events = NULL; 
static SemaphoreHandle_t stats_mutex = NULL;  //Todos estos son los handles de recursos que se crearan mas adelante 

shared_stats_t global_stats = {0}; //Inicializamos estructura en 0s 

//----------------------------------
// TAREAS PRODUCTORAS 
// ---------------------------------
/*Simular generar datos de temperatura cada 2 segundos*/
void generate_data_temperature (void *pvParameters){
    sensor_data_t sensor_data; 
    sensor_data.ID = 1; //Todos los sensores de temperatura tienen este ID 

    ESP_LOGI(TAG,"Sensor de temperatura Iniciado");
    xEventGroupSetBits(system_events, SENSOR_TEMPERATURE_READY_BIT); 
    while (1){ // Simular lectura de sensor (valor entre 20-40°C)           
        sensor_data.value = 20.0 + (esp_random() % 2000) / 100.0;
        sensor_data.timestamp = xTaskGetTickCount();
        
        // Intentar enviar dato a la cola
        if (xQueueSend(Sensor_queue, &sensor_data, pdMS_TO_TICKS(100)) == pdTRUE) {
            ESP_LOGI(TAG, "Temp: %.2f°C enviada", sensor_data.value);
        } else {
            ESP_LOGW(TAG, "Cola llena, dato de temperatura perdido");
        } 
        vTaskDelay(pdMS_TO_TICKS(2000)); // Esperar 2 segundos antes de la siguiente lectura
    }
}

/*Simular generar datos de humedad cada 3 segundos*/
void generate_data_humidity (void *pvParameters){
    sensor_data_t sensor_data; 
    sensor_data.ID = 2; //Todos los sensores de humedad tienen este ID 

    ESP_LOGI(TAG,"Sensor de humedad Iniciado");
    xEventGroupSetBits(system_events,SENSOR_HUMIDITY_READY_BIT); 
    while (1){ // Simular lectura de sensor (valor entre 30-90% RH)          
        sensor_data.value = 20.0 + (esp_random() % 2000) / 100.0;
        sensor_data.timestamp = xTaskGetTickCount();
        
        // Intentar enviar dato a la cola
        if (xQueueSend(Sensor_queue, &sensor_data, pdMS_TO_TICKS(100)) == pdTRUE) {
            ESP_LOGI(TAG, "Temp: %.2f°C enviada", sensor_data.value);
        } else {
            ESP_LOGW(TAG, "Cola llena, dato de temperatura perdido");
        } 
        vTaskDelay(pdMS_TO_TICKS(2000)); // Esperar 2 segundos antes de la siguiente lectura
    }
}


void app_main(void)
{

}