#include <stdio.h>
#include <linux/time.h>
#include <stdint.h>
#include <unistd.h>
#include <pigpio.h>

#define TICK_MS 100

#define PERIODO_TAREA1 100
#define PERIODO_TAREA2 300
#define PERIODO_TAREA3 500

#define LED_TAREA1 17
#define LED_TAREA2 27
#define LED_TAREA3 22

#define MS_TO_NS(ms) ((ms) * 1000000L)

long tiempo_transcurrido_ms(struct timespec inicio) {
    struct timespec ahora;
    clock_gettime(CLOCK_MONOTONIC, &ahora);

    long segundos = ahora.tv_sec - inicio.tv_sec;
    long nanosegundos = ahora.tv_nsec - inicio.tv_nsec;

    return (segundos * 1000) + (nanosegundos / 1000000);
}

void sumar_ms(struct timespec *tiempo, long ms) {
    tiempo->tv_nsec += MS_TO_NS(ms);

    while (tiempo->tv_nsec >= 1000000000L) {
        tiempo->tv_nsec -= 1000000000L;
        tiempo->tv_sec++;
    }
}

void tarea1(long tiempo_ms) {
    static int estado = 0;

    estado = !estado;
    gpioWrite(LED_TAREA1, estado);

    printf("[Tarea 1] LED GPIO %d -> %s | Tiempo: %ld ms\n",
           LED_TAREA1,
           estado ? "ON" : "OFF",
           tiempo_ms);
}

void tarea2(long tiempo_ms) {
    static int estado = 0;

    estado = !estado;
    gpioWrite(LED_TAREA2, estado);

    printf("[Tarea 2] LED GPIO %d -> %s | Tiempo: %ld ms\n",
           LED_TAREA2,
           estado ? "ON" : "OFF",
           tiempo_ms);
}

void tarea3(long tiempo_ms) {
    static int estado = 0;

    estado = !estado;
    gpioWrite(LED_TAREA3, estado);

    printf("[Tarea 3] LED GPIO %d -> %s | Tiempo: %ld ms\n",
           LED_TAREA3,
           estado ? "ON" : "OFF",
           tiempo_ms);
}

int main() {
    struct timespec inicio;
    struct timespec proximo_tick;

    if (gpioInitialise() < 0) {
        printf("Error: no se pudo inicializar pigpio\n");
        return 1;
    }

    gpioSetMode(LED_TAREA1, PI_OUTPUT);
    gpioSetMode(LED_TAREA2, PI_OUTPUT);
    gpioSetMode(LED_TAREA3, PI_OUTPUT);

    gpioWrite(LED_TAREA1, 0);
    gpioWrite(LED_TAREA2, 0);
    gpioWrite(LED_TAREA3, 0);

    clock_gettime(CLOCK_MONOTONIC, &inicio);
    proximo_tick = inicio;

    long contador_ms = 0;

    printf("Inicio del ejecutivo ciclico con LEDs\n");
    printf("Tick base: %d ms\n\n", TICK_MS);

    while (1) {
        long tiempo_ms = tiempo_transcurrido_ms(inicio);

        if (contador_ms % PERIODO_TAREA1 == 0) {
            tarea1(tiempo_ms);
        }

        if (contador_ms % PERIODO_TAREA2 == 0) {
            tarea2(tiempo_ms);
        }

        if (contador_ms % PERIODO_TAREA3 == 0) {
            tarea3(tiempo_ms);
        }

        printf("---------------------------------\n");

        contador_ms += TICK_MS;

        sumar_ms(&proximo_tick, TICK_MS);

        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &proximo_tick, NULL);
    }

    gpioTerminate();

    return 0;
}