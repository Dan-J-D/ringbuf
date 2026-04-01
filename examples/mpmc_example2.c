#define MPMC_RINGBUF_IMPLEMENTATION

// Can comment this out to remove statistics data and make it faster
#define MPMC_RINGBUF_STATISTICS

#include "mpmc_ringbuf.h"

#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <time.h>

void *writer(struct mpmc_ringbuf *rb)
{
    time_t start = time(NULL);
    uint8_t a = 0;
    mpmc_ringbuf_err_t err;
    while (difftime(time(NULL), start) < 3.f)
    {
        err = mpmc_ringbuf_write(rb, &a, sizeof(a));
        if (err == RbNotEnoughSpace)
            continue;

        if (err != RbSuccess)
        {
            fprintf(stderr, "[writer] Error - mpmc_ringbuf_write() returns code: %s\n", mpmc_ringbuf_strerr(err));
            return NULL;
        }

        a++;
    }
    return NULL;
}

void *reader(struct mpmc_ringbuf *rb)
{
    time_t start = time(NULL);
    uint8_t expected = 0, got = 0;
    size_t read = sizeof(got);
    mpmc_ringbuf_err_t err;
    while (difftime(time(NULL), start) < 3.5f)
    {
        err = mpmc_ringbuf_read(rb, &got, &read);
        if (err == RbEmpty)
            continue;

        if (err != RbSuccess)
        {
            fprintf(stderr, "[reader] Error - mpmc_ringbuf_read() returns code: %s\n", mpmc_ringbuf_strerr(err));
            return NULL;
        }

        if (expected != got)
        {
            fprintf(stderr, "[reader] Error - expected %d and got %d\n", expected, got);
            return NULL;
        }

        expected++;
    }

    return NULL;
}

int main(void)
{
    pthread_t wthread[2], rthread[1];
    int ret;

    enum
    {
        rbdata_size = 0x40000
    };
    void *rbdata = calloc(1, rbdata_size);
    struct mpmc_ringbuf *rb = (struct mpmc_ringbuf *)calloc(1, sizeof(struct mpmc_ringbuf));
    mpmc_ringbuf_err_t rbe = mpmc_ringbuf_init(rb, rbdata, rbdata_size);
    if (rbe != RbSuccess)
    {
        fprintf(stderr, "Error - mpmc_ringbuf_init() return code: %d\n", rbe);
        return 1;
    }

    ret = pthread_create(&wthread[0], NULL, (void *(*)(void *))writer, rb);
    if (ret)
    {
        fprintf(stderr, "Error - pthread_create() return code: %d\n", ret);
        return 1;
    }

    ret = pthread_create(&wthread[1], NULL, (void *(*)(void *))writer, rb);
    if (ret)
    {
        fprintf(stderr, "Error - pthread_create() return code: %d\n", ret);
        return 1;
    }

    ret = pthread_create(&rthread[0], NULL, (void *(*)(void *))reader, rb);
    if (ret)
    {
        fprintf(stderr, "Error - pthread_create() return code: %d\n", ret);
        return 1;
    }

    ret = pthread_create(&rthread[1], NULL, (void *(*)(void *))reader, rb);
    if (ret)
    {
        fprintf(stderr, "Error - pthread_create() return code: %d\n", ret);
        return 1;
    }

    ret = pthread_join(wthread[0], NULL);
    if (ret)
    {
        fprintf(stderr, "Error - pthread_join() return code: %d\n", ret);
        return 1;
    }
    ret = pthread_join(wthread[1], NULL);
    if (ret)
    {
        fprintf(stderr, "Error - pthread_join() return code: %d\n", ret);
        return 1;
    }

    ret = pthread_join(rthread[0], NULL);
    if (ret)
    {
        fprintf(stderr, "Error - pthread_join() return code: %d\n", ret);
        return 1;
    }
    ret = pthread_join(rthread[1], NULL);
    if (ret)
    {
        fprintf(stderr, "Error - pthread_join() return code: %d\n", ret);
        return 1;
    }

#ifdef RINGBUF_STATISTICS
    struct mpmc_ringbuf_stats rb_stats;
    double avg_read_ns = mpmc_ringbuf_avg_read_ns(rb), avg_write_ns = mpmc_ringbuf_avg_write_ns(rb);
    mpmc_ringbuf_get_stats(rb, &rb_stats);

    printf("Stats:\n");
    printf("\tbytes_written: %lu\n", rb_stats.bytes_written);
    printf("\tbytes_read: %lu\n", rb_stats.bytes_read);
    printf("\twrites: %lu\n", rb_stats.writes);
    printf("\treads: %lu\n", rb_stats.reads);
    printf("\ttotal_write_ns: %lu\n", rb_stats.total_write_ns);
    printf("\ttotal_read_ns: %lu\n", rb_stats.total_read_ns);
    printf("\t\tavg_write_ns: %lf\n", avg_write_ns);
    printf("\t\tavg_read_ns: %lf\n", avg_read_ns);
#endif

    free(rb);
    free(rbdata);

    printf("Success\n");
    return 0;
}
