EXAMPLE1_CFLAGS := -Wall -Wextra -Wpedantic -fPIE -I..
EXAMPLE2_CFLAGS := -Wall -Wextra -Wpedantic -fPIE -I.. -lpthread -m64

.PHONY := clean
all: example1 example2 mpmc_example1 mpmc_example2

example1: example1.c
	$(CC) $(EXAMPLE1_CFLAGS) example1.c -o example1

example2: example2.c
	$(CC) $(EXAMPLE2_CFLAGS) example2.c -o example2

mpmc_example1: mpmc_example1.c
	$(CC) $(EXAMPLE1_CFLAGS) mpmc_example1.c -o mpmc_example1

mpmc_example2: mpmc_example2.c
	$(CC) $(EXAMPLE2_CFLAGS) mpmc_example2.c -o mpmc_example2 -ggdb

clean:
	rm example1 example2 mpmc_example1 mpmc_example2
