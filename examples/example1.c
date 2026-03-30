#define RINGBUF_IMPLEMENTATION
#include "ringbuf.h"

#include <stdint.h>
#include <memory.h>
#include <stdio.h>

int main(void)
{
    uint8_t buffer[8192];
    struct ringbuf rb;

    // make sure the buffer is zero'd before any ringbuf functions get called
    memset(buffer, 0, sizeof(buffer));

    ringbuf_init(&rb, buffer, sizeof(buffer));

    // Write data
    uint8_t data[] = "Hello, world!";
    ringbuf_write(&rb, data, sizeof(data));

    // Read data
    uint8_t out[256];
    size_t out_len = sizeof(out);
    ringbuf_err_t err = ringbuf_read(&rb, out, &out_len);
    assert(err == RbSuccess);

    // validate out is "Hello, world!"
    assert(out_len == sizeof(data));
    assert(memcmp(out, data, out_len) == 0);
    printf("success\n");

    return 0;
}