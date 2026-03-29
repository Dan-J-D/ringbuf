#define RINGBUF_IMPLEMENTATION
#include "../ringbuf.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static int initialized = 0;
static struct ringbuf_t rb;
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

    if (!initialized) {
        reset_ringbuf();
        initialized = 1;
    }

    uint8_t op = data[0];

    if (op == 0) {
        reset_ringbuf();
        return 0;
    }

    if (op == 1 && size >= 2) {
        uint8_t len = data[1] % 129;
        if (size >= 2 + len) {
            ringbuf_write(&rb, data + 2, len);
        }
    }

    if (op == 2 && size >= 2) {
        size_t out_len = data[1] % 257;
        uint8_t out[257];
        ringbuf_err err = ringbuf_read(&rb, out, &out_len);
        (void)err;
    }

    if (op == 3 && size >= 2) {
        uint8_t num_ops = data[1] % 17;
        size_t off = 2;
        for (uint8_t i = 0; i < num_ops && off + 1 < size; i++) {
            uint8_t len = data[off] % 65;
            if (off + 1 + len <= size) {
                ringbuf_write(&rb, data + off + 1, len);
            }
            off += 1 + len;
        }
    }

    if (op == 4 && size >= 3) {
        uint8_t write_len = data[1] % 33;
        uint8_t read_len = data[2] % 65;
        size_t off = 3;
        if (off + write_len <= size) {
            ringbuf_write(&rb, data + off, write_len);
            off += write_len;
        }
        if (off + read_len <= size) {
            size_t OL = read_len;
            uint8_t out[65];
            ringbuf_read(&rb, out, &OL);
        }
    }

    return 0;
}
