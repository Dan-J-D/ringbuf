#define MPMC_RINGBUF_IMPLEMENTATION
#include "../mpmc_ringbuf.h"

#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static struct mpmc_ringbuf rb;
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
        mpmc_ringbuf_err_t err = mpmc_ringbuf_write(&rb, data, sizeof(data));
        if (!(err == RbSuccess || err == RbNotEnoughSpace))
        {
            fprintf(stderr, "[error] mpmc_ringbuf_write() returned '%s'\n", mpmc_ringbuf_stderr(err));
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
        mpmc_ringbuf_read(&rb, out, &cap);
    }
    uint8_t out[128];
    while (atomic_load_explicit(&writers_done, memory_order_acquire))
    {
        size_t cap = sizeof(out);
        mpmc_ringbuf_err_t err = mpmc_ringbuf_read(&rb, out, &cap);
        if (err == RbEmpty)
        {
            break;
        }
        else if (!(err == RbSuccess))
        {
            fprintf(stderr, "[error] mpmc_ringbuf_read() returned '%s'\n", mpmc_ringbuf_stderr(err));
            return NULL;
        }
    }
    return NULL;
}

int main(void)
{
    memset(buffer, 0, sizeof(buffer));
    memset(&rb, 0, sizeof(rb));
    mpmc_ringbuf_init(&rb, buffer, sizeof(buffer));

    atomic_init(&done, 0);
    atomic_init(&writers_done, 0);

    pthread_t writers[2], readers[2];
    pthread_create(&readers[0], NULL, reader_thread, NULL);
    pthread_create(&readers[1], NULL, reader_thread, NULL);
    pthread_create(&writers[0], NULL, writer_thread, NULL);
    pthread_create(&writers[1], NULL, writer_thread, NULL);

    pause();

    return 0;

    pthread_join(writers[0], NULL);
    pthread_join(writers[1], NULL);
    pthread_join(readers[0], NULL);
    pthread_join(readers[1], NULL);

    printf("done\n");
    return 0;
}
