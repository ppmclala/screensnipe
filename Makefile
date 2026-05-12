CC      = gcc
CFLAGS  = -O2 -Wall -Wextra -I/usr/include/freetype2
LDFLAGS = -lX11 -lXext -lXft

all: border-overlay

border-overlay: border-overlay.c
	$(CC) $(CFLAGS) -o border-overlay border-overlay.c $(LDFLAGS)

clean:
	rm -f border-overlay
