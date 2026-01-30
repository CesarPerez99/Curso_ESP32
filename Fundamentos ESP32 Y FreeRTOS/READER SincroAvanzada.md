# ARCHIVO: SincroAvanzada.c
## Descripción
 
## Librerias
Librerias basicas para manejo de RTOS y logear en la consola.\
*esp_random.h*: Se usa para crear datos aleatorios que simulen la lectura de sensores.
## Macros 
Se define el tamaño de datos que puede contener la cola de sensores, el valor maximo para simular los datos de un sensor y el tamaño de las tareas que se crearan.\
También se le asignara un bit a cada sensor, para poder manejar un grupo de eventos dentro del programa. 
## Estructuras y Enums 
Se crean 2 estructuras, la primera guarda información referente al sensor, como los datos, id, entre otros.\
La segunda es una estructura que guarda los valores promedio de cada sensor, además de un contador que indica cuantas muestras han sido procesadas. Esta estructura sera un recurso compartido entre mas tareas, por lo que el acceso a ella debera ser protegido mediante un semaforo, en este caso un mutex. 
## Variables globales
Se crean diversos *handles* para cada recurso que se va a utilizar. 
-Cola donde llegan estructura de datos de los sensores
-Semaforos para inicializacion y control de recursos
-Mutex para estructuras que contienen los valores promedio de las lecturas y el total de muestras
-Grupo de eventos para sincronización con tareas. 
## FUNCIONES 
## *Funcion tarea: Sensor de temperatura*
### Parámetros
void *pvParameters: Permite pasar cualquier tipo de parametro al momento de ejecutar la tarea. En este caso el argumento no se utiliza.
### Descripción 
Esta función simula la lectura de un sensor de temperatura a través de datos aleatorios, así como el envío de dichos datos a traves de una cola compartida.\
La tarea crea una estructura ded datos para guardar la información leída por el sensor, su ID correspondiente es el 1. Posteriormente  de crearla notifica en el grupo de eventos que el sensor esta listo y despues entra al bucle infinito. 
