#define _POSIX_C_SOURCE 199309L  // Para CLOCK_MONOTONIC

#include <stdio.h>
#include <pigpio.h>
#include <time.h>

#define LED_GPIO 17
#define PERIOD_MS 500

int main()
{
    if (gpioInitialise() < 0) {
        printf("Error inicializando pigpio\n");
        return 1;
    }

    gpioSetMode(LED_GPIO, PI_OUTPUT);

    struct timespec last_time, current_time;
    long elapsed_ms;

    int led_state = 0;

    clock_gettime(CLOCK_MONOTONIC, &last_time);

    while (1)
    {
        clock_gettime(CLOCK_MONOTONIC, &current_time);

        elapsed_ms = (current_time.tv_sec - last_time.tv_sec) * 1000;
        elapsed_ms += (current_time.tv_nsec - last_time.tv_nsec) / 1000000;

        if (elapsed_ms >= PERIOD_MS)
        {
            led_state = !led_state;
            gpioWrite(LED_GPIO, led_state);

            if (led_state)
                printf("LED_ON\n");
            else
                printf("LED_OFF\n");

            last_time = current_time;
        }
    }

    gpioTerminate();
    return 0;
}