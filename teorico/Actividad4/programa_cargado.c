#include <stdio.h>
#include <unistd.h>

int main() {
    printf("Ejecutable cargado desde el directorio actual\n");
    printf("PID del ejecutable: %d\n", getpid());

    return 0;
}