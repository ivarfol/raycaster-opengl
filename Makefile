linux: raycast.c ppmparser.c mapparser.c tri.c
	gcc raycast.c ppmparser.c mapparser.c tri.c -o ray -lglut -lGL -lGLU -lm

linuxdebug: raycast.c ppmparser.c mapparser.c tri.c
	gcc raycast.c ppmparser.c mapparser.c tri.c -o ray -lglut -lGL -lGLU -lm -g
