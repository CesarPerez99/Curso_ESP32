# ARCHIVO: Leds e Interrupciones.c
## Descripción
El propósito de este programa es mostrar el uso las interrupciones y tareas a través del manejo de distintos leds y botones. El objetivo principal es gestionar diferentes secuencias de encendido y apagado en cada led a través de los botones, todo esto a traves de interrupciones y tareas para hacer mas eficiente el desarrollo. 
## Librerias
Librerias basicas para manejo de RTOS y logear en la consola.

## Macros
Nombres de los botones y los leds asociados a un determinado pin GPIO, además de las mascaras para cada uno de estos grupos, una mascara para el de botones (input) y otra para los leds (output).

## Variables globales. 
Creamos una cola de eventos para los puertos GPIO, un enum para enlistar los tipos de evento que tenemos; en este caso solo son por la presión de algun boton y varibles auxiliares para el manejo del antirrebote por software.

## FUNCIONES 
## *Función de interrupción* 
### Paremetros
void *arg: Permite pasar cualquier tipo de parametro al momento de registrar la interrupción. En este caso este argumento contiene el número de GPIO que generó la interrupción que es casteado a uint32_t para su uso. 
### Descripcioón 
Esta función es el manejador de interrupciones, su principal tarea es identificar que botón fue presionado, filtrar rebotes y notificar a través de una cola de eventos.\
Cuando se entra a la ISR, se obtiene el GPIO que se presiono y obtenemos el tiempo actual del sistema. En base a este GPIO determinamos que botón fue presionado y almacenamos en una variable el tiempo de la ultima ves que se presiono dicho botón.\
Con estos datos implementamos una logica anti-rebote que compara la diferencia de la ultima ves que presionamos ese boton con el tiempo actual del sistema, si el valor esta por encima del tiempo que establecimos se considera una presión real, si no, un rebote.\
Cuando la presión es válida, actualizamos el tiempo final asociado a ese botón, registramos el evento y lo mandamos a través de una cola.\
Al final revisamos si el evento desbloqueo alguna tarea de mayor prioridad y de ser así, forzamos el cambio de contexto al salir de la ISR para que se ejecute dicha tarea.\
***portYIELD_FROM_ISR()***: Forza a un cambio de contexto al de mas alta prioridad.\
***IRAM_ATTR***: Pone la función en la IRAM para acceso directo y evita errores de colisión en este espacio de memoria.\
## *Función Task*: Led Rojo
### Parametros 
void *pvParameters: Permite pasar cualquier tipo de parametro al momento de ejecutar la tarea. En este caso el argumento no se utiliza. 
### Descripción
Esta función se encarga de controlar el estado del Led Rojo que esta asociado directamente con la presión del botón 1. Esta tarea en si, solo alterna (toggle) el estado del Led Rojo cuando recibe un evento valido asociado al Boton1, además de imprimir un log asociado a este cambio.\
La tarea permanece bloqueada en espera de un evento en la *cola de gpios* hasta máximo 100ms. Si en este tiempo no se recibe nada, la tarea ejecuta una instrucción de delay de 10ms sin hacer nada, y luego vuelve a ciclarse en la espera de un evento en la cola.\
En caso de recibir un evento, este dato lo guardamos y lo comparamos, ya que solamente se procesan eventos asociados al Boton1. De ser valido el evento, alternamos una variable de Gpio y con esa cambiamos el estado del Led Rojo, además de imprimir un Log en consola con este cambio.\
Si el evento no es valido, no lo reenviamos, simplemente se consume. 
## *Función Task*: Led Amarillo
### Parametros 
void *pvParameters: Permite pasar cualquier tipo de parametro al momento de ejecutar la tarea. En este caso el argumento no se utiliza. 
### Descripción
Esta función se encarga de controlar el parpadeo del Led Amarillo asociado con la presión del botón 2. Esta tarea se encarga de activar o desactivar el parpadeo del Led amarillo a través del bóton 2, además de imprimir un log asociado a estos cambios.\
La tarea va a permanecer bloqueada en espera de un evento en la *cola de gpios* hasta máximo 500ms. Si en este tiempo no se recibe nada, se revisa el estado de la variable *led_amarillo_parpadeando*, si esta en true, lo que hacemos es alternar el estado del led amarillo a través de una variable. Con esto conseguimos que el led se mantenga parpadeando cuando no se reciben eventos asociado al Boton2.\
En caso de recibir un evento, este dato lo guardamos y lo comparamos, ya que solo se procesan eventos asociados al Botón 2. De ser valido el evento, vamos a alternar la variable *led_amarillo_parpadeando*; si estaba en true ahora será false y viceversa. En caso de que esta variable cambie a false, apagaremos directamente el led, además de imprimir es un log que indique estos cambios. 
## *Función Task*: Led Verde
### Parametros 
void *pvParameters: Permite pasar cualquier tipo de parametro al momento de ejecutar la tarea. En este caso el argumento no se utiliza. 
### Descripción
Esta función se encarga de controlar el parpadeo del Led Verde asociado con la presión del botón 3. Esta tarea se encarga de activar una secuencia de parpadeos en el led verde cuando se presiona el boton, además indica a través de un log cuando dicha sencuencia es completada.\
La tarea permanece bloqueada en espera de un evento en la *cola de gpios* hasta máximo 100ms. Si en este tiempo no se recibe nada, la tarea ejecuta una instrucción de delay de 10ms sin hacer nada, y luego vuelve a ciclarse en la espera de un evento en la cola.\
En caso de recibir un evento, este dato lo guardamos y lo comparamos, ya que solamente se procesan eventos asociados al Boton 3. De ser valido el evento, iniciaremos una secuencia de parpadeos en el led verde y esta no se detendra hasta terminar la secuencia.
## *configurar_gpio* 
### Paremetros
void: No recibe argumentos 
### Descripción
Esta función se encarga de setear las configuraciones necesarias a los puertos GPIO a través de estructuras de configuración. 
## *configurar_interrupciones* 
### Paremetros
void: No recibe argumentos 
### Descripción
Esta función se encarga de setear la función de interrupción a cada uno de los Botones 

## app_main 
La función principal se encarga de crear las variables y estructuras necesarias para que cada una de las tareas opere correctamente. Además de que inicializa todas las variables globales en 0, asi como inicia apagando todos los leds.\
En este caso las tareas todas son definidas con prioridad 0 y se escriben logs cada que se termina de ejecutar alguna función de configuracion. 