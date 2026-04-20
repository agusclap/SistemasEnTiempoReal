#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <sched.h>
#include <string.h>
#include <pigpio.h>

#define I2C_BUS 1
#define AHT10_ADDR 0x38
#define FAN_GPIO 17

#define REPOSO 0
#define ALERTA 1
#define VENTILACION 2

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

float temperatura = 0.0;
int estado = REPOSO;
int ventilador = 0;
int seguir = 1;

int i2c_handle;
uint32_t inicio_programa;

/* para cortar con Ctrl+C */
void terminar(int sig)
{
    seguir = 0;
}

/* lectura simple del AHT10 */
int leer_temperatura(float *temp)
{
    char cmd[3];
    char data[6];

    cmd[0] = 0xAC;
    cmd[1] = 0x33;
    cmd[2] = 0x00;

    if (i2cWriteDevice(i2c_handle, cmd, 3) != 0)
        return -1;

    usleep(80000); /* 80 ms */

    if (i2cReadDevice(i2c_handle, data, 6) != 6)
        return -1;

    if ((data[0] & 0x80) != 0)
        return -1;

    uint32_t temp_raw;
    temp_raw = ((uint32_t)(data[3] & 0x0F) << 16) |
               ((uint32_t)(unsigned char)data[4] << 8) |
               ((uint32_t)(unsigned char)data[5]);

    *temp = ((float)temp_raw * 200.0 / 1048576.0) - 50.0;

    return 0;
}

/* Hilo A: adquisicion */
void *tarea_A(void *arg)
{
    struct sched_param p;
    p.sched_priority = sched_get_priority_max(SCHED_FIFO);
    pthread_setschedparam(pthread_self(), SCHED_FIFO, &p);

    /* inicializacion del sensor */
    char init_cmd[3] = {0xE1, 0x08, 0x00};

    if (i2cWriteDevice(i2c_handle, init_cmd, 3) != 0)
    {
        printf("Error al inicializar AHT10\n");
        seguir = 0;
        return NULL;
    }

    usleep(20000);

    while (seguir)
    {
        float t;

        if (leer_temperatura(&t) == 0)
        {
            pthread_mutex_lock(&mutex);
            temperatura = t;
            pthread_mutex_unlock(&mutex);
        }
        else
        {
            printf("Error leyendo temperatura\n");
        }

        sleep(1);
    }

    return NULL;
}

/* Hilo B: control logico */
void *tarea_B(void *arg)
{
    struct sched_param p;
    int max = sched_get_priority_max(SCHED_FIFO);
    int min = sched_get_priority_min(SCHED_FIFO);

    p.sched_priority = (max + min) / 2;
    pthread_setschedparam(pthread_self(), SCHED_FIFO, &p);

    uint32_t inicio_alerta = 0;
    uint32_t inicio_vent = 0;

    while (seguir)
    {
        float t;

        pthread_mutex_lock(&mutex);
        t = temperatura;
        pthread_mutex_unlock(&mutex);

        if (estado == REPOSO)
        {
            if (t > 30.0)
            {
                estado = ALERTA;
                inicio_alerta = gpioTick();
            }
        }
        else if (estado == ALERTA)
        {
            if (t > 30.0)
            {
                if (gpioTick() - inicio_alerta >= 60000000)
                {
                    estado = VENTILACION;
                    ventilador = 1;
                    gpioWrite(FAN_GPIO, 1);
                    inicio_vent = gpioTick();
                }
            }
            else
            {
                estado = REPOSO;
            }
        }
        else if (estado == VENTILACION)
        {
            if ((gpioTick() - inicio_vent >= 120000000) || (t < 25.0))
            {
                estado = REPOSO;
                ventilador = 0;
                gpioWrite(FAN_GPIO, 0);
            }
        }

        usleep(200000); /* 200 ms */
    }

    gpioWrite(FAN_GPIO, 0);
    return NULL;
}

/* Hilo C: diagnostico */
void *tarea_C(void *arg)
{
    uint32_t ahora, segundos;
    float t;

    while (seguir)
    {
        pthread_mutex_lock(&mutex);
        t = temperatura;
        pthread_mutex_unlock(&mutex);

        ahora = gpioTick();
        segundos = (ahora - inicio_programa) / 1000000;

        printf("Tiempo: %u s | Temp: %.2f C | ", segundos, t);

        if (estado == REPOSO)
            printf("Estado: REPOSO | ");
        else if (estado == ALERTA)
            printf("Estado: ALERTA | ");
        else
            printf("Estado: VENTILACION | ");

        if (ventilador == 1)
            printf("Ventilador: ENCENDIDO\n");
        else
            printf("Ventilador: APAGADO\n");

        sleep(5);
    }

    return NULL;
}

int main()
{
    signal(SIGINT, terminar);

    if (gpioInitialise() < 0)
    {
        printf("Error en gpioInitialise()\n");
        return 1;
    }

    gpioSetMode(FAN_GPIO, PI_OUTPUT);
    gpioWrite(FAN_GPIO, 0);

    i2c_handle = i2cOpen(I2C_BUS, AHT10_ADDR, 0);
    if (i2c_handle < 0)
    {
        printf("Error al abrir I2C\n");
        gpioTerminate();
        return 1;
    }

    inicio_programa = gpioTick();

    pthread_t hiloA, hiloB, hiloC;

    pthread_create(&hiloA, NULL, tarea_A, NULL);
    pthread_create(&hiloB, NULL, tarea_B, NULL);
    pthread_create(&hiloC, NULL, tarea_C, NULL);

    pthread_join(hiloA, NULL);
    pthread_join(hiloB, NULL);
    pthread_join(hiloC, NULL);

    i2cClose(i2c_handle);
    pthread_mutex_destroy(&mutex);
    gpioTerminate();

    return 0;
}