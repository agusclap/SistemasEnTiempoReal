#include <stdio.h>
#include <pthread.h>
#include <sched.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>

// Variables globales
long cont_est = 0, cont_nav = 0, cont_tel = 0;
int activo = 1;

// 1. Tarea de Control de Estabilidad (Crítica)
void* task_estabilidad(void* a) {
    while(activo) cont_est++;
    return NULL;
}

// 2. Tarea de Navegación (Media)
void* task_navegacion(void* a) {
    while(activo) cont_nav++;
    return NULL;
}

// 3. Tarea de Telemetría (Baja)
void* task_telemetria(void* a) {
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGALRM);

    int s;
    while(activo) {
        sigwait(&set, &s); // espera señal del timer
        cont_tel++;
        printf("[Telemetría] Enviando log...\n");
    }
    return NULL;
}

int main() {

    // Bloqueamos SIGALRM en el main
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGALRM);
    pthread_sigmask(SIG_BLOCK, &set, NULL);

    pthread_t h1, h2, h3;

    // ============================
    // CASO A: SIN PRIORIDADES RT
    // ============================
    pthread_create(&h1, NULL, task_estabilidad, NULL);
    pthread_create(&h2, NULL, task_navegacion, NULL);
    pthread_create(&h3, NULL, task_telemetria, NULL);

    // ============================
    // TIMER POSIX (500 ms)
    // ============================
    timer_t tid;

    struct sigevent sev;
    sev.sigev_notify = SIGEV_SIGNAL;
    sev.sigev_signo = SIGALRM;

    timer_create(CLOCK_REALTIME, &sev, &tid);

    struct itimerspec ts;
    ts.it_interval.tv_sec = 0;
    ts.it_interval.tv_nsec = 500000000; // 500 ms
    ts.it_value.tv_sec = 0;
    ts.it_value.tv_nsec = 500000000;

    timer_settime(tid, 0, &ts, NULL);

    printf("Caso A: SCHED_OTHER (normal)\n");
    printf("Ejecutando durante 10 segundos...\n");

    sleep(10);

    // detener ejecución
    activo = 0;

    // despertar telemetría por si está bloqueado
    pthread_kill(h3, SIGALRM);

    // esperar hilos
    pthread_join(h1, NULL);
    pthread_join(h2, NULL);
    pthread_join(h3, NULL);

    timer_delete(tid);

    // ============================
    // RESULTADOS
    // ============================
    printf("\n--- TABLA COMPARATIVA DE MÉTRICAS ---\n");
    printf("| Tarea          | Prioridad | Iteraciones |\n");
    printf("|----------------|-----------|-------------|\n");
    printf("| Estabilidad    | -         | %ld |\n", cont_est);
    printf("| Navegación     | -         | %ld |\n", cont_nav);
    printf("| Telemetría     | Timer     | %ld |\n", cont_tel);

    return 0;
}