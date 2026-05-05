// Controla fisicamente el led
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <pigpio.h>

#define SOCKET_PATH "/tmp/control_led.sock"
#define LED_GPIO 17
#define BUFFER_SIZE 100

typedef struct {
    int led_estado; // 0 = OFF, 1 = ON
    pthread_mutex_t mutex;
} EstadoSistema;

EstadoSistema estado;

void *atender_cliente(void *arg) {
    int cliente_fd = *(int *)arg;
    free(arg);

    char buffer[BUFFER_SIZE];
    memset(buffer, 0, sizeof(buffer));

    read(cliente_fd, buffer, sizeof(buffer) - 1);

    // Sacar salto de línea si existe
    buffer[strcspn(buffer, "\n")] = 0;

    char respuesta[BUFFER_SIZE];

    pthread_mutex_lock(&estado.mutex);

    if (strcmp(buffer, "ON") == 0) {
        gpioWrite(LED_GPIO, 1);
        estado.led_estado = 1;
        strcpy(respuesta, "LED_OK: ON\n");
    }
    else if (strcmp(buffer, "OFF") == 0) {
        gpioWrite(LED_GPIO, 0);
        estado.led_estado = 0;
        strcpy(respuesta, "LED_OK: OFF\n");
    }
    else if (strcmp(buffer, "STATUS") == 0) {
        if (estado.led_estado == 1) {
            strcpy(respuesta, "LED_STATUS: ON\n");
        } else {
            strcpy(respuesta, "LED_STATUS: OFF\n");
        }
    }
    else {
        strcpy(respuesta, "ERROR: Comando invalido\n");
    }

    pthread_mutex_unlock(&estado.mutex);

    write(cliente_fd, respuesta, strlen(respuesta));

    close(cliente_fd);
    return NULL;
}

int main() {
    int servidor_fd;
    struct sockaddr_un direccion;

    estado.led_estado = 0;
    pthread_mutex_init(&estado.mutex, NULL);

    if (gpioInitialise() < 0) {
        printf("Error al inicializar pigpio\n");
        return 1;
    }

    gpioSetMode(LED_GPIO, PI_OUTPUT);
    gpioWrite(LED_GPIO, 0);

    servidor_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (servidor_fd < 0) {
        perror("Error creando socket");
        gpioTerminate();
        return 1;
    }

    unlink(SOCKET_PATH);

    memset(&direccion, 0, sizeof(direccion));
    direccion.sun_family = AF_UNIX;
    strcpy(direccion.sun_path, SOCKET_PATH);

    if (bind(servidor_fd, (struct sockaddr *)&direccion, sizeof(direccion)) < 0) {
        perror("Error en bind");
        close(servidor_fd);
        gpioTerminate();
        return 1;
    }

    if (listen(servidor_fd, 5) < 0) {
        perror("Error en listen");
        close(servidor_fd);
        gpioTerminate();
        return 1;
    }

    printf("Servidor iniciado en %s\n", SOCKET_PATH);
    printf("Esperando clientes...\n");

    while (1) {
        int *cliente_fd = malloc(sizeof(int));
        *cliente_fd = accept(servidor_fd, NULL, NULL);

        if (*cliente_fd < 0) {
            perror("Error en accept");
            free(cliente_fd);
            continue;
        }

        pthread_t hilo;
        pthread_create(&hilo, NULL, atender_cliente, cliente_fd);
        pthread_detach(hilo);
    }

    close(servidor_fd);
    unlink(SOCKET_PATH);
    gpioTerminate();
    pthread_mutex_destroy(&estado.mutex);

    return 0;
}