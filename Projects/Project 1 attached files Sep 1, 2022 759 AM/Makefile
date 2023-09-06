## [M1]

#  Explaint the following here

#We are setting the current directory to the realpath to the server.c and client.c

#  ...

CUR_DIR := $(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))



## [M2]

#  Explaint the following here

#  we are setting the compiler as gcc with the flags given below. these will be used to build the environment for compiliation. 

CC = gcc

CFLAGS += -g -O2 -Werror -Wall

LDFLAGS +=



## [M3] -- REMOVED

#  ...



## [M5]

#  Explaint the following here

#  SRC_FILES are the files which end with .c so server.c and client.c in this case. these are out source code files

SRC_FILES = $(wildcard *.c)



## [M6]

#  Explaint the following here

#  these are the executables made on compiling the src files. 

EXE_FILES = $(SRC_FILES:.c=)



## [M7]

#  Explaint the following here

#  Execute all the executable files. this is the default option

all: $(EXE_FILES)



## [M8] -- UPDATED

#  Explaint the following here

#  The compilation command using the compiler, the compiling options and the object files. 

%:%.c

	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< 



## [M9]

#  Explaint the following here

#  removing the executable files

clean:

	rm -f $(EXE_FILES)



## [M10]

#  Explaint the following here

#  it will delete all the object files etc...

.PHONY: all clean