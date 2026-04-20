#define _POSIX_C_SOURCE 199309L

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <time.h>
#include <pthread.h>
#include <unistd.h>
#include <pigpio.h>
#include <sched.h>
#include <string.h>

#define SERVO_PIN 24
#define BUTTON_PIN 17
#define LED_PIN 18

#define MIN_PULSE 500
#define MAX_PULSE 2500

#define PASO_ANGULO 2
#define PERIODO_SERVO_MS 20
#define DEBOUNCE_MS 150

volatile int posicion_servo = 0;
volatile int direccion = 1;          // 1 sube, -1 baja
volatile int alerta_activada = 0;    // 0 seguro, 1 alerta
volatile int sistema_activo = 1;

pthread_mutex_t mutex_estado = PTHREAD_MUTEX_INITIALIZER;
timer_t timerid;

/* Convierte ángulo [0..180] a pulso para servo */
int angulo_a_pulso(int angulo)
{
    if (angulo < 0) angulo = 0;
    if (angulo > 180) angulo = 180;

    return MIN_PULSE + (angulo * (MAX_PULSE - MIN_PULSE) / 180);
}

/* Callback del timer: se ejecuta cada 1 segundo */
void telemetria_callback(union sigval sv)
{
    int pos;
    int alerta;

    pthread_mutex_lock(&mutex_estado);
    pos = posicion_servo;
    alerta = alerta_activada;
    pthread_mutex_unlock(&mutex_estado);

    printf("[TELEMETRIA] Posición actual del servo: %d° | Estado: %s\n",
           pos,
           alerta ? "ALERTA" : "SEGURO");
    fflush(stdout);
}

/* Hilo de monitoreo de seguridad */
void* hilo_seguridad(void* arg)
{
    int ultimo_estado = 1; // con pull-up: 1 = no presionado, 0 = presionado
    uint32_t ultimo_tiempo = 0;

    while (sistema_activo)
    {
        int estado_actual = gpioRead(BUTTON_PIN);
        uint32_t ahora = gpioTick(); // microsegundos desde arranque

        /* Detección de flanco de presión */
        if (estado_actual == 0 && ultimo_estado == 1)
        {
            if ((ahora - ultimo_tiempo) > (DEBOUNCE_MS * 1000))
            {
                pthread_mutex_lock(&mutex_estado);
                alerta_activada = 1;
                pthread_mutex_unlock(&mutex_estado);

                gpioWrite(LED_PIN, 1);

                printf("[ALERTA] Parada de emergencia activada - Latencia detectada.\n");
                fflush(stdout);

                ultimo_tiempo = ahora;
            }
        }

        /* Si se suelta el botón, vuelve a modo seguro */
        if (estado_actual == 1 && ultimo_estado == 0)
        {
            pthread_mutex_lock(&mutex_estado);
            alerta_activada = 0;
            pthread_mutex_unlock(&mutex_estado);

            gpioWrite(LED_PIN, 0);

            printf("[INFO] Parada de emergencia liberada - Sistema en modo seguro.\n");
            fflush(stdout);

            ultimo_tiempo = ahora;
        }

        ultimo_estado = estado_actual;

        /* Polling rápido para minimizar latencia */
        gpioDelay(1000); // 1 ms
    }

    return NULL;
}

/* Configura el timer periódico de 1 segundo */
int configurar_timer()
{
    struct sigevent sev;
    struct itimerspec its;

    memset(&sev, 0, sizeof(sev));
    memset(&its, 0, sizeof(its));

    sev.sigev_notify = SIGEV_THREAD;
    sev.sigev_notify_function = telemetria_callback;
    sev.sigev_value.sival_ptr = &timerid;

    if (timer_create(CLOCK_MONOTONIC, &sev, &timerid) == -1)
    {
        perror("timer_create");
        return -1;
    }

    its.it_value.tv_sec = 1;
    its.it_value.tv_nsec = 0;
    its.it_interval.tv_sec = 1;
    its.it_interval.tv_nsec = 0;

    if (timer_settime(timerid, 0, &its, NULL) == -1)
    {
        perror("timer_settime");
        return -1;
    }

    return 0;
}

int main()
{
    pthread_t th_seguridad;
    pthread_attr_t attr;
    struct sched_param param;
    struct timespec pausa;

    pausa.tv_sec = 0;
    pausa.tv_nsec = PERIODO_SERVO_MS * 1000000L;

    if (gpioInitialise() < 0)
    {
        fprintf(stderr, "Error al inicializar pigpio.\n");
        return 1;
    }

    gpioSetMode(SERVO_PIN, PI_OUTPUT);
    gpioSetMode(BUTTON_PIN, PI_INPUT);
    gpioSetMode(LED_PIN, PI_OUTPUT);

    gpioSetPullUpDown(BUTTON_PIN, PI_PUD_UP);
    gpioWrite(LED_PIN, 0);

    if (pthread_attr_init(&attr) != 0)
    {
        perror("pthread_attr_init");
        gpioTerminate();
        return 1;
    }

    /* Configurar prioridad de tiempo real para el hilo de seguridad */
    if (pthread_attr_setschedpolicy(&attr, SCHED_FIFO) != 0)
    {
        perror("pthread_attr_setschedpolicy");
    }

    param.sched_priority = 80;
    if (pthread_attr_setschedparam(&attr, &param) != 0)
    {
        perror("pthread_attr_setschedparam");
    }

    if (pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED) != 0)
    {
        perror("pthread_attr_setinheritsched");
    }

    if (pthread_create(&th_seguridad, &attr, hilo_seguridad, NULL) != 0)
    {
        perror("pthread_create");
        pthread_attr_destroy(&attr);
        gpioTerminate();
        return 1;
    }

    pthread_attr_destroy(&attr);

    if (configurar_timer() != 0)
    {
        sistema_activo = 0;
        pthread_join(th_seguridad, NULL);
        gpioTerminate();
        return 1;
    }

    printf("Sistema iniciado.\n");
    printf("Servo en movimiento continuo.\n");
    printf("Botón de emergencia activo en GPIO %d.\n", BUTTON_PIN);
    printf("Telemetría cada 1 segundo.\n");
    fflush(stdout);

    /* Ejecutivo cíclico principal */
    while (1)
    {
        int alerta_local;
        int pos_local;

        pthread_mutex_lock(&mutex_estado);
        alerta_local = alerta_activada;

        if (!alerta_local)
        {
            posicion_servo += direccion * PASO_ANGULO;

            if (posicion_servo >= 180)
            {
                posicion_servo = 180;
                direccion = -1;
            }
            else if (posicion_servo <= 0)
            {
                posicion_servo = 0;
                direccion = 1;
            }

            pos_local = posicion_servo;
        }
        else
        {
            pos_local = posicion_servo;
        }
        pthread_mutex_unlock(&mutex_estado);

        if (!alerta_local)
        {
            gpioServo(SERVO_PIN, angulo_a_pulso(pos_local));
        }
        else
        {
            /* Mantiene posición actual al detenerse */
            gpioServo(SERVO_PIN, angulo_a_pulso(pos_local));
        }

        /* No usar sleep(): se usa nanosleep para temporización controlada */
        clock_nanosleep(CLOCK_MONOTONIC, 0, &pausa, NULL);
    }

    /* Nunca llega acá, pero queda correcto */
    sistema_activo = 0;
    timer_delete(timerid);
    pthread_join(th_seguridad, NULL);
    gpioServo(SERVO_PIN, 0);
    gpioTerminate();

    return 0;
}