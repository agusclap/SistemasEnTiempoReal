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

// 1. Tarea de Control de Estabilidad (Crítica - Prioridad 80)
void* task_estabilidad(void* a) {
    while (activo) {
        cont_est++;
    }
    return NULL;
}

// 2. Tarea de Navegación (Media - Prioridad 40)
void* task_navegacion(void* a) {
    while (activo) {
        cont_nav++;
    }
    return NULL;
}

// 3. Tarea de Telemetría (Baja - Prioridad 10)
void* task_telemetria(void* a) {
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGALRM);

    int s;
    while (activo) {
        sigwait(&set, &s);
        if (!activo) break;
        cont_tel++;
        printf("[Telemetría] Enviando log...\n");
        fflush(stdout);
    }
    return NULL;
}

// Configura FIFO + prioridad
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
    // Bloquear SIGALRM en main para que lo reciba el hilo de telemetría vía sigwait
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGALRM);
    pthread_sigmask(SIG_BLOCK, &set, NULL);

    pthread_t h1, h2, h3;
    pthread_attr_t a1, a2, a3;

    // Prioridades RT
    set_prio(&a1, 80);
    set_prio(&a2, 40);
    set_prio(&a3, 10);

    // Afinidad: todos al CPU 0
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(0, &cpuset);

    pthread_attr_setaffinity_np(&a1, sizeof(cpu_set_t), &cpuset);
    pthread_attr_setaffinity_np(&a2, sizeof(cpu_set_t), &cpuset);
    pthread_attr_setaffinity_np(&a3, sizeof(cpu_set_t), &cpuset);

    // Timer POSIX cada 500 ms
    timer_t tid;
    struct sigevent sev;
    memset(&sev, 0, sizeof(sev));
    sev.sigev_notify = SIGEV_SIGNAL;
    sev.sigev_signo = SIGALRM;

    if (timer_create(CLOCK_REALTIME, &sev, &tid) == -1) {
        perror("timer_create");
        return 1;
    }

    struct itimerspec ts;
    memset(&ts, 0, sizeof(ts));
    ts.it_interval.tv_sec = 0;
    ts.it_interval.tv_nsec = 500000000; // 500 ms
    ts.it_value.tv_sec = 0;
    ts.it_value.tv_nsec = 500000000;

    if (timer_settime(tid, 0, &ts, NULL) == -1) {
        perror("timer_settime");
        return 1;
    }

    // Crear hilos
    if (pthread_create(&h1, &a1, task_estabilidad, NULL) != 0) {
        perror("pthread_create h1");
        return 1;
    }

    if (pthread_create(&h2, &a2, task_navegacion, NULL) != 0) {
        perror("pthread_create h2");
        return 1;
    }

    if (pthread_create(&h3, &a3, task_telemetria, NULL) != 0) {
        perror("pthread_create h3");
        return 1;
    }

    pthread_attr_destroy(&a1);
    pthread_attr_destroy(&a2);
    pthread_attr_destroy(&a3);

    printf("Caso B: SCHED_FIFO (tiempo real) + afinidad a CPU 0\n");
    printf("Ejecutando durante 10 segundos...\n");
    fflush(stdout);

    sleep(10);

    activo = 0;
    pthread_kill(h3, SIGALRM);

    pthread_join(h1, NULL);
    pthread_join(h2, NULL);
    pthread_join(h3, NULL);

    timer_delete(tid);

    printf("\n--- TABLA COMPARATIVA DE MÉTRICAS ---\n");
    printf("| Tarea          | Prioridad | Iteraciones |\n");
    printf("|----------------|-----------|-------------|\n");
    printf("| Estabilidad    | 80 (Alta) | %ld |\n", cont_est);
    printf("| Navegación     | 40 (Med)  | %ld |\n", cont_nav);
    printf("| Telemetría     | 10 (Baja) | %ld |\n", cont_tel);

    return 0;
}