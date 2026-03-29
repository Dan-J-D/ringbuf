CC = clang
CFLAGS_COMMON = -g -O1 -Wall -Wextra

SANITIZER ?= address

CFLAGS_asan = $(CFLAGS_COMMON) -fsanitize=address,fuzzer
CFLAGS_msan = $(CFLAGS_COMMON) -fsanitize=memory,fuzzer
CFLAGS_ubsan = $(CFLAGS_COMMON) -fsanitize=undefined,fuzzer
CFLAGS_lsan = $(CFLAGS_COMMON) -fsanitize=leak,fuzzer

CORPUS_asan = corpus_asan
CORPUS_msan = corpus_msan
CORPUS_ubsan = corpus_ubsan
CORPUS_lsan = corpus_lsan

FUZZER_asan = fuzz_ringbuf_asan
FUZZER_msan = fuzz_ringbuf_msan
FUZZER_ubsan = fuzz_ringbuf_ubsan
FUZZER_lsan = fuzz_ringbuf_lsan

all: $(FUZZER_asan) $(FUZZER_msan) $(FUZZER_ubsan) $(FUZZER_lsan)

$(FUZZER_asan): fuzz_ringbuf.c ../ringbuf.h
	$(CC) $(CFLAGS_asan) -o $@ $<

$(FUZZER_msan): fuzz_ringbuf.c ../ringbuf.h
	$(CC) $(CFLAGS_msan) -o $@ $<

$(FUZZER_ubsan): fuzz_ringbuf.c ../ringbuf.h
	$(CC) $(CFLAGS_ubsan) -o $@ $<

$(FUZZER_lsan): fuzz_ringbuf.c ../ringbuf.h
	$(CC) $(CFLAGS_lsan) -o $@ $<

$(CORPUS_asan) $(CORPUS_msan) $(CORPUS_ubsan) $(CORPUS_lsan):
	mkdir -p $@

run: $(FUZZER_asan) $(FUZZER_msan) $(FUZZER_ubsan) $(FUZZER_lsan) $(CORPUS_asan) $(CORPUS_msan) $(CORPUS_ubsan) $(CORPUS_lsan)
	@echo "Running all sanitizers..."
	./$(FUZZER_asan) $(CORPUS_asan) -max_len=65536 &
	./$(FUZZER_msan) $(CORPUS_msan) -max_len=65536 &
	./$(FUZZER_ubsan) $(CORPUS_ubsan) -max_len=65536 &
	./$(FUZZER_lsan) $(CORPUS_lsan) -max_len=65536 &
	wait

run_asan: $(FUZZER_asan) $(CORPUS_asan)
	./$(FUZZER_asan) $(CORPUS_asan) -max_len=65536 -jobs=4 -workers=4

run_msan: $(FUZZER_msan) $(CORPUS_msan)
	./$(FUZZER_msan) $(CORPUS_msan) -max_len=65536 -jobs=4 -workers=4

run_ubsan: $(FUZZER_ubsan) $(CORPUS_ubsan)
	./$(FUZZER_ubsan) $(CORPUS_ubsan) -max_len=65536 -jobs=4 -workers=4

run_lsan: $(FUZZER_lsan) $(CORPUS_lsan)
	./$(FUZZER_lsan) $(CORPUS_lsan) -max_len=65536 -jobs=4 -workers=4

clean:
	rm -f $(FUZZER_asan) $(FUZZER_msan) $(FUZZER_ubsan) $(FUZZER_lsan) fuzz-*
	rm -rf corpus_asan corpus_msan corpus_ubsan corpus_lsan crash-* hang-*
