#define RINGBUF_IMPLEMENTATION
#define RINGBUF_MPMC
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
    while (!atomic_load_explicit(&done, memory_order_acquire))
    {
        uint8_t data[64];
        for (int i = 0; i < (int)sizeof(data); i++)
        {
            data[i] = (uint8_t)((counter + i) & 0xFF);
        }
        ringbuf_err_t err = ringbuf_mpmc_write(&rb, data, sizeof(data));
        if (!(err == RbSuccess || err == RbNotEnoughSpace))
        {
            fprintf(stderr, "[error] ringbuf_mpmc_write() returned '%s'\n", ringbuf_strerr(err));
            return NULL;
        }
        counter++;
    }
    atomic_store_explicit(&writers_done, 1, memory_order_release);
    return NULL;
}

static void *reader_thread(void *arg)
{
    (void)arg;
    while (!atomic_load_explicit(&writers_done, memory_order_acquire))
    {
        uint8_t out[128];
        size_t cap = sizeof(out);
        ringbuf_read(&rb, out, &cap);
    }
    uint8_t out[128];
    while (atomic_load_explicit(&writers_done, memory_order_acquire))
    {
        size_t cap = sizeof(out);
        ringbuf_err_t err = ringbuf_mpmc_read(&rb, out, &cap);
        if (err == RbEmpty)
        {
            break;
        }
        else if (!(err == RbSuccess))
        {
            fprintf(stderr, "[error] ringbuf_mpmc_read() returned '%s'\n", ringbuf_strerr(err));
            return NULL;
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

#define THREAD_COUNT 6
    pthread_t writers[THREAD_COUNT], readers[THREAD_COUNT];
    for (size_t i = 0; i < THREAD_COUNT; i++)
        pthread_create(&readers[i], NULL, reader_thread, NULL);
    for (size_t i = 0; i < THREAD_COUNT; i++)
        pthread_create(&writers[i], NULL, writer_thread, NULL);

    pause();

    return 0;

    for (size_t i = 0; i < THREAD_COUNT; i++)
        pthread_join(writers[i], NULL);
    for (size_t i = 0; i < THREAD_COUNT; i++)
        pthread_join(readers[i], NULL);

    printf("done\n");
    return 0;
}
