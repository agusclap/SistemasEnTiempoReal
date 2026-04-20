#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <pthread.h>
#include <sched.h>
#include <pigpio.h>

#define AHT10_ADDR 0x38
#define I2C_BUS 1
#define FAN_GPIO 18

#define TEMP_ALTA 30.0f
#define TEMP_BAJA 25.0f

#define TIEMPO_SOBRETEMP_US 60000000UL   // 60 s
#define TIEMPO_VENT_US      120000000UL  // 120 s

typedef enum {
    REPOSO,
    ALERTA,
    VENTILACION
} Estado;

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

float temperatura = 0.0f;
Estado estado = REPOSO;
int ventilador = 0;
int ejecutando = 1;

/* ---------------- FUNCIONES AUXILIARES ---------------- */

const char* texto_estado(Estado e) {
    switch (e) {
        case REPOSO: return "REPOSO";
        case ALERTA: return "ALERTA";
        case VENTILACION: return "VENTILACION";
        default: return "DESCONOCIDO";
    }
}

uint32_t tiempo_transcurrido(uint32_t inicio, uint32_t fin) {
    return fin - inicio;
}

void guardar_temperatura(float t) {
    pthread_mutex_lock(&mutex);
    temperatura = t;
    pthread_mutex_unlock(&mutex);
}

float leer_temperatura_global() {
    float t;
    pthread_mutex_lock(&mutex);
    t = temperatura;
    pthread_mutex_unlock(&mutex);
    return t;
}

void cambiar_estado(Estado nuevo_estado) {
    pthread_mutex_lock(&mutex);
    estado = nuevo_estado;
    pthread_mutex_unlock(&mutex);
}

Estado leer_estado() {
    Estado e;
    pthread_mutex_lock(&mutex);
    e = estado;
    pthread_mutex_unlock(&mutex);
    return e;
}

void set_ventilador(int on) {
    gpioWrite(FAN_GPIO, on);

    pthread_mutex_lock(&mutex);
    ventilador = on;
    pthread_mutex_unlock(&mutex);
}

/* ---------------- AHT10 ---------------- */

int aht10_init(int fd) {
    char cmd[3] = {0xE1, 0x08, 0x00};

    if (i2cWriteDevice(fd, cmd, 3) < 0)
        return -1;

    time_sleep(0.04);
    return 0;
}

int aht10_medir(int fd, float *temp) {
    char cmd[3] = {0xAC, 0x33, 0x00};
    char data[6];

    if (i2cWriteDevice(fd, cmd, 3) < 0)
        return -1;

    time_sleep(0.08);

    if (i2cReadDevice(fd, data, 6) < 0)
        return -1;

    if (data[0] & 0x80)
        return -1;

    uint32_t raw_temp = ((uint32_t)(data[3] & 0x0F) << 16) |
                        ((uint32_t)(uint8_t)data[4] << 8) |
                        (uint32_t)(uint8_t)data[5];

    *temp = ((raw_temp * 200.0f) / 1048576.0f) - 50.0f;
    return 0;
}

/* ---------------- CONFIGURACION DE HILOS ---------------- */

void configurar_fifo(pthread_t hilo, int prioridad) {
    struct sched_param param;
    param.sched_priority = prioridad;
    pthread_setschedparam(hilo, SCHED_FIFO, &param);
}

void configurar_other(pthread_t hilo) {
    struct sched_param param;
    param.sched_priority = 0;
    pthread_setschedparam(hilo, SCHED_OTHER, &param);
}

/* ---------------- HILO A: ADQUISICION ---------------- */

void* hilo_adquisicion(void *arg) {
    int fd = *(int*)arg;

    while (ejecutando) {
        float t;

        if (aht10_medir(fd, &t) == 0) {
            guardar_temperatura(t);
        } else {
            printf("[Adquisicion] Error al leer sensor\n");
        }

        sleep(1);
    }

    return NULL;
}

/* ---------------- HILO B: CONTROL ---------------- */

void* hilo_control(void *arg) {
    (void)arg;

    uint32_t inicio_sobretemp = 0;
    uint32_t inicio_vent = 0;
    int contando_sobretemp = 0;

    while (ejecutando) {
        float t = leer_temperatura_global();
        Estado e = leer_estado();
        uint32_t ahora = gpioTick();

        switch (e) {
            case REPOSO:
                if (t > TEMP_ALTA) {
                    inicio_sobretemp = ahora;
                    contando_sobretemp = 1;
                    cambiar_estado(ALERTA);
                }
                break;

            case ALERTA:
                if (t > TEMP_ALTA) {
                    if (contando_sobretemp &&
                        tiempo_transcurrido(inicio_sobretemp, ahora) >= TIEMPO_SOBRETEMP_US) {
                        set_ventilador(1);
                        inicio_vent = ahora;
                        cambiar_estado(VENTILACION);
                        contando_sobretemp = 0;
                    }
                } else {
                    contando_sobretemp = 0;
                    cambiar_estado(REPOSO);
                }
                break;

            case VENTILACION:
                if (t < TEMP_BAJA ||
                    tiempo_transcurrido(inicio_vent, ahora) >= TIEMPO_VENT_US) {
                    set_ventilador(0);
                    cambiar_estado(REPOSO);
                }
                break;
        }

        usleep(100000); // 100 ms
    }

    return NULL;
}

/* ---------------- HILO C: DIAGNOSTICO ---------------- */

void* hilo_diagnostico(void *arg) {
    (void)arg;

    uint32_t inicio = gpioTick();

    while (ejecutando) {
        float t;
        Estado e;
        int fan;

        pthread_mutex_lock(&mutex);
        t = temperatura;
        e = estado;
        fan = ventilador;
        pthread_mutex_unlock(&mutex);

        double segundos = tiempo_transcurrido(inicio, gpioTick()) / 1000000.0;

        printf("[Diagnostico] Tiempo: %.1f s | Temp: %.2f C | Estado: %s | Ventilador: %s\n",
               segundos, t, texto_estado(e), fan ? "ENCENDIDO" : "APAGADO");

        sleep(5);
    }

    return NULL;
}

/* ---------------- MAIN ---------------- */

int main() {
    pthread_t thA, thB, thC;
    int fd;

    if (gpioInitialise() < 0) {
        printf("Error al iniciar pigpio\n");
        return 1;
    }

    gpioSetMode(FAN_GPIO, PI_OUTPUT);
    gpioWrite(FAN_GPIO, 0);

    fd = i2cOpen(I2C_BUS, AHT10_ADDR, 0);
    if (fd < 0) {
        printf("Error al abrir I2C\n");
        gpioTerminate();
        return 1;
    }

    if (aht10_init(fd) < 0) {
        printf("Error al inicializar AHT10\n");
        i2cClose(fd);
        gpioTerminate();
        return 1;
    }

    pthread_create(&thA, NULL, hilo_adquisicion, &fd);
    pthread_create(&thB, NULL, hilo_control, NULL);
    pthread_create(&thC, NULL, hilo_diagnostico, NULL);

    configurar_fifo(thA, 80);
    configurar_fifo(thB, 60);
    configurar_other(thC);

    printf("Sistema iniciado\n");

    pthread_join(thA, NULL);
    pthread_join(thB, NULL);
    pthread_join(thC, NULL);

    set_ventilador(0);
    i2cClose(fd);
    gpioTerminate();

    return 0;
}