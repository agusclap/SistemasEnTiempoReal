#include <stdio.h>
#include <unistd.h>

void esperar(int segundos);
void luzVerde();
void luzAmarilla();
void luzRoja();

int main() {

    while(1) {
        luzVerde();
        luzAmarilla();
        luzRoja();
    }

    return 0;
}

void esperar(int segundos) {
    for(int i = segundos; i > 0; i--) {
        printf("Tiempo restante: %d\n", i);
        sleep(1);
    }
}

void luzVerde() {
    printf("\nSEMAFORO VERDE - Los autos pueden avanzar\n");
    esperar(5);
}

void luzAmarilla() {
    printf("\nSEMAFORO AMARILLO - Precaucion\n");
    esperar(2);
}

void luzRoja() {
    printf("\nSEMAFORO ROJO - Los autos deben detenerse\n");
    esperar(5);
}