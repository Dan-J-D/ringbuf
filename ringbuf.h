#ifndef RINGBUF_H_
#define RINGBUF_H_

#include <stddef.h>
#include <stdint.h>

#if defined(__cplusplus)
/* C++ does not have standard restrict, check compiler extensions */
#if defined(__GNUC__) || defined(__clang__)
#define RESTRICT __restrict__
#elif defined(_MSC_VER)
#define RESTRICT __restrict
#else
#define RESTRICT
#endif
#else
/* C code */
#if __STDC_VERSION__ >= 199901L
#define RESTRICT restrict /* C99 or later */
#else
#define RESTRICT /* pre-C99, empty */
#endif
#endif

#define ERRORS          \
    X(RbSuccess)        \
    X(RbOutOfMemory)    \
    X(RbNotEnoughSpace) \
    X(RbEmpty)          \
    X(RbBufferTooSmall) \
    X(RbCorrupt)

#define X(name) name,
typedef enum ringbuf_err
{
    ERRORS
} ringbuf_err;
#undef X

struct ringbuf_t;

#ifdef RINGBUF_STATISTICS
struct ringbuf_stats_t
{
    uint64_t bytes_written;
    uint64_t bytes_read;
    uint64_t writes;
    uint64_t reads;
    uint64_t total_write_ns;
    uint64_t total_read_ns;
};
#endif // RINGBUF_STATISTICS

ringbuf_err ringbuf_init(struct ringbuf_t *rb, volatile void *buf, size_t buf_size);
ringbuf_err ringbuf_write(struct ringbuf_t *RESTRICT rb, const uint8_t *RESTRICT data, const size_t data_len);
ringbuf_err ringbuf_read(struct ringbuf_t *RESTRICT rb, uint8_t *RESTRICT out, size_t *RESTRICT out_len);

const char *ringbuf_strerr(ringbuf_err e);

#ifdef RINGBUF_STATISTICS
void ringbuf_get_stats(struct ringbuf_t *rb, struct ringbuf_stats_t *out);
double ringbuf_avg_write_ns(struct ringbuf_t *rb);
double ringbuf_avg_read_ns(struct ringbuf_t *rb);
#endif

#ifdef RINGBUF_IMPLEMENTATION

#include <stdatomic.h>
#include <assert.h>

#ifdef RINGBUF_STATISTICS
#include <time.h>
#endif

#define CACHE_LINE_SIZE (64)
#define RING_BUFFER_ALIGNMENT (0x1000)

struct ringbuf_t
{
    volatile struct buf_t *buf;
    size_t buf_data_size;
#ifdef RINGBUF_STATISTICS
    struct ringbuf_stats_t stats;
#endif
};

// Purpose of padding is so the variables are in different cache lines
struct buf_t
{
    atomic_size_t head;
    uint8_t pad_1[CACHE_LINE_SIZE - sizeof(atomic_size_t)];

    atomic_size_t tail;
    uint8_t pad_2[CACHE_LINE_SIZE - sizeof(atomic_size_t)];

    uint8_t data[];
};

// returns 0 if there isnt enough space
static inline uintptr_t align_sized(uintptr_t ptr, size_t *ptr_size, size_t alignment)
{
    size_t unalignment = ptr % alignment;
    if (unalignment == 0)
        return ptr;

    if (unalignment > *ptr_size)
        return 0;

    *ptr_size -= alignment - unalignment;
    return ptr + (alignment - unalignment);
}

// if buf is shared memory, then caching must be disabled
// buf must be pre zero'd out
ringbuf_err ringbuf_init(struct ringbuf_t *rb, volatile void *buf, size_t buf_size)
{
    assert(rb != NULL);
    assert(buf != NULL);
    assert(buf_size > 0);

    buf = (void *)align_sized((uintptr_t)buf, &buf_size, RING_BUFFER_ALIGNMENT);
    assert(buf != NULL); // not enough space in the buffer for ring buffer
    assert(buf_size > sizeof(struct buf_t));

    rb->buf = buf;
    rb->buf_data_size = buf_size - sizeof(struct buf_t);

    return RbSuccess;
}

ringbuf_err ringbuf_write(struct ringbuf_t *RESTRICT rb, const uint8_t *RESTRICT data, const size_t data_len)
{
    assert(rb != NULL);
    assert(data != NULL);
    assert(data_len > 0);

#ifdef RINGBUF_STATISTICS
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
#endif

    size_t head = atomic_load_explicit(&rb->buf->head, memory_order_relaxed);
    size_t tail = atomic_load_explicit(&rb->buf->tail, memory_order_acquire);

    size_t available;
    if (head > tail)
        available = rb->buf_data_size - head + tail;
    else if (head < tail)
        available = tail - head;
    else
        available = rb->buf_data_size - 1;

    if (available <= sizeof(size_t) + data_len)
        return RbNotEnoughSpace;

    size_t pos = head;

    for (size_t i = 0; i < sizeof(size_t); i++)
    {
        rb->buf->data[pos] = ((uint8_t *)&data_len)[i];
        pos = (pos + 1) % rb->buf_data_size;
    }

    for (size_t i = 0; i < data_len; i++)
    {
        rb->buf->data[pos] = data[i];
        pos = (pos + 1) % rb->buf_data_size;
    }

    atomic_store_explicit(&rb->buf->head, pos, memory_order_release);

#ifdef RINGBUF_STATISTICS
    clock_gettime(CLOCK_MONOTONIC, &end);
    rb->stats.total_write_ns += (end.tv_sec - start.tv_sec) * 1000000000ULL + (end.tv_nsec - start.tv_nsec);
    rb->stats.writes++;
    rb->stats.bytes_written += sizeof(size_t) + data_len;
#endif

    return RbSuccess;
}

ringbuf_err ringbuf_read(struct ringbuf_t *RESTRICT rb, uint8_t *RESTRICT out, size_t *RESTRICT out_len)
{
    assert(rb != NULL);
    assert(out != NULL);
    assert(out_len != NULL);
    assert(*out_len > 0);

#ifdef RINGBUF_STATISTICS
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
#endif

    size_t tail = atomic_load_explicit(&rb->buf->tail, memory_order_relaxed);
    size_t head = atomic_load_explicit(&rb->buf->head, memory_order_acquire);

    if (tail == head)
        return RbEmpty;

    size_t pos = tail;

    size_t data_len = 0;
    for (size_t i = 0; i < sizeof(size_t); i++)
    {
        ((uint8_t *)&data_len)[i] = rb->buf->data[pos];
        pos = (pos + 1) % rb->buf_data_size;
    }

    size_t available;
    if (head >= tail)
        available = head - tail;
    else
        available = rb->buf_data_size - tail + head;

    if (available < sizeof(size_t) + data_len)
        return RbCorrupt;

    size_t capacity = *out_len;
    if (capacity < data_len)
    {
        *out_len = data_len;
        return RbBufferTooSmall;
    }

    for (size_t i = 0; i < data_len; i++)
    {
        out[i] = rb->buf->data[pos];
        pos = (pos + 1) % rb->buf_data_size;
    }

    *out_len = data_len;

    atomic_store_explicit(&rb->buf->tail, pos, memory_order_release);

#ifdef RINGBUF_STATISTICS
    clock_gettime(CLOCK_MONOTONIC, &end);
    rb->stats.total_read_ns += (end.tv_sec - start.tv_sec) * 1000000000ULL + (end.tv_nsec - start.tv_nsec);
    rb->stats.reads++;
    rb->stats.bytes_read += sizeof(size_t) + data_len;
#endif

    return RbSuccess;
}

const char *ringbuf_strerr(ringbuf_err e)
{
#define X(name) \
    case name:  \
        return #name;
    switch (e)
    {
        ERRORS
    }
#undef X

    return "Unknown Error";
}

#ifdef RINGBUF_STATISTICS
void ringbuf_get_stats(struct ringbuf_t *rb, struct ringbuf_stats_t *out)
{
    assert(rb != NULL);
    assert(out != NULL);
    *out = rb->stats;
}

double ringbuf_avg_write_ns(struct ringbuf_t *rb)
{
    if (rb->stats.writes == 0)
        return 0;
    return (double)rb->stats.total_write_ns / rb->stats.writes;
}

double ringbuf_avg_read_ns(struct ringbuf_t *rb)
{
    if (rb->stats.reads == 0)
        return 0;
    return (double)rb->stats.total_read_ns / rb->stats.reads;
}
#endif // RINGBUF_STATISTICS

#endif // RINGBUF_IMPLEMENTATION

#undef ERRORS
#undef RESTRICT

#endif // RINGBUF_H_
