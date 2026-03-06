#include <stdio.h>
#include <unistd.h>

int main() {

    int segundos = 0;

    while(1) {
        printf("Tiempo transcurrido: %d segundos\n", segundos);
        sleep(1);
        segundos++;
    }

    return 0;
}