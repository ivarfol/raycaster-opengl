raycast: raycast.c
	gcc raycast.c ppmparser.c HSV.c -o ray -lglut -lGL -lGLU -lm
