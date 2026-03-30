#define RINGBUF_IMPLEMENTATION
#include "../ringbuf.h"

#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static struct ringbuf rb;
static uint8_t buffer[65536];
static _Atomic int done;
static _Atomic int writers_done;

static void *writer_thread(void *arg)
{
    (void)arg;
    uint64_t counter = 0;
    while (!atomic_load_explicit(&done, memory_order_acquire)) {
        uint8_t data[64];
        for (int i = 0; i < (int)sizeof(data); i++) {
            data[i] = (uint8_t)((counter + i) & 0xFF);
        }
        ringbuf_write(&rb, data, sizeof(data));
        counter++;
    }
    atomic_store_explicit(&writers_done, 1, memory_order_release);
    return NULL;
}

static void *reader_thread(void *arg)
{
    (void)arg;
    while (!atomic_load_explicit(&writers_done, memory_order_acquire)) {
        uint8_t out[128];
        size_t cap = sizeof(out);
        ringbuf_read(&rb, out, &cap);
    }
    uint8_t out[128];
    while (atomic_load_explicit(&writers_done, memory_order_acquire)) {
        size_t cap = sizeof(out);
        ringbuf_err_t err = ringbuf_read(&rb, out, &cap);
        if (err == RbEmpty) {
            break;
        }
    }
    return NULL;
}

int main(void)
{
    memset(buffer, 0, sizeof(buffer));
    memset(&rb, 0, sizeof(rb));
    ringbuf_init(&rb, buffer, sizeof(buffer));

    atomic_init(&done, 0);
    atomic_init(&writers_done, 0);

    pthread_t writer, reader;
    pthread_create(&reader, NULL, reader_thread, NULL);
    pthread_create(&writer, NULL, writer_thread, NULL);

    pause();

    return 0;

    pthread_join(writer, NULL);
    pthread_join(reader, NULL);

    printf("done\n");
    return 0;
}
