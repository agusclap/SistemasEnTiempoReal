#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>

int main() {
    pid_t pid1, pid2;

    pid1 = fork();

    if (pid1 == 0) {
        execl("./ejecutable1", "ejecutable1", NULL);

        printf("Error al ejecutar ejecutable1\n");
        return 1;
    }

    pid2 = fork();

    if (pid2 == 0) {
        execl("./ejecutable2", "ejecutable2", NULL);

        printf("Error al ejecutar ejecutable2\n");
        return 1;
    }

    wait(NULL);
    wait(NULL);

    printf("Proceso padre finalizado\n");

    return 0;
}