CC = gcc
CFLAGS = -Wall -g -Iinclude

LDFLAGS =

all: folders orchestrator client

orchestrator: bin/orchestrator

client: bin/client

folders:
	@mkdir -p src include obj bin tmp

bin/orchestrator: obj/orchestrator.o
	$(CC) $(LDFLAGS) $^ -o $@

bin/client: src/client.c
	$(CC) $(CFLAGS) $^ -o $@

obj/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	@find obj -type f -delete
	@find tmp -type f -delete
	@find bin -type f -delete