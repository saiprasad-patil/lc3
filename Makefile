SRC_FILES = src/lc3.c
CC_FLAGS = -Wall -Wextra -g -std=c11
CC = gcc

all:
	$(CC) $(SRC_FILES) $(CC_FLAGS) -o lc3 

clean:
	rm lc3
