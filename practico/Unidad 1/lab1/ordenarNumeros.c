#include <stdio.h>

void ordenarAscendente(int arr[], int size);
void ordenarDescendente(int arr[], int size);

int main (void) {
    int size = 0, op = 0, i = 0;
    
    printf("Ingrese la cantidad de numeros a ordenar \n");
    scanf("%d", &size);

    int array[size];

    for(i = 0; i<size; i++){
        printf("Ingrese el numero (%d) \n", i);
        scanf("%d",&array[i]);
    }

    printf("Ingrese la opcion (1 o 2) para ordenar de manera ascendente o descendente\n");
    printf("1. Orden ascendente\n");
    printf("2. Orden descendente\n");
    scanf("%d", &op);
    switch (op){
        case 1:
            ordenarAscendente(array, size);
            break;
        case 2:
            ordenarDescendente(array, size);
            break;
        default:
            printf("Ninguna de las opciones ingresada es correcta\n");
    }
    printf("\nFin del programa.\n");
}


void ordenarAscendente(int arr[], int size){
    int i, j, temp, k;
    for (i=0; i < size-1; i++) {
        for(j = 0; j < size - i - 1; j++) {

            if (arr[j] > arr[j+1]) {
                temp = arr[j];
                arr[j] = arr[j+1];
                arr[j+1] = temp;
            }
        }
    }

    printf("El arreglo ordenado:\n");
    for (k = 0; k<size; k++){
        printf("%d\t", arr[k]);
    }
}

void ordenarDescendente(int arr[], int size){
    int i, j, temp, k;
    for (i = 0; i<size-1;i++){
        for(j = 0; j<size-i-1; j++){
            if(arr[j] < arr[j+1]) {
                temp = arr[j];
                arr[j] = arr[j + 1];
                arr[j+1] = temp;
            }
        }
    }

    printf("El arreglo ordenado:\n");
    for (k = 0; k<size; k++){
        printf("%d\t", arr[k]);
    }
}