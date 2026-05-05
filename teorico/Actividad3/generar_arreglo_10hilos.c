#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/syscall.h>

#define CANT_HILOS 10

void *funcion_hilo(void *arg) {
    int numero = *(int *)arg;

    printf("Hilo %d - TID: %ld\n", numero, syscall(SYS_gettid));

    return NULL;
}

int main() {
    pthread_t hilos[CANT_HILOS];
    int numeros[CANT_HILOS];

    for (int i = 0; i < CANT_HILOS; i++) {
        numeros[i] = i + 1;
        pthread_create(&hilos[i], NULL, funcion_hilo, &numeros[i]);
    }

    for (int i = 0; i < CANT_HILOS; i++) {
        pthread_join(hilos[i], NULL);
    }

    return 0;
}