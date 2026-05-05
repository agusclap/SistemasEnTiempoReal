#define _GNU_SOURCE
#include <stdio.h>
#include <pthread.h>
#include <sched.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>
#include <string.h>

volatile long cont_est = 0, cont_nav = 0, cont_tel = 0;
volatile int activo = 1;

pthread_mutex_t recurso;

// Alta prioridad: necesita el recurso
void* task_estabilidad(void* a) {
    while (activo) {
        pthread_mutex_lock(&recurso);
        cont_est++;
        pthread_mutex_unlock(&recurso);
    }
    return NULL;
}

// Prioridad media: no usa el recurso
void* task_navegacion(void* a) {
    while (activo) {
        cont_nav++;
    }
    return NULL;
}

// Baja prioridad: toma el recurso y lo retiene
void* task_telemetria(void* a) {
    while (activo) {
        pthread_mutex_lock(&recurso);

        cont_tel++;
        printf("[Telemetría] Mutex tomado por tarea baja...\n");
        fflush(stdout);

        usleep(300000); // 300 ms reteniendo el recurso

        pthread_mutex_unlock(&recurso);

        usleep(200000); // deja una pequeña ventana antes de volver a tomarlo
    }
    return NULL;
}

void set_prio(pthread_attr_t *at, int p) {
    struct sched_param sp;
    memset(&sp, 0, sizeof(sp));
    sp.sched_priority = p;

    pthread_attr_init(at);
    pthread_attr_setschedpolicy(at, SCHED_FIFO);
    pthread_attr_setschedparam(at, &sp);
    pthread_attr_setinheritsched(at, PTHREAD_EXPLICIT_SCHED);
}

int main() {
    pthread_t h1, h2, h3;
    pthread_attr_t a1, a2, a3;

    pthread_mutex_init(&recurso, NULL);

    set_prio(&a1, 80); // Estabilidad
    set_prio(&a2, 40); // Navegación
    set_prio(&a3, 10); // Telemetría

    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(0, &cpuset);

    pthread_attr_setaffinity_np(&a1, sizeof(cpu_set_t), &cpuset);
    pthread_attr_setaffinity_np(&a2, sizeof(cpu_set_t), &cpuset);
    pthread_attr_setaffinity_np(&a3, sizeof(cpu_set_t), &cpuset);

    // Crear primero telemetría para que pueda tomar el mutex
    if (pthread_create(&h3, &a3, task_telemetria, NULL) != 0) {
        perror("pthread_create h3");
        return 1;
    }

    // Le damos una pequeña ventaja para que tome el recurso
    usleep(100000);

    if (pthread_create(&h1, &a1, task_estabilidad, NULL) != 0) {
        perror("pthread_create h1");
        return 1;
    }

    if (pthread_create(&h2, &a2, task_navegacion, NULL) != 0) {
        perror("pthread_create h2");
        return 1;
    }

    pthread_attr_destroy(&a1);
    pthread_attr_destroy(&a2);
    pthread_attr_destroy(&a3);

    printf("Caso C: Inversión de prioridad con mutex\n");
    printf("Ejecutando durante 10 segundos...\n");
    fflush(stdout);

    sleep(10);

    activo = 0;

    pthread_join(h1, NULL);
    pthread_join(h2, NULL);
    pthread_join(h3, NULL);

    pthread_mutex_destroy(&recurso);

    printf("\n--- TABLA COMPARATIVA DE MÉTRICAS ---\n");
    printf("| Tarea          | Prioridad | Iteraciones |\n");
    printf("|----------------|-----------|-------------|\n");
    printf("| Estabilidad    | 80 (Alta) | %ld |\n", cont_est);
    printf("| Navegación     | 40 (Med)  | %ld |\n", cont_nav);
    printf("| Telemetría     | 10 (Baja) | %ld |\n", cont_tel);

    return 0;
}