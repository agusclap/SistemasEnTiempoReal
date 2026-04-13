#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <sched.h>
#include <errno.h>
#include <pigpio.h>

#define AHT10_ADDR          0x38
#define I2C_BUS             1
#define FAN_GPIO            18

#define TEMP_ON_THRESHOLD   30.0f
#define TEMP_OFF_THRESHOLD  25.0f

#define OVER_TEMP_TIME_US   60000000UL   // 60 s
#define FAN_HOLD_TIME_US    120000000UL  // 120 s

typedef enum {
    ESTADO_REPOSO = 0,
    ESTADO_ALERTA,
    ESTADO_VENTILACION
} estado_t;

/* -------------------- Variables globales compartidas -------------------- */

static pthread_mutex_t mutex_datos = PTHREAD_MUTEX_INITIALIZER;

static float temperatura_actual = 0.0f;
static estado_t estado_actual = ESTADO_REPOSO;
static int ventilador_encendido = 0;
static int sistema_activo = 1;

/* -------------------- Utilidades -------------------- */

static const char *estado_a_texto(estado_t e) {
    switch (e) {
        case ESTADO_REPOSO:       return "REPOSO";
        case ESTADO_ALERTA:       return "ALERTA";
        case ESTADO_VENTILACION:  return "VENTILACION";
        default:                  return "DESCONOCIDO";
    }
}

/* Delta de tiempo usando gpioTick() */
static uint32_t delta_ticks(uint32_t inicio, uint32_t fin) {
    return fin - inicio; /* maneja overflow natural de uint32_t */
}

/* -------------------- Manejo AHT10 -------------------- */

/*
 * Inicialización típica del AHT10.
 * Según implementación real, el sensor puede requerir pequeños ajustes.
 */
static int aht10_init(int i2c_handle) {
    char cmd[3] = {0xE1, 0x08, 0x00};

    if (i2cWriteDevice(i2c_handle, cmd, 3) < 0) {
        return -1;
    }

    time_sleep(0.04); // 40 ms
    return 0;
}

static int aht10_trigger_measurement(int i2c_handle) {
    char cmd[3] = {0xAC, 0x33, 0x00};

    if (i2cWriteDevice(i2c_handle, cmd, 3) < 0) {
        return -1;
    }

    time_sleep(0.08); // esperar medición
    return 0;
}

/*
 * Lee 6 bytes y extrae temperatura.
 * Fórmula típica AHT10:
 * Temp = ((rawTemp / 2^20) * 200) - 50
 */
static int aht10_read_temperature(int i2c_handle, float *temp_c) {
    char data[6];

    if (aht10_trigger_measurement(i2c_handle) < 0) {
        return -1;
    }

    if (i2cReadDevice(i2c_handle, data, 6) < 0) {
        return -1;
    }

    /* Bit busy en byte 0, bit 7 */
    if (data[0] & 0x80) {
        return -1;
    }

    uint32_t raw_temp = ((uint32_t)(data[3] & 0x0F) << 16) |
                        ((uint32_t)(uint8_t)data[4] << 8) |
                        (uint32_t)(uint8_t)data[5];

    *temp_c = ((raw_temp * 200.0f) / 1048576.0f) - 50.0f;
    return 0;
}

/* -------------------- GPIO ventilador -------------------- */

static void set_ventilador(int on) {
    gpioWrite(FAN_GPIO, on ? 1 : 0);

    pthread_mutex_lock(&mutex_datos);
    ventilador_encendido = on ? 1 : 0;
    pthread_mutex_unlock(&mutex_datos);
}

/* -------------------- Configuración de hilos -------------------- */

static int configurar_hilo_fifo(pthread_t thread, int prioridad) {
    struct sched_param param;
    param.sched_priority = prioridad;

    if (pthread_setschedparam(thread, SCHED_FIFO, &param) != 0) {
        return -1;
    }
    return 0;
}

static int configurar_hilo_other(pthread_t thread) {
    struct sched_param param;
    param.sched_priority = 0;

    if (pthread_setschedparam(thread, SCHED_OTHER, &param) != 0) {
        return -1;
    }
    return 0;
}

/* -------------------- Tarea A: adquisición -------------------- */

void *tarea_adquisicion(void *arg) {
    int i2c_handle = *(int *)arg;

    while (sistema_activo) {
        float temp_leida = 0.0f;

        /* IMPORTANTE: leer I2C sin tener el mutex tomado */
        if (aht10_read_temperature(i2c_handle, &temp_leida) == 0) {
            pthread_mutex_lock(&mutex_datos);
            temperatura_actual = temp_leida;
            pthread_mutex_unlock(&mutex_datos);
        } else {
            /* Se podría loguear error, pero fuera de mutex */
            fprintf(stderr, "[Adquisicion] Error al leer AHT10\n");
        }

        sleep(1);
    }

    return NULL;
}

/* -------------------- Tarea B: control lógico -------------------- */

void *tarea_control(void *arg) {
    (void)arg;

    uint32_t t_inicio_sobretemp = 0;
    int sobretemp_activa = 0;

    uint32_t t_inicio_vent = 0;

    while (sistema_activo) {
        float temp_local;
        estado_t estado_local;

        /* Copia rápida de datos compartidos */
        pthread_mutex_lock(&mutex_datos);
        temp_local = temperatura_actual;
        estado_local = estado_actual;
        pthread_mutex_unlock(&mutex_datos);

        uint32_t ahora = gpioTick();

        switch (estado_local) {
            case ESTADO_REPOSO:
                if (temp_local > TEMP_ON_THRESHOLD) {
                    if (!sobretemp_activa) {
                        sobretemp_activa = 1;
                        t_inicio_sobretemp = ahora;
                    }

                    pthread_mutex_lock(&mutex_datos);
                    estado_actual = ESTADO_ALERTA;
                    pthread_mutex_unlock(&mutex_datos);
                } else {
                    sobretemp_activa = 0;
                }
                break;

            case ESTADO_ALERTA:
                if (temp_local > TEMP_ON_THRESHOLD) {
                    if (!sobretemp_activa) {
                        sobretemp_activa = 1;
                        t_inicio_sobretemp = ahora;
                    }

                    if (delta_ticks(t_inicio_sobretemp, ahora) >= OVER_TEMP_TIME_US) {
                        set_ventilador(1);
                        t_inicio_vent = gpioTick();

                        pthread_mutex_lock(&mutex_datos);
                        estado_actual = ESTADO_VENTILACION;
                        pthread_mutex_unlock(&mutex_datos);

                        sobretemp_activa = 0;
                    }
                } else {
                    sobretemp_activa = 0;

                    pthread_mutex_lock(&mutex_datos);
                    estado_actual = ESTADO_REPOSO;
                    pthread_mutex_unlock(&mutex_datos);
                }
                break;

            case ESTADO_VENTILACION:
                if (temp_local < TEMP_OFF_THRESHOLD) {
                    set_ventilador(0);

                    pthread_mutex_lock(&mutex_datos);
                    estado_actual = ESTADO_REPOSO;
                    pthread_mutex_unlock(&mutex_datos);
                } else if (delta_ticks(t_inicio_vent, ahora) >= FAN_HOLD_TIME_US) {
                    set_ventilador(0);

                    pthread_mutex_lock(&mutex_datos);
                    estado_actual = ESTADO_REPOSO;
                    pthread_mutex_unlock(&mutex_datos);
                }
                break;

            default:
                pthread_mutex_lock(&mutex_datos);
                estado_actual = ESTADO_REPOSO;
                pthread_mutex_unlock(&mutex_datos);
                break;
        }

        usleep(100000); // 100 ms de período de control
    }

    return NULL;
}

/* -------------------- Tarea C: diagnóstico -------------------- */

void *tarea_diagnostico(void *arg) {
    (void)arg;

    uint32_t t0 = gpioTick();

    while (sistema_activo) {
        float temp_local;
        int fan_local;
        estado_t estado_local;

        pthread_mutex_lock(&mutex_datos);
        temp_local = temperatura_actual;
        fan_local = ventilador_encendido;
        estado_local = estado_actual;
        pthread_mutex_unlock(&mutex_datos);

        uint32_t ahora = gpioTick();
        double tiempo_seg = delta_ticks(t0, ahora) / 1000000.0;

        printf("[Diagnostico] Tiempo: %.1f s | Temp: %.2f C | Estado: %s | Ventilador: %s\n",
               tiempo_seg,
               temp_local,
               estado_a_texto(estado_local),
               fan_local ? "ENCENDIDO" : "APAGADO");

        sleep(5);
    }

    return NULL;
}

/* -------------------- Main -------------------- */

int main(void) {
    pthread_t th_adq, th_ctrl, th_diag;
    int i2c_handle;

    if (gpioInitialise() < 0) {
        fprintf(stderr, "Error: gpioInitialise() fallo\n");
        return EXIT_FAILURE;
    }

    gpioSetMode(FAN_GPIO, PI_OUTPUT);
    gpioWrite(FAN_GPIO, 0);

    i2c_handle = i2cOpen(I2C_BUS, AHT10_ADDR, 0);
    if (i2c_handle < 0) {
        fprintf(stderr, "Error: i2cOpen() fallo\n");
        gpioTerminate();
        return EXIT_FAILURE;
    }

    if (aht10_init(i2c_handle) < 0) {
        fprintf(stderr, "Error: no se pudo inicializar el AHT10\n");
        i2cClose(i2c_handle);
        gpioTerminate();
        return EXIT_FAILURE;
    }

    if (pthread_create(&th_adq, NULL, tarea_adquisicion, &i2c_handle) != 0) {
        fprintf(stderr, "Error creando hilo adquisicion\n");
        i2cClose(i2c_handle);
        gpioTerminate();
        return EXIT_FAILURE;
    }

    if (pthread_create(&th_ctrl, NULL, tarea_control, NULL) != 0) {
        fprintf(stderr, "Error creando hilo control\n");
        sistema_activo = 0;
        pthread_join(th_adq, NULL);
        i2cClose(i2c_handle);
        gpioTerminate();
        return EXIT_FAILURE;
    }

    if (pthread_create(&th_diag, NULL, tarea_diagnostico, NULL) != 0) {
        fprintf(stderr, "Error creando hilo diagnostico\n");
        sistema_activo = 0;
        pthread_join(th_adq, NULL);
        pthread_join(th_ctrl, NULL);
        i2cClose(i2c_handle);
        gpioTerminate();
        return EXIT_FAILURE;
    }

    /* Prioridades */
    if (configurar_hilo_fifo(th_adq, 80) != 0) {
        perror("No se pudo asignar SCHED_FIFO a adquisicion");
    }

    if (configurar_hilo_fifo(th_ctrl, 60) != 0) {
        perror("No se pudo asignar SCHED_FIFO a control");
    }

    if (configurar_hilo_other(th_diag) != 0) {
        perror("No se pudo asignar SCHED_OTHER a diagnostico");
    }

    printf("Sistema iniciado. Ctrl+C para salir.\n");

    pthread_join(th_adq, NULL);
    pthread_join(th_ctrl, NULL);
    pthread_join(th_diag, NULL);

    set_ventilador(0);
    i2cClose(i2c_handle);
    gpioTerminate();

    return EXIT_SUCCESS;
}