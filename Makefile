CFLAGS = -Wall -O2 -std=gnu99 -D_POSIX_C_SOURCE=200112L -D_DEFAULT_SOURCE -I/usr/include/freetype2 `pkg-config --cflags x11 xft fontconfig`
LIBS = -lX11 -lXext -lXft -lfontconfig -lfreetype -lm

diamondwm: diamondwm.c
	gcc $(CFLAGS) -o diamondwm diamondwm.c $(LIBS)

clean:
	rm -f diamondwm

install: diamondwm
	cp diamondwm /usr/local/bin/

.PHONY: clean install
