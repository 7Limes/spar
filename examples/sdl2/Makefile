all: spar-sdl2

spar-sdl2: spar-sdl2.c ../../spar.h
	gcc spar-sdl2.c \
		-lSDL2 -lSDL2_image -lm \
		-o spar_sdl2

clean:
	rm -f spar_sdl2