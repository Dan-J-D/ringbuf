#define RINGBUF_IMPLEMENTATION
#include "../ringbuf.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

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

static struct ringbuf rb;
static uint8_t buffer[0x2000] __attribute__((aligned(0x1000)));
static int initialized = 0;

static int reset_ringbuf(void)
{
    memset(buffer, 0, sizeof(buffer));
    memset(&rb, 0, sizeof(rb));
    ringbuf_err_t err = ringbuf_init(&rb, buffer, sizeof(buffer));
    if (err != RbSuccess)
        return 0;
    return 1;
}

static ringbuf_err_t do_write(const uint8_t *data, size_t data_len)
{
    if (data_len == 0)
        return RbSuccess;
    if (rb.buf == NULL)
        return RbCorrupt;
#ifdef RINGBUF_MPMC
    return ringbuf_mpmc_write(&rb, data, data_len);
#else
    return ringbuf_write(&rb, data, data_len);
#endif
}

static ringbuf_err_t do_read(size_t max_read)
{
    if (rb.buf == NULL)
        return RbCorrupt;
    uint8_t tmp[256];
    size_t len = max_read < sizeof(tmp) ? max_read : sizeof(tmp);
    if (len == 0)
        return RbSuccess;
#ifdef RINGBUF_MPMC
    return ringbuf_mpmc_read(&rb, tmp, &len);
#else
    return ringbuf_read(&rb, tmp, &len);
#endif
}

static size_t min_size(size_t a, size_t b)
{
    return a < b ? a : b;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size < 2)
        return 0;

    if (!initialized)
    {
        if (!reset_ringbuf())
            return 0;
        initialized = 1;
    }

    uint8_t op = data[0] % NUM_OPS;
    size_t off = 1;

    switch (op)
    {
    case OP_RESET:
    {
        reset_ringbuf();
        break;
    }

    case OP_WRITE:
    {
        if (off < size)
        {
            size_t len = data[off] % 257;
            if (len > 0 && off + 1 + len <= size)
            {
                do_write(data + off + 1, len);
            }
        }
        break;
    }

    case OP_READ:
    {
        if (off < size)
        {
            size_t max_read = data[off] % 257;
            do_read(max_read);
        }
        break;
    }

    case OP_WRITE_READ_MIX:
    {
        if (off + 2 <= size)
        {
            size_t write_len = data[off] % 129;
            size_t read_len = data[off + 1] % 65;
            if (write_len > 0 && off + 2 + write_len <= size)
            {
                do_write(data + off + 2, write_len);
            }
            do_read(read_len);
        }
        break;
    }

    case OP_MULTI_WRITE:
    {
        if (off < size)
        {
            uint8_t num_writes = (data[off] % 17) + 1;
            size_t pos = off + 1;
            for (uint8_t i = 0; i < num_writes && pos + 1 < size; i++)
            {
                size_t len = data[pos] % 257;
                pos++;
                if (len > 0 && pos + len <= size)
                {
                    do_write(data + pos, len);
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
            uint8_t num_reads = (data[off] % 17) + 1;
            size_t pos = off + 1;
            for (uint8_t i = 0; i < num_reads && pos < size; i++)
            {
                size_t max_read = data[pos] % 257;
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
            uint8_t num_ops = (data[off] % 17) + 1;
            size_t pos = off + 1;
            for (uint8_t i = 0; i < num_ops && pos + 1 < size; i++)
            {
                uint8_t sub_op = data[pos] % 3;
                pos++;
                switch (sub_op)
                {
                case 0:
                {
                    size_t len = data[pos] % 257;
                    pos++;
                    if (len > 0 && pos + len <= size)
                    {
                        do_write(data + pos, len);
                        pos += len;
                    }
                    break;
                }
                case 1:
                {
                    size_t max_read = data[pos] % 257;
                    pos++;
                    do_read(max_read);
                    break;
                }
                case 2:
                {
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
        uint8_t num_ops = (data[off] % 33) + 1;
        size_t pos = off + 1;
        for (uint8_t i = 0; i < num_ops && pos < size; i++)
        {
            size_t len = data[pos] % 17;
            pos++;
            if (len > 0 && pos + len <= size)
            {
                do_write(data + pos, len);
                pos += len;
            }
            if (i % 3 == 2)
            {
                do_read(data[pos % size] % 17);
            }
        }
        break;
    }

    case OP_STRESS_MEDIUM:
    {
        uint8_t num_ops = (data[off] % 17) + 1;
        size_t pos = off + 1;
        for (uint8_t i = 0; i < num_ops && pos < size; i++)
        {
            size_t write_len = data[pos] % 129;
            pos++;
            if (write_len > 0 && pos + write_len <= size)
            {
                do_write(data + pos, write_len);
                pos += write_len;
            }

            size_t read_len = data[pos % size] % 65;
            do_read(read_len);
        }
        break;
    }

    case OP_STRESS_LARGE:
    {
        uint8_t num_ops = (data[off] % 9) + 1;
        size_t pos = off + 1;
        for (uint8_t i = 0; i < num_ops && pos < size; i++)
        {
            size_t write_len = data[pos] % 513;
            pos++;
            if (write_len > 0 && pos + write_len <= size)
            {
                do_write(data + pos, write_len);
                pos += write_len;
            }

            if (i % 2 == 1)
            {
                size_t read_len = data[pos % size] % 257;
                do_read(read_len);
            }
        }
        break;
    }

#ifndef RINGBUF_MPMC
    case OP_CORRUPTION_INJECTION:
    {
        if (off + 2 <= size)
        {
            uint8_t num_writes = (data[off] % 5) + 1;
            uint8_t num_reads = (data[off + 1] % 5) + 1;
            size_t pos = off + 2;

            for (uint8_t i = 0; i < num_writes && pos < size; i++)
            {
                size_t len = data[pos] % 65;
                pos++;
                if (len > 0 && pos + len <= size)
                {
                    do_write(data + pos, len);
                    pos += len;
                }
            }

            uint8_t saved_buffer[sizeof(buffer)];
            memcpy(saved_buffer, buffer, sizeof(buffer));

            size_t corrupt_pos = data[pos % min_size(size, 4096)] % sizeof(buffer);
            size_t corrupt_len = data[(pos + 1) % min_size(size, 4096)] % 65;
            if (corrupt_len > 0 && corrupt_pos + corrupt_len <= sizeof(buffer) && pos + 2 + corrupt_len <= size)
            {
                memcpy(buffer + corrupt_pos, data + pos + 2, corrupt_len);
            }

            if (off + 3 < size)
            {
                uint8_t corrupt_write[65];
                size_t write_len = data[off + 2] % 65;
                if (write_len > 0 && off + 3 + write_len <= size && off + 2 + write_len <= size)
                {
                    memcpy(corrupt_write, data + off + 3, write_len);
                    for (size_t j = 0; j < write_len; j++)
                    {
                        corrupt_write[j] ^= data[(pos + j + 1) % min_size(size, 4096)];
                    }
                    do_write(corrupt_write, write_len);
                }
            }

            for (uint8_t i = 0; i < num_reads; i++)
            {
                do_read(data[(off + i + 1) % min_size(size, 256)] % 65);
            }

            if (off + 2 < size)
            {
                do_read(data[off + 2] % 129);
            }

            memcpy(buffer, saved_buffer, sizeof(buffer));
        }
        break;
    }
#endif

    case OP_STRERR:
    {
        ringbuf_strerr((ringbuf_err_t)(off < size ? data[off] : 0));
        break;
    }

    default:
        break;
    }

    return 0;
}
