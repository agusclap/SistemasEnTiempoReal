#include <stdio.h>
#include <linux/time.h>
#include <stdint.h>
#include <unistd.h>

#define TICK_MS 100

#define PERIODO_TAREA1 100
#define PERIODO_TAREA2 300
#define PERIODO_TAREA3 500

// Convierte milisegundos a nanosegundos
#define MS_TO_NS(ms) ((ms) * 1000000L)

// Obtiene el tiempo transcurrido en milisegundos desde un instante inicial
long tiempo_transcurrido_ms(struct timespec inicio) {
    struct timespec ahora;
    clock_gettime(CLOCK_MONOTONIC, &ahora);

    long segundos = ahora.tv_sec - inicio.tv_sec;
    long nanosegundos = ahora.tv_nsec - inicio.tv_nsec;

    return (segundos * 1000) + (nanosegundos / 1000000);
}

// Suma milisegundos a una estructura timespec
void sumar_ms(struct timespec *tiempo, long ms) {
    tiempo->tv_nsec += MS_TO_NS(ms);

    while (tiempo->tv_nsec >= 1000000000L) {
        tiempo->tv_nsec -= 1000000000L;
        tiempo->tv_sec++;
    }
}

// Tarea periódica 1
void tarea1(long tiempo_ms) {
    printf("[Tarea 1] Ejecutada a los %ld ms\n", tiempo_ms);
}

// Tarea periódica 2
void tarea2(long tiempo_ms) {
    printf("[Tarea 2] Ejecutada a los %ld ms\n", tiempo_ms);
}

// Tarea periódica 3
void tarea3(long tiempo_ms) {
    printf("[Tarea 3] Ejecutada a los %ld ms\n", tiempo_ms);
}

int main() {
    struct timespec inicio;
    struct timespec proximo_tick;

    clock_gettime(CLOCK_MONOTONIC, &inicio);
    proximo_tick = inicio;

    long contador_ms = 0;

    printf("Inicio del ejecutivo ciclico\n");
    printf("Tick base: %d ms\n\n", TICK_MS);

    while (1) {
        long tiempo_ms = tiempo_transcurrido_ms(inicio);

        /*
         * Ejecutivo ciclico:
         * Cada 100 ms se evalua que tareas deben ejecutarse.
         */

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

        // Calcula el instante absoluto del proximo tick
        sumar_ms(&proximo_tick, TICK_MS);

        // Espera hasta el proximo tick usando tiempo absoluto
        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &proximo_tick, NULL);
    }

    return 0;
}