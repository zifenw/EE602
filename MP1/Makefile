CC = gcc
CFLAGS = -Wall -Wextra

all: echo echos

echo: echo.c writen.c readline.c mp1.h
	$(CC) $(CFLAGS) -o echo echo.c writen.c readline.c

echos: echos.c writen.c mp1.h
	$(CC) $(CFLAGS) -o echos echos.c writen.c

clean:
	rm -f echo echos *.o