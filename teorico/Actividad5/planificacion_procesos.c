#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <sched.h>

#define CANT_PROCESOS 3

const char* nombre_politica(int politica) {
    switch (politica) {
        case SCHED_OTHER: return "SCHED_OTHER";
        case SCHED_FIFO:  return "SCHED_FIFO";
        case SCHED_RR:    return "SCHED_RR";
        default:          return "DESCONOCIDA";
    }
}

void imprimir_info() {
    int politica;
    struct sched_param param;
    int nice_value;

    politica = sched_getscheduler(0);
    sched_getparam(0, &param);
    nice_value = getpriority(PRIO_PROCESS, 0);

    printf("PID: %d | Politica: %s | Prioridad: %d | Nice: %d\n",
           getpid(),
           nombre_politica(politica),
           param.sched_priority,
           nice_value);
}

int main() {
    pid_t hijos[CANT_PROCESOS];

    for (int i = 0; i < CANT_PROCESOS; i++) {
        hijos[i] = fork();

        if (hijos[i] == 0) {
            while (1) {
                imprimir_info();
                sleep(1);
            }
        }
    }

    sleep(10);

    printf("\nEl proceso padre cambia las prioridades nice de los hijos\n\n");

    setpriority(PRIO_PROCESS, hijos[0], 5);
    setpriority(PRIO_PROCESS, hijos[1], 10);
    setpriority(PRIO_PROCESS, hijos[2], 15);

    sleep(10);

    for (int i = 0; i < CANT_PROCESOS; i++) {
        kill(hijos[i], SIGTERM);
    }

    for (int i = 0; i < CANT_PROCESOS; i++) {
        wait(NULL);
    }

    printf("Proceso padre finalizado\n");

    return 0;
}