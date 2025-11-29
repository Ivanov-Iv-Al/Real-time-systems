CC = gcc
CFLAGS = -Wall -Wextra -std=c99
TARGETS = read_input poll_inputs

all: $(TARGETS)

read_input: read_input.c
	$(CC) $(CFLAGS) -o $@ $<

poll_inputs: poll_inputs.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f $(TARGETS)

install: all
	@echo "Binaries compiled successfully"

.PHONY: all clean install