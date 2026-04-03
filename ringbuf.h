#ifndef RINGBUF_H_
#define RINGBUF_H_

#include <stddef.h>
#include <stdint.h>

#if defined(__cplusplus)
#if defined(__GNUC__) || defined(__clang__)
#define RESTRICT __restrict__
#elif defined(_MSC_VER)
#define RESTRICT __restrict
#else
#define RESTRICT
#endif
#else
#if __STDC_VERSION__ >= 199901L
#define RESTRICT restrict
#else
#define RESTRICT
#endif
#endif

#define ERRORS          \
    X(RbSuccess)        \
    X(RbNotEnoughSpace) \
    X(RbEmpty)          \
    X(RbBufferTooSmall)

#define X(name) name,
typedef enum ringbuf_err
{
    ERRORS
} ringbuf_err_t;
#undef X

struct ringbuf;

#ifdef RINGBUF_STATISTICS
#ifdef RINGBUF_MPMC
struct ringbuf_stats
{
    _Atomic uint64_t bytes_written;
    _Atomic uint64_t bytes_read;
    _Atomic uint64_t writes;
    _Atomic uint64_t reads;
    _Atomic uint64_t total_write_ns;
    _Atomic uint64_t total_read_ns;
};
#else
struct ringbuf_stats
{
    uint64_t bytes_written;
    uint64_t bytes_read;
    uint64_t writes;
    uint64_t reads;
    uint64_t total_write_ns;
    uint64_t total_read_ns;
};
#endif
#endif

/**
 * @brief Iinitalizes ring buffer with a pre-allocated buffer
 *
 * @param rb a pre-allocated buffer atleast the sizeof(struct ringbuf)
 * @param buf a pre-allocated buffer where the main atomic structure/data is stored
 * @param buf_size the size of param `buf`
 * @return ringbuf_err_t RbSuccess on success, RbBufferTooSmall if buffer has insufficient space
 */
ringbuf_err_t ringbuf_init(struct ringbuf *rb, volatile void *buf, size_t buf_size);

/**
 * @brief Writes data to the ring buffer
 *
 * @param rb the ring buffer to write to
 * @param data the data to write
 * @param data_len length of the data to write
 * @return ringbuf_err_t RbSuccess on success, RbNotEnoughSpace if buffer has insufficient space
 */
ringbuf_err_t ringbuf_write(struct ringbuf *RESTRICT rb, const uint8_t *RESTRICT data, const size_t data_len);

/**
 * @brief Reads data from the ring buffer
 *
 * @param rb the ring buffer to read from
 * @param out output buffer to store the read data
 * @param out_len on input, the size of the output buffer; on output, the number of bytes read
 * @return ringbuf_err_t RbSuccess on success, RbEmpty if buffer is empty, RbBufferTooSmall if output buffer is too small
 */
ringbuf_err_t ringbuf_read(struct ringbuf *RESTRICT rb, uint8_t *RESTRICT out, size_t *RESTRICT out_len);

/**
 * @brief Returns a string representation of a ringbuf error code
 *
 * @param e the error code
 * @return const char* string representation of the error
 */
const char *ringbuf_strerr(const ringbuf_err_t e);

#ifdef RINGBUF_STATISTICS
/**
 * @brief Gets statistics for the ring buffer
 *
 * @param rb the ring buffer
 * @param out output structure to store the statistics
 */
void ringbuf_get_stats(const struct ringbuf *rb, struct ringbuf_stats *out);

/**
 * @brief Gets the average time taken for a write operation in nanoseconds
 *
 * @param rb the ring buffer
 * @return double average write time in nanoseconds, or 0 if no writes have been performed
 */
double ringbuf_avg_write_ns(const struct ringbuf *rb);

/**
 * @brief Gets the average time taken for a read operation in nanoseconds
 *
 * @param rb the ring buffer
 * @return double average read time in nanoseconds, or 0 if no reads have been performed
 */
double ringbuf_avg_read_ns(const struct ringbuf *rb);
#endif

#ifdef RINGBUF_IMPLEMENTATION

#include <assert.h>

#ifdef RINGBUF_STATISTICS
#ifdef _WIN32
#include <windows.h>
typedef struct ringbuf_time
{
    LARGE_INTEGER counter;
} ringbuf_time_t;
static inline void ringbuf_get_time(ringbuf_time_t *t)
{
    QueryPerformanceCounter(&t->counter);
}
static inline uint64_t ringbuf_get_elapsed_ns(ringbuf_time_t *start, ringbuf_time_t *end)
{
    LARGE_INTEGER freq;
    QueryPerformanceFrequency(&freq);
    return (uint64_t)((end->counter.QuadPart - start->counter.QuadPart) * 1000000000ULL / freq.QuadPart);
}
#else
#include <time.h>
typedef struct ringbuf_time
{
    struct timespec ts;
} ringbuf_time_t;
static inline void ringbuf_get_time(ringbuf_time_t *t)
{
    clock_gettime(CLOCK_MONOTONIC, &t->ts);
}
static inline uint64_t ringbuf_get_elapsed_ns(ringbuf_time_t *start, ringbuf_time_t *end)
{
    return (uint64_t)(end->ts.tv_sec - start->ts.tv_sec) * 1000000000ULL + (uint64_t)(end->ts.tv_nsec - start->ts.tv_nsec);
}
#endif
#endif

#ifndef RINGBUF_CACHE_LINE_SIZE
#define RINGBUF_CACHE_LINE_SIZE (64)
#endif

#ifndef RINGBUF_ALIGNMENT
#define RINGBUF_ALIGNMENT (0x1000)
#endif

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L && !defined(__STDC_NO_ATOMICS__)
#include <stdatomic.h>
#define ringbuf_atomic_load_explicit(ptr, order) \
    atomic_load_explicit(ptr, order)
#define ringbuf_atomic_store_explicit(ptr, val, order) \
    atomic_store_explicit(ptr, val, order)
#define ringbuf_atomic_compare_exchange_strong_explicit(ptr, expected, desired, succ, fail) \
    atomic_compare_exchange_strong_explicit(ptr, expected, desired, succ, fail)
#define ringbuf_atomic_compare_exchange_weak_explicit(ptr, expected, desired, succ, fail) \
    atomic_compare_exchange_weak_explicit(ptr, expected, desired, succ, fail)
#define RINGBUF_MEMORY_ORDER_RELAXED memory_order_relaxed
#define RINGBUF_MEMORY_ORDER_ACQUIRE memory_order_acquire
#define RINGBUF_MEMORY_ORDER_RELEASE memory_order_release
#define RINGBUF_MEMORY_ORDER_ACQUIRE_RELEASE memory_order_acq_rel
#define RINGBUF_MEMORY_ORDER_SEQ_CST memory_order_seq_cst
#elif defined(__GNUC__) || defined(__clang__)
#define RINGBUF_MEMORY_ORDER_RELAXED __ATOMIC_RELAXED
#define RINGBUF_MEMORY_ORDER_ACQUIRE __ATOMIC_ACQUIRE
#define RINGBUF_MEMORY_ORDER_RELEASE __ATOMIC_RELEASE
#define RINGBUF_MEMORY_ORDER_ACQUIRE_RELEASE __ATOMIC_ACQ_REL
#define RINGBUF_MEMORY_ORDER_SEQ_CST __ATOMIC_SEQ_CST
static inline size_t ringbuf_atomic_load_explicit(volatile size_t *ptr, int order)
{
    return __atomic_load_n(ptr, order);
}
static inline void ringbuf_atomic_store_explicit(volatile size_t *ptr, size_t val, int order)
{
    __atomic_store_n(ptr, val, order);
}
static inline int ringbuf_atomic_compare_exchange_strong_explicit(volatile size_t *ptr, size_t *expected, size_t desired, int succ, int fail)
{
    return __atomic_compare_exchange_n(ptr, expected, desired, 0, succ, fail);
}
static inline int ringbuf_atomic_compare_exchange_weak_explicit(volatile size_t *ptr, size_t *expected, size_t desired, int succ, int fail)
{
    return __atomic_compare_exchange_n(ptr, expected, desired, 1, succ, fail);
}
#elif defined(_MSC_VER)
#include <intrin.h>
#define RINGBUF_MEMORY_ORDER_RELAXED 0
#define RINGBUF_MEMORY_ORDER_ACQUIRE 2
#define RINGBUF_MEMORY_ORDER_RELEASE 3
#define RINGBUF_MEMORY_ORDER_SEQ_CST 5
static inline size_t ringbuf_atomic_load_explicit(volatile size_t *ptr, int order)
{
    (void)order;
    _ReadWriteBarrier();
    return *ptr;
}
static inline void ringbuf_atomic_store_explicit(volatile size_t *ptr, size_t val, int order)
{
    (void)order;
    _ReadWriteBarrier();
    *ptr = val;
}
static inline int ringbuf_atomic_compare_exchange_strong_explicit(volatile size_t *ptr, size_t *expected, size_t desired, int succ, int fail)
{
    (void)succ;
    (void)fail;
    size_t old = _InterlockedCompareExchange((volatile long *)ptr, (long)desired, (long)*expected);
    if (old == *expected)
    {
        return 1;
    }
    *expected = old;
    return 0;
}
static inline int ringbuf_atomic_compare_exchange_weak_explicit(volatile size_t *ptr, size_t *expected, size_t desired, int succ, int fail)
{
    return ringbuf_atomic_compare_exchange_strong_explicit(ptr, expected, desired, succ, fail);
}
#else
#error "Unsupported platform: atomics require C11, GCC/Clang __atomic intrinsics, or MSVC"
#endif

struct ringbuf
{
    volatile struct ringbuf_buf *buf;
    size_t buf_data_size;
#ifdef RINGBUF_STATISTICS
    struct ringbuf_stats stats;
#endif
};

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L && !defined(__STDC_NO_ATOMICS__)
#define RINGBUF_ATOMIC_TYPE atomic_size_t
#else
#define RINGBUF_ATOMIC_TYPE volatile size_t
#endif

struct ringbuf_buf
{
    RINGBUF_ATOMIC_TYPE head;
    uint8_t pad_1[RINGBUF_CACHE_LINE_SIZE - sizeof(RINGBUF_ATOMIC_TYPE)];

    RINGBUF_ATOMIC_TYPE tail;
    uint8_t pad_2[RINGBUF_CACHE_LINE_SIZE - sizeof(RINGBUF_ATOMIC_TYPE)];

#ifdef RINGBUF_MPMC
    RINGBUF_ATOMIC_TYPE head_pending;
    uint8_t pad_3[RINGBUF_CACHE_LINE_SIZE - sizeof(RINGBUF_ATOMIC_TYPE)];

    RINGBUF_ATOMIC_TYPE tail_pending;
    uint8_t pad_4[RINGBUF_CACHE_LINE_SIZE - sizeof(RINGBUF_ATOMIC_TYPE)];

    RINGBUF_ATOMIC_TYPE speculative_read_lock;
    uint8_t pad_5[RINGBUF_CACHE_LINE_SIZE - sizeof(RINGBUF_ATOMIC_TYPE)];
#endif

    uint8_t data[];
};

#undef RINGBUF_ATOMIC_TYPE

static inline uintptr_t ringbuf_align_sized(uintptr_t ptr, size_t *ptr_size, size_t alignment)
{
    size_t unalignment = ptr % alignment;
    if (unalignment == 0)
        return ptr;

    if (unalignment > *ptr_size)
        return 0;

    *ptr_size -= alignment - unalignment;
    return ptr + (alignment - unalignment);
}

ringbuf_err_t ringbuf_init(struct ringbuf *rb, volatile void *buf, size_t buf_size)
{
    assert(rb != NULL);
    assert(buf != NULL);
    assert(buf_size > 0);

    buf = (void *)ringbuf_align_sized((uintptr_t)buf, &buf_size, RINGBUF_ALIGNMENT);
    if (buf == NULL)
        return RbBufferTooSmall;
    if (buf_size <= sizeof(struct ringbuf_buf))
        return RbBufferTooSmall;

    rb->buf = buf;
    rb->buf_data_size = buf_size - sizeof(struct ringbuf_buf);

    return RbSuccess;
}

ringbuf_err_t ringbuf_write(struct ringbuf *RESTRICT rb, const uint8_t *RESTRICT data, const size_t data_len)
{
    assert(rb != NULL);
    assert(data != NULL);
    assert(data_len > 0);

#ifdef RINGBUF_STATISTICS
    ringbuf_time_t start, end;
    ringbuf_get_time(&start);
#endif

    size_t head = ringbuf_atomic_load_explicit(&rb->buf->head, RINGBUF_MEMORY_ORDER_RELAXED);
    size_t tail = ringbuf_atomic_load_explicit(&rb->buf->tail, RINGBUF_MEMORY_ORDER_ACQUIRE);

    assert(head < rb->buf_data_size && tail < rb->buf_data_size);

    size_t available;
    if (head > tail)
        available = rb->buf_data_size - head + tail;
    else if (head < tail)
        available = tail - head;
    else
        available = rb->buf_data_size - 1;

    assert(available <= rb->buf_data_size);

    size_t pos = head;

    // variable length numeric encoding
    int8_t end_byte;

#define LITTLE_ENDIAN_DEFINE                           \
    end_byte = sizeof(data_len);                       \
    for (; end_byte > 0; end_byte--)                   \
        if (((uint8_t *)&data_len)[end_byte - 1] != 0) \
    break
#define BIG_ENDIAN_DEFINE                               \
    end_byte = 1;                                       \
    for (; end_byte < sizeof(data_len) + 1; end_byte++) \
        if (((uint8_t *)&data_len)[end_byte - 1] != 0)  \
    break

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    LITTLE_ENDIAN_DEFINE;
#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    BIG_ENDIAN_DEFINE;
#else
    {
        uint16_t i = 1;
        if (*(uint8_t *)&i == 1)
            LITTLE_ENDIAN_DEFINE;
        else
            BIG_ENDIAN_DEFINE;
    }
#endif

#undef LITTLE_ENDIAN_DEFINE
#undef BIG_ENDIAN_DEFINE

    if (available <= 1 + end_byte + data_len)
        return RbNotEnoughSpace;

    assert(end_byte <= (int8_t)sizeof(size_t));

    rb->buf->data[pos] = (uint8_t)end_byte;
    pos = (pos + 1) % rb->buf_data_size;

    for (size_t i = 0; i < (uint8_t)end_byte; i++)
    {
#define LITTLE_ENDIAN_DEFINE \
    rb->buf->data[pos] = ((uint8_t *)&data_len)[i];
#define BIG_ENDIAN_DEFINE \
    rb->buf->data[pos] = ((uint8_t *)&data_len)[sizeof(data_len) - i];

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
        LITTLE_ENDIAN_DEFINE;
#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
        BIG_ENDIAN_DEFINE;
#else
        {
            uint16_t i = 1;
            if (*(uint8_t *)&i == 1)
                LITTLE_ENDIAN_DEFINE;
            else
                BIG_ENDIAN_DEFINE;
        }
#endif

#undef LITTLE_ENDIAN_DEFINE
#undef BIG_ENDIAN_DEFINE

        pos = (pos + 1) % rb->buf_data_size;
    }

    for (size_t i = 0; i < data_len; i++)
    {
        rb->buf->data[pos] = data[i];
        pos = (pos + 1) % rb->buf_data_size;
    }

    ringbuf_atomic_store_explicit(&rb->buf->head, pos, RINGBUF_MEMORY_ORDER_RELEASE);

#ifdef RINGBUF_STATISTICS
    ringbuf_get_time(&end);
    rb->stats.total_write_ns += ringbuf_get_elapsed_ns(&start, &end);
    rb->stats.writes++;
    rb->stats.bytes_written += (size_t)end_byte + 1 + data_len;
#endif

    return RbSuccess;
}

ringbuf_err_t ringbuf_read(struct ringbuf *RESTRICT rb, uint8_t *RESTRICT out, size_t *RESTRICT out_len)
{
    assert(rb != NULL);
    assert(out != NULL);
    assert(out_len != NULL);
    assert(*out_len > 0);

#ifdef RINGBUF_STATISTICS
    ringbuf_time_t start, end;
    ringbuf_get_time(&start);
#endif

    size_t tail = ringbuf_atomic_load_explicit(&rb->buf->tail, RINGBUF_MEMORY_ORDER_RELAXED);
    size_t head = ringbuf_atomic_load_explicit(&rb->buf->head, RINGBUF_MEMORY_ORDER_ACQUIRE);

    assert(head < rb->buf_data_size && tail < rb->buf_data_size);

    if (tail == head)
        return RbEmpty;

    size_t pos = tail;
    size_t data_len = 0;

    // variable length numeric encoding
    uint8_t num_len = rb->buf->data[pos];
    pos = (pos + 1) % rb->buf_data_size;

    assert(num_len <= sizeof(size_t));

#define MIN(a, b) ((a) < (b) ? (a) : (b))
    for (size_t i = 0; i < MIN(num_len, sizeof(size_t)); i++)
    {
#define LITTLE_ENDIAN_DEFINE \
    ((uint8_t *)&data_len)[i] = rb->buf->data[pos];
#define BIG_ENDIAN_DEFINE \
    ((uint8_t *)&data_len)[sizeof(data_len) - i] = rb->buf->data[pos];

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
        LITTLE_ENDIAN_DEFINE;
#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
        BIG_ENDIAN_DEFINE;
#else
        {
            uint16_t i = 1;
            if (*(uint8_t *)&i == 1)
                LITTLE_ENDIAN_DEFINE;
            else
                BIG_ENDIAN_DEFINE;
        }
#endif

#undef LITTLE_ENDIAN_DEFINE
#undef BIG_ENDIAN_DEFINE
#undef MIN

        pos = (pos + 1) % rb->buf_data_size;
    }

    size_t available;
    if (head >= tail)
        available = head - tail;
    else
        available = rb->buf_data_size - tail + head;

    assert(available <= rb->buf_data_size);
    assert(num_len <= sizeof(size_t));

    assert(available >= 1 + num_len + data_len);

    size_t capacity = *out_len;
    if (capacity < data_len)
    {
        *out_len = data_len;
        pos = (tail + 1 + num_len + data_len) % rb->buf_data_size;
        return RbBufferTooSmall;
    }

    for (size_t i = 0; i < data_len; i++)
    {
        out[i] = rb->buf->data[pos];
        pos = (pos + 1) % rb->buf_data_size;
    }

    *out_len = data_len;

    ringbuf_atomic_store_explicit(&rb->buf->tail, pos, RINGBUF_MEMORY_ORDER_RELEASE);

#ifdef RINGBUF_STATISTICS
    ringbuf_get_time(&end);
    rb->stats.total_read_ns += ringbuf_get_elapsed_ns(&start, &end);
    rb->stats.reads++;
    rb->stats.bytes_read += (size_t)num_len + 1 + data_len;
#endif

    return RbSuccess;
}

#ifdef RINGBUF_MPMC
ringbuf_err_t ringbuf_mpmc_write(struct ringbuf *RESTRICT rb, const uint8_t *RESTRICT data, const size_t data_len)
{
    assert(rb != NULL);
    assert(data != NULL);
    assert(data_len > 0);

#ifdef RINGBUF_STATISTICS
    ringbuf_time_t start, end;
    ringbuf_get_time(&start);
#endif

    // variable length numeric encoding
    size_t end_byte;

#define LITTLE_ENDIAN_DEFINE                           \
    end_byte = sizeof(data_len);                       \
    for (; end_byte > 0; end_byte--)                   \
        if (((uint8_t *)&data_len)[end_byte - 1] != 0) \
    break
#define BIG_ENDIAN_DEFINE                               \
    end_byte = 1;                                       \
    for (; end_byte < sizeof(data_len) + 1; end_byte++) \
        if (((uint8_t *)&data_len)[end_byte - 1] != 0)  \
    break

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    LITTLE_ENDIAN_DEFINE;
#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    BIG_ENDIAN_DEFINE;
#else
    {
        uint16_t i = 1;
        if (*(uint8_t *)&i == 1)
            LITTLE_ENDIAN_DEFINE;
        else
            BIG_ENDIAN_DEFINE;
    }
#endif

#undef LITTLE_ENDIAN_DEFINE
#undef BIG_ENDIAN_DEFINE

    size_t head_pending, tail;

    // reason for recalculating availability / re-loading head_pending
    // is to make sure this thread cant hang forever. Because if another thread
    // writes to `head_pending` and increments it, if we do a while loop for the
    // atomic compare exchange then it will never exchange unless of the off chance
    // it wraps back around and lands on the same value.
    // But, we want to avoid this situation.
    while (1)
    {
        head_pending = ringbuf_atomic_load_explicit(&rb->buf->head_pending, RINGBUF_MEMORY_ORDER_ACQUIRE);
        tail = ringbuf_atomic_load_explicit(&rb->buf->tail, RINGBUF_MEMORY_ORDER_ACQUIRE);

        assert(head_pending < rb->buf_data_size && tail < rb->buf_data_size);

        size_t available;
        if (head_pending > tail)
            available = rb->buf_data_size - head_pending + tail;
        else if (head_pending < tail)
            available = tail - head_pending;
        else
            available = rb->buf_data_size - 1;

        if (available <= 1 + end_byte + data_len)
            return RbNotEnoughSpace;

        assert(available <= rb->buf_data_size);

        if (ringbuf_atomic_compare_exchange_strong_explicit(&rb->buf->head_pending, &head_pending, (head_pending + 1 + end_byte + data_len) % rb->buf_data_size, RINGBUF_MEMORY_ORDER_ACQUIRE_RELEASE, RINGBUF_MEMORY_ORDER_RELAXED))
            break;
    }

    size_t pos = head_pending;
    rb->buf->data[pos] = (uint8_t)end_byte;
    pos = (pos + sizeof(uint8_t)) % rb->buf_data_size;

    for (size_t i = 0; i < (uint8_t)end_byte; i++)
    {
#define LITTLE_ENDIAN_DEFINE \
    rb->buf->data[pos] = ((uint8_t *)&data_len)[i];
#define BIG_ENDIAN_DEFINE \
    rb->buf->data[pos] = ((uint8_t *)&data_len)[sizeof(data_len) - i];

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
        LITTLE_ENDIAN_DEFINE;
#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
        BIG_ENDIAN_DEFINE;
#else
        {
            uint16_t i = 1;
            if (*(uint8_t *)&i == 1)
                LITTLE_ENDIAN_DEFINE;
            else
                BIG_ENDIAN_DEFINE;
        }
#endif

#undef LITTLE_ENDIAN_DEFINE
#undef BIG_ENDIAN_DEFINE

        pos = (pos + 1) % rb->buf_data_size;
    }

    for (size_t i = 0; i < data_len; i++)
    {
        rb->buf->data[pos] = data[i];
        pos = (pos + 1) % rb->buf_data_size;
    }

    size_t expecting = head_pending;
    if (!ringbuf_atomic_compare_exchange_strong_explicit(&rb->buf->head, &expecting, pos, RINGBUF_MEMORY_ORDER_ACQUIRE_RELEASE, RINGBUF_MEMORY_ORDER_RELAXED))
    {
        do
        {
            expecting = head_pending;
        } while (!ringbuf_atomic_compare_exchange_weak_explicit(&rb->buf->head, &expecting, pos, RINGBUF_MEMORY_ORDER_RELEASE, RINGBUF_MEMORY_ORDER_RELAXED));
    }

#ifdef RINGBUF_STATISTICS
    ringbuf_get_time(&end);
#ifdef RINGBUF_MPMC
    atomic_fetch_add_explicit(&rb->stats.total_write_ns, ringbuf_get_elapsed_ns(&start, &end), RINGBUF_MEMORY_ORDER_RELAXED);
    atomic_fetch_add_explicit(&rb->stats.writes, 1, RINGBUF_MEMORY_ORDER_RELAXED);
    atomic_fetch_add_explicit(&rb->stats.bytes_written, (size_t)end_byte + 1 + data_len, RINGBUF_MEMORY_ORDER_RELAXED);
#else
    rb->stats.total_write_ns += ringbuf_get_elapsed_ns(&start, &end);
    rb->stats.writes++;
    rb->stats.bytes_written += (size_t)end_byte + 1 + data_len;
#endif
#endif

    return RbSuccess;
}

ringbuf_err_t ringbuf_mpmc_read(struct ringbuf *RESTRICT rb, uint8_t *RESTRICT out, size_t *RESTRICT out_len)
{
    assert(rb != NULL);
    assert(out != NULL);
    assert(out_len != NULL);
    assert(*out_len > 0);

#ifdef RINGBUF_STATISTICS
    ringbuf_time_t start, end;
    ringbuf_get_time(&start);
#endif

    size_t tail_pending, head, pos, data_len;
    uint8_t num_len;

    // same reason as in `ringbuf_write`
    while (1)
    {
        size_t expecting = 0;
        if (!ringbuf_atomic_compare_exchange_strong_explicit(&rb->buf->speculative_read_lock, &expecting, 1, RINGBUF_MEMORY_ORDER_ACQUIRE_RELEASE, RINGBUF_MEMORY_ORDER_RELAXED))
        {
            do
            {
                expecting = 0;
            } while (!ringbuf_atomic_compare_exchange_weak_explicit(&rb->buf->speculative_read_lock, &expecting, 1, RINGBUF_MEMORY_ORDER_RELEASE, RINGBUF_MEMORY_ORDER_RELAXED));
        }

        tail_pending = ringbuf_atomic_load_explicit(&rb->buf->tail_pending, RINGBUF_MEMORY_ORDER_ACQUIRE);
        head = ringbuf_atomic_load_explicit(&rb->buf->head, RINGBUF_MEMORY_ORDER_ACQUIRE);

        assert(head < rb->buf_data_size && tail_pending < rb->buf_data_size);

        if (tail_pending == head)
        {
            ringbuf_atomic_store_explicit(&rb->buf->speculative_read_lock, 0, RINGBUF_MEMORY_ORDER_RELEASE);
            return RbEmpty;
        }

        pos = tail_pending;
        data_len = 0;

        // variable length numeric encoding
        num_len = rb->buf->data[pos];
        pos = (pos + 1) % rb->buf_data_size;

#define MIN(a, b) ((a) < (b) ? (a) : (b))
        for (size_t i = 0; i < MIN(num_len, sizeof(size_t)); i++)
        {
#define LITTLE_ENDIAN_DEFINE \
    ((uint8_t *)&data_len)[i] = rb->buf->data[pos];
#define BIG_ENDIAN_DEFINE \
    ((uint8_t *)&data_len)[sizeof(data_len) - i] = rb->buf->data[pos];

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
            LITTLE_ENDIAN_DEFINE;
#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
            BIG_ENDIAN_DEFINE;
#else
            {
                uint16_t i = 1;
                if (*(uint8_t *)&i == 1)
                    LITTLE_ENDIAN_DEFINE;
                else
                    BIG_ENDIAN_DEFINE;
            }
#endif

#undef LITTLE_ENDIAN_DEFINE
#undef BIG_ENDIAN_DEFINE
#undef MIN

            pos = (pos + 1) % rb->buf_data_size;
        }

        size_t available;
        if (head >= tail_pending)
            available = head - tail_pending;
        else
            available = rb->buf_data_size - tail_pending + head;

        assert(available <= rb->buf_data_size);
        assert(num_len <= sizeof(size_t));

        assert(available >= 1 + num_len + data_len);

        size_t capacity = *out_len;
        if (capacity < data_len)
        {
            *out_len = data_len;
            pos = (tail_pending + 1 + num_len + data_len) % rb->buf_data_size;
            ringbuf_atomic_store_explicit(&rb->buf->speculative_read_lock, 0, RINGBUF_MEMORY_ORDER_RELEASE);
            return RbBufferTooSmall;
        }

        if (ringbuf_atomic_compare_exchange_strong_explicit(&rb->buf->tail_pending, &tail_pending, (tail_pending + 1 + num_len + data_len) % rb->buf_data_size, RINGBUF_MEMORY_ORDER_ACQUIRE_RELEASE, RINGBUF_MEMORY_ORDER_RELAXED))
        {
            ringbuf_atomic_store_explicit(&rb->buf->speculative_read_lock, 0, RINGBUF_MEMORY_ORDER_RELEASE);
            break;
        }

        ringbuf_atomic_store_explicit(&rb->buf->speculative_read_lock, 0, RINGBUF_MEMORY_ORDER_RELEASE);
    }

    for (size_t i = 0; i < data_len; i++)
    {
        out[i] = rb->buf->data[pos];
        pos = (pos + 1) % rb->buf_data_size;
    }

    *out_len = data_len;

    size_t expecting = tail_pending;
    if (!ringbuf_atomic_compare_exchange_strong_explicit(&rb->buf->tail, &expecting, pos, RINGBUF_MEMORY_ORDER_ACQUIRE_RELEASE, RINGBUF_MEMORY_ORDER_RELAXED))
    {
        do
        {
            expecting = tail_pending;
        } while (!ringbuf_atomic_compare_exchange_weak_explicit(&rb->buf->tail, &expecting, pos, RINGBUF_MEMORY_ORDER_RELEASE, RINGBUF_MEMORY_ORDER_RELAXED));
    }

#ifdef RINGBUF_STATISTICS
    ringbuf_get_time(&end);
#ifdef RINGBUF_MPMC
    atomic_fetch_add_explicit(&rb->stats.total_read_ns, ringbuf_get_elapsed_ns(&start, &end), RINGBUF_MEMORY_ORDER_RELAXED);
    atomic_fetch_add_explicit(&rb->stats.reads, 1, RINGBUF_MEMORY_ORDER_RELAXED);
    atomic_fetch_add_explicit(&rb->stats.bytes_read, (size_t)num_len + 1 + data_len, RINGBUF_MEMORY_ORDER_RELAXED);
#else
    rb->stats.total_read_ns += ringbuf_get_elapsed_ns(&start, &end);
    rb->stats.reads++;
    rb->stats.bytes_read += (size_t)num_len + 1 + data_len;
#endif
#endif

    return RbSuccess;
}
#endif // RINGBUF_MPMC

const char *ringbuf_strerr(ringbuf_err_t e)
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
void ringbuf_get_stats(const struct ringbuf *rb, struct ringbuf_stats *out)
{
    assert(rb != NULL);
    assert(out != NULL);
    *out = rb->stats;
}

double ringbuf_avg_write_ns(const struct ringbuf *rb)
{
    if (rb->stats.writes == 0)
        return 0;
    return (double)rb->stats.total_write_ns / rb->stats.writes;
}

double ringbuf_avg_read_ns(const struct ringbuf *rb)
{
    if (rb->stats.reads == 0)
        return 0;
    return (double)rb->stats.total_read_ns / rb->stats.reads;
}
#endif

#undef RINGBUF_MEMORY_ORDER_RELAXED
#undef RINGBUF_MEMORY_ORDER_ACQUIRE
#undef RINGBUF_MEMORY_ORDER_RELEASE
#undef RINGBUF_MEMORY_ORDER_SEQ_CST

#undef RINGBUF_ALIGNMENT
#undef RINGBUF_CACHE_LINE_SIZE

#endif

#undef ERRORS
#undef RESTRICT

#endif
