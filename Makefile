CXX      := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -O2 -Iinclude
CORE     := src/cpu.cpp src/assembler.cpp src/tracer.cpp
PIPE     := src/pipeline.cpp src/cache.cpp
COMP     := src/compiler.cpp
TARGET   := cpuemu

# --- scalar emulator (base) -----------------------------------------
$(TARGET): $(CORE) src/main.cpp
	$(CXX) $(CXXFLAGS) $(CORE) src/main.cpp -o $(TARGET)

run: $(TARGET)
	./$(TARGET) examples/asm/fibonacci.asm

step: $(TARGET)
	./$(TARGET) examples/asm/gcd.asm --step

gcd: $(TARGET)
	./$(TARGET) examples/asm/gcd.asm

# --- pipelined emulator ---------------------------------------------
pipe: $(CORE) $(PIPE) src/pipemain.cpp
	$(CXX) $(CXXFLAGS) $(CORE) $(PIPE) src/pipemain.cpp -o pipeemu
	./pipeemu examples/asm/fibonacci.asm

# --- cache report ---------------------------------------------------
cache: $(CORE) $(PIPE) src/cachemain.cpp
	$(CXX) $(CXXFLAGS) $(CORE) $(PIPE) src/cachemain.cpp -o cacheemu
	./cacheemu examples/asm/arraysum.asm

# --- MiniC compiler -------------------------------------------------
compiler: $(CORE) $(PIPE) $(COMP) src/compmain.cpp
	$(CXX) $(CXXFLAGS) $(CORE) $(PIPE) $(COMP) src/compmain.cpp -o minicc
	./minicc examples/minic/factorial.mc

# --- tests ----------------------------------------------------------
test: $(CORE) tests/test_cpu.cpp
	$(CXX) $(CXXFLAGS) $(CORE) tests/test_cpu.cpp -o run_tests
	./run_tests

test-pipe: $(CORE) $(PIPE) tests/test_pipeline.cpp
	$(CXX) $(CXXFLAGS) $(CORE) $(PIPE) tests/test_pipeline.cpp -o run_pipe_tests
	./run_pipe_tests

test-cache: $(CORE) $(PIPE) tests/test_cache.cpp tests/test_cache_pipeline.cpp
	$(CXX) $(CXXFLAGS) src/cache.cpp tests/test_cache.cpp -o run_cache_tests
	./run_cache_tests
	$(CXX) $(CXXFLAGS) $(CORE) $(PIPE) tests/test_cache_pipeline.cpp -o run_cache_pipe_tests
	./run_cache_pipe_tests

test-compiler: $(CORE) $(COMP) tests/test_compiler.cpp
	$(CXX) $(CXXFLAGS) $(CORE) $(COMP) tests/test_compiler.cpp -o run_comp_tests
	./run_comp_tests

# run every test suite
test-all: test test-pipe test-cache test-compiler

# --- benchmark ------------------------------------------------------
bench: $(CORE) src/bench.cpp
	$(CXX) $(CXXFLAGS) $(CORE) src/bench.cpp -o run_bench
	./run_bench

clean:
	rm -f $(TARGET) pipeemu cacheemu minicc run_tests run_pipe_tests run_cache_tests run_cache_pipe_tests run_comp_tests run_bench trace.csv trace_plot.png

.PHONY: run step gcd pipe cache compiler test test-pipe test-cache test-compiler test-all bench clean
