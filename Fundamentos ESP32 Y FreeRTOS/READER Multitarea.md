# ARCHIVO: Multitarea.c
## Descripción
El propósito de este programa es mostrar el uso del multitasking utilizando diversas tareas que comparten una variable compartida. Además, con este programa también se ve el uso de semaforos, cuya finalidad es resguardar o tener una forma de cuidar el uso de variables o recursos criticos en nuestro sistema, que comparten 2 o mas tareas. Esto se demuestra teniendo una tarea para conmutar un led, otra para aumentar un contador y otra para loggear los datos del sistema en el momento de ejecución. 
## Librerias
Librerias basicas para manejo de RTOS y logear en la consola.\
*esp_system.h*: Contiene funciones que permiten obtener cosas referentes al sistema/SoC. 
## Macros
Aquí definimos las prioridades para cada una de nuestras tareas y el stack que reservaremos para cada una de ellas, además de asignarle una palabra a nuestro pin GPIO que controlara el LED. 
## Variables globales 
Entre las mas importantes esta un handler de semaforo para un mutex counter y el contador global. 

## FUNCIONES
## *Tarea del LED*
### Parámetros
void *pvParameters: Permite pasar cualquier tipo de parametro al momento de ejecutar la tarea. En este caso el argumento no se utiliza.
### Descripción 
Esta función se encarga de establecer la configuración incial del pin GPIO controla el LED, además de definir la lógica para que este led se encuentre conmutando.\
Al inicio de la tarea se realiza una sola vez la configuración del pin mediante una estructura de *gpio_cofig_t*. Posteriormente, dentro del ciclo infinito, lo que se hace es alternar el estado de una variable, y con ella seteamos y cambiamos el estado del LED\
Cada cambio se imprime a través de un LOG de consola y la tarea entra a un delay de 1s, antes de volver a realizar otra iteración 
## *Tarea del contador*
### Parámetros
void *pvParameters: Permite pasar cualquier tipo de parametro al momento de ejecutar la tarea. En este caso el argumento no se utiliza.
### Descripción
Esta función se encarga de aumentar el contador global en 1 e indicar mediante un log si pudo realizar esta operación o no.\
Al incio de la tarea, se imprime en consola en que Nucleo estara corriendo esta tarea, con fines de monitoreo. Posteriormente, dentro del ciclo infinito, vamos a intentar tomar el *mutex* que protege al contador global, hasta maximo 100ms.\
En que caso de que pueda tomarse, se incrementa el contador global, y se actualiza el contador local de la tarea, mostrando este dato en la consola. En caso de que no se pueda tomar el mutex, se notificara esto mediante un log\
En cualquiera caso, la tarea entra en un retardo de 2 segundos antes de realizar una nueva iteración. 
## *Tarea monitor del sistema*
### Parámetros
void *pvParameters: Permite pasar cualquier tipo de parametro al momento de ejecutar la tarea. En este caso el argumento no se utiliza.
### Descripción
Esta función se encarga de imprimir información actual del sistema en consola, además del valor del contador global.\
Al incio de la tarea, se imprime en consola en que Nucleo estara corriendo esta tarea, con fines de monitoreo. Posteriormente, dentro del ciclo infinito, obtendremos información referente al heap mediante funciones que estan en la librería *esp_system.h*\
Se obtiene el contador global de forma segura a través de un mutex, teniendo como limite 100ms.\
Despues, todos estos datos obtenidos se imprimen en consola, además del tiempo de ejecución del sistema y se entra a un retardo de 5 segundos antes de realizar otra iteración. 
## app_main 
La función app_main se encarga de inicializar los recursos principales del sistema. En ella se crea el mutex utilizado para proteger el contador global, así como los handles de cada tarea, los cuales se asignan al momento de su creación.\
Dentro de app_main se incluye un ciclo infinito que imprime un mensaje de ejecución cada 10 segundos. Este comportamiento simula la ejecución continua de la tarea principal y representa el espacio donde podrían añadirse otras funciones o lógica adicional del sistema.
