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
FUZZER_cov = fuzz_ringbuf_cov

all: $(FUZZER_asan) $(FUZZER_msan) $(FUZZER_ubsan) $(FUZZER_lsan) $(FUZZER_cov) $(FUZZER_tsan)

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
	./test_tsan

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
	rm -f $(FUZZER_asan) $(FUZZER_msan) $(FUZZER_ubsan) $(FUZZER_lsan) $(FUZZER_tsan) $(FUZZER_cov) fuzz-* slow-unit-*
	rm -rf corpus_asan corpus_msan corpus_ubsan corpus_lsan corpus_tsan crash-* hang-*
	rm -f cov.profraw cov.profdata default.profraw
