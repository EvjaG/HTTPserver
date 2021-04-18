all: threadpool.c server.c
	gcc -g -Wall server.c -o server -lpthread
all-GDB: threadpool.c server.c
	gcc -g -Wall server.c -o server -lpthread
#-fno-stack-protector