#define RINGBUF_IMPLEMENTATION
#define RINGBUF_STATISTICS
#include "../ringbuf.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static struct ringbuf rb;
static uint8_t buffer[0x2000] __attribute__((aligned(0x1000)));
static int initialized = 0;

#ifndef RINGBUF_MEMORY_ORDER_RELAXED
#define RINGBUF_MEMORY_ORDER_RELAXED __ATOMIC_RELAXED
#endif
#ifndef RINGBUF_MEMORY_ORDER_ACQUIRE
#define RINGBUF_MEMORY_ORDER_ACQUIRE __ATOMIC_ACQUIRE
#endif

static struct
{
    size_t write_success;
    size_t write_not_enough_space;
    size_t read_success;
    size_t read_empty;
    size_t read_buffer_too_small;
} invariants = {0};

static void reset_invariants(void)
{
    memset(&invariants, 0, sizeof(invariants));
}

static void track_write(ringbuf_err_t err)
{
    switch (err)
    {
    case RbSuccess:
        invariants.write_success++;
        break;
    case RbNotEnoughSpace:
        invariants.write_not_enough_space++;
        break;
    default:
        break;
    }
}

static void track_read(ringbuf_err_t err)
{
    switch (err)
    {
    case RbSuccess:
        invariants.read_success++;
        break;
    case RbEmpty:
        invariants.read_empty++;
        break;
    case RbBufferTooSmall:
        invariants.read_buffer_too_small++;
        break;
    default:
        break;
    }
}

static void check_invariants(void)
{
    struct ringbuf_stats stats;
    ringbuf_get_stats(&rb, &stats);

    size_t head = ringbuf_atomic_load_explicit(&rb.buf->head, RINGBUF_MEMORY_ORDER_RELAXED);
    size_t tail = ringbuf_atomic_load_explicit(&rb.buf->tail, RINGBUF_MEMORY_ORDER_ACQUIRE);

    if (stats.bytes_written < stats.bytes_read)
    {
        fprintf(stderr, "INVARIANT FAIL: bytes_written=%lu < bytes_read=%lu\n", stats.bytes_written, stats.bytes_read);
        abort();
    }

    if (stats.bytes_written - stats.bytes_read > rb.buf_data_size)
    {
        fprintf(stderr, "INVARIANT FAIL: unread_bytes=%lu > buf_data_size=%lu\n",
                stats.bytes_written - stats.bytes_read, rb.buf_data_size);
        abort();
    }

    if (stats.writes < stats.reads)
    {
        fprintf(stderr, "INVARIANT FAIL: writes=%lu < reads=%lu\n", stats.writes, stats.reads);
        abort();
    }

    if (head > rb.buf_data_size)
    {
        fprintf(stderr, "INVARIANT FAIL: head=%lu > buf_data_size=%lu\n", head, rb.buf_data_size);
        abort();
    }

    if (tail > rb.buf_data_size)
    {
        fprintf(stderr, "INVARIANT FAIL: tail=%lu > buf_data_size=%lu\n", tail, rb.buf_data_size);
        abort();
    }

#ifdef RINGBUF_MPMC
    size_t head_pending = ringbuf_atomic_load_explicit(&rb.buf->head_pending, RINGBUF_MEMORY_ORDER_ACQUIRE);
    size_t tail_pending = ringbuf_atomic_load_explicit(&rb.buf->tail_pending, RINGBUF_MEMORY_ORDER_ACQUIRE);

    if (head_pending > rb.buf_data_size)
    {
        fprintf(stderr, "INVARIANT FAIL: head_pending=%lu > buf_data_size=%lu\n", head_pending, rb.buf_data_size);
        abort();
    }

    if (tail_pending > rb.buf_data_size)
    {
        fprintf(stderr, "INVARIANT FAIL: tail_pending=%lu > buf_data_size=%lu\n", tail_pending, rb.buf_data_size);
        abort();
    }

    size_t speculative_read_lock = ringbuf_atomic_load_explicit(&rb.buf->speculative_read_lock, RINGBUF_MEMORY_ORDER_ACQUIRE);
    if (speculative_read_lock > 1)
    {
        fprintf(stderr, "INVARIANT FAIL: speculative_read_lock=%lu (expected 0 or 1)\n", speculative_read_lock);
        abort();
    }
#endif
}

static int reset_ringbuf(void)
{
    memset(buffer, 0, sizeof(buffer));
    memset(&rb, 0, sizeof(rb));
    ringbuf_err_t err = ringbuf_init(&rb, buffer, sizeof(buffer));
    if (err != RbSuccess)
        return 0;
    reset_invariants();
    return 1;
}

static ringbuf_err_t do_write(const uint8_t *data, size_t data_len)
{
    if (data_len == 0)
        return RbSuccess;
    ringbuf_err_t err;
#ifdef RINGBUF_MPMC
    err = ringbuf_mpmc_write(&rb, data, data_len);
#else
    err = ringbuf_write(&rb, data, data_len);
#endif
    track_write(err);
    return err;
}

static ringbuf_err_t do_read(size_t max_read)
{
    uint8_t tmp[256];
    size_t len = max_read < sizeof(tmp) ? max_read : sizeof(tmp);
    if (len == 0)
        return RbSuccess;
    ringbuf_err_t err;
#ifdef RINGBUF_MPMC
    err = ringbuf_mpmc_read(&rb, tmp, &len);
#else
    err = ringbuf_read(&rb, tmp, &len);
#endif
    track_read(err);
    return err;
}

enum
{
    OP_RESET,
    OP_WRITE,
    OP_READ,
    OP_WRITE_READ_MIX,
    OP_MULTI_WRITE,
    OP_MULTI_READ,
    OP_MIXED_OPS,
    OP_STRESS_SMALL,
    OP_STRESS_MEDIUM,
    OP_STRESS_LARGE,
    OP_CORRUPTION_INJECTION,
    OP_STRERR,
    NUM_OPS
};

int main(int argc, char **argv)
{
    FILE *f;
    unsigned char buffer[65536];
    size_t size;

    if (argc > 1)
    {
        f = fopen(argv[1], "rb");
        if (!f)
        {
            fprintf(stderr, "Failed to open %s\n", argv[1]);
            return 1;
        }
        size = fread(buffer, 1, sizeof(buffer), f);
        fclose(f);
    }
    else
    {
        size = fread(buffer, 1, sizeof(buffer), stdin);
    }

    if (size < 2)
        return 0;

    if (!initialized)
    {
        if (!reset_ringbuf())
            return 0;
        initialized = 1;
    }

    unsigned char op = buffer[0] % NUM_OPS;
    size_t off = 1;

    switch (op)
    {
    case OP_RESET:
    {
        check_invariants();
        reset_ringbuf();
        break;
    }

    case OP_WRITE:
    {
        if (off < size)
        {
            size_t len = buffer[off] % 257;
            if (len > 0 && off + 1 + len <= size)
            {
                do_write(buffer + off + 1, len);
            }
        }
        break;
    }

    case OP_READ:
    {
        if (off < size)
        {
            size_t max_read = buffer[off] % 257;
            do_read(max_read);
        }
        break;
    }

    case OP_WRITE_READ_MIX:
    {
        if (off + 2 <= size)
        {
            size_t write_len = buffer[off] % 129;
            size_t read_len = buffer[off + 1] % 65;
            if (write_len > 0 && off + 2 + write_len <= size)
            {
                do_write(buffer + off + 2, write_len);
            }
            do_read(read_len);
        }
        break;
    }

    case OP_MULTI_WRITE:
    {
        if (off < size)
        {
            uint8_t num_writes = (buffer[off] % 17) + 1;
            size_t pos = off + 1;
            for (uint8_t i = 0; i < num_writes && pos + 1 < size; i++)
            {
                size_t len = buffer[pos] % 257;
                pos++;
                if (len > 0 && pos + len <= size)
                {
                    do_write(buffer + pos, len);
                    pos += len;
                }
                else
                {
                    break;
                }
            }
        }
        break;
    }

    case OP_MULTI_READ:
    {
        if (off < size)
        {
            uint8_t num_reads = (buffer[off] % 17) + 1;
            size_t pos = off + 1;
            for (uint8_t i = 0; i < num_reads && pos < size; i++)
            {
                size_t max_read = buffer[pos] % 257;
                pos++;
                do_read(max_read);
            }
        }
        break;
    }

    case OP_MIXED_OPS:
    {
        if (off < size)
        {
            uint8_t num_ops = (buffer[off] % 17) + 1;
            size_t pos = off + 1;
            for (uint8_t i = 0; i < num_ops && pos + 1 < size; i++)
            {
                uint8_t sub_op = buffer[pos] % 3;
                pos++;
                switch (sub_op)
                {
                case 0:
                {
                    size_t len = buffer[pos] % 257;
                    pos++;
                    if (len > 0 && pos + len <= size)
                    {
                        do_write(buffer + pos, len);
                        pos += len;
                    }
                    break;
                }
                case 1:
                {
                    size_t max_read = buffer[pos] % 257;
                    pos++;
                    do_read(max_read);
                    break;
                }
                case 2:
                {
                    check_invariants();
                    reset_ringbuf();
                    break;
                }
                }
            }
        }
        break;
    }

    case OP_STRESS_SMALL:
    {
        uint8_t num_ops = (buffer[off] % 33) + 1;
        size_t pos = off + 1;
        for (uint8_t i = 0; i < num_ops && pos < size; i++)
        {
            size_t len = buffer[pos] % 17;
            pos++;
            if (len > 0 && pos + len <= size)
            {
                do_write(buffer + pos, len);
                pos += len;
            }
            if (i % 3 == 2)
            {
                do_read(buffer[pos % size] % 17);
            }
        }
        break;
    }

    case OP_STRESS_MEDIUM:
    {
        uint8_t num_ops = (buffer[off] % 17) + 1;
        size_t pos = off + 1;
        for (uint8_t i = 0; i < num_ops && pos < size; i++)
        {
            size_t write_len = buffer[pos] % 129;
            pos++;
            if (write_len > 0 && pos + write_len <= size)
            {
                do_write(buffer + pos, write_len);
                pos += write_len;
            }

            size_t read_len = buffer[pos % size] % 65;
            do_read(read_len);
        }
        break;
    }

    case OP_STRESS_LARGE:
    {
        uint8_t num_ops = (buffer[off] % 9) + 1;
        size_t pos = off + 1;
        for (uint8_t i = 0; i < num_ops && pos < size; i++)
        {
            size_t write_len = buffer[pos] % 513;
            pos++;
            if (write_len > 0 && pos + write_len <= size)
            {
                do_write(buffer + pos, write_len);
                pos += write_len;
            }

            if (i % 2 == 1)
            {
                size_t read_len = buffer[pos % size] % 257;
                do_read(read_len);
            }
        }
        break;
    }

    case OP_STRERR:
    {
        ringbuf_strerr((ringbuf_err_t)(off < size ? buffer[off] : 0));
        break;
    }

    default:
        break;
    }

#ifndef RINGBUF_MPMC
    if (op != OP_CORRUPTION_INJECTION && op != OP_RESET)
#endif
        check_invariants();

    return 0;
}
