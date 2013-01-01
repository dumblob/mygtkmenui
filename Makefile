CC = gcc
CFLAGS = -Wall -lm \
	-DG_DISABLE_DEPRECATED \
	-DGDK_DISABLE_DEPRECATED \
	-DGDK_PIXBUF_DISABLE_DEPRECATED \
	-DGTK_DISABLE_DEPRECATED
BIN = mygtkmenui
GKT = `pkg-config --cflags --libs gtk+-3.0`
#GKT = `pkg-config --cflags --libs gtk+-2.0`

all: binary

binary: main.c
	$(CC) main.c -o $(BIN) $(CFLAGS) $(GKT)
	strip -s $(BIN)

clean:
	rm -f *.o $(BIN)
