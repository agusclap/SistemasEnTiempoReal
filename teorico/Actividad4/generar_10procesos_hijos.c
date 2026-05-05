#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>

#define CANT_PROCESOS 10

int main() {
    pid_t pid;

    for (int i = 0; i < CANT_PROCESOS; i++) {
        pid = fork();

        if (pid == 0) {
            printf("Proceso hijo %d - PID: %d - PPID: %d\n",
                   i + 1, getpid(), getppid());
            return 0;
        }
    }

    for (int i = 0; i < CANT_PROCESOS; i++) {
        wait(NULL);
    }

    printf("Proceso padre finalizado - PID: %d\n", getpid());

    return 0;
}