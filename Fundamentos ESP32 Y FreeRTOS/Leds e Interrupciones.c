#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"        // Librería base de FreeRTOS
#include "freertos/task.h"            // Para manejo de tareas
#include "freertos/queue.h"           // Para manejo de colas
#include "freertos/semphr.h"          // Para semáforos
#include "driver/gpio.h"              // Driver GPIO del ESP-IDF
#include "esp_log.h"                  // Para logging y debug

// Definición de etiqueta para logging
static const char *TAG = "GPIO_INTERRUPT_DEMO";

// Definición de pines para LEDs (salidas)
#define LED_ROJO_PIN     GPIO_NUM_2
#define LED_AMARILLO_PIN GPIO_NUM_4
#define LED_VERDE_PIN    GPIO_NUM_5

// Definición de pines para botones (entradas con interrupciones)
#define BOTON_1_PIN      GPIO_NUM_18
#define BOTON_2_PIN      GPIO_NUM_19
#define BOTON_3_PIN      GPIO_NUM_21

// Máscara de bits para configuración GPIO de salida
#define GPIO_OUTPUT_PIN_SEL ((1ULL<<LED_ROJO_PIN) | (1ULL<<LED_AMARILLO_PIN) | (1ULL<<LED_VERDE_PIN))

// Máscara de bits para configuración GPIO de entrada
#define GPIO_INPUT_PIN_SEL  ((1ULL<<BOTON_1_PIN) | (1ULL<<BOTON_2_PIN) | (1ULL<<BOTON_3_PIN))

// Definición de eventos para la cola de interrupciones
typedef enum {
    EVENTO_BOTON_1 = 1,    // Evento del botón 1
    EVENTO_BOTON_2,        // Evento del botón 2
    EVENTO_BOTON_3         // Evento del botón 3
} evento_interrupcion_t;

// Variables globales
static QueueHandle_t cola_eventos_gpio = NULL;  // Cola para eventos de GPIO
static bool led_amarillo_parpadeando = false;   // Estado del parpadeo LED amarillo
static bool secuencia_verde_activa = false;     // Estado de secuencia LED verde

// Variables para anti-rebote (debounce)
static volatile uint32_t ultimo_tiempo_boton1 = 0;  // Último tiempo de presión botón 1
static volatile uint32_t ultimo_tiempo_boton2 = 0;  // Último tiempo de presión botón 2
static volatile uint32_t ultimo_tiempo_boton3 = 0;  // Último tiempo de presión botón 3
static const uint32_t TIEMPO_DEBOUNCE_MS = 200;     // Tiempo mínimo entre presiones (200ms)

/**
 * Rutina de Servicio de Interrupción (ISR) para GPIO
 * Esta función se ejecuta cuando ocurre una interrupción en cualquier GPIO configurado
 * Incluye lógica de anti-rebote por software
 * 
 * @param arg: Argumento pasado durante la instalación del ISR (número de pin)
 */
static void IRAM_ATTR gpio_isr_handler(void* arg)
{
    // Convierte el argumento a número de GPIO
    uint32_t gpio_num = (uint32_t) arg;
    evento_interrupcion_t evento;
    
    // Obtiene el tiempo actual en milisegundos (desde el arranque del sistema)
    uint32_t tiempo_actual = xTaskGetTickCountFromISR() * portTICK_PERIOD_MS;
    
    // Variables para almacenar el último tiempo según el botón
    volatile uint32_t* ultimo_tiempo;
    
    // Determina qué evento enviar y qué variable de tiempo usar según el pin
    switch(gpio_num) {
        case BOTON_1_PIN:
            evento = EVENTO_BOTON_1;
            ultimo_tiempo = &ultimo_tiempo_boton1;
            break;
        case BOTON_2_PIN:
            evento = EVENTO_BOTON_2;
            ultimo_tiempo = &ultimo_tiempo_boton2;
            break;
        case BOTON_3_PIN:
            evento = EVENTO_BOTON_3;
            ultimo_tiempo = &ultimo_tiempo_boton3;
            break;
        default:
            return;  // Pin no reconocido, salir de la ISR
    }
    
    // Implementación de anti-rebote: verifica si ha pasado suficiente tiempo
    if((tiempo_actual - *ultimo_tiempo) >= TIEMPO_DEBOUNCE_MS) {
        
        // Actualiza el tiempo de la última presión válida
        *ultimo_tiempo = tiempo_actual;
        
        // Envía el evento a la cola desde la ISR
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        
        // xQueueSendFromISR es la versión thread-safe para usar en ISRs
        xQueueSendFromISR(cola_eventos_gpio, &evento, &xHigherPriorityTaskWoken);
        
        // Si una tarea de mayor prioridad fue desbloqueada, forzar cambio de contexto
        if(xHigherPriorityTaskWoken) {
            portYIELD_FROM_ISR();
        }
    }
    // Si no ha pasado suficiente tiempo, la interrupción se ignora (anti-rebote)
}

/**
 * Tarea para controlar el LED rojo
 * Maneja el encendido/apagado alternado del LED rojo
 */
static void tarea_led_rojo(void *pvParameters)
{
    ESP_LOGI(TAG, "Tarea LED Rojo iniciada");
    
    // Estado inicial del LED rojo (apagado)
    bool estado_led_rojo = false;
    
    // Bucle infinito de la tarea
    while(1) {
        evento_interrupcion_t evento_recibido;
        
        // Espera por eventos en la cola con timeout de 100ms
        if(xQueueReceive(cola_eventos_gpio, &evento_recibido, pdMS_TO_TICKS(100))) {
            
            // Procesa solo eventos del botón 1
            if(evento_recibido == EVENTO_BOTON_1) {
                
                // Cambia el estado del LED (toggle)
                estado_led_rojo = !estado_led_rojo;
                
                // Establece el nivel del GPIO según el nuevo estado
                gpio_set_level(LED_ROJO_PIN, estado_led_rojo);
                
                // Log del cambio de estado
                ESP_LOGI(TAG, "LED Rojo: %s", estado_led_rojo ? "ENCENDIDO" : "APAGADO");
                
                // NO reenvía el evento - lo consume completamente
            }
            // Si no es el evento para esta tarea, NO hace nada
            // El evento se descarta y no se reenvía
        }
        
        // Pequeña pausa para permitir que otras tareas se ejecuten
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

/**
 * Tarea para controlar el LED amarillo
 * Maneja el parpadeo continuo del LED amarillo
 */
static void tarea_led_amarillo(void *pvParameters)
{
    ESP_LOGI(TAG, "Tarea LED Amarillo iniciada");
    
    // Estado actual del LED durante el parpadeo
    bool estado_parpadeo = false;
    
    // Bucle infinito de la tarea
    while(1) {
        evento_interrupcion_t evento_recibido;
        
        // Espera eventos con timeout de 500ms para permitir parpadeo
        if(xQueueReceive(cola_eventos_gpio, &evento_recibido, pdMS_TO_TICKS(500))) {
            
            // Procesa eventos del botón 2
            if(evento_recibido == EVENTO_BOTON_2) {
                
                // Cambia el estado del parpadeo
                led_amarillo_parpadeando = !led_amarillo_parpadeando;
                
                ESP_LOGI(TAG, "Parpadeo LED Amarillo: %s", 
                        led_amarillo_parpadeando ? "ACTIVADO" : "DESACTIVADO");
                
                // Si se desactiva el parpadeo, apaga el LED
                if(!led_amarillo_parpadeando) {
                    gpio_set_level(LED_AMARILLO_PIN, 0);
                    estado_parpadeo = false;
                }
                
                // NO reenvía el evento - lo consume completamente
            }
            // Si no es el evento para esta tarea, NO hace nada
            // El evento se descarta y no se reenvía
        }
        
        // Lógica de parpadeo cuando está activado
        if(led_amarillo_parpadeando) {
            // Alterna el estado del LED
            estado_parpadeo = !estado_parpadeo;
            gpio_set_level(LED_AMARILLO_PIN, estado_parpadeo);
        }
    }
}

/**
 * Tarea para controlar el LED verde
 * Maneja una secuencia de parpadeo específica del LED verde
 */
static void tarea_led_verde(void *pvParameters)
{
    ESP_LOGI(TAG, "Tarea LED Verde iniciada");
    
    // Bucle infinito de la tarea
    while(1) {
        evento_interrupcion_t evento_recibido;
        
        // Espera por eventos con timeout de 100ms
        if(xQueueReceive(cola_eventos_gpio, &evento_recibido, pdMS_TO_TICKS(100))) {
            
            // Procesa eventos del botón 3
            if(evento_recibido == EVENTO_BOTON_3) {
                
                // Verifica que no hay otra secuencia en curso
                if(!secuencia_verde_activa) {
                    
                    // Activa la bandera de secuencia
                    secuencia_verde_activa = true;
                    
                    ESP_LOGI(TAG, "Secuencia LED Verde iniciada");
                    
                    // Ejecuta secuencia: 3 parpadeos rápidos
                    for(int i = 0; i < 3; i++) {
                        gpio_set_level(LED_VERDE_PIN, 1);    // Enciende LED
                        vTaskDelay(pdMS_TO_TICKS(200));      // Espera 200ms
                        gpio_set_level(LED_VERDE_PIN, 0);    // Apaga LED
                        vTaskDelay(pdMS_TO_TICKS(200));      // Espera 200ms
                    }
                    
                    // Pausa entre secuencias
                    vTaskDelay(pdMS_TO_TICKS(1000));
                    
                    // Ejecuta secuencia: encendido prolongado
                    gpio_set_level(LED_VERDE_PIN, 1);        // Enciende LED
                    vTaskDelay(pdMS_TO_TICKS(2000));         // Mantiene encendido 2 segundos
                    gpio_set_level(LED_VERDE_PIN, 0);        // Apaga LED
                    
                    // Marca el final de la secuencia
                    secuencia_verde_activa = false;
                    
                    ESP_LOGI(TAG, "Secuencia LED Verde completada");
                }
                
                // NO reenvía el evento - lo consume completamente
            }
            // Si no es el evento para esta tarea, NO hace nada
            // El evento se descarta y no se reenvía
        }
        
        // Pequeña pausa para permitir que otras tareas se ejecuten
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

/**
 * Función para configurar los pines GPIO
 * Configura los pines de entrada (botones) y salida (LEDs)
 */
static void configurar_gpio(void)
{
    ESP_LOGI(TAG, "Configurando pines GPIO...");
    
    // Estructura de configuración para pines de salida (LEDs)
    gpio_config_t io_conf_output = {};
    
    // Máscara de bits con los pines a configurar como salida
    io_conf_output.pin_bit_mask = GPIO_OUTPUT_PIN_SEL;
    
    // Configura como modo salida
    io_conf_output.mode = GPIO_MODE_OUTPUT;
    
    // Deshabilita resistencia pull-up
    io_conf_output.pull_up_en = GPIO_PULLUP_DISABLE;
    
    // Deshabilita resistencia pull-down
    io_conf_output.pull_down_en = GPIO_PULLDOWN_DISABLE;
    
    // Deshabilita interrupciones para salidas
    io_conf_output.intr_type = GPIO_INTR_DISABLE;
    
    // Aplica la configuración
    ESP_ERROR_CHECK(gpio_config(&io_conf_output));
    
    // Estructura de configuración para pines de entrada (botones)
    gpio_config_t io_conf_input = {};
    
    // Máscara de bits con los pines a configurar como entrada
    io_conf_input.pin_bit_mask = GPIO_INPUT_PIN_SEL;
    
    // Configura como modo entrada
    io_conf_input.mode = GPIO_MODE_INPUT;
    
    // Habilita resistencia pull-up interna (botón conectado a GND)
    io_conf_input.pull_up_en = GPIO_PULLUP_ENABLE;
    
    // Deshabilita resistencia pull-down
    io_conf_input.pull_down_en = GPIO_PULLDOWN_DISABLE;
    
    // Configura interrupción en flanco descendente (presión del botón)
    io_conf_input.intr_type = GPIO_INTR_NEGEDGE;
    
    // Aplica la configuración
    ESP_ERROR_CHECK(gpio_config(&io_conf_input));
    
    ESP_LOGI(TAG, "Configuración GPIO completada");
}

/**
 * Función para configurar las interrupciones GPIO
 * Instala el servicio de interrupciones y asocia handlers
 */
static void configurar_interrupciones(void)
{
    ESP_LOGI(TAG, "Configurando interrupciones GPIO...");
    
    // Instala el servicio de interrupciones GPIO
    // ESP_INTR_FLAG_DEFAULT: usa la prioridad por defecto
    ESP_ERROR_CHECK(gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT));
    
    // Asocia el handler de interrupción al botón 1
    ESP_ERROR_CHECK(gpio_isr_handler_add(BOTON_1_PIN, gpio_isr_handler, (void*) BOTON_1_PIN));
    
    // Asocia el handler de interrupción al botón 2
    ESP_ERROR_CHECK(gpio_isr_handler_add(BOTON_2_PIN, gpio_isr_handler, (void*) BOTON_2_PIN));
    
    // Asocia el handler de interrupción al botón 3
    ESP_ERROR_CHECK(gpio_isr_handler_add(BOTON_3_PIN, gpio_isr_handler, (void*) BOTON_3_PIN));
    
    ESP_LOGI(TAG, "Interrupciones GPIO configuradas correctamente");
}

/**
 * Función principal de la aplicación
 * Punto de entrada del programa
 */
void app_main(void)
{
    ESP_LOGI(TAG, "=== Iniciando Práctica 3.1: Control de LEDs e Interrupciones ===");
    
    // Crea la cola para eventos de GPIO
    // Capacidad: 10 elementos, tamaño: sizeof(evento_interrupcion_t)
    cola_eventos_gpio = xQueueCreate(10, sizeof(evento_interrupcion_t));
    
    // Verifica que la cola se haya creado correctamente
    if(cola_eventos_gpio == NULL) {
        ESP_LOGE(TAG, "Error: No se pudo crear la cola de eventos GPIO");
        return;
    }
    
    ESP_LOGI(TAG, "Cola de eventos GPIO creada exitosamente");
    
    // Configura los pines GPIO
    configurar_gpio();
    
    // Configura las interrupciones
    configurar_interrupciones();
    
    // Establece estado inicial de todos los LEDs (apagados)
    gpio_set_level(LED_ROJO_PIN, 0);
    gpio_set_level(LED_AMARILLO_PIN, 0);
    gpio_set_level(LED_VERDE_PIN, 0);
    
    // Inicializa las variables de tiempo para anti-rebote
    ultimo_tiempo_boton1 = 0;
    ultimo_tiempo_boton2 = 0;
    ultimo_tiempo_boton3 = 0;
    
    ESP_LOGI(TAG, "Estado inicial de LEDs establecido (todos apagados)");
    ESP_LOGI(TAG, "Sistema anti-rebote configurado con %lu ms de retardo", TIEMPO_DEBOUNCE_MS);
    
    // Crea la tarea para controlar el LED rojo
    // Parámetros: función, nombre, stack size, parámetros, prioridad, handle
    xTaskCreate(tarea_led_rojo, "tarea_led_rojo", 2048, NULL, 10, NULL);
    
    // Crea la tarea para controlar el LED amarillo
    xTaskCreate(tarea_led_amarillo, "tarea_led_amarillo", 2048, NULL, 10, NULL);
    
    // Crea la tarea para controlar el LED verde
    xTaskCreate(tarea_led_verde, "tarea_led_verde", 2048, NULL, 10, NULL);
    
    ESP_LOGI(TAG, "Todas las tareas creadas. Sistema listo para uso.");
    ESP_LOGI(TAG, "Presiona los botones para controlar los LEDs:");
    ESP_LOGI(TAG, "  - Botón 1: Alternar LED rojo");
    ESP_LOGI(TAG, "  - Botón 2: Activar/desactivar parpadeo LED amarillo");
    ESP_LOGI(TAG, "  - Botón 3: Ejecutar secuencia en LED verde");
    
    // La tarea principal termina aquí, FreeRTOS continúa ejecutando las otras tareas
}