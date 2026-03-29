CFLAGS := -Wall -Wextra -Wpedantic -fPIE -I.. -lpthread -m64

.PHONY := clean
main: main.c
	$(CC) $(CFLAGS) main.c -o main

clean:
	rm main
