#define MPMC_RINGBUF_IMPLEMENTATION
#include "mpmc_ringbuf.h"

#include <stdint.h>
#include <memory.h>
#include <stdio.h>

int main(void)
{
    uint8_t buffer[8192];
    struct mpmc_ringbuf rb;

    // make sure the buffer is zero'd before any mpmc_ringbuf functions get called
    memset(buffer, 0, sizeof(buffer));

    mpmc_ringbuf_init(&rb, buffer, sizeof(buffer));

    // Write data
    uint8_t data[] = "Hello, world!";
    mpmc_ringbuf_write(&rb, data, sizeof(data));

    // Read data
    uint8_t out[256];
    size_t out_len = sizeof(out);
    mpmc_ringbuf_err_t err = mpmc_ringbuf_read(&rb, out, &out_len);
    assert(err == RbSuccess);

    // validate out is "Hello, world!"
    assert(out_len == sizeof(data));
    assert(memcmp(out, data, out_len) == 0);
    printf("success\n");

    return 0;
}