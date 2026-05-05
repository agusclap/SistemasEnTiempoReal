#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <pthread.h>
#include <mqueue.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <pigpio.h>

#define MQ_NAME         "/cola_mpu6050"
#define SAMPLE_HZ       100
#define SAMPLE_PERIOD_US 10000   // 10 ms = 100 Hz

#define MPU6050_ADDR    0x68
#define REG_PWR_MGMT_1  0x6B
#define REG_ACCEL_XOUT_H 0x3B

#define WINDOW_SIZE     10       // media móvil simple
#define SERVO_GPIO      18       // opcional

typedef struct {
    float x;
    float y;
    float z;
} sample_t;

static volatile int running = 1;

static int i2c_handle = -1;
static mqd_t mq;

// mutex para I2C
static pthread_mutex_t i2c_mutex = PTHREAD_MUTEX_INITIALIZER;

// mutex para variable compartida con servo
static pthread_mutex_t state_mutex = PTHREAD_MUTEX_INITIALIZER;
static float filtered_x_shared = 0.0f;

// --------------------------------------------------
// Utilidades
// --------------------------------------------------

void handle_sigint(int sig) {
    (void)sig;
    running = 0;
}

void sleep_exact_us(long us) {
    struct timespec ts;
    ts.tv_sec = us / 1000000;
    ts.tv_nsec = (us % 1000000) * 1000;
    nanosleep(&ts, NULL);
}

int16_t combinar_bytes(uint8_t high, uint8_t low) {
    return (int16_t)((high << 8) | low);
}

// Convierte raw del acelerómetro a "g"
// asumiendo rango ±2g => 16384 LSB/g
float raw_to_g(int16_t raw) {
    return raw / 16384.0f;
}

// Mapea aceleración en X a ángulo aproximado de servo
// de -1g..+1g a 0..180
int accel_to_angle(float x_g) {
    if (x_g < -1.0f) x_g = -1.0f;
    if (x_g >  1.0f) x_g =  1.0f;

    float angle = (x_g + 1.0f) * 90.0f; // -1 -> 0, 0 -> 90, +1 -> 180
    return (int)angle;
}

// Convierte ángulo 0..180 a pulso SG90 aprox
int angle_to_pulse(int angle) {
    if (angle < 0) angle = 0;
    if (angle > 180) angle = 180;
    return 500 + (angle * 2000) / 180; // 500us a 2500us
}

// --------------------------------------------------
// MPU6050
// --------------------------------------------------

int mpu6050_init(void) {
    pthread_mutex_lock(&i2c_mutex);

    i2c_handle = i2cOpen(1, MPU6050_ADDR, 0);
    if (i2c_handle < 0) {
        pthread_mutex_unlock(&i2c_mutex);
        fprintf(stderr, "Error: no se pudo abrir MPU6050\n");
        return -1;
    }

    // sacar de sleep
    if (i2cWriteByteData(i2c_handle, REG_PWR_MGMT_1, 0x00) < 0) {
        pthread_mutex_unlock(&i2c_mutex);
        fprintf(stderr, "Error: no se pudo inicializar MPU6050\n");
        return -1;
    }

    pthread_mutex_unlock(&i2c_mutex);
    fprintf(stderr, "MPU6050 listo\n");
    return 0;
}

int mpu6050_read_accel(sample_t *s) {
    uint8_t data[6];

    pthread_mutex_lock(&i2c_mutex);

    int n = i2cReadI2CBlockData(i2c_handle, REG_ACCEL_XOUT_H, (char *)data, 6);

    pthread_mutex_unlock(&i2c_mutex);

    if (n != 6) {
        return -1;
    }

    int16_t ax_raw = combinar_bytes(data[0], data[1]);
    int16_t ay_raw = combinar_bytes(data[2], data[3]);
    int16_t az_raw = combinar_bytes(data[4], data[5]);

    s->x = raw_to_g(ax_raw);
    s->y = raw_to_g(ay_raw);
    s->z = raw_to_g(az_raw);

    return 0;
}

// --------------------------------------------------
// Hilo productor
// --------------------------------------------------

void *producer_thread(void *arg) {
    (void)arg;

    sample_t s;

    while (running) {
        if (mpu6050_read_accel(&s) == 0) {
            if (mq_send(mq, (const char *)&s, sizeof(sample_t), 0) < 0) {
                fprintf(stderr, "Error: mq_send\n");
            }
        } else {
            fprintf(stderr, "Error: lectura MPU6050\n");
        }

        sleep_exact_us(SAMPLE_PERIOD_US);
    }

    return NULL;
}

// --------------------------------------------------
// Hilo consumidor
// --------------------------------------------------

void *consumer_thread(void *arg) {
    (void)arg;

    sample_t buffer[WINDOW_SIZE];
    int count = 0;
    int index = 0;

    memset(buffer, 0, sizeof(buffer));

    while (running) {
        sample_t s;
        ssize_t bytes = mq_receive(mq, (char *)&s, sizeof(sample_t), NULL);

        if (bytes < 0) {
            if (running) fprintf(stderr, "Error: mq_receive\n");
            continue;
        }

        buffer[index] = s;
        index = (index + 1) % WINDOW_SIZE;

        if (count < WINDOW_SIZE) count++;

        float sum_x = 0.0f, sum_y = 0.0f, sum_z = 0.0f;
        for (int i = 0; i < count; i++) {
            sum_x += buffer[i].x;
            sum_y += buffer[i].y;
            sum_z += buffer[i].z;
        }

        float fx = sum_x / count;
        float fy = sum_y / count;
        float fz = sum_z / count;

        // compartir eje X filtrado con el servo
        pthread_mutex_lock(&state_mutex);
        filtered_x_shared = fx;
        pthread_mutex_unlock(&state_mutex);

        // MUY IMPORTANTE: solo CSV por stdout
        printf("%.4f,%.4f,%.4f\n", fx, fy, fz);
        fflush(stdout);
    }

    return NULL;
}

// --------------------------------------------------
// Hilo opcional de servo
// --------------------------------------------------

void *servo_thread(void *arg) {
    (void)arg;

    while (running) {
        float x;

        pthread_mutex_lock(&state_mutex);
        x = filtered_x_shared;
        pthread_mutex_unlock(&state_mutex);

        int angle = accel_to_angle(x);
        int pulse = angle_to_pulse(angle);

        gpioServo(SERVO_GPIO, pulse);

        sleep_exact_us(20000); // 20 ms
    }

    gpioServo(SERVO_GPIO, 0);
    return NULL;
}

// --------------------------------------------------
// Main
// --------------------------------------------------

int main(int argc, char *argv[]) {
    int use_servo = 0;

    if (argc > 1 && strcmp(argv[1], "--servo") == 0) {
        use_servo = 1;
    }

    signal(SIGINT, handle_sigint);

    if (gpioInitialise() < 0) {
        fprintf(stderr, "Error: gpioInitialise fallo\n");
        return 1;
    }

    if (mpu6050_init() != 0) {
        gpioTerminate();
        return 1;
    }

    struct mq_attr attr;
    attr.mq_flags = 0;
    attr.mq_maxmsg = 10;
    attr.mq_msgsize = sizeof(sample_t);
    attr.mq_curmsgs = 0;

    mq_unlink(MQ_NAME);
    mq = mq_open(MQ_NAME, O_CREAT | O_RDWR, 0644, &attr);
    if (mq == (mqd_t)-1) {
        fprintf(stderr, "Error: mq_open\n");
        if (i2c_handle >= 0) i2cClose(i2c_handle);
        gpioTerminate();
        return 1;
    }

    pthread_t prod, cons, servo;

    if (pthread_create(&prod, NULL, producer_thread, NULL) != 0) {
        fprintf(stderr, "Error: pthread_create productor\n");
        mq_close(mq);
        mq_unlink(MQ_NAME);
        if (i2c_handle >= 0) i2cClose(i2c_handle);
        gpioTerminate();
        return 1;
    }

    if (pthread_create(&cons, NULL, consumer_thread, NULL) != 0) {
        fprintf(stderr, "Error: pthread_create consumidor\n");
        running = 0;
        pthread_join(prod, NULL);
        mq_close(mq);
        mq_unlink(MQ_NAME);
        if (i2c_handle >= 0) i2cClose(i2c_handle);
        gpioTerminate();
        return 1;
    }

    if (use_servo) {
        if (pthread_create(&servo, NULL, servo_thread, NULL) != 0) {
            fprintf(stderr, "Error: pthread_create servo\n");
            running = 0;
        }
    }

    pthread_join(prod, NULL);
    pthread_join(cons, NULL);
    if (use_servo) pthread_join(servo, NULL);

    mq_close(mq);
    mq_unlink(MQ_NAME);

    if (i2c_handle >= 0) i2cClose(i2c_handle);

    gpioTerminate();
    fprintf(stderr, "Programa finalizado\n");
    return 0;
}