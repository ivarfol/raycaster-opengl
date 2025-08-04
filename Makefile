linux: raycast.c ppmparser.c mapparser.c
	gcc raycast.c ppmparser.c mapparser.c -o ray -lglut -lGL -lGLU -lm
