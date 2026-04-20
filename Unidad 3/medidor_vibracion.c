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
#include <errno.h>
#include <pigpio.h>

#define MQ_NAME              "/cola_mpu6050"
#define SAMPLE_PERIOD_US     10000     // 10 ms = 100 Hz
#define WINDOW_SIZE          10

#define MPU6050_ADDR         0x68
#define REG_PWR_MGMT_1       0x6B
#define REG_ACCEL_XOUT_H     0x3B
#define REG_ACCEL_CONFIG     0x1C

#define SERVO_GPIO           18

typedef struct {
    float x;
    float y;
    float z;
} sample_t;

static volatile int running = 1;

static mqd_t mq;
static int i2c_handle = -1;

static pthread_mutex_t i2c_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t state_mutex = PTHREAD_MUTEX_INITIALIZER;

static float filtered_x_shared = 0.0f;

/* =========================================================
   Utilidades
   ========================================================= */

void handle_sigint(int sig) {
    (void)sig;
    running = 0;
}

void sleep_us(long us) {
    struct timespec ts;
    ts.tv_sec = us / 1000000;
    ts.tv_nsec = (us % 1000000) * 1000;
    nanosleep(&ts, NULL);
}

int16_t join_bytes(uint8_t high, uint8_t low) {
    return (int16_t)((high << 8) | low);
}

float raw_to_g(int16_t raw) {
    // Para rango ±2g => 16384 LSB/g
    return raw / 16384.0f;
}

int clamp_int(int value, int min, int max) {
    if (value < min) return min;
    if (value > max) return max;
    return value;
}

int accel_x_to_angle(float x_g) {
    // Mapea -1g..+1g a 0..180
    if (x_g < -1.0f) x_g = -1.0f;
    if (x_g >  1.0f) x_g =  1.0f;

    return (int)((x_g + 1.0f) * 90.0f);
}

int angle_to_pulsewidth(int angle) {
    angle = clamp_int(angle, 0, 180);
    // SG90 aprox: 500us a 2500us
    return 500 + (angle * 2000) / 180;
}

/* =========================================================
   MPU6050
   ========================================================= */

int mpu6050_init(void) {
    pthread_mutex_lock(&i2c_mutex);

    i2c_handle = i2cOpen(1, MPU6050_ADDR, 0);
    if (i2c_handle < 0) {
        pthread_mutex_unlock(&i2c_mutex);
        fprintf(stderr, "Error: no se pudo abrir el MPU6050 por I2C\n");
        return -1;
    }

    // Despertar sensor
    if (i2cWriteByteData(i2c_handle, REG_PWR_MGMT_1, 0x00) < 0) {
        pthread_mutex_unlock(&i2c_mutex);
        fprintf(stderr, "Error: no se pudo salir del modo sleep del MPU6050\n");
        return -1;
    }

    // Acelerómetro en ±2g (00 en bits AFS_SEL)
    if (i2cWriteByteData(i2c_handle, REG_ACCEL_CONFIG, 0x00) < 0) {
        pthread_mutex_unlock(&i2c_mutex);
        fprintf(stderr, "Error: no se pudo configurar el rango del acelerómetro\n");
        return -1;
    }

    pthread_mutex_unlock(&i2c_mutex);

    sleep_us(100000); // 100 ms de estabilización
    fprintf(stderr, "MPU6050 inicializado correctamente\n");
    return 0;
}

int mpu6050_read_accel(sample_t *out) {
    uint8_t data[6];

    pthread_mutex_lock(&i2c_mutex);
    int n = i2cReadI2CBlockData(i2c_handle, REG_ACCEL_XOUT_H, (char *)data, 6);
    pthread_mutex_unlock(&i2c_mutex);

    if (n != 6) {
        return -1;
    }

    int16_t ax_raw = join_bytes(data[0], data[1]);
    int16_t ay_raw = join_bytes(data[2], data[3]);
    int16_t az_raw = join_bytes(data[4], data[5]);

    out->x = raw_to_g(ax_raw);
    out->y = raw_to_g(ay_raw);
    out->z = raw_to_g(az_raw);

    return 0;
}

/* =========================================================
   Hilo productor
   ========================================================= */

void *producer_thread(void *arg) {
    (void)arg;

    while (running) {
        sample_t s;

        if (mpu6050_read_accel(&s) == 0) {
            if (mq_send(mq, (const char *)&s, sizeof(sample_t), 0) < 0) {
                fprintf(stderr, "Error: mq_send fallo\n");
            }
        } else {
            fprintf(stderr, "Error: lectura del MPU6050\n");
        }

        sleep_us(SAMPLE_PERIOD_US);
    }

    return NULL;
}

/* =========================================================
   Hilo consumidor
   ========================================================= */

void *consumer_thread(void *arg) {
    (void)arg;

    sample_t window[WINDOW_SIZE];
    memset(window, 0, sizeof(window));

    int index = 0;
    int count = 0;

    while (running) {
        sample_t s;
        ssize_t bytes = mq_receive(mq, (char *)&s, sizeof(sample_t), NULL);

        if (bytes < 0) {
            if (running) {
                fprintf(stderr, "Error: mq_receive fallo\n");
            }
            continue;
        }

        window[index] = s;
        index = (index + 1) % WINDOW_SIZE;

        if (count < WINDOW_SIZE) {
            count++;
        }

        float sum_x = 0.0f;
        float sum_y = 0.0f;
        float sum_z = 0.0f;

        for (int i = 0; i < count; i++) {
            sum_x += window[i].x;
            sum_y += window[i].y;
            sum_z += window[i].z;
        }

        float fx = sum_x / count;
        float fy = sum_y / count;
        float fz = sum_z / count;

        pthread_mutex_lock(&state_mutex);
        filtered_x_shared = fx;
        pthread_mutex_unlock(&state_mutex);

        // SOLO CSV por stdout
        printf("%.4f,%.4f,%.4f\n", fx, fy, fz);
        fflush(stdout);
    }

    return NULL;
}

/* =========================================================
   Hilo opcional servo
   ========================================================= */

void *servo_thread(void *arg) {
    (void)arg;

    while (running) {
        float x;

        pthread_mutex_lock(&state_mutex);
        x = filtered_x_shared;
        pthread_mutex_unlock(&state_mutex);

        int angle = accel_x_to_angle(x);
        int pulse = angle_to_pulsewidth(angle);

        gpioServo(SERVO_GPIO, pulse);

        sleep_us(20000); // 20 ms
    }

    gpioServo(SERVO_GPIO, 0);
    return NULL;
}

/* =========================================================
   Main
   ========================================================= */

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
        if (i2c_handle >= 0) {
            i2cClose(i2c_handle);
        }
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
        fprintf(stderr, "Error: mq_open fallo\n");
        i2cClose(i2c_handle);
        gpioTerminate();
        return 1;
    }

    pthread_t producer;
    pthread_t consumer;
    pthread_t servo;

    if (pthread_create(&producer, NULL, producer_thread, NULL) != 0) {
        fprintf(stderr, "Error: no se pudo crear el hilo productor\n");
        mq_close(mq);
        mq_unlink(MQ_NAME);
        i2cClose(i2c_handle);
        gpioTerminate();
        return 1;
    }

    if (pthread_create(&consumer, NULL, consumer_thread, NULL) != 0) {
        fprintf(stderr, "Error: no se pudo crear el hilo consumidor\n");
        running = 0;
        pthread_join(producer, NULL);
        mq_close(mq);
        mq_unlink(MQ_NAME);
        i2cClose(i2c_handle);
        gpioTerminate();
        return 1;
    }

    if (use_servo) {
        if (gpioSetMode(SERVO_GPIO, PI_OUTPUT) != 0) {
            fprintf(stderr, "Advertencia: no se pudo configurar el pin del servo\n");
        }

        if (pthread_create(&servo, NULL, servo_thread, NULL) != 0) {
            fprintf(stderr, "Advertencia: no se pudo crear el hilo del servo\n");
            use_servo = 0;
        }
    }

    pthread_join(producer, NULL);
    pthread_join(consumer, NULL);

    if (use_servo) {
        pthread_join(servo, NULL);
    }

    mq_close(mq);
    mq_unlink(MQ_NAME);

    if (i2c_handle >= 0) {
        i2cClose(i2c_handle);
    }

    gpioTerminate();
    fprintf(stderr, "Programa finalizado\n");
    return 0;
}