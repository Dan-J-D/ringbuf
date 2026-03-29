# ringbuf

A single-header, lock-free **SPSC** (Single Producer Single Consumer) ring buffer written in C99/C11.

## Features

- **Single header** - drop `ringbuf.h` into your project
- **Lock-free** - uses C11 atomics for thread-safe operations without locks
- **Cache-line padded** - head/tail separated to prevent false sharing
- **Variable-length data** - automatically prefixes writes with size metadata
- **Optional statistics** - enable with `RINGBUF_STATISTICS` for timing metrics
- **Shared memory compatible** - works with memory-mapped buffers

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
#include "ringbuf.h"

uint8_t buffer[8192];
struct ringbuf_t rb;

// make sure the buffer is zero'd before any ringbuf functions get called
memset(buffer, 0, sizeof(buffer));

ringbuf_init(&rb, buffer, sizeof(buffer));

// Write data
uint8_t data[] = "Hello, world!";
ringbuf_write(&rb, data, sizeof(data));

// Read data
uint8_t out[256];
size_t out_len = sizeof(out);
ringbuf_err err = ringbuf_read(&rb, out, &out_len);
if (err == RbSuccess) {
    // out contains "Hello, world!"
}
```

## Error Codes

| Error | Description |
|-------|-------------|
| `RbSuccess` | Operation completed successfully |
| `RbOutOfMemory` | Buffer allocation failed (rare) |
| `RbNotEnoughSpace` | Not enough free space in buffer |
| `RbEmpty` | No data available to read |
| `RbBufferTooSmall` | Output buffer too small for read |
| `RbCorrupt` | Data corruption detected |

## API Reference

### `ringbuf_init`

```c
ringbuf_err ringbuf_init(struct ringbuf_t *rb, volatile void *buf, size_t buf_size);
```

Initializes a ring buffer using a pre-allocated buffer. The buffer must be:
- Zero-initialized
- At least 4KB + sizeof(struct buf_t)
- 4KB-aligned (handled automatically)

### `ringbuf_write`

```c
ringbuf_err ringbuf_write(struct ringbuf_t *RESTRICT rb,
                          const uint8_t *RESTRICT data,
                          const size_t data_len);
```

Writes data to the buffer. Returns `RbNotEnoughSpace` if insufficient space is available.

### `ringbuf_read`

```c
ringbuf_err ringbuf_read(struct ringbuf_t *RESTRICT rb,
                         uint8_t *RESTRICT out,
                         size_t *RESTRICT out_len);
```

Reads data from the buffer into `out`. On `RbBufferTooSmall`, `*out_len` contains the required size.

### `ringbuf_strerr`

```c
const char *ringbuf_strerr(ringbuf_err e);
```

Returns a string description of an error code.

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

## Thread Safety

The ring buffer uses C11 atomics with memory ordering:
- **Writes**: `memory_order_relaxed` for loading head, `memory_order_acquire` for loading tail, `memory_order_release` for storing head
- **Reads**: `memory_order_relaxed` for loading tail, `memory_order_acquire` for loading head, `memory_order_release` for storing tail

This allows a single producer and single consumer (SPSC) to operate concurrently without locks. For multi-producer or multi-consumer scenarios, you would need additional synchronization.

## Memory Layout

```
struct buf_t {
    atomic_size_t head;        // 64-byte cache line
    uint8_t pad_1[...];
    atomic_size_t tail;       // 64-byte cache line
    uint8_t pad_2[...];
    uint8_t data[];            // flexible array member
}
```

Each variable is padded to a full cache line (64 bytes) to prevent false sharing between head and tail.

## License

Licensed under the Apache License, Version 2.0. See [LICENSE](LICENSE) for details.
