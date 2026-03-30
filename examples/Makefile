EXAMPLE1_CFLAGS := -Wall -Wextra -Wpedantic -fPIE -I.. -ggdb
EXAMPLE2_CFLAGS := -Wall -Wextra -Wpedantic -fPIE -I.. -lpthread -m64

.PHONY := clean
all: example1 example2

example1: example1.c
	$(CC) $(EXAMPLE1_CFLAGS) example1.c -o example1

example2: example2.c
	$(CC) $(EXAMPLE2_CFLAGS) example2.c -o example2

clean:
	rm example1 example2
