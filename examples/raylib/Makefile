all: spar-raylib

spar-raylib: spar-raylib.c spar.h
	gcc spar-raylib.c \
		-lraylib -lGL -lm \
		-o spar_raylib

clean:
	rm -f spar_raylib