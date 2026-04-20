#include <stdio.h>
#include <linux/time.h>
#include <unistd.h>

int main() {
    struct timespec start, current;
    clock_gettime(CLOCK_MONOTONIC, &start);

    // Representan el proximo instante de ejecución (deadline de cada tarea)
    long next1 = 100;
    long next2 = 300;
    long next3 = 500;

    while (1) {
        clock_gettime(CLOCK_MONOTONIC, &current);

        long elapsed_ms = (current.tv_sec - start.tv_sec) * 1000 +
                          (current.tv_nsec - start.tv_nsec) / 1000000;

        if (elapsed_ms >= next1) {
            printf("Tarea 1: %ld ms\n", elapsed_ms);
            next1 += 100;
        }

        if (elapsed_ms >= next2) {
            printf("Tarea 2: %ld ms\n", elapsed_ms);
            next2 += 300;
        }

        if (elapsed_ms >= next3) {
            printf("Tarea 3: %ld ms\n", elapsed_ms);
            next3 += 500;
        }

        usleep(1000); // opcional pero recomendable
    }

    return 0;
}

/*
La salida muestra que las tareas se ejecutan de forma periódica
según sus temporizaciones nominales. Los pequeños desfasajes de 1 ms
 se deben a la resolución del temporizador, al retardo del scheduler
  de Linux y al tiempo de ejecución de las instrucciones de impresión.
*/