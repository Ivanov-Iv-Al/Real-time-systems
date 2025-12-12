CC = gcc
CFLAGS = -Wall -Wextra -std=gnu11 -D_POSIX_C_SOURCE=199309L -D_GNU_SOURCE -I./src
LDFLAGS = -lrt

.PHONY: all clean

all: task1_latency task2_mlock task3_benchmark

task1_latency: src/task1_latency.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

task2_mlock: src/task2_mlock.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

task3_benchmark: src/task3_benchmark.c src/mempool.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

run_task1:
	./task1_latency

run_task2:
	sudo ./task2_mlock

run_task3:
	sudo ./task3_benchmark

clean:
	rm -f task1_latency task2_mlock task3_benchmark