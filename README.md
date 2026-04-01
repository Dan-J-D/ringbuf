# ringbuf

A single-header, lock-free ring buffer written in C99+. Designed for fast thread-to-thread and cross-process shared memory communication.

## Variants

- **SPSC** (Single Producer Single Consumer) - `ringbuf.h`
- **MPMC** (Multi Producer Multi Consumer) - `mpmc_ringbuf.h`

The MPMC variant is identical to SPSC, except:
- All symbols are prefixed with `mpmc_` or `MPMC_`
- The internal buffer header is 2x larger (256 bytes with default cache line vs 128 bytes) due to additional head_pending and tail_pending fields

## Features

- **Single header** - drop `ringbuf.h` or `mpmc_ringbuf.h` into your project
- **Lock-free** - uses atomics for thread-safe operations without locks
- **Cache-line padded** - head/tail separated to prevent false sharing
- **Variable-length data** - automatically prefixes writes with size metadata
- **Big & Little Endianess** - works with both big & little endian systems
- **Optional statistics** - enable with `#define RINGBUF_STATISTICS` (SPSC) or `#define MPMC_RINGBUF_STATISTICS` (MPMC) for timing metrics
- **Shared memory compatible** - works with memory-mapped buffers
- **Highly portable** - works with any C99+ compiler on architectures including x86, x86-64, ARM, ARM64, RISC-V, and more
    - **Important** - on some non x86/x86-64 based systems (like ARM based systems) they might have cache Lines that are not 64 bytes, its very important to set `RINGBUF_CACHE_LINE_SIZE`/`MPMC_RINGBUF_CACHE_LINE_SIZE` and `RINGBUF_ALIGNMENT`/`MPMC_RINGBUF_ALIGNMENT` correctly

## Portability

By default, ringbuf uses cache-line padding optimized for most architectures. You can customize this:

- `RINGBUF_CACHE_LINE_SIZE` / `MPMC_RINGBUF_CACHE_LINE_SIZE` - override the cache line size (default: 64)
- `RINGBUF_ALIGNMENT` / `MPMC_RINGBUF_ALIGNMENT` - override the alignment boundary (default: 0x1000)

## Quick Start

### 1. Add to your project

Copy `ringbuf.h` (SPSC) or `mpmc_ringbuf.h` (MPMC) into your project.

### 2. Define the implementation

Add `#define RINGBUF_IMPLEMENTATION` (SPSC) or `#define MPMC_RINGBUF_IMPLEMENTATION` (MPMC) before including the header in one source file:

```c
#define RINGBUF_IMPLEMENTATION
#include "ringbuf.h"
```

### 3. Initialize and use

```c
#define RINGBUF_IMPLEMENTATION
#include "ringbuf.h"

#include <stdint.h>
#include <memory.h>
#include <stdio.h>

int main(void)
{
    uint8_t buffer[8192];
    struct ringbuf rb;

    memset(buffer, 0, sizeof(buffer));

    ringbuf_init(&rb, buffer, sizeof(buffer));

    uint8_t data[] = "Hello, world!";
    ringbuf_write(&rb, data, sizeof(data));

    uint8_t out[256];
    size_t out_len = sizeof(out);
    ringbuf_err_t err = ringbuf_read(&rb, out, &out_len);
    assert(err == RbSuccess);

    assert(out_len == sizeof(data));
    assert(memcmp(out, data, out_len) == 0);
    printf("success\n");

    return 0;
}
```

## Optional Statistics

Define `RINGBUF_STATISTICS` (SPSC) or `MPMC_RINGBUF_STATISTICS` (MPMC) before including the header to enable timing statistics:

```c
#define RINGBUF_STATISTICS
#define RINGBUF_IMPLEMENTATION
#include "ringbuf.h"
```

Functions available:
- `ringbuf_get_stats(rb, out)` / `mpmc_ringbuf_get_stats(rb, out)` - get all statistics
- `ringbuf_avg_write_ns(rb)` / `mpmc_ringbuf_avg_write_ns(rb)` - average write time in nanoseconds
- `ringbuf_avg_read_ns(rb)` / `mpmc_ringbuf_avg_read_ns(rb)` - average read time in nanoseconds

## License

Licensed under the Apache License, Version 2.0. See [LICENSE](LICENSE) for details.
