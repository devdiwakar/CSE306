CUR_DIR := $(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))

CC = gcc
CFLAGS += -g -O2 -Werror -Wall
LDFLAGS +=
SRC_FILES = $(wildcard *.c)

EXE_FILES = $(SRC_FILES:.c=)

all: $(EXE_FILES)

%:%.c
	$(CC) -o $@ $< 

clean:
	rm -f $(EXE_FILES)

.PHONY: all clean
