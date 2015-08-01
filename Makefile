CC ?= gcc
CFLAGS = -Wall -lm \
	-DG_DISABLE_DEPRECATED \
	-DGDK_DISABLE_DEPRECATED \
	-DGDK_PIXBUF_DISABLE_DEPRECATED
#	-DGTK_DISABLE_DEPRECATED
BIN = mygtkmenui
GTK ?= `pkg-config --cflags --libs gtk+-3.0`
#GTK ?= `pkg-config --cflags --libs gtk+-2.0`

all: binary

binary: main.c
	$(CC) main.c -o $(BIN) $(CFLAGS) $(GTK)
	strip -s $(BIN)

clean:
	rm -f *.o $(BIN)
