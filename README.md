# ringbuf

A single-header, lock-free **SPSC** (Single Producer Single Consumer) ring buffer written in C99+. Designed for fast thread-to-thread and cross-process shared memory communication.

## Features

- **Single header** - drop `ringbuf.h` into your project
- **Lock-free** - uses atomics for thread-safe operations without locks
- **Cache-line padded** - head/tail separated to prevent false sharing
- **Variable-length data** - automatically prefixes writes with size metadata
- **Optional statistics** - enable with `#define RINGBUF_STATISTICS` for timing metrics
- **Shared memory compatible** - works with memory-mapped buffers
- **Highly portable** - works with any C99/C11 compiler on architectures including x86, x86-64, ARM, ARM64, RISC-V, and more

## Portability

By default, ringbuf uses cache-line padding optimized for most architectures. You can customize this:

- `RINGBUF_CACHE_LINE_SIZE` - override the cache line size (default: 64)
- `RINGBUF_ALIGNMENT` - override the alignment boundary (default: 0x1000)

## Quick Start

### 1. Add to your project

Copy `ringbuf.h` into your project or include it directly.

### 2. Define the implementation

Add `#define RINGBUF_IMPLEMENTATION` before including the header in one source file:

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
```

## Optional Statistics

Define `RINGBUF_STATISTICS` before including the header to enable timing statistics:

```c
#define RINGBUF_STATISTICS
#define RINGBUF_IMPLEMENTATION
#include "ringbuf.h"
```

Functions available:
- `ringbuf_get_stats(rb, out)` - get all statistics
- `ringbuf_avg_write_ns(rb)` - average write time in nanoseconds
- `ringbuf_avg_read_ns(rb)` - average read time in nanoseconds

## License

Licensed under the Apache License, Version 2.0. See [LICENSE](LICENSE) for details.
