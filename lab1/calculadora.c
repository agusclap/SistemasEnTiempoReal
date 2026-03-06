#include <stdio.h>

int main (void){

    double a = 0,b = 0;
    int op = 0;
    printf("Ingrese el primer numero: ");
    scanf("%lf",&a);
    printf("Ingrese el segundo numero: ");
    scanf("%lf",&b);

    printf("Ingrese la operacion a realizar: \n1. Suma\n2. Resta\n3. Multiplicacion\n4. Division\n");
    scanf("%d",&op);
    switch (op) {
        case 1:
            printf("El resultado de la suma es: %f", a + b);
            break;
        case 2:
            printf("El resultado de la resta es: %f", a - b);
            break;
        case 3:
            printf("El resultado de la multiplicacion es: %f", a * b);
            break;
        case 4:
            if (b != 0) {
                printf("El resultado de la division es: %f", a / b);
            } else {
                printf("Error: Division por cero no permitida.");
            }
            break;
        default:
            printf("Operacion no valida.");
    }
    printf("\n");
    return 0;
}