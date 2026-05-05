#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/resource.h>
#include <sched.h>
#include <sys/syscall.h>

#define CANT_HILOS 3

pthread_t hilos[CANT_HILOS];

const char* nombre_politica(int politica) {
    switch (politica) {
        case SCHED_OTHER: return "SCHED_OTHER";
        case SCHED_FIFO:  return "SCHED_FIFO";
        case SCHED_RR:    return "SCHED_RR";
        default:          return "DESCONOCIDA";
    }
}

void* funcion_hilo(void* arg) {
    int id = *(int*)arg;

    while (1) {
        int politica;
        struct sched_param param;
        pid_t tid = syscall(SYS_gettid);

        pthread_getschedparam(pthread_self(), &politica, &param);

        printf("Hilo %d | TID: %d | Politica: %s | Prioridad: %d\n",
               id,
               tid,
               nombre_politica(politica),
               param.sched_priority);

        sleep(1);
    }

    return NULL;
}

int main() {
    int ids[CANT_HILOS];

    for (int i = 0; i < CANT_HILOS; i++) {
        ids[i] = i + 1;
        pthread_create(&hilos[i], NULL, funcion_hilo, &ids[i]);
    }

    sleep(10);

    printf("\nEl proceso principal cambia prioridades de los hilos\n\n");

    struct sched_param param;

    param.sched_priority = 10;
    pthread_setschedparam(hilos[0], SCHED_FIFO, &param);

    param.sched_priority = 20;
    pthread_setschedparam(hilos[1], SCHED_FIFO, &param);

    param.sched_priority = 30;
    pthread_setschedparam(hilos[2], SCHED_FIFO, &param);

    sleep(10);

    for (int i = 0; i < CANT_HILOS; i++) {
        pthread_cancel(hilos[i]);
    }

    for (int i = 0; i < CANT_HILOS; i++) {
        pthread_join(hilos[i], NULL);
    }

    printf("Programa finalizado\n");

    return 0;
}