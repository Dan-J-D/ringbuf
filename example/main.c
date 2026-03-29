#define RINGBUF_IMPLEMENTATION

// Can comment this out to remove statistics data and make it faster
#define RINGBUF_STATISTICS

#include "ringbuf.h"

#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <time.h>

void *writer(struct ringbuf_t *rb)
{
    time_t start = time(NULL);
    uint8_t a = 0;
    ringbuf_err err;
    while (difftime(time(NULL), start) < 3.f)
    {
        err = ringbuf_write(rb, &a, sizeof(a));
        if (err == RbNotEnoughSpace)
            continue;

        if (err != RbSuccess)
        {
            fprintf(stderr, "[writer] Error - ringbuf_write() returns code: %s\n", ringbuf_strerr(err));
            return NULL;
        }

        a = 0;
    }
    return NULL;
}

void *reader(struct ringbuf_t *rb)
{
    time_t start = time(NULL);
    uint8_t expected = 0, got = 0;
    size_t read = sizeof(got);
    ringbuf_err err;
    while (difftime(time(NULL), start) < 3.5f)
    {
        err = ringbuf_read(rb, &got, &read);
        if (err == RbEmpty)
            continue;

        if (err != RbSuccess)
        {
            fprintf(stderr, "[reader] Error - ringbuf_read() returns code: %s\n", ringbuf_strerr(err));
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
    pthread_t wthread, rthread;
    int ret;

    enum
    {
        rbdata_size = 0x40000
    };
    void *rbdata = calloc(1, rbdata_size);
    struct ringbuf_t *rb = (struct ringbuf_t *)calloc(1, sizeof(struct ringbuf_t));
    ringbuf_err rbe = ringbuf_init(rb, rbdata, rbdata_size);
    if (rbe != RbSuccess)
    {
        fprintf(stderr, "Error - ringbuf_init() return code: %d\n", rbe);
        return 1;
    }

    ret = pthread_create(&wthread, NULL, (void *(*)(void *))writer, rb);
    if (ret)
    {
        fprintf(stderr, "Error - pthread_create() return code: %d\n", ret);
        return 1;
    }

    ret = pthread_create(&rthread, NULL, (void *(*)(void *))reader, rb);
    if (ret)
    {
        fprintf(stderr, "Error - pthread_create() return code: %d\n", ret);
        return 1;
    }

    ret = pthread_join(wthread, NULL);
    if (ret)
    {
        fprintf(stderr, "Error - pthread_join() return code: %d\n", ret);
        return 1;
    }
    ret = pthread_join(rthread, NULL);
    if (ret)
    {
        fprintf(stderr, "Error - pthread_join() return code: %d\n", ret);
        return 1;
    }

#ifdef RINGBUF_STATISTICS
    struct ringbuf_stats_t rb_stats;
    double avg_read_ns = ringbuf_avg_read_ns(rb), avg_write_ns = ringbuf_avg_write_ns(rb);
    ringbuf_get_stats(rb, &rb_stats);

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
