CFLAGS += -W -Wall -g -O3
CFLAGS += `pkg-config --cflags sdl cairo poppler-glib`

TARGETS=nyanpresent

all: $(TARGETS)

summoned: nyanpresent.o cairosdl.o
	$(CC) -o $@ $+ `pkg-config --libs sdl cairo poppler-glib` `sdl-config --libs` `sdl-config --cflags` -lSDL_image

clean:
	$(RM) $(TARGETS)
	$(RM) *.o
	$(RM) *~
