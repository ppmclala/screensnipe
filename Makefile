CC      = gcc
CFLAGS  = -O2 -Wall -Wextra -I/usr/include/freetype2
PKG_CONFIG_PATH ?= /usr/lib/x86_64-linux-gnu/pkgconfig:/usr/share/pkgconfig

all: border-overlay border-overlay-wayland

border-overlay: border-overlay.c
	$(CC) $(CFLAGS) -o border-overlay border-overlay.c -lX11 -lXext -lXft

border-overlay-wayland: border-overlay-wayland.c
	$(CC) $(CFLAGS) -o border-overlay-wayland border-overlay-wayland.c \
		$(shell pkg-config --cflags --libs gtk+-3.0 gtk-layer-shell-0)

clean:
	rm -f border-overlay border-overlay-wayland
