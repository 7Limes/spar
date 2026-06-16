all: spargui

spargui: spargui.c ../../spar.h raygui.h da.h tinyfiledialogs.c
	gcc spargui.c tinyfiledialogs.c \
		-lraylib -lm \
		-o spargui

clean:
	rm spargui