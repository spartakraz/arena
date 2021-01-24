all: build_program

build_program: allocator.c
	gcc -g -std=gnu99 -D_GNU_SOURCE=1 -Wall -Wfatal-errors allocator.c -o allocator -lpthread
