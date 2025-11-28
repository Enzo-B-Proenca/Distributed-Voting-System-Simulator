CC = gcc
INCLUDE = include
SRC = src
CFLAGS = -I $(INCLUDE) -Wall -Wextra -O2 
ASAN = -fsanitize=address
TARGETS = server client


all: $(TARGETS)

WIP:
	client-wip server-wip

server-wip: 
	$(CC) $(SRC)/server.c -pthread $(CFLAGS) $(ASAN) -o server-test 

.PHONY: clean all WIP

clean:
	rm -f $(TARGETS)

