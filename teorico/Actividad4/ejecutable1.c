#include <stdio.h>
#include <unistd.h>

int main() {
    printf("Soy el ejecutable 1\n");
    printf("PID: %d\n", getpid());

    return 0;
}