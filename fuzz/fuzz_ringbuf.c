#define RINGBUF_IMPLEMENTATION
#include "../ringbuf.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static int initialized = 0;
static struct ringbuf rb;
static uint8_t buffer[65536];

static void reset_ringbuf(void)
{
    memset(buffer, 0, sizeof(buffer));
    memset(&rb, 0, sizeof(rb));
    ringbuf_init(&rb, buffer, sizeof(buffer));
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size < 1)
        return 0;

    if (!initialized)
    {
        reset_ringbuf();
        initialized = 1;
    }

    uint8_t op = data[0];
    op %= 16;

    if (op == 0)
    {
        reset_ringbuf();
        return 0;
    }

    if (op == 1 && size >= 2)
    {
        uint8_t len = data[1] % 129;
        if (len > 0 && size >= 2 + len)
        {
            ringbuf_write(&rb, data + 2, len);
        }
    }

    if (op == 2 && size >= 2)
    {
        size_t out_len = data[1] % 257;
        if (out_len > 0)
        {
            uint8_t out[257];
            ringbuf_err_t err = ringbuf_read(&rb, out, &out_len);
            (void)err;
        }
    }

    if (op == 3 && size >= 2)
    {
        uint8_t num_ops = data[1] % 17;
        size_t off = 2;
        for (uint8_t i = 0; i < num_ops && off + 1 < size; i++)
        {
            uint8_t len = data[off] % 65;
            if (len > 0 && off + 1 + len <= size)
            {
                ringbuf_write(&rb, data + off + 1, len);
            }
            off += 1 + len;
        }
    }

    if (op == 4 && size >= 3)
    {
        uint8_t write_len = data[1] % 33;
        uint8_t read_len = data[2] % 65;
        size_t off = 3;
        if (write_len > 0 && off + write_len <= size)
        {
            ringbuf_write(&rb, data + off, write_len);
            off += write_len;
        }
        if (read_len > 0 && off + read_len <= size)
        {
            size_t OL = read_len;
            uint8_t out[65];
            ringbuf_read(&rb, out, &OL);
        }
    }

    if (op == 5 && size >= 3)
    {
        uint8_t num_cycles = data[1] % 17;
        size_t off = 2;
        for (uint8_t i = 0; i < num_cycles && off + 130 < size; i++)
        {
            uint8_t write_len = data[off] % 129;
            uint8_t read_len = data[off + 1] % 129;
            off += 2;

            if (write_len > 0)
            {
                ringbuf_write(&rb, data + off, write_len);
            }
            off += write_len;

            if (read_len > 0)
            {
                size_t OL = read_len;
                uint8_t out[129];
                ringbuf_err_t err = ringbuf_read(&rb, out, &OL);
                (void)err;
            }
        }
    }

    if (op == 6 && size >= 2)
    {
        uint8_t num_writes = data[1] % 65;
        size_t off = 2;
        for (uint8_t i = 0; i < num_writes && off + 129 < size; i++)
        {
            uint8_t len = data[off] % 129;
            if (len > 0 && off + 1 + len <= size)
            {
                ringbuf_write(&rb, data + off + 1, len);
            }
            off += 1 + len;
        }
    }

    if (op == 7)
    {
        ringbuf_strerr(RbSuccess);
        ringbuf_strerr(RbNotEnoughSpace);
        ringbuf_strerr(RbEmpty);
        ringbuf_strerr(RbBufferTooSmall);
        ringbuf_strerr(RbCorrupt);
    }

    if (op == 8 && size >= 2)
    {
        uint8_t tiny_buf[64];
        struct ringbuf tiny_rb;
        uint8_t tiny_len = data[1] % 63 + 1;
        ringbuf_err_t err = ringbuf_init(&tiny_rb, tiny_buf, tiny_len);
        (void)err;
    }

    if (op == 9 && size >= 3)
    {
        uint8_t misaligned_buf[128];
        uintptr_t align_offset = data[1] % 16 + 1;
        uint8_t small_buf_size = data[2] % 32 + 1;
        uint8_t *misaligned = misaligned_buf + align_offset;
        struct ringbuf mis_rb;
        ringbuf_init(&mis_rb, misaligned, small_buf_size);
    }

    if (op == 10)
    {
        for (int i = 0; i < 5000; i++)
        {
            uint8_t wdata[64];
            for (int j = 0; j < (int)sizeof(wdata); j++)
                wdata[j] = (uint8_t)(i ^ j);
            ringbuf_write(&rb, wdata, 64);
        }
    }

    if (op == 11 && size >= 5)
    {
        uint8_t len = data[1] % 16 + 1;
        if (len > 0 && size >= 2 + len)
        {
            ringbuf_write(&rb, data + 2, len);
            size_t read_len = 1;
            uint8_t tmp[1];
            ringbuf_read(&rb, tmp, &read_len);
            uint8_t corrupt_len[sizeof(size_t)];
            for (size_t i = 0; i < sizeof(size_t); i++)
                corrupt_len[i] = data[3 + i % (size - 4)];
            for (size_t i = 0; i < sizeof(size_t); i++)
                rb.buf->data[(rb.buf->tail + i) % rb.buf_data_size] = corrupt_len[i];
            size_t bad_len = 16;
            uint8_t out[16];
            ringbuf_read(&rb, out, &bad_len);
        }
    }

    if (op == 12 && size >= 3)
    {
        uint8_t write_sz = data[1] % 16 + 1;
        uint8_t read_sz = data[2] % 16 + 1;
        for (int i = 0; i < 200; i++)
        {
            uint8_t wdata[32];
            for (int j = 0; j < (int)sizeof(wdata); j++)
                wdata[j] = (uint8_t)(i ^ j);
            ringbuf_write(&rb, wdata, write_sz);
            if (i % 3 == 0)
            {
                size_t rlen = read_sz;
                uint8_t rbuf[32];
                ringbuf_read(&rb, rbuf, &rlen);
            }
        }
    }

    if (op == 13 && size >= 3)
    {
        uint8_t num_writes = data[1] % 8 + 1;
        uint8_t read_buf_size = data[2] % 8 + 1;
        size_t off = 3;
        for (uint8_t i = 0; i < num_writes && off + 16 <= size; i++)
        {
            ringbuf_write(&rb, data + off, 16);
            off += 16;
        }
        for (uint8_t i = 0; i < 3; i++)
        {
            uint8_t out[32];
            size_t cap = read_buf_size;
            ringbuf_read(&rb, out, &cap);
        }
    }

    if (op == 14)
    {
        for (int i = 0; i < 10000; i++)
        {
            uint8_t wdata[32];
            for (int j = 0; j < (int)sizeof(wdata); j++)
                wdata[j] = (uint8_t)(i ^ j);
            ringbuf_write(&rb, wdata, 32);
        }
    }

    if (op == 15)
    {
        uint8_t wdata[128];
        for (int j = 0; j < (int)sizeof(wdata); j++)
            wdata[j] = (uint8_t)(j);
        ringbuf_write(&rb, wdata, 128);
        size_t cap = 4;
        uint8_t out[4];
        ringbuf_read(&rb, out, &cap);
    }

    if (op == 16)
    {
        uint8_t tiny_buf[64];
        struct ringbuf tiny_rb;
        ringbuf_init(&tiny_rb, tiny_buf, sizeof(tiny_buf));
    }

    return 0;
}
