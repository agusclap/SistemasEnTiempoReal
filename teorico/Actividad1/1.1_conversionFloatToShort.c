#include <stdio.h>
#include <stdint.h>

int main() {
    float valor_float;
    short valor_short;

    printf("Ingrese un valor float: ");
    scanf("%f", &valor_float);

    valor_short = (short) valor_float;

    printf("Valor ingresado en float: %f\n", valor_float);
    printf("Valor convertido a short: %d\n", valor_short);

    return 0;
}