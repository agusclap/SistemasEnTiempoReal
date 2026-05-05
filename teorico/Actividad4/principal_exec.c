#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>

int main() {
    pid_t pid = fork();

    if (pid == 0) {
        execl("./programa_cargado", "programa_cargado", NULL);

        printf("Error al ejecutar el programa\n");
        return 1;
    } else {
        wait(NULL);
        printf("Proceso padre finalizado\n");
    }

    return 0;
}