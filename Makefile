CC=gcc
CFLAGS=-g -Wall

backup: backup.c
	$(CC) $(CFLAGS) -o backup backup.c

clean:
	rm -f backup
