#include <stdio.h>

int main (void) {
    int i = 0;
    int contador = 0;
    int enPalabra = 0;
    char palabra[100];

    printf("Ingresa la palabra a contar:");
    fgets(palabra, 100, stdin);

    for(i = 0; palabra[i] != '\0'; i++){
        if(palabra[i] != ' ' && palabra[i] != '\n'){
            if(enPalabra == 0) {
                contador++;
                enPalabra = 1;
            }
        } else {
            enPalabra = 0;
        }
    }
    printf("Cantidad de palabras: %d \n" , contador);


    return 0;
}