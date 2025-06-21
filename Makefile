raycast: raycast.c
	gcc raycast.c ppmparser.c -o ray -lglut -lGL -lGLU -lm
