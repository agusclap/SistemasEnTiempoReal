#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/syscall.h>

void *funcion_hilo(void *arg) {
    printf("Hilo creado\n");
    printf("PID: %d\n", getpid());
    printf("TID: %ld\n", syscall(SYS_gettid));

    return NULL;
}

int main() {
    pthread_t hilo;

    pthread_create(&hilo, NULL, funcion_hilo, NULL);

    pthread_join(hilo, NULL);

    return 0;
}