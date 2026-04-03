CC = clang
CFLAGS_COMMON = -g -O1 -Wall -Wextra

SANITIZER ?= address

CFLAGS_asan = $(CFLAGS_COMMON) -fsanitize=address,fuzzer
CFLAGS_msan = $(CFLAGS_COMMON) -fsanitize=memory,fuzzer
CFLAGS_ubsan = $(CFLAGS_COMMON) -fsanitize=undefined,fuzzer
CFLAGS_lsan = $(CFLAGS_COMMON) -fsanitize=leak,fuzzer

CFLAGS_cov = $(CFLAGS_COMMON) -fprofile-instr-generate -fcoverage-mapping -fsanitize=address,fuzzer

CORPUS_asan = corpus_asan
CORPUS_msan = corpus_msan
CORPUS_ubsan = corpus_ubsan
CORPUS_lsan = corpus_lsan

FUZZER_asan = fuzz_ringbuf_asan
FUZZER_msan = fuzz_ringbuf_msan
FUZZER_ubsan = fuzz_ringbuf_ubsan
FUZZER_lsan = fuzz_ringbuf_lsan
FUZZER_tsan = fuzz_ringbuf_tsan
FUZZER_tsan_mpmc = fuzz_ringbuf_tsan_mpmc
FUZZER_cov = fuzz_ringbuf_cov

all: $(FUZZER_asan) $(FUZZER_msan) $(FUZZER_ubsan) $(FUZZER_lsan) $(FUZZER_cov) $(FUZZER_tsan) $(FUZZER_tsan_mpmc)

$(FUZZER_asan): fuzz_ringbuf.c ../ringbuf.h
	$(CC) $(CFLAGS_asan) -o $@ $<

$(FUZZER_msan): fuzz_ringbuf.c ../ringbuf.h
	$(CC) $(CFLAGS_msan) -o $@ $<

$(FUZZER_ubsan): fuzz_ringbuf.c ../ringbuf.h
	$(CC) $(CFLAGS_ubsan) -o $@ $<

$(FUZZER_lsan): fuzz_ringbuf.c ../ringbuf.h
	$(CC) $(CFLAGS_lsan) -o $@ $<

$(FUZZER_cov): fuzz_ringbuf.c ../ringbuf.h
	$(CC) $(CFLAGS_cov) -o $@ $<

$(FUZZER_tsan): test_tsan.c ../ringbuf.h
	$(CC) $(CFLAGS_COMMON) -fsanitize=thread -fPIE -pie -o $@ $< -pthread

$(FUZZER_tsan_mpmc): test_tsan_mpmc.c ../ringbuf.h
	$(CC) $(CFLAGS_COMMON) -fsanitize=thread -fPIE -pie -o $@ $< -pthread

$(CORPUS_asan) $(CORPUS_msan) $(CORPUS_ubsan) $(CORPUS_lsan):
	mkdir -p $@

run: $(FUZZER_asan) $(FUZZER_msan) $(FUZZER_ubsan) $(FUZZER_lsan) $(CORPUS_asan) $(CORPUS_msan) $(CORPUS_ubsan) $(CORPUS_lsan)
	@echo "Running all sanitizers..."
	@bash -c '\
		./$(FUZZER_asan) $(CORPUS_asan) -max_len=65536 &\
		./$(FUZZER_msan) $(CORPUS_msan) -max_len=65536 &\
		./$(FUZZER_ubsan) $(CORPUS_ubsan) -max_len=65536 &\
		./$(FUZZER_lsan) $(CORPUS_lsan) -max_len=65536 &\
		trap '"'"'jobs -p | xargs -r kill'"'"' EXIT INT TERM;\
		wait'

run_asan: $(FUZZER_asan) $(CORPUS_asan)
	./$(FUZZER_asan) $(CORPUS_asan) -max_len=65536 -jobs=4 -workers=4

run_msan: $(FUZZER_msan) $(CORPUS_msan)
	./$(FUZZER_msan) $(CORPUS_msan) -max_len=65536 -jobs=4 -workers=4

run_ubsan: $(FUZZER_ubsan) $(CORPUS_ubsan)
	./$(FUZZER_ubsan) $(CORPUS_ubsan) -max_len=65536 -jobs=4 -workers=4

run_lsan: $(FUZZER_lsan) $(CORPUS_lsan)
	./$(FUZZER_lsan) $(CORPUS_lsan) -max_len=65536 -jobs=4 -workers=4

run_tsan: $(FUZZER_tsan)
	./fuzz_ringbuf_tsan

run_tsan_mpmc: $(FUZZER_tsan_mpmc)
	./fuzz_ringbuf_tsan_mpmc

cov_report: $(FUZZER_cov) $(CORPUS_asan)
	LLVM_PROFILE_FILE="cov.profraw" ./$(FUZZER_cov) $(CORPUS_asan) -max_len=65536 -runs=0
	llvm-profdata-18 merge cov.profraw -o cov.profdata 2>/dev/null || true
	@echo "=== Overall Coverage ==="
	llvm-cov-18 report ./$(FUZZER_cov) -instr-profile=cov.profdata 2>/dev/null || echo "Install llvm-tools"
	@echo ""
	@echo "=== ringbuf.h Coverage ==="
	llvm-cov-18 report ./$(FUZZER_cov) -instr-profile=cov.profdata ../ringbuf.h 2>/dev/null || true

cov_show: $(FUZZER_cov) $(CORPUS_asan)
	LLVM_PROFILE_FILE="cov.profraw" ./$(FUZZER_cov) $(CORPUS_asan) -max_len=65536 -runs=0
	llvm-profdata-18 merge cov.profraw -o cov.profdata 2>/dev/null || true
	llvm-cov-18 show ./$(FUZZER_cov) -instr-profile=cov.profdata --show-line-counts-or-regions 2>/dev/null || echo "Install llvm-tools"

clean_cov:
	rm -f cov.profraw cov.profdata

clean:
	rm -f $(FUZZER_asan) $(FUZZER_msan) $(FUZZER_ubsan) $(FUZZER_lsan) $(FUZZER_tsan) $(FUZZER_tsan_mpmc) $(FUZZER_cov) $(AFL_FUZZER_asan) $(AFL_FUZZER_ubsan) fuzz-* slow-unit-*
	rm -rf corpus_asan corpus_msan corpus_ubsan corpus_lsan corpus_tsan $(AFL_CORPUS_asan) $(AFL_CORPUS_ubsan) crash-* hang-*
	rm -rf afl_output_asan afl_output_ubsan
	rm -f cov.profraw cov.profdata default.profraw

AFL_DIR ?= /usr
AFLCC = $(AFL_DIR)/bin/afl-clang-fast
AFLCXX = $(AFL_DIR)/bin/afl-clang-fast++

AFL_CFLAGS = -g -O1 -Wall -Wextra -fsanitize=address
AFL_CFLAGS += -DAFL_HAVE_UINT64_TYPEDEF

AFL_FUZZER_asan = fuzz_ringbuf_afl_asan
AFL_FUZZER_ubsan = fuzz_ringbuf_afl_ubsan
AFL_CORPUS_asan = corpus_afl_asan
AFL_CORPUS_ubsan = corpus_afl_ubsan

afl_all: $(AFL_FUZZER_asan) $(AFL_FUZZER_ubsan)

$(AFL_FUZZER_asan): fuzz_ringbuf_afl.c ../ringbuf.h
	CC=$(AFLCC) CXX=$(AFLCXX) $(AFLCC) $(AFL_CFLAGS) -o $@ $<

$(AFL_FUZZER_ubsan): fuzz_ringbuf_afl.c ../ringbuf.h
	CC=$(AFLCC) CXX=$(AFLCXX) $(AFLCC) $(AFL_CFLAGS) -fsanitize=undefined -o $@ $<

$(AFL_CORPUS_asan) $(AFL_CORPUS_ubsan):
	mkdir -p $@

run_afl_asan: $(AFL_FUZZER_asan) $(AFL_CORPUS_asan)
	@if [ -z "$$(ls -A $(AFL_CORPUS_asan) 2>/dev/null)" ]; then \
		echo "Creating seed corpus..."; \
		mkdir -p $(AFL_CORPUS_asan); \
		printf '\\x00\\x01\\x00' > $(AFL_CORPUS_asan)/seed; \
	fi
	afl-fuzz -i $(AFL_CORPUS_asan) -o afl_output_asan -- ./$(AFL_FUZZER_asan) @@

run_afl_ubsan: $(AFL_FUZZER_ubsan) $(AFL_CORPUS_ubsan)
	@if [ -z "$$(ls -A $(AFL_CORPUS_ubsan) 2>/dev/null)" ]; then \
		echo "Creating seed corpus..."; \
		mkdir -p $(AFL_CORPUS_ubsan); \
		printf '\\x00\\x01\\x00' > $(AFL_CORPUS_ubsan)/seed; \
	fi
	afl-fuzz -i $(AFL_CORPUS_ubsan) -o afl_output_ubsan -- ./$(AFL_FUZZER_ubsan) @@

run_afl_asan_parallel: $(AFL_FUZZER_asan) $(AFL_CORPUS_asan)
	@if [ -z "$$(ls -A $(AFL_CORPUS_asan) 2>/dev/null)" ]; then \
		echo "Creating seed corpus..."; \
		mkdir -p $(AFL_CORPUS_asan); \
		printf '\\x00\\x01\\x00' > $(AFL_CORPUS_asan)/seed; \
	fi
	afl-fuzz -i $(AFL_CORPUS_asan) -o afl_output_asan -M afl_primary_asan -- ./$(AFL_FUZZER_asan) @@
	afl-fuzz -i $(AFL_CORPUS_asan) -o afl_output_asan -S afl_secondary_asan -- ./$(AFL_FUZZER_asan) @@
