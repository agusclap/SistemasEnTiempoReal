#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

#define SOCKET_PATH "/tmp/control_led.sock"
#define BUFFER_SIZE 100

int main(int argc, char *argv[]) {
    int cliente_fd;
    struct sockaddr_un direccion;
    char buffer[BUFFER_SIZE];

    if (argc != 2) {
        printf("Uso: %s ON | OFF | STATUS\n", argv[0]);
        return 1;
    }

    cliente_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (cliente_fd < 0) {
        perror("Error creando socket");
        return 1;
    }

    memset(&direccion, 0, sizeof(direccion));
    direccion.sun_family = AF_UNIX;
    strcpy(direccion.sun_path, SOCKET_PATH);

    if (connect(cliente_fd, (struct sockaddr *)&direccion, sizeof(direccion)) < 0) {
        perror("Error conectando al servidor");
        close(cliente_fd);
        return 1;
    }

    write(cliente_fd, argv[1], strlen(argv[1]));

    memset(buffer, 0, sizeof(buffer));
    read(cliente_fd, buffer, sizeof(buffer) - 1);

    printf("Respuesta del servidor: %s", buffer);

    close(cliente_fd);

    return 0;
}