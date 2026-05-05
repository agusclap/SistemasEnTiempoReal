#include <stdio.h>
#include <stdint.h>

int main() {
    short valor_short;
    float valor_float;

    printf("Ingrese un valor short: ");
    scanf("%hd", &valor_short);

    valor_float = (float) valor_short;

    printf("Valor ingresado en short: %d\n", valor_short);
    printf("Valor convertido a float: %f\n", valor_float);

    return 0;
}